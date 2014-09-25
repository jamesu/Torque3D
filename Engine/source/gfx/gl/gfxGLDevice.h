//-----------------------------------------------------------------------------
// Copyright (c) 2012 GarageGames, LLC
// Portions Copyright (c) 2013-2014 Mode 7 Limited
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to
// deal in the Software without restriction, including without limitation the
// rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
// sell copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
// IN THE SOFTWARE.
//-----------------------------------------------------------------------------

#ifndef _GFXGLDEVICE_H_
#define _GFXGLDEVICE_H_

//#define BIND_FANCY_VAO

#include "platform/platform.h"

#include "gfx/gfxDevice.h"
#include "gfx/gfxInit.h"

#ifndef T_GL_H
#include "gfx/gl/tGL/tGL.h"
#endif

#include "windowManager/platformWindow.h"
#include "gfx/gfxFence.h"
#include "gfx/gfxResource.h"
#include "gfx/gl/gfxGLStateBlock.h"

class GFXGLVertexBuffer;
class GFXGLPrimitiveBuffer;
class GFXGLTextureTarget;
class GFXGLCubemap;
class GFXGLVertexDecl;
class GFXGLShaderConstBuffer;
class GFXGLVAO;
class GFXGLShader;

class GFXGLDevice : public GFXDevice
{
friend class GFXGLShader;

public:
   enum GL_AttributeLocation
   {
      GL_VertexAttrib_Position = 0,
      GL_VertexAttrib_Normal,
      GL_VertexAttrib_Color,
      GL_VertexAttrib_Tangent,
      GL_VertexAttrib_TangentW,
      GL_VertexAttrib_Binormal,
      GL_VertexAttrib_BlendIndex,
      GL_VertexAttrib_BlendWeight,
      GL_VertexAttrib_TexCoord0,
      GL_VertexAttrib_TexCoord1, // 9
      GL_VertexAttrib_TexCoord2,
      GL_VertexAttrib_TexCoord3,
      GL_VertexAttrib_TexCoord4,
      GL_VertexAttrib_TexCoord5,
      GL_VertexAttrib_TexCoord6,
      GL_VertexAttrib_TexCoord7,

      GL_NumVertexAttributes
   };

   void zombify();
   void resurrect();
   GFXGLDevice(U32 adapterIndex);
   virtual ~GFXGLDevice();

   static void enumerateAdapters( Vector<GFXAdapter*> &adapterList );
   static GFXDevice *createInstance( U32 adapterIndex );

   virtual void init( const GFXVideoMode &mode, PlatformWindow *window = NULL );

   virtual void activate() { }
   virtual void deactivate() { }
   virtual GFXAdapterType getAdapterType() { return OpenGL; }

   virtual void enterDebugEvent(ColorI color, const char *name);
   virtual void leaveDebugEvent();
   virtual void setDebugMarker(ColorI color, const char *name);

   virtual void enumerateVideoModes();

   virtual U32 getTotalVideoMemory();

   virtual GFXCubemap * createCubemap();

   virtual F32 getFillConventionOffset() const { return 0.0f; }


   ///@}

   /// @name Render Target functions
   /// @{

   ///
   virtual GFXTextureTarget *allocRenderToTextureTarget();
   virtual GFXWindowTarget *allocWindowTarget(PlatformWindow *window);
   virtual void _updateRenderTargets();

   ///@}

   /// @name Shader functions
   /// @{
   virtual F32 getPixelShaderVersion() const { return mPixelShaderVersion; }
   virtual void  setPixelShaderVersion( F32 version ) { mPixelShaderVersion = version; }
   
   virtual void setShader(GFXShader* shd);
   virtual void disableShaders(); ///< Equivalent to setShader(NULL)
   
   /// @attention GL cannot check if the given format supports blending or filtering!
   virtual GFXFormat selectSupportedFormat(GFXTextureProfile *profile,
	   const Vector<GFXFormat> &formats, bool texture, bool mustblend, bool mustfilter);
      
   /// Returns the number of texture samplers that can be used in a shader rendering pass
   virtual U32 getNumSamplers() const;

   /// Returns the number of simultaneous render targets supported by the device.
   virtual U32 getNumRenderTargets() const;

   virtual GFXShader* createShader();
      
   virtual void clear( U32 flags, ColorI color, F32 z, U32 stencil );
   virtual bool beginSceneInternal();
   virtual void endSceneInternal();

