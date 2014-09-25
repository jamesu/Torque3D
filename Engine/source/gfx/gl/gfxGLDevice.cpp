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

#include "platform/platform.h"
#include "gfx/gl/gfxGLDevice.h"

#include "gfx/gfxCubemap.h"
#include "gfx/screenshot.h"
#include "gfx/gfxDrawUtil.h"

#include "gfx/gl/gfxGLEnumTranslate.h"
#include "gfx/gl/gfxGLVertexBuffer.h"
#include "gfx/gl/gfxGLPrimitiveBuffer.h"
#include "gfx/gl/gfxGLTextureTarget.h"
#include "gfx/gl/gfxGLTextureManager.h"
#include "gfx/gl/gfxGLTextureObject.h"
#include "gfx/gl/gfxGLCubemap.h"
#include "gfx/gl/gfxGLCardProfiler.h"
#include "gfx/gl/gfxGLWindowTarget.h"
#include "gfx/gl/tGL/tGL.h"
#include "platform/platformDlibrary.h"
#include "gfx/gl/gfxGLShader.h"
#include "gfx/primBuilder.h"
#include "console/console.h"
#include "gfx/gl/gfxGLOcclusionQuery.h"
#include "gfx/gl/gfxGLUtils.h"
#include "gfx/gl/gfxGLVAO.h"
#include "materials/shaderData.h"
#include "gfx/gl/gfxGLAppleFence.h"

#include <stdio.h>
U32 gNumProgramChanges = 0;

GFXAdapter::CreateDeviceInstanceDelegate GFXGLDevice::mCreateDeviceInstance(GFXGLDevice::createInstance); 

GFXDevice *GFXGLDevice::createInstance( U32 adapterIndex )
{
   return new GFXGLDevice(adapterIndex);
}

namespace GL
{
   extern void gglPerformBinds();
   extern void gglPerformExtensionBinds(void *context);
}

void loadGLCore()
{
   static bool coreLoaded = false; // Guess what this is for.
   if(coreLoaded)
      return;
   coreLoaded = true;
   
   // Make sure we've got our GL bindings.
   GL::gglPerformBinds();
}

void loadGLExtensions(void *context)
{
   static bool extensionsLoaded = false;
   if(extensionsLoaded)
      return;
   extensionsLoaded = true;
   
   GL::gglPerformExtensionBinds(context);
}

void STDCALL glDebugCallback(GLenum source, GLenum type, GLuint id,
   GLenum severity, GLsizei length, const GLchar* message, void* userParam)
{
   printf("OPENGL: %s", message);
}

void STDCALL glAmdDebugCallback(GLuint id, GLenum category, GLenum severity, GLsizei length,
   const GLchar* message,GLvoid* userParam)
{
   printf("OPENGL: %s",message);
}

void GFXGLDevice::initGLState(void* context)
{
   // load core again
   loadGLCore();

   // load exts for the first time
   loadGLExtensions(context);

   GFXGLEnumTranslate::init();

   mCurrentFBORead = 0;
   mCurrentFBOWrite = 0;

   

   mLastClearColor = ColorI(0,0,0,0);
   mLastClearZ = 0;
   mLastClearStencil = 0;

   glClearColor(0,0,0,0);
   glClearDepth(0);
   glClearStencil(0);

   const GLubyte* renderer = glGetString (GL_RENDERER); // get renderer string
   const GLubyte* version = glGetString (GL_VERSION); // version as a string
   
   // Deal with the card profiler here when we know we have a valid context.
   mCardProfiler = new GFXGLCardProfiler();
   mCardProfiler->init(); 
   glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, (GLint*)&mMaxShaderTextures);
   glGetIntegerv(GL_MAX_TEXTURE_UNITS, (GLint*)&mMaxFFTextures);
   glGetIntegerv(GL_MAX_COLOR_ATTACHMENTS, (GLint*)&mMaxTRColors);
   mMaxTRColors = getMin( mMaxTRColors, (U32)(GFXTextureTarget::MaxRenderSlotId-1) );
	
	
   mNumSamplers = getMin((U32)TEXTURE_STAGE_COUNT, (U32)mMaxShaderTextures);
   
   glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
   
   // Apple's drivers lie and claim that everything supports fragment shaders.  Conveniently they don't lie about the number
   // of supported image units.  Checking for 16 or more image units ensures that we don't try and use pixel shaders on
   // cards which don't support them.
   if(mCardProfiler->queryProfile("GL::suppFragmentShader") && mMaxShaderTextures >= 16)
      setPixelShaderVersion(2.0f);
   else
      setPixelShaderVersion(0.0f);
   
   // MACHAX - Setting mPixelShaderVersion to 3.0 will allow Advanced Lighting
   // to run.  At the time of writing (6/18) it doesn't quite work yet.
   if(mCardProfiler->queryProfile("GL::suppGLSL3.0", false))
      setPixelShaderVersion(3.0f);
      
   mSupportsAnisotropic = mCardProfiler->queryProfile( "GL::suppAnisotropic" );

   GFXGLBufferStorage::smAllowBufferStorage = mCardProfiler->queryProfile("GL::suppBufferStorage");
   GFXGLBufferStorage::smAllowPinnedMemory = mCardProfiler->queryProfile("GL::suppPinnedMemory");
   GFXGLBufferStorage::smAllowArbSync = mCardProfiler->queryProfile("GL::suppArbSync");

   mUseFences = GFXGLBufferStorage::smAllowBufferStorage || GFXGLBufferStorage::smAllowPinnedMemory;

#if TORQUE_DEBUGX
   if( gglHasExtension(KHR_debug)||gglHasExtension(ARB_debug_output))
   {
      glEnable(GL_DEBUG_OUTPUT);
      glDebugMessageCallback(glDebugCallback, NULL);      
      glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
      GLuint unusedIds = 0;
      glDebugMessageControl(GL_DONT_CARE,
            GL_DONT_CARE,
            GL_DONT_CARE,
            0,
            &unusedIds,
            GL_TRUE);
   }
   else if(gglHasExtension(AMD_debug_output))
   {
      glEnable(GL_DEBUG_OUTPUT);
      glDebugMessageCallbackAMD(glAmdDebugCallback, NULL);      
      glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
      GLuint unusedIds = 0;
      glDebugMessageEnableAMD(GL_DONT_CARE, GL_DONT_CARE, 0,&unusedIds, GL_TRUE);
   }
#endif
   
   mCurrentVAO = 0;
   mCurrentActiveTextureUnit = -1;
   mCurrentShader = NULL;
   mCurrentProgram = 0;

   // Alloc fences for streaming buffer wait
   mFences = new GFXFence*[GFX_MAX_FORWARD_FRAMES];
   for (U32 i=0; i<GFX_MAX_FORWARD_FRAMES; i++)
   {
      mFences[i] = createFence();
   }
   mCurrentFence = -1;
   
   // Set root VAO
   mRootVAO = allocVAO(NULL);
   
   _setVAO(mRootVAO, true);
   mRootVAO->updateState(true);
   
   CHECK_GL_ERROR();
}

GFXGLDevice::GFXGLDevice(U32 adapterIndex) :
   mAdapterIndex(adapterIndex),
   mDrawInstancesCount(0),
   m_mCurrentWorld(true),
   m_mCurrentView(true),
   mContext(NULL),
   mPixelFormat(NULL),
   mPixelShaderVersion(0.0f),
   mMaxShaderTextures(2),
   mMaxFFTextures(2),
   mMaxTRColors(1),
   mClip(0, 0, 0, 0),
   mCurrentShader( NULL ),
   mCurrentProgram( 0 ),
   mFutureVertexDecl( NULL ),
	mNumSamplers(0),
   mCurrentVAO(0),
   mWindowRT(NULL),
   mCurrentVertexBufferId(0),
   mFences(0)
{
   dMemset(mEnabledVertexAttributes, '\0', sizeof(mEnabledVertexAttributes));

   GFXGLEnumTranslate::init(); // initial init, we do more once we have a context

   GFXVertexColor::setSwizzle( &Swizzles::rgba );
   mDeviceSwizzle32 = &Swizzles::rgba;
   mDeviceSwizzle24 = &Swizzles::rgb;

   mTextureManager = new GFXGLTextureManager();
   gScreenShot = new ScreenShotGL();

   for(U32 i = 0; i < TEXTURE_STAGE_COUNT; i++)
   {
      mActiveTextureType[i] = GL_ZERO;
      mActiveTextureId[i] = (U32)-1;
   }
}

GFXGLDevice::~GFXGLDevice()
{
   // Free the vertex declarations.
   VertexDeclMap::Iterator iter = mVertexDecls.begin();
   for ( ; iter != mVertexDecls.end(); iter++ )
      delete iter->value;
	
   mCurrentStateBlock = NULL;

   // Wait for fences to finish
   if (mFences)
   {
      for (U32 i=0; i<GFX_MAX_FORWARD_FRAMES; i++)
      {
         mFences[i]->block();
      }
      delete[] mFences;
      mFences = NULL;
   }

   // Forcibly clean up the pools
   mVolatilePBList.setSize(0);
   mVolatileVBList.setSize(0);

   // Clear out our current texture references
   for (U32 i = 0; i < TEXTURE_STAGE_COUNT; i++)
   {
      mCurrentTexture[i] = NULL;
      mNewTexture[i] = NULL;
      mCurrentCubemap[i] = NULL;
      mNewCubemap[i] = NULL;
   }

   mRTStack.clear();
   mCurrentRT = NULL;

   if( mTextureManager )
   {
      mTextureManager->zombify();
      mTextureManager->kill();
   }

   GFXResource* walk = mResourceListHead;
   while(walk)
   {
      walk->zombify();
      walk = walk->getNextResource();
   }
      
   if( mCardProfiler )
      SAFE_DELETE( mCardProfiler );

   SAFE_DELETE( gScreenShot );
}

void GFXGLDevice::zombify()
{
   mTextureManager->zombify();
   
   _setCurrentFBO(0);
   _setActiveTexture(0);
   
   mCurrentActiveTextureUnit = -1;
   mCurrentVAO = 0;
   mCurrentVertexBufferId = 0;
	
   GFXResource* walk = mResourceListHead;
   while(walk)
   {
      walk->zombify();
      walk = walk->getNextResource();
   }
   
   // Switch to default VAO if applicable
   if (gglHasExtension(APPLE_vertex_array_object))
   {
      glBindVertexArrayAPPLE(0);
   }
   else if (gglHasExtension(ARB_vertex_array_object))
   {
      glBindVertexArray(0);
   }
   
   // Clear attributes state
   for (U32 i=0; i<GFXGLDevice::GL_NumVertexAttributes; i++)
   {
      glDisableVertexAttribArray(i);
   }
}

void GFXGLDevice::resurrect()
{
   GFXResource* walk = mResourceListHead;
   while(walk)
   {
      walk->resurrect();
      walk = walk->getNextResource();
   }
   mTextureManager->resurrect();
   
   _setVAO(mRootVAO, true);
	
	// force reset state
	updateStates(true);
}

GFXGLPrimitiveBuffer* GFXGLDevice::findPBPool( U32 indicesNeeded )
{
   PROFILE_SCOPE( GFXGLDevice_findPBPool );
   AssertFatal(indicesNeeded <= MAX_DYNAMIC_INDICES, "Cannot get more than MAX_DYNAMIC_VERTS in a volatile buffer. Up the constant!");

   // Verts needed is ignored on the base device, 360 is different
   for( U32 i=0; i<mVolatilePBList.size(); i++ )
   {
      if (!mVolatilePBList[i]->mClearAtFrameEnd && mVolatilePBList[i]->getStorageIndicesFree() > indicesNeeded)
      {
         return mVolatilePBList[i];
      }
      else
      {
         mVolatilePBList[i]->mClearAtFrameEnd = true;
      }
   }

   return NULL;
}

GFXGLPrimitiveBuffer* GFXGLDevice::createPBPool( )
{
   PROFILE_SCOPE( GFXGLDevice_createPBPool );

   // this is a bit funky, but it will avoid problems with (lack of) copy constructors
   //    with a push_back() situation
   mVolatilePBList.increment();
   StrongRefPtr<GFXGLPrimitiveBuffer> newBuff;
   mVolatilePBList.last() = new GFXGLPrimitiveBuffer();
   newBuff = mVolatilePBList.last();

   newBuff->mIndexCount   = 0;
   newBuff->mBufferType = GFXBufferTypeVolatile;
   newBuff->mDevice = this;

   newBuff->mStorage = GFXGLBufferStorage::createBuffer(this, GL_ELEMENT_ARRAY_BUFFER, GFXBufferTypeVolatile, sizeof(U16) * MAX_DYNAMIC_INDICES);
   return newBuff;
}

GFXGLVertexBuffer* GFXGLDevice::findVBPool( const GFXVertexFormat *vertexFormat, U32 vertsNeeded )
{
   PROFILE_SCOPE( GFXGLDevice_findVBPool );
   AssertFatal(vertsNeeded <= MAX_DYNAMIC_VERTS, "Cannot get more than MAX_DYNAMIC_VERTS in a volatile buffer. Up the constant!");

   // Verts needed is ignored on the base device, 360 is different
   for( U32 i=0; i<mVolatileVBList.size(); i++ )
   {
      if( mVolatileVBList[i]->mVertexFormat.isEqual( *vertexFormat ) )
      {
         if (!mVolatileVBList[i]->mClearAtFrameEnd && mVolatileVBList[i]->getStorageVertsFree() >= vertsNeeded)
         {
            return mVolatileVBList[i];
         }
         else
         {
            mVolatileVBList[i]->mClearAtFrameEnd = true;
         }
      }
   }

   return NULL;
}