   virtual void drawPrimitive( GFXPrimitiveType primType, U32 vertexStart, U32 primitiveCount );

   virtual void drawIndexedPrimitive(  GFXPrimitiveType primType, 
                                       U32 startVertex, 
                                       U32 minIndex, 
                                       U32 numVerts, 
                                       U32 startIndex, 
                                       U32 primitiveCount );

   virtual void setClipRect( const RectI &rect );
   virtual const RectI &getClipRect() const { return mClip; }

   virtual void preDestroy() { Parent::preDestroy(); }

   virtual U32 getMaxDynamicVerts() { return MAX_DYNAMIC_VERTS; }
   virtual U32 getMaxDynamicIndices() { return MAX_DYNAMIC_INDICES; }
   
   GFXFence *createFence();
   
   GFXOcclusionQuery* createOcclusionQuery();

   GFXGLStateBlockRef getCurrentStateBlock() { return mCurrentGLStateBlock; }
   
   virtual void setupGenericShaders( GenericShaderType type = GSColor );
   
   ///
   bool supportsAnisotropic() const { return mSupportsAnisotropic; }
   
   GFXTextureObject* getDefaultDepthTex() const;
   
   void _setTempBoundTexture(GLenum typeId, GLuint texId);
   
protected:   
   /// Called by GFXDevice to create a device specific stateblock
   virtual GFXStateBlockRef createStateBlockInternal(const GFXStateBlockDesc& desc);
   /// Called by GFXDevice to actually set a stateblock.
   virtual void setStateBlockInternal(GFXStateBlock* block, bool force);   

   /// Called by base GFXDevice to actually set a const buffer
   virtual void setShaderConstBufferInternal(GFXShaderConstBuffer* buffer);

   virtual void setTextureInternal(U32 textureUnit, const GFXTextureObject*texture);
   virtual void setCubemapInternal(U32 cubemap, GFXGLCubemap* texture);

   virtual void setLightInternal(U32 lightStage, const GFXLightInfo light, bool lightEnable);
   virtual void setLightMaterialInternal(const GFXLightMaterial mat);
   virtual void setGlobalAmbientInternal(ColorF color);

   /// @name State Initalization.
   /// @{

   /// State initalization. This MUST BE CALLED in setVideoMode after the device
   /// is created.
   virtual void initStates() { }

   virtual void setMatrix( GFXMatrixType mtype, const MatrixF &mat );

   virtual void updateModelView();

   virtual GFXVertexBuffer *allocVertexBuffer(  U32 numVerts, 
                                                const GFXVertexFormat *vertexFormat,
                                                U32 vertSize, 
                                                GFXBufferType bufferType );
   virtual GFXPrimitiveBuffer *allocPrimitiveBuffer( U32 numIndices, U32 numPrimitives, GFXBufferType bufferType );
   
   virtual GFXVertexDecl* allocVertexDecl( const GFXVertexFormat *vertexFormat );

   virtual void setVertexDecl( const GFXVertexDecl *decl );

   virtual void setVertexStream( U32 stream, GFXVertexBuffer *buffer );
   virtual void setVertexStreamFrequency( U32 stream, U32 frequency );
   
   virtual void onDrawStateChanged();

   /// For handle with DX9 API texel-to-pixel mapping offset
   virtual bool hasTexelPixelOffset() const { return false; }
	
	virtual void updatePrimitiveBuffer( GFXPrimitiveBuffer *newBuffer );

   virtual void zombifyTexturesAndTargets();
   virtual void resurrectTexturesAndTargets();
   
public:
   void _setActiveTexture(GLuint texId);
   
   void _setVAO(GFXGLVAO *vao, bool dirtyState=true);
   
   // Temporarily sets the vertex & index buffers, changing to the root if the vertex or index buffer is different
   void _setWorkingVertexBuffer(GLuint idx, bool dirtyState=true);
   void _setWorkingIndexBuffer(GLuint idx, bool dirtyState=true);

   GLuint _getWorkingVertexBuffer() { return mCurrentVertexBufferId; }

   void _setCurrentFBORead(GLuint readId);
   void _setCurrentFBOWrite(GLuint readId);
   void _setCurrentFBO(GLuint readWriteId);
   
   GLuint _getCurrentFBORead() { return mCurrentFBORead; }
   GLuint _getCurrentFBOWrite() { return mCurrentFBOWrite; }