GFXGLVertexBuffer * GFXGLDevice::createVBPool( const GFXVertexFormat *vertexFormat, U32 vertSize )
{
   PROFILE_SCOPE( GFXD3D9Device_createVBPool );

   // this is a bit funky, but it will avoid problems with (lack of) copy constructors
   //    with a push_back() situation
   mVolatileVBList.increment();
   StrongRefPtr<GFXGLVertexBuffer> newBuff;
   mVolatileVBList.last() = new GFXGLVertexBuffer();
   newBuff = mVolatileVBList.last();

   newBuff->mNumVerts   = 0;
   newBuff->mBufferType = GFXBufferTypeVolatile;
   newBuff->mVertexFormat.copy( *vertexFormat );
   newBuff->mVertexSize = vertSize;
   newBuff->mDevice = this;

   // Requesting it will allocate it.
   vertexFormat->getDecl();

   newBuff->mStorage = GFXGLBufferStorage::createBuffer(this, GL_ARRAY_BUFFER, GFXBufferTypeVolatile, vertSize * MAX_DYNAMIC_VERTS);
   return newBuff;
}

GFXVertexBuffer *GFXGLDevice::allocVertexBuffer(   U32 numVerts, 
                                                   const GFXVertexFormat *vertexFormat, 
                                                   U32 vertSize, 
                                                   GFXBufferType bufferType ) 
{ 
   GFXGLVertexBuffer* buf = new GFXGLVertexBuffer( GFX, numVerts, vertexFormat, vertSize, bufferType );
   buf->registerResourceWithDevice(this);
   buf->resurrect();
   return buf;
}

GFXPrimitiveBuffer *GFXGLDevice::allocPrimitiveBuffer( U32 numIndices, U32 numPrimitives, GFXBufferType bufferType ) 
{
   AssertFatal(bufferType != GFXBufferTypeVolatile, "Not implemented");
   GFXGLPrimitiveBuffer* buf = new GFXGLPrimitiveBuffer(GFX, numIndices, numPrimitives, bufferType);
   buf->registerResourceWithDevice(this);
   buf->resurrect();
   return buf;
}

void GFXGLDevice::setVertexStream( U32 stream, GFXVertexBuffer *buffer )
{
   if (stream != 0)
      return;

   // Set the volatile buffer which is used to
   // offset the start index when doing draw calls.
   if ( buffer && buffer->mBufferType == GFXBufferTypeVolatile )
      mVolatileVB = static_cast<GFXGLVertexBuffer*>(buffer);
   else
      mVolatileVB = NULL;
}

void GFXGLDevice::setVertexStreamFrequency( U32 stream, U32 frequency )
{
   // changed in ::onDrawStateChanged
   if (stream == 0)
   {
      mDrawInstancesCount = frequency;
   }
}

void GFXGLDevice::onDrawStateChanged()
{
   // Update the relevant VBO
   if (mCurrentVertexBuffer[0].getPointer())
   {
      GFXGLVAO *vao = static_cast<GFXGLVertexBuffer*>(mCurrentVertexBuffer[0].getPointer())->mVAO;
      vao->setVertexDecl(mFutureVertexDecl);
      
      // Update the VAO if we need to
      for (int i=0; i<VERTEX_STREAM_COUNT; i++)
      {
         // NOTE: since we cannot offset elements for streams other than the first, we need to apply the offset in the VAO
         const GFXGLVertexBuffer *buffer = mCurrentVertexBuffer[i].isValid() ? static_cast<GFXGLVertexBuffer*>(mCurrentVertexBuffer[i].getPointer()) : NULL;
         vao->setVertexStream(i, buffer ? buffer->getHandle() : 0, (buffer && (i > 0)) ? buffer->mVolatileStart * buffer->mVertexSize : 0);
         vao->setVertexStreamFrequency(i, mVertexBufferFrequency[i]);
      }
      vao->setPrimitiveBuffer(mCurrentPrimitiveBuffer.isValid() ? static_cast<GFXGLPrimitiveBuffer*>(mCurrentPrimitiveBuffer.getPointer())->getHandle() : 0);
      
      // Bind & update
      _setVAO(vao);
      vao->updateState();
   }
}

GFXCubemap* GFXGLDevice::createCubemap()
{ 
   GFXGLCubemap* cube = new GFXGLCubemap();
   cube->registerResourceWithDevice(this);
   return cube; 
};

bool GFXGLDevice::beginSceneInternal() 
{
   CHECK_GL_ERROR();
   
   // Block on current fence if we fall behind
   if (mCurrentFence < 0 || mCurrentFence == GFX_MAX_FORWARD_FRAMES)
      mCurrentFence = 0;

   if (mUseFences)
   {
      mFences[mCurrentFence]->block();
   }

   // If we have any volatile VBOS which use this fence, reset them
   for( U32 i=0; i<mVolatileVBList.size(); i++ )
   {
      mVolatileVBList[i]->onFenceDone(mCurrentFence);
   }

   for( U32 i=0; i<mVolatilePBList.size(); i++ )
   {
      mVolatilePBList[i]->onFenceDone(mCurrentFence);
   }

   mCanCurrentlyRender = true;
   return true;
}

void GFXGLDevice::endSceneInternal() 
{
   CHECK_GL_ERROR();

   // nothing to do for opengl
   mCanCurrentlyRender = false;

   // Tell all buffers to mark their position for this fence
   for (U32 i=0; i<mVolatileVBList.size(); i++)
   {
      mVolatileVBList[i]->onFenceMarked(mCurrentFence);
      if( mVolatileVBList[i]->mClearAtFrameEnd )
      {
         mVolatileVBList[i]->dispose();
         mVolatileVBList[i]->mClearAtFrameEnd = false;
      }
   }

   for (U32 i=0; i<mVolatilePBList.size(); i++)
   {
      mVolatilePBList[i]->onFenceMarked(mCurrentFence);
      if( mVolatilePBList[i]->mClearAtFrameEnd )
      {
         mVolatilePBList[i]->dispose();
         mVolatilePBList[i]->mClearAtFrameEnd = false;
      }
   }

   // File a fence for this frame
   if (mUseFences)
   {
      mFences[mCurrentFence]->issue();
   }
   mCurrentFence++;
}

void GFXGLDevice::clear(U32 flags, ColorI color, F32 z, U32 stencil)
{
   // Make sure we have flushed our render target state.
   _updateRenderTargets();
   
   bool writeAllColors = true;
   bool zwrite = true;
   U32 stencilWrite = 0xFFFFFFFF;
   const GFXStateBlockDesc *desc = NULL;
   if (mCurrentGLStateBlock)
   {
      desc = &mCurrentGLStateBlock->getDesc();
      zwrite = desc->zWriteEnable;
      writeAllColors = desc->colorWriteRed && desc->colorWriteGreen && desc->colorWriteBlue && desc->colorWriteAlpha;
      stencilWrite = desc->stencilWriteMask;
   }
   
   if (!writeAllColors)
      glColorMask(true, true, true, true);
   if (!zwrite)
      glDepthMask(true);
   if (stencilWrite != 0xFFFFFFFF)
      glStencilMask(0xFFFFFFFF);
   
   if (mLastClearColor != color)
   {
      ColorF c = color;
      glClearColor(c.red, c.green, c.blue, c.alpha);
      mLastClearColor = color;
   }

   if (mLastClearZ != z)
   {
      glClearDepth(z);
      mLastClearZ = z;
   }

   if (mLastClearStencil != stencil)
   {
      glClearStencil(stencil);
      mLastClearStencil = stencil;
   }

   GLbitfield clearflags = 0;
   clearflags |= (flags & GFXClearTarget)   ? GL_COLOR_BUFFER_BIT : 0;
   clearflags |= (flags & GFXClearZBuffer)  ? GL_DEPTH_BUFFER_BIT : 0;
   clearflags |= (flags & GFXClearStencil)  ? GL_STENCIL_BUFFER_BIT : 0;
	
   glClear(clearflags);
	
   if(!writeAllColors)
      glColorMask(desc->colorWriteRed, desc->colorWriteGreen, desc->colorWriteBlue, desc->colorWriteAlpha);
   
   if(!zwrite)
      glDepthMask(false);
	
   if(stencilWrite != 0xFFFFFFFF)
      glStencilMask(stencilWrite);
}

// Given a primitive type and a number of primitives, return the number of indexes/vertexes used.
GLsizei GFXGLDevice::primCountToIndexCount(GFXPrimitiveType primType, U32 primitiveCount)
{
   switch (primType)
   {
      case GFXPointList :
         return primitiveCount;
         break;
      case GFXLineList :
         return primitiveCount * 2;
         break;
      case GFXLineStrip :
         return primitiveCount + 1;
         break;
      case GFXTriangleList :
         return primitiveCount * 3;
         break;
      case GFXTriangleStrip :
         return 2 + primitiveCount;
         break;
      case GFXTriangleFan :
         return 2 + primitiveCount;
         break;
      default:
         AssertFatal(false, "GFXGLDevice::primCountToIndexCount - unrecognized prim type");
         break;
   }
   
   return 0;
}

void GFXGLDevice::preDrawPrimitive()
{
   if( mStateDirty )
   {
      updateStates();
   }
   
   if(mCurrentShaderConstBuffer)
      setShaderConstBufferInternal(mCurrentShaderConstBuffer);
}

inline void GFXGLDevice::postDrawPrimitive(U32 primitiveCount)
{
   mDeviceStatistics.mDrawCalls++;

   if ( mVertexBufferFrequency[0] > 1 )
      mDeviceStatistics.mPolyCount += primitiveCount * mVertexBufferFrequency[0];
   else
      mDeviceStatistics.mPolyCount += primitiveCount;
}

void GFXGLDevice::drawPrimitive( GFXPrimitiveType primType, U32 vertexStart, U32 primitiveCount ) 
{
   CHECK_GL_ERROR();
   
	
   if( mStateDirty )
   {
      updateStates();
   }
   
   if(mCurrentShaderConstBuffer)
      setShaderConstBufferInternal(mCurrentShaderConstBuffer);   

   if ( mVolatileVB )
      vertexStart += mVolatileVB->mVolatileStart;

   if (mDrawInstancesCount)
      glDrawArraysInstanced(GFXGLPrimType[primType], vertexStart, primCountToIndexCount(primType, primitiveCount), mDrawInstancesCount);
  else
      glDrawArrays(GFXGLPrimType[primType], vertexStart, primCountToIndexCount(primType, primitiveCount));

   CHECK_GL_ERROR();
}

void GFXGLDevice::drawIndexedPrimitive(   GFXPrimitiveType primType, 
                                          U32 startVertex, 
                                          U32 minIndex, 
                                          U32 numVerts, 
                                          U32 startIndex, 
                                          U32 primitiveCount )
{
   CHECK_GL_ERROR();
   
   if( mStateDirty )
   {
      updateStates();
   }
   
   if(mCurrentShaderConstBuffer)
      setShaderConstBufferInternal(mCurrentShaderConstBuffer);
   
   if ( mVolatileVB )
      startVertex += mVolatileVB->mVolatileStart;
   
   U16* buf = (U16*)(NULL) + startIndex + mCurrentPrimitiveBuffer->mVolatileStart;
   
   if(mDrawInstancesCount)
   {
      glDrawElementsInstancedBaseVertex(GFXGLPrimType[primType], primCountToIndexCount(primType, primitiveCount), GL_UNSIGNED_SHORT, buf, mDrawInstancesCount, startVertex);
   }
   else
      glDrawElementsBaseVertex(GFXGLPrimType[primType], primCountToIndexCount(primType, primitiveCount), GL_UNSIGNED_SHORT, buf, startVertex);
   
   postDrawPrimitive(primitiveCount);

   CHECK_GL_ERROR();
}

void GFXGLDevice::setLightInternal(U32 lightStage, const GFXLightInfo light, bool lightEnable)
{
   // FFP only
}

void GFXGLDevice::setLightMaterialInternal(const GFXLightMaterial mat)
{
   // FFP only
}

void GFXGLDevice::setGlobalAmbientInternal(ColorF color)
{
}

void GFXGLDevice::setTextureInternal(U32 textureUnit, const GFXTextureObject*texture)
{
   GFXGLTextureObject *tex = static_cast<GFXGLTextureObject*>(const_cast<GFXTextureObject*>(texture));
   if (tex)
   {
      tex->bind(textureUnit);
      if (mActiveTextureType[textureUnit] != tex->getBinding())
      {
         if (mActiveTextureType[textureUnit] != GL_ZERO)
            glBindTexture(mActiveTextureType[textureUnit], 0);
         mActiveTextureType[textureUnit] = tex->getBinding();
      }
      mActiveTextureId[textureUnit] = tex->getHandle();
   }
   else if(mActiveTextureType[textureUnit] != GL_ZERO)
   {
      _setActiveTexture(GL_TEXTURE0 + textureUnit);
      glBindTexture(mActiveTextureType[textureUnit], 0);
      mActiveTextureType[textureUnit] = GL_ZERO;
      mActiveTextureId[textureUnit] = 0;
   }
}

void GFXGLDevice::setCubemapInternal(U32 textureUnit, GFXGLCubemap* texture)
{
   if(texture)
   {
      _setActiveTexture(GL_TEXTURE0 + textureUnit);
      if(mActiveTextureType[textureUnit] != GL_TEXTURE_CUBE_MAP && mActiveTextureType[textureUnit] != GL_ZERO)
      {
         glBindTexture(mActiveTextureType[textureUnit], 0);
      }
      mActiveTextureType[textureUnit] = GL_TEXTURE_CUBE_MAP;
      texture->bind(textureUnit);
      mActiveTextureId[textureUnit] = texture->getHandle();
   }
   else if(mActiveTextureType[textureUnit] != GL_ZERO)
   {
      _setActiveTexture(GL_TEXTURE0 + textureUnit);
      glBindTexture(mActiveTextureType[textureUnit], 0);
      mActiveTextureType[textureUnit] = GL_ZERO;
      mActiveTextureId[textureUnit] = 0;
   }
}

void GFXGLDevice::setMatrix( GFXMatrixType mtype, const MatrixF &mat )
{
   // FFP only
}

void GFXGLDevice::updateModelView()
{
   // FFP only
}