   GFXGLVAO *allocVAO(GFXGLVertexBuffer *rootBufffer);
   GFXGLVAO *getRootVAO();
   
   // Activates the root VAO
   void activateRootVAO();

private:
   typedef GFXDevice Parent;
   
   friend class GFXGLTextureObject;
   friend class GFXGLCubemap;
   friend class GFXGLWindowTarget;
   friend class GFXGLPrimitiveBuffer;
   friend class GFXGLVertexBuffer;

   static GFXAdapter::CreateDeviceInstanceDelegate mCreateDeviceInstance;
	
   typedef StrongRefPtr<GFXGLVertexBuffer> GLVBPTR;
   typedef StrongRefPtr<GFXGLPrimitiveBuffer> GLPBPTR;
   Vector<GLVBPTR> mVolatileVBList;
   Vector<GLPBPTR> mVolatilePBList;
	
   /// Used to lookup a vertex declaration for the vertex format.
   /// @see allocVertexDecl
   typedef Map<String,GFXGLVertexDecl*> VertexDeclMap;
   VertexDeclMap mVertexDecls;

   U32 mAdapterIndex;
   U32 mDrawInstancesCount;
   
   GFXShader* mCurrentShader;
   GLuint mCurrentProgram;

   GLuint mCurrentFBORead;
   GLuint mCurrentFBOWrite;

   ColorI mLastClearColor;
   F32 mLastClearZ;
   U32 mLastClearStencil;

   GFXShaderRef mGenericShader[GS_COUNT];
   GFXShaderConstBufferRef mGenericShaderBuffer[GS_COUNT];
   GFXShaderConstHandle *mModelViewProjSC[GS_COUNT];
   GLuint mCurrentVertexBufferId;
   
   /// Since GL does not have separate world and view matrices we need to track them
   MatrixF m_mCurrentWorld;
   MatrixF m_mCurrentView;

   void* mContext;
   void* mPixelFormat;

   F32 mPixelShaderVersion;
   
   bool mSupportsAnisotropic;
   bool mUseFences;
   
   U32 mMaxShaderTextures;
   U32 mMaxFFTextures;

   RectI mClip;

   U32 mMaxTRColors;

   char mMatrixMode;

   
   bool mEnabledVertexAttributes[GL_NumVertexAttributes];

   const GFXGLVertexDecl *mFutureVertexDecl;
	
	
	GFXGLShaderConstBuffer *mInternalConstBufferActive;
   
   GFXGLStateBlockRef mCurrentGLStateBlock;
	
	U32 mNumSamplers;
   
   GLenum mActiveTextureType[TEXTURE_STAGE_COUNT];
   GLuint mActiveTextureId[TEXTURE_STAGE_COUNT];
   
   Vector< StrongRefPtr<GFXGLVertexBuffer> > mVolatileVBs; ///< Pool of existing volatile VBs so we can reuse previously created ones
   Vector< StrongRefPtr<GFXGLPrimitiveBuffer> > mVolatilePBs; ///< Pool of existing volatile PBs so we can reuse previously created ones
   
   GFXGLVertexBuffer *mVolatileVB; // current violate vbo
   
   // Binding state
   GLint mCurrentActiveTextureUnit;
   GFXGLVAO* mCurrentVAO; // NOTE: encapsulates index and vertex buffer state
   
   StrongRefPtr<GFXGLVAO> mRootVAO; // Main VAO
   
   GFXWindowTargetRef mWindowRT;
   
   /// For streaming buffers
   GFXFence** mFences;
   S32 mCurrentFence;

   GLsizei primCountToIndexCount(GFXPrimitiveType primType, U32 primitiveCount);
   void preDrawPrimitive();
   void postDrawPrimitive(U32 primitiveCount);  
   
   GFXGLPrimitiveBuffer* findPBPool( U32 indicesNeeded );
   virtual GFXGLPrimitiveBuffer* createPBPool();

   GFXGLVertexBuffer* findVBPool( const GFXVertexFormat *vertexFormat, U32 vertsNeeded );
   virtual GFXGLVertexBuffer* createVBPool( const GFXVertexFormat *vertexFormat, U32 vertSize );
   
   void initGLState(void* ctx); ///< Guaranteed to be called after all extensions have been loaded, use to init card profiler, shader version, max samplers, etc.
};

#endif