void GFXGLDevice::setClipRect( const RectI &inRect )
{
   AssertFatal(mCurrentRT.isValid(), "GFXGLDevice::setClipRect - must have a render target set to do any rendering operations!");

   // Clip the rect against the renderable size.
   Point2I size = mCurrentRT->getSize();
   RectI maxRect(Point2I(0,0), size);
   mClip = inRect;
   mClip.intersect(maxRect);
   
   // Create projection matrix.  See http://www.opengl.org/documentation/specs/man_pages/hardcopy/GL/html/gl/ortho.html
   const F32 left = mClip.point.x;
   const F32 right = mClip.point.x + mClip.extent.x;
   const F32 bottom = mClip.extent.y;
   const F32 top = 0.0f;
   const F32 near = 0.0f;
   const F32 far = 1.0f;
   
   const F32 tx = -(right + left)/(right - left);
   const F32 ty = -(top + bottom)/(top - bottom);
   const F32 tz = -(far + near)/(far - near);
   
   static Point4F pt;
   pt.set(2.0f / (right - left), 0.0f, 0.0f, 0.0f);
   mProjectionMatrix.setColumn(0, pt);
   
   pt.set(0.0f, 2.0f/(top - bottom), 0.0f, 0.0f);
   mProjectionMatrix.setColumn(1, pt);
   
   pt.set(0.0f, 0.0f, -2.0f/(far - near), 0.0f);
   mProjectionMatrix.setColumn(2, pt);
   
   pt.set(tx, ty, tz, 1.0f);
   mProjectionMatrix.setColumn(3, pt);
   
   static MatrixF scaleMat = MatrixF(1).scale(Point3F(1,-1,1));
   static MatrixF translate(true);
   
   // Translate projection matrix.
   if (!mFlipClipRect)
   {
      pt.set(0.0f, -mClip.point.y, 0.0f, 1.0f);
      translate.setColumn(3, pt);
      mProjectionMatrix *= translate;
   }
   else
   {
      pt.set(0.0f, -size.y + size.y - (mClip.point.y + mClip.extent.y), 0.0f, 1.0f);
      translate.setColumn(3, pt);
      mProjectionMatrix *= scaleMat * translate;
   }
   
   static MatrixF mTempMatrix(true);
   setViewMatrix( mTempMatrix );
   setWorldMatrix( mTempMatrix );
   
   setProjectionMatrix(mProjectionMatrix);

   // Set the viewport to the clip rect (with y flip)
   if (!mFlipClipRect)
   {
      RectI viewport(mClip.point.x, size.y - (mClip.point.y + mClip.extent.y), mClip.extent.x, mClip.extent.y);
      setViewport(viewport);
   }
   else
   {
      RectI viewport(mClip.point.x, mClip.point.y, mClip.extent.x, mClip.extent.y);
      setViewport(viewport);
   }
}

/// Creates a state block object based on the desc passed in.  This object
/// represents an immutable state.
GFXStateBlockRef GFXGLDevice::createStateBlockInternal(const GFXStateBlockDesc& desc)
{
   return GFXStateBlockRef(new GFXGLStateBlock(desc));
}

/// Activates a stateblock
void GFXGLDevice::setStateBlockInternal(GFXStateBlock* block, bool force)
{
   AssertFatal(dynamic_cast<GFXGLStateBlock*>(block), "GFXGLDevice::setStateBlockInternal - Incorrect stateblock type for this device!");
   GFXGLStateBlock* glBlock = static_cast<GFXGLStateBlock*>(block);
   GFXGLStateBlock* glCurrent = static_cast<GFXGLStateBlock*>(mCurrentStateBlock.getPointer());
   if (force)
      glCurrent = NULL;
      
   glBlock->activate(glCurrent); // Doesn't use current yet.
   mCurrentGLStateBlock = glBlock;
}

//------------------------------------------------------------------------------

GFXTextureTarget * GFXGLDevice::allocRenderToTextureTarget()
{
   GFXGLTextureTarget *targ = new GFXGLTextureTarget();
   targ->registerResourceWithDevice(this);
   return targ;
}

GFXFence * GFXGLDevice::createFence()
{
   GFXFence *fence = NULL;
   
   if (gglHasExtension(ARB_sync))
   {
      fence = new GFXGLSyncFence( this );
   }
   else if (gglHasExtension(APPLE_fence))
   {
      fence = new GFXGLAppleFence( this );
   }
   
   if(!fence)
   {
      fence = new GFXGeneralFence( this );
   }
   
   fence->registerResourceWithDevice(this);
   return fence;
}

GFXOcclusionQuery* GFXGLDevice::createOcclusionQuery()
{   
   GFXOcclusionQuery *query = new GFXGLOcclusionQuery( this );
   query->registerResourceWithDevice(this);
   return query;
}

void GFXGLDevice::setupGenericShaders( GenericShaderType type ) 
{
   AssertFatal(type != GSTargetRestore, "");

   if( mGenericShader[GSColor] == NULL )
   {
      ShaderData *shaderData;

      shaderData = new ShaderData();
      shaderData->setField("OGLVertexShaderFile", "shaders/common/fixedFunction/gl/colorV.glsl");
      shaderData->setField("OGLPixelShaderFile", "shaders/common/fixedFunction/gl/colorP.glsl");
      shaderData->setField("pixVersion", "2.0");
      shaderData->registerObject();
      mGenericShader[GSColor] =  shaderData->getShader();
      mGenericShaderBuffer[GSColor] = mGenericShader[GSColor]->allocConstBuffer();
      mModelViewProjSC[GSColor] = mGenericShader[GSColor]->getShaderConstHandle( "$modelview" );

      shaderData = new ShaderData();
      shaderData->setField("OGLVertexShaderFile", "shaders/common/fixedFunction/gl/modColorTextureV.glsl");
      shaderData->setField("OGLPixelShaderFile", "shaders/common/fixedFunction/gl/modColorTextureP.glsl");
      shaderData->setSamplerName("$diffuseMap", 0);
      shaderData->setField("pixVersion", "2.0");
      shaderData->registerObject();
      mGenericShader[GSModColorTexture] = shaderData->getShader();
      mGenericShaderBuffer[GSModColorTexture] = mGenericShader[GSModColorTexture]->allocConstBuffer();
      mModelViewProjSC[GSModColorTexture] = mGenericShader[GSModColorTexture]->getShaderConstHandle( "$modelview" );

      shaderData = new ShaderData();
      shaderData->setField("OGLVertexShaderFile", "shaders/common/fixedFunction/gl/addColorTextureV.glsl");
      shaderData->setField("OGLPixelShaderFile", "shaders/common/fixedFunction/gl/addColorTextureP.glsl");
      shaderData->setSamplerName("$diffuseMap", 0);
      shaderData->setField("pixVersion", "2.0");
      shaderData->registerObject();
      mGenericShader[GSAddColorTexture] = shaderData->getShader();
      mGenericShaderBuffer[GSAddColorTexture] = mGenericShader[GSAddColorTexture]->allocConstBuffer();
      mModelViewProjSC[GSAddColorTexture] = mGenericShader[GSAddColorTexture]->getShaderConstHandle( "$modelview" );

      shaderData = new ShaderData();
      shaderData->setField("OGLVertexShaderFile", "shaders/common/fixedFunction/gl/textureV.glsl");
      shaderData->setField("OGLPixelShaderFile", "shaders/common/fixedFunction/gl/textureP.glsl");
      shaderData->setSamplerName("$diffuseMap", 0);
      shaderData->setField("pixVersion", "2.0");
      shaderData->registerObject();
      mGenericShader[GSTexture] = shaderData->getShader();
      mGenericShaderBuffer[GSTexture] = mGenericShader[GSTexture]->allocConstBuffer();
      mModelViewProjSC[GSTexture] = mGenericShader[GSTexture]->getShaderConstHandle( "$modelview" );
   }

   MatrixF tempMatrix =  mProjectionMatrix * mViewMatrix * mWorldMatrix[mWorldStackSize];  
   mGenericShaderBuffer[type]->setSafe(mModelViewProjSC[type], tempMatrix);

   setShader( mGenericShader[type] );
   setShaderConstBuffer( mGenericShaderBuffer[type] );
}

GFXShader* GFXGLDevice::createShader()
{
   GFXGLShader* shader = new GFXGLShader();
   shader->registerResourceWithDevice( this );
   return shader;
}

bool sBreakOnShaderSwitch;

void GFXGLDevice::setShader( GFXShader *shader )
{
   GLuint program = shader ? static_cast<GFXGLShader*>(shader)->getProgram() : 0;
   if(mCurrentProgram == program)
      return;
   
   if (sBreakOnShaderSwitch)
   {
      int dummy = 1;
   }

   glUseProgram(program);
   mCurrentProgram = program;
   mCurrentShader = shader;
   gNumProgramChanges++;
}

void GFXGLDevice::disableShaders()
{
   setupGenericShaders();
}

void GFXGLDevice::setShaderConstBufferInternal(GFXShaderConstBuffer* buffer)
{
   static_cast<GFXGLShaderConstBuffer*>(buffer)->activate();
}

U32 GFXGLDevice::getNumSamplers() const
{
	return mNumSamplers;
}



GFXTextureObject* GFXGLDevice::getDefaultDepthTex() const
{
   if(mWindowRT.isValid())
      return static_cast<GFXGLWindowTarget*>( mWindowRT.getPointer() )->mBackBufferDepthTex.getPointer();
   
   return NULL;
}


U32 GFXGLDevice::getNumRenderTargets() const
{
   return mMaxTRColors; 
}

void GFXGLDevice::_updateRenderTargets()
{
   _setActiveTexture(GL_TEXTURE0);
   
   if ( mRTDirty || mCurrentRT->isPendingState() )
   {
      if ( mRTDeactivate )
      {
         mRTDeactivate->deactivate();
         mRTDeactivate = NULL;   
      }
      
      // NOTE: The render target changes is not really accurate
      // as the GFXTextureTarget supports MRT internally.  So when
      // we activate a GFXTarget it could result in multiple calls
      // to SetRenderTarget on the actual device.
      mDeviceStatistics.mRenderTargetChanges++;

      GFXGLTextureTarget *tex = dynamic_cast<GFXGLTextureTarget*>( mCurrentRT.getPointer() );
      if ( tex )
      {
         tex->applyState();
         tex->makeActive();
      }
      else
      {
         GFXGLWindowTarget *win = dynamic_cast<GFXGLWindowTarget*>( mCurrentRT.getPointer() );
         AssertFatal( win != NULL, 
                     "GFXGLDevice::_updateRenderTargets() - invalid target subclass passed!" );
         
         win->makeActive();
         
         
         if( win->mContext != static_cast<GFXGLDevice*>(GFX)->mContext )
         {
            mRTDirty = false;
            GFX->updateStates(true);
         }
      }
      
      mRTDirty = false;
   }
   
   if ( mViewportDirty )
   {
		AssertFatal(mViewport.extent.x != 0, "Invalid viewport");
      glViewport( mViewport.point.x, mViewport.point.y, mViewport.extent.x, mViewport.extent.y ); 
      mViewportDirty = false;
   }
}

GFXFormat GFXGLDevice::selectSupportedFormat(   GFXTextureProfile* profile, 
                                                const Vector<GFXFormat>& formats, 
                                                bool texture, 
                                                bool mustblend,
                                                bool mustfilter )
{
   for(U32 i = 0; i < formats.size(); i++)
   {
      // Single channel textures are not supported by FBOs.
      if(profile->testFlag(GFXTextureProfile::RenderTarget) && (formats[i] == GFXFormatA8 || formats[i] == GFXFormatL8 || formats[i] == GFXFormatL16))
         continue;
      if(GFXGLTextureInternalFormat[formats[i]] == GL_ZERO)
         continue;
      
      return formats[i];
   }
   
   return GFXFormatR8G8B8A8;
}

void GFXGLDevice::setVertexDecl( const GFXVertexDecl *aDecl )
{
   const GFXGLVertexDecl *decl = static_cast<const GFXGLVertexDecl*>(aDecl);
   mFutureVertexDecl = decl;
}

GFXVertexDecl* GFXGLDevice::allocVertexDecl( const GFXVertexFormat *vertexFormat ) 
{
   GFXGLVertexDecl *decl = mVertexDecls[vertexFormat->getDescription()];
   if ( decl )
      return decl;
	
   decl = new GFXGLVertexDecl();
   decl->mElements.reserve(vertexFormat->getElementCount());
   //Con::printf("allocVertexDecl %x sz %u :: %s", decl, vertexFormat->getSizeInBytes(), vertexFormat->getDescription().c_str());

   const U32 formatSize = vertexFormat->getSizeInBytes();

   // Loop thru the vertex format elements adding the array state...
   U32 texCoordIndex = 0;
   U8* offsets[4] = { 0 };
   U32 sizes[4] = { 0 };

   for ( U32 i=0; i < vertexFormat->getElementCount(); i++ )
   {
      const GFXVertexElement &element = vertexFormat->getElement( i );
      decl->mElements.increment();
      GFXGLVertexDecl::Element &glElement = decl->mElements.last();

      glElement.streamIdx = element.getStreamIndex();

      if ( element.isSemantic( GFXSemantic::POSITION ) )
      {
         glElement.attrIndex = GFXGLDevice::GL_VertexAttrib_Position;
         if(glElement.attrIndex == -1)
         {
            decl->mElements.pop_back();
            offsets[element.getStreamIndex()] += element.getSizeInBytes();
            continue;
         }
         glElement.elementCount = element.getSizeInBytes() / 4;
         glElement.normalized = false;
         glElement.type = GL_FLOAT;
         glElement.pointerFirst = offsets[element.getStreamIndex()];
         offsets[element.getStreamIndex()] += element.getSizeInBytes();
      }
      else if ( element.isSemantic( GFXSemantic::NORMAL ) )
      {
         glElement.attrIndex = GFXGLDevice::GL_VertexAttrib_Normal;
         if(glElement.attrIndex == -1)     
         {
            decl->mElements.pop_back();
            offsets[element.getStreamIndex()] += element.getSizeInBytes();
            continue;
         }
         glElement.elementCount = 3;
         glElement.normalized = false;
         glElement.type = GL_FLOAT;
         glElement.pointerFirst = offsets[element.getStreamIndex()];
         offsets[element.getStreamIndex()] += element.getSizeInBytes();
      }
      else if ( element.isSemantic( GFXSemantic::TANGENT ) )
      {
         glElement.attrIndex = GFXGLDevice::GL_VertexAttrib_Tangent;
         if(glElement.attrIndex == -1)     
         {
            decl->mElements.pop_back();
            offsets[element.getStreamIndex()] += element.getSizeInBytes() * 3;
            continue;
         }
         glElement.elementCount = 3;
         glElement.normalized = false;
         glElement.type = GL_FLOAT;
         glElement.pointerFirst = offsets[element.getStreamIndex()];
         offsets[element.getStreamIndex()] += element.getSizeInBytes();
      }
      else if ( element.isSemantic( GFXSemantic::TANGENTW ) )
      {
         glElement.attrIndex = GFXGLDevice::GL_VertexAttrib_TangentW;
         if(glElement.attrIndex == -1)     
         {
            decl->mElements.pop_back();
            offsets[element.getStreamIndex()] += element.getSizeInBytes();
            continue;
         }
         glElement.elementCount = 3;
         glElement.normalized = false;
         glElement.type = GL_FLOAT;
         glElement.pointerFirst = offsets[element.getStreamIndex()];
         offsets[element.getStreamIndex()] += element.getSizeInBytes();
      }
      else if ( element.isSemantic( GFXSemantic::BINORMAL ) )
      {
         glElement.attrIndex = GFXGLDevice::GL_VertexAttrib_Binormal;
         if(glElement.attrIndex == -1)     
         {
            decl->mElements.pop_back();
            offsets[element.getStreamIndex()] += element.getSizeInBytes();
            continue;
         }
         glElement.elementCount = 3;
         glElement.normalized = false;
         glElement.type = GL_FLOAT;
         glElement.pointerFirst = offsets[element.getStreamIndex()];
         offsets[element.getStreamIndex()] += element.getSizeInBytes();
      }
      else if ( element.isSemantic( GFXSemantic::COLOR ) )
      {
         glElement.attrIndex = GFXGLDevice::GL_VertexAttrib_Color;
         if(glElement.attrIndex == -1)     
         {
            decl->mElements.pop_back();
            offsets[element.getStreamIndex()] += element.getSizeInBytes();
            continue;
         }
         glElement.elementCount = element.getSizeInBytes();
         glElement.normalized = true;
         glElement.type = GL_UNSIGNED_BYTE;
         glElement.pointerFirst = offsets[element.getStreamIndex()];
         offsets[element.getStreamIndex()] += element.getSizeInBytes();
      }
      else if ( element.isSemantic( GFXSemantic::BLENDWEIGHT ) )
      {
         glElement.attrIndex = GFXGLDevice::GL_VertexAttrib_BlendWeight;
         if(glElement.attrIndex == -1)
         {
            decl->mElements.pop_back();
            offsets[element.getStreamIndex()] += element.getSizeInBytes();
            continue;
         }
         glElement.elementCount = 4;
         glElement.normalized = false;
         glElement.type = GL_FLOAT;
         glElement.pointerFirst = offsets[element.getStreamIndex()];
         offsets[element.getStreamIndex()] += element.getSizeInBytes();
      }
      else if ( element.isSemantic( GFXSemantic::BLENDINDICES ) )
      {
         glElement.attrIndex = GFXGLDevice::GL_VertexAttrib_BlendIndex;
         if(glElement.attrIndex == -1)
         {
            decl->mElements.pop_back();
            offsets[element.getStreamIndex()] += element.getSizeInBytes();
            continue;
         }
         glElement.elementCount = 4;
         glElement.normalized = false;
         glElement.type = GL_UNSIGNED_BYTE;
         glElement.pointerFirst = offsets[element.getStreamIndex()];
         offsets[element.getStreamIndex()] += element.getSizeInBytes();
      }
      else // Everything else is a texture coordinate.
      {
         String name = element.getSemantic();
         glElement.elementCount = element.getSizeInBytes() / 4;
         glElement.attrIndex = GFXGLDevice::GL_VertexAttrib_TexCoord0 + texCoordIndex;
         if(glElement.attrIndex == -1)     
         {
            decl->mElements.pop_back();
            offsets[element.getStreamIndex()] += element.getSizeInBytes();
            continue;
         }
            
         glElement.normalized = false;
         glElement.type = GL_FLOAT;
         glElement.pointerFirst = offsets[element.getStreamIndex()];
         offsets[element.getStreamIndex()] += element.getSizeInBytes();
         
         AssertFatal(glElement.attrIndex <= GFXGLDevice::GL_VertexAttrib_TexCoord7, "Too many texture coords!");

         ++texCoordIndex;
      }
   }

   // Calculate stride for each stream element
   for ( U32 i=0; i < decl->mElements.size(); i++ )
   {
      GFXGLVertexDecl::Element &glElement = decl->mElements[i];
      glElement.stride = (U32)offsets[glElement.streamIdx];
   }

   // Store it in the cache.
   mVertexDecls[vertexFormat->getDescription()] = decl;
	
   return decl;
}

void GFXGLDevice::updatePrimitiveBuffer( GFXPrimitiveBuffer *newBuffer )
{
   // Deferred to VBO
}


void GFXGLDevice::enterDebugEvent(ColorI color, const char *name)
{
	if (gglHasExtension(EXT_debug_marker))
	{
		glPushGroupMarkerEXT(dStrlen(name), name);
	}
}

void GFXGLDevice::leaveDebugEvent()
{
	if (gglHasExtension(EXT_debug_marker))
	{
		glPopGroupMarkerEXT();
	}
}

void GFXGLDevice::setDebugMarker(ColorI color, const char *name)
{
	if (gglHasExtension(EXT_debug_marker))
	{
		glInsertEventMarkerEXT(dStrlen(name), name);
	}
}

void GFXGLDevice::_setWorkingVertexBuffer(GLuint idx, bool dirtyState)
{
   if (mCurrentVertexBufferId != idx)
   {
      glBindBuffer(GL_ARRAY_BUFFER, idx);
      mCurrentVertexBufferId = idx;
      mVertexBufferDirty[0] = true;
      mStateDirty = true;
   }
}

void GFXGLDevice::_setWorkingIndexBuffer(GLuint idx, bool dirtyState)
{
   if (!mCurrentVAO)
      activateRootVAO();
   
   if (mCurrentVAO->mCurrentPrimitiveBuffer != idx)
   {
      activateRootVAO();
      glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, idx);
      //Con::printf("!!GL_ELEMENT_ARRAY_BUFFER NOW %u", idx);
      mCurrentVAO->mCurrentPrimitiveBuffer = idx;
      mPrimitiveBufferDirty = true;
      mStateDirty = true;
   }
   //else
   //{
   //   Con::printf("!!We think GL_ELEMENT_ARRAY_BUFFER is %u. VAO is %u", idx, mCurrentVAO->getHandle());
   //}
}

void GFXGLDevice::_setActiveTexture(GLuint idx)
{
   if (mCurrentActiveTextureUnit != idx)
   {
      glActiveTexture(idx);
      mCurrentActiveTextureUnit = idx;
   }
}

void GFXGLDevice::_setVAO(GFXGLVAO *vao, bool force)
{
   //Con::printf("!!We think VAO<%x> is %u. Now %u.", mCurrentVAO, mCurrentVAO ? mCurrentVAO->getHandle() : 0, vao->getHandle());
   if (mCurrentVAO != vao)
   {
      if (gglHasExtension(APPLE_vertex_array_object))
      {
         glBindVertexArrayAPPLE(vao->getHandle());
      }
      else if (gglHasExtension(ARB_vertex_array_object))
      {
          glBindVertexArray(vao->getHandle());
      }
      mCurrentVAO = vao;
   }
}

GFXGLVAO *GFXGLDevice::allocVAO(GFXGLVertexBuffer *rootBufffer)
{
   GFXGLVAO *vao = new GFXGLVAO();
   vao->registerResourceWithDevice(this);
   vao->resurrect();
   return vao;
}

GFXGLVAO *GFXGLDevice::getRootVAO()
{
   return mRootVAO.getPointer();
}

void GFXGLDevice::activateRootVAO()
{
   _setVAO(mRootVAO, false);
}

void GFXGLDevice::_setTempBoundTexture(GLenum typeId, GLuint texId)
{
   if (mActiveTextureType[0] != typeId || texId != mActiveTextureId[0])
   {
      if (texId != 0)
      {
         if(mActiveTextureType[0] != GL_ZERO && mActiveTextureType[0] != typeId) { glBindTexture(mActiveTextureType[0], 0); }
         mActiveTextureType[0] = typeId;
         mActiveTextureId[0] = texId;
         glBindTexture(mActiveTextureType[0], texId);
      }
      else if(mActiveTextureType[0] != GL_ZERO)
      {
         glBindTexture(mActiveTextureType[0], texId);
         mActiveTextureType[0] = GL_ZERO;
         mActiveTextureId[0] = 0;
      }
   }
   
   mTextureDirty[0] = true;
   mStateDirty = true;
}

extern U32 GFXMacGetTotalVideoMemory(U32 adapterIdx);

U32 GFXGLDevice::getTotalVideoMemory()
{
#ifdef TORQUE_OS_MAC
   return GFXMacGetTotalVideoMemory(mAdapterIndex);
#else
   // Source: http://www.opengl.org/registry/specs/ATI/meminfo.txt
   if( gglHasExtension(ATI_meminfo) )
   {
      GLint mem[4] = {0};
      glGetIntegerv(GL_TEXTURE_FREE_MEMORY_ATI, mem);  // Retrieve the texture pool
      
      /* With mem[0] i get only the total memory free in the pool in KB
      *
      * mem[0] - total memory free in the pool
      * mem[1] - largest available free block in the pool
      * mem[2] - total auxiliary memory free
      * mem[3] - largest auxiliary free block
      */

      return  mem[0] / 1024;
   }
   
   //source http://www.opengl.org/registry/specs/NVX/gpu_memory_info.txt
   else if( gglHasExtension(NVX_gpu_memory_info) )
   {
      GLint mem = 0;
      glGetIntegerv(GL_GPU_MEMORY_INFO_TOTAL_AVAILABLE_MEMORY_NVX, &mem);
      return mem / 1024;
   }
   
#ifdef TORQUE_OS_MAC
   // Convert our adapterIndex (i.e. our CGDirectDisplayID) into an OpenGL display mask
   GLuint display = CGDisplayIDToOpenGLDisplayMask((CGDirectDisplayID)mAdapterIndex);
   CGLRendererInfoObj rend;
#if MAC_OS_X_VERSION_MIN_REQUIRED >= MAC_OS_X_VERSION_10_5
   GLint nrend;
#else
   long int nrend;
#endif
   CGLQueryRendererInfo(display, &rend, &nrend);
   if(!nrend)
      return 0;
   
#if MAC_OS_X_VERSION_MIN_REQUIRED >= MAC_OS_X_VERSION_10_5
   GLint vidMem;
#else
   long int vidMem;
#endif
   CGLDescribeRenderer(rend, 0, kCGLRPVideoMemory, &vidMem);
   CGLDestroyRendererInfo(rend);
   
   // convert bytes to MB
   vidMem /= (1024 * 1024);
   
   return vidMem;
#endif

   // TODO OPENGL, add supprt for INTEL cards.
   
   return 0;
#endif
}

void GFXGLDevice::_setCurrentFBORead(GLuint readId)
{
   if (mCurrentFBORead != readId)
   {
      glBindFramebuffer(GL_READ_FRAMEBUFFER, readId);
      mCurrentFBORead = readId;
   }
}

void GFXGLDevice::_setCurrentFBOWrite(GLuint writeId)
{
   if (mCurrentFBOWrite != writeId)
   {
      glBindFramebuffer(GL_DRAW_FRAMEBUFFER, writeId);
      mCurrentFBOWrite = writeId;
   }
}

void GFXGLDevice::_setCurrentFBO(GLuint readWriteId)
{
   if (mCurrentFBORead != readWriteId || mCurrentFBOWrite != readWriteId)
   {
      glBindFramebuffer(GL_FRAMEBUFFER, readWriteId);
      mCurrentFBORead = mCurrentFBOWrite = readWriteId;
   }
}

void GFXGLDevice::zombifyTexturesAndTargets()
{
   mTextureManager->zombify();
   
   GFXResource* walk = mResourceListHead;
   while(walk)
   {
      if (dynamic_cast<GFXTextureTarget*>(walk)) walk->zombify();
      walk = walk->getNextResource();
   }
}

void GFXGLDevice::resurrectTexturesAndTargets()
{
   mTextureManager->resurrect();
   
   GFXResource* walk = mResourceListHead;
   while(walk)
   {
      if (dynamic_cast<GFXTextureTarget*>(walk)) walk->resurrect();
      walk = walk->getNextResource();
   }
}

//
// Register this device with GFXInit
//
class GFXGLRegisterDevice
{
public:
   GFXGLRegisterDevice()
   {
      GFXInit::getRegisterDeviceSignal().notify(&GFXGLDevice::enumerateAdapters);
   }
};

static GFXGLRegisterDevice pGLRegisterDevice;

ConsoleFunction(cycleResources, void, 1, 1, "")
{
   static_cast<GFXGLDevice*>(GFX)->zombify();
   static_cast<GFXGLDevice*>(GFX)->resurrect();
}

GBitmap* ScreenShotGL::_captureBackBuffer()
{
   GFXTarget *target = GFX->getActiveRenderTarget();
   Point2I size = target->getSize();
   
   U8 *pixels = (U8*)dMalloc(size.x * size.y * 3);
   glReadPixels(0, 0, size.x, size.y, GL_RGB, GL_UNSIGNED_BYTE, pixels);
   
   GBitmap * bitmap = new GBitmap;
   bitmap->allocateBitmap(size.x, size.y);
   
   // flip the rows
   for(U32 y = 0; y < size.y; y++)
      dMemcpy(bitmap->getAddress(0, size.y - y - 1), pixels + y * size.x * 3, size.x * 3);
   
   dFree(pixels);
   
   return bitmap;
}

