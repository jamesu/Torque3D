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

#ifndef _SHADERGEN_HLSL_SHADERFEATUREHLSL_H_
#define _SHADERGEN_HLSL_SHADERFEATUREHLSL_H_

#ifndef _SHADERFEATURE_H_
#include "shaderGen/shaderFeature.h"
#endif

struct LangElement;
struct MaterialFeatureData;
struct RenderPassData;


class ShaderFeatureCommon : public ShaderFeature
{
public:
   ShaderFeatureCommon();

   ///
   Var* getOutTexCoord( const char *name,
                        const char *vertexCoordName,
                        const char *type,
                        bool mapsToSampler,
                        bool useTexAnim,
                        const char* texMatrixName,
                        MultiLine *meta,
                        Vector<ShaderComponent*> &componentList );

   /// Returns an input texture coord by name adding it
   /// to the input connector if it doesn't exist.
   static Var* getInTexCoord( const char *name,
                              const char *type,
                              bool mapsToSampler,
                              Vector<ShaderComponent*> &componentList );

   static Var* getInColor( const char *name,
                           const char *type,
                           Vector<ShaderComponent*> &componentList );

   ///
   static Var* addOutVpos( MultiLine *meta,
                           Vector<ShaderComponent*> &componentList );

   /// Returns the VPOS input register for the pixel shader.
   static Var* getInVpos(  MultiLine *meta,
                           Vector<ShaderComponent*> &componentList );

   /// Returns the VNORMAL input register for the pixel shader.
   static Var* getInVnormal(  MultiLine *meta,
                              Vector<ShaderComponent*> &componentList );

   /// Returns the "objToTangentSpace" transform or creates one if this
   /// is the first feature to need it.
   Var* getOutObjToTangentSpace( Vector<ShaderComponent*> &componentList,
                                 MultiLine *meta,
                                 const MaterialFeatureData &fd );

   /// Returns the existing output "outWorldToTangent" transform or 
   /// creates one if this is the first feature to need it.
   Var* getOutWorldToTangent( Vector<ShaderComponent*> &componentList,
                              MultiLine *meta,
                              const MaterialFeatureData &fd );

   /// Returns the input "worldToTanget" space transform 
   /// adding it to the input connector if it doesn't exist.
   static Var* getInWorldToTangent( Vector<ShaderComponent*> &componentList );

   /// Returns the existing output "outViewToTangent" transform or 
   /// creates one if this is the first feature to need it.
   Var* getOutViewToTangent(  Vector<ShaderComponent*> &componentList,
                              MultiLine *meta,
                              const MaterialFeatureData &fd );

   /// Returns the input "viewToTangent" space transform 
   /// adding it to the input connector if it doesn't exist.
   static Var* getInViewToTangent( Vector<ShaderComponent*> &componentList );

   /// Calculates the world space position in the vertex shader and 
   /// assigns it to the passed language element.  It does not pass 
   /// it across the connector to the pixel shader.
   /// @see addOutWsPosition
   void getWsPosition(  Vector<ShaderComponent*> &componentList,                                       
                        bool useInstancing,
                        MultiLine *meta,
                        LangElement *wsPosition );

   /// Calculates the view space position in the vertex shader and 
   /// assigns it to the passed language element.  It does not pass 
   /// it across the connector to the pixel shader.
   /// @see addOutWsPosition
   void getVsPosition(  Vector<ShaderComponent*> &componentList,                                       
                        bool useInstancing,
                        MultiLine *meta,
                        LangElement *vsPosition );

   /// Calculates the view space normal in the vertex shader and 
   /// assigns it to the passed language element.  It does not pass 
   /// it across the connector to the pixel shader.
   /// @see addOutWsPosition
   void getVsNormal(  Vector<ShaderComponent*> &componentList,                                       
                      bool useInstancing,
                      MultiLine *meta,
                      LangElement *vsNormal );
                      
   /// Adds the "wsPosition" to the input connector if it doesn't exist.
   Var* addOutWsPosition(  Vector<ShaderComponent*> &componentList,                                       
                           bool useInstancing,
                           MultiLine *meta );

   /// Adds the "vsPosition" to the input connector if it doesn't exist.
   Var* addOutVsPosition(  Vector<ShaderComponent*> &componentList,                                       
                           bool useInstancing,
                           MultiLine *meta );

   /// Adds the "vsNormal" to the input connector if it doesn't exist.
   Var* addOutVsNormal(  Vector<ShaderComponent*> &componentList,                                       
                         bool useInstancing,
                         MultiLine *meta );

   /// Returns the input world space position from the connector.
   static Var* getInWsPosition( Vector<ShaderComponent*> &componentList );

   /// Returns the input view space position from the connector.
   static Var* getInVsPosition( Vector<ShaderComponent*> &componentList );

   /// Returns the world space view vector from the wsPosition.
   static Var* getWsView( Var *wsPosition, MultiLine *meta );

   /// Returns the input normal map texture.
   static Var* getNormalMapTex();

   /// martinJ - Searches for the existing sampler and creates it if it doesn't exist
   static Var* findOrCreateSampler( const char* name );

   /// martinJ - Searches for the existing variable and creates it if it doesn't exist
   static Var* findOrCreateVar( const char* name, const char* type, bool uniform );

   ///
   Var* addOutDetailTexCoord( Vector<ShaderComponent*> &componentList, 
                              MultiLine *meta,
                              bool useTexAnim );

   ///
   Var* getObjTrans( Vector<ShaderComponent*> &componentList,                                       
                     bool useInstancing,
                     MultiLine *meta );

   /// martinJ - gets the previous frame's object transform
   Var* getPrevObjTrans( Vector<ShaderComponent*> &componentList,                                       
                         bool useInstancing,
                         MultiLine *meta );

   ///
   Var* getModelView(   Vector<ShaderComponent*> &componentList,                                       
                        bool useInstancing,
                        MultiLine *meta );

   /// martinJ - gets the transform comprised of previousFrameWorldMatrix * currentFrameViewMatrix * currentFrameProjectionMatrix
   Var* getPrevModelView(   Vector<ShaderComponent*> &componentList,                                       
                            bool useInstancing,
                            MultiLine *meta );

   ///
   Var* getWorldView(   Vector<ShaderComponent*> &componentList,                                       
                        bool useInstancing,
                        MultiLine *meta );

   ///
   Var* getInvWorldView(   Vector<ShaderComponent*> &componentList,                                       
                           bool useInstancing,
                           MultiLine *meta );

   // ShaderFeature
   Var* getVertTexCoord( const String &name );
   LangElement* setupTexSpaceMat(  Vector<ShaderComponent*> &componentList, Var **texSpaceMat );
   LangElement* assignColor( LangElement *elem, Material::BlendOp blend, LangElement *lerpElem = NULL, ShaderFeature::OutputTarget outputTarget = ShaderFeature::DefaultTarget );
   LangElement* expandNormalMap( LangElement *sampleNormalOp, LangElement *normalDecl, LangElement *normalVar, const MaterialFeatureData &fd );

   // Retrieves the shader names for common constants and functions
   static const char* getHalfFloatName();
   static const char* getHalfVector2Name();
   static const char* getHalfVector3Name();
   static const char* getHalfVector4Name();
   static const char* getVector2Name();
   static const char* getVector3Name();
   static const char* getVector4Name();
   static const char* getMatrix3x3Name();
   static const char* getMatrix4x3Name();
   static const char* getMatrix4x4Name();
   static const char* getTexture2DFunction();
   static const char* getTexture2DLodFunction();
   static const char* getTextureCubeFunction();

   static GFXAdapterType getCurrentAdapterType();
   static void setCurrentAdapterType(GFXAdapterType type);

protected:
   static const char* smHalfFloatName;
   static const char* smHalfVector2Name;
   static const char* smHalfVector3Name;
   static const char* smHalfVector4Name;
   static const char* smVector2Name;
   static const char* smVector3Name;
   static const char* smVector4Name;
   static const char* smMatrix3x3Name;
   static const char* smMatrix4x3Name;
   static const char* smMatrix4x4Name;
   static const char* smTexture2DFunction;
   static const char* smTexture2DLodFunction;
   static const char* smTextureCubeFunction;
   static GFXAdapterType smCurrentAdapterType;
};

inline const char* ShaderFeatureCommon::getHalfFloatName()
{
  return smHalfFloatName;
}

inline const char* ShaderFeatureCommon::getHalfVector2Name()
{
  return smHalfVector2Name;
}

inline const char* ShaderFeatureCommon::getHalfVector3Name()
{
  return smHalfVector3Name;
}

inline const char* ShaderFeatureCommon::getHalfVector4Name()
{
  return smHalfVector4Name;
}

inline const char* ShaderFeatureCommon::getVector2Name()
{
  return smVector2Name;
}

inline const char* ShaderFeatureCommon::getVector3Name()
{
  return smVector3Name;
}

inline const char* ShaderFeatureCommon::getVector4Name()
{
  return smVector4Name;
}

inline const char* ShaderFeatureCommon::getMatrix3x3Name()
{
  return smMatrix3x3Name;
}

inline const char* ShaderFeatureCommon::getMatrix4x3Name()
{
   return smMatrix4x3Name;
}

inline const char* ShaderFeatureCommon::getMatrix4x4Name()
{
  return smMatrix4x4Name;
}

inline const char* ShaderFeatureCommon::getTexture2DFunction()
{
  return smTexture2DFunction;
}

inline const char* ShaderFeatureCommon::getTexture2DLodFunction()
{
  return smTexture2DLodFunction;
}

inline const char* ShaderFeatureCommon::getTextureCubeFunction()
{
  return smTextureCubeFunction;
}

class NamedFeature : public ShaderFeatureCommon
{
protected:
   String mName;

public:
   NamedFeature( const String &name )
      : mName( name )
   {}

   virtual String getName() { return mName; }
};

class RenderTargetZero : public ShaderFeatureCommon
{
protected:
   ShaderFeature::OutputTarget mOutputTargetMask;
   String mFeatureName;

public:
   RenderTargetZero( const ShaderFeature::OutputTarget target )
      : mOutputTargetMask( target )
   {
      char buffer[256];
      dSprintf(buffer, sizeof(buffer), "Render Target Output = 0.0, output mask %04b", mOutputTargetMask);
      mFeatureName = buffer;
   }

   virtual String getName() { return mFeatureName; }

   virtual void processPix( Vector<ShaderComponent*> &componentList, 
      const MaterialFeatureData &fd );
   
   virtual U32 getOutputTargets( const MaterialFeatureData &fd ) const { return mOutputTargetMask; }
};


/// Vertex position
class VertPosition : public ShaderFeatureCommon
{
public:
   virtual void processVert( Vector<ShaderComponent*> &componentList,
                             const MaterialFeatureData &fd );
                             
   virtual String getName()
   {
      return "Vert Position";
   }

   virtual void determineFeature(   Material *material,
                                    const GFXVertexFormat *vertexFormat,
                                    U32 stageNum,
                                    const FeatureType &type,
                                    const FeatureSet &features,
                                    MaterialFeatureData *outFeatureData );

};


/// Vertex lighting based on the normal and the light 
/// direction passed through the vertex color.
class RTLightingFeat : public ShaderFeatureCommon
{
protected:

   ShaderIncludeDependency mDep;

public:

   RTLightingFeat();

   virtual void processVert( Vector<ShaderComponent*> &componentList,
                             const MaterialFeatureData &fd );

   virtual void processPix( Vector<ShaderComponent*> &componentList, 
                            const MaterialFeatureData &fd );

   virtual Material::BlendOp getBlendOp(){ return Material::None; }
   
   virtual Resources getResources( const MaterialFeatureData &fd );

   virtual String getName()
   {
      return "RT Lighting";
   }
};


/// Base texture
class DiffuseMapFeat : public ShaderFeatureCommon
{
public:
   virtual void processVert( Vector<ShaderComponent*> &componentList,
                             const MaterialFeatureData &fd );

   virtual void processPix( Vector<ShaderComponent*> &componentList, 
                            const MaterialFeatureData &fd );

   virtual Material::BlendOp getBlendOp(){ return Material::LerpAlpha; }

   virtual Resources getResources( const MaterialFeatureData &fd );

   // Sets textures and texture flags for current pass
   virtual void setTexData( Material::StageData &stageDat,
                            const MaterialFeatureData &fd,
                            RenderPassData &passData,
                            U32 &texIndex );
                            
   virtual String getName()
   {
      return "Base Texture";
   }

   static const char* diffuseMapSamplerName;
};


/// Overlay texture
class OverlayTexFeat : public ShaderFeatureCommon
{
public:
   virtual void processVert( Vector<ShaderComponent*> &componentList,
                             const MaterialFeatureData &fd );

   virtual void processPix( Vector<ShaderComponent*> &componentList, 
                            const MaterialFeatureData &fd );

   virtual Material::BlendOp getBlendOp(){ return Material::LerpAlpha; }

   virtual Resources getResources( const MaterialFeatureData &fd );

   // Sets textures and texture flags for current pass
   virtual void setTexData( Material::StageData &stageDat,
                            const MaterialFeatureData &fd,
                            RenderPassData &passData,
                            U32 &texIndex );

   virtual String getName()
   {
      return "Overlay Texture";
   }

   static const char* overlayMapSamplerName;
};


/// Diffuse color
class DiffuseFeature : public ShaderFeatureCommon
{
public:   
   virtual void processPix(   Vector<ShaderComponent*> &componentList, 
                              const MaterialFeatureData &fd );

   virtual Material::BlendOp getBlendOp(){ return Material::None; }

   virtual String getName()
   {
      return "Diffuse Color";
   }
};

/// Diffuse vertex color
class DiffuseVertColorFeature : public ShaderFeatureCommon
{
public:   

   virtual void processVert(  Vector< ShaderComponent* >& componentList,
                              const MaterialFeatureData& fd );
   virtual void processPix(   Vector< ShaderComponent* >&componentList, 
                              const MaterialFeatureData& fd );

   virtual Material::BlendOp getBlendOp(){ return Material::None; }

   virtual String getName()
   {
      return "Diffuse Vertex Color";
   }
};

/// Lightmap
class LightmapFeat : public ShaderFeatureCommon
{
public:
   virtual void processVert(  Vector<ShaderComponent*> &componentList,
                              const MaterialFeatureData &fd );

   virtual void processPix(   Vector<ShaderComponent*> &componentList, 
                              const MaterialFeatureData &fd );

   virtual Material::BlendOp getBlendOp(){ return Material::LerpAlpha; }

   virtual Resources getResources( const MaterialFeatureData &fd );

   // Sets textures and texture flags for current pass
   virtual void setTexData( Material::StageData &stageDat,
                            const MaterialFeatureData &fd,
                            RenderPassData &passData,
                            U32 &texIndex );
                            
   virtual String getName()
   {
      return "Lightmap";
   }

   virtual U32 getOutputTargets( const MaterialFeatureData &fd ) const;
   
   static const char* lightMapSamplerName;
};


/// Tonemap
class TonemapFeat : public ShaderFeatureCommon
{
public:
   virtual void processVert(  Vector<ShaderComponent*> &componentList,
                              const MaterialFeatureData &fd );

   virtual void processPix(   Vector<ShaderComponent*> &componentList, 
                              const MaterialFeatureData &fd );

   virtual Material::BlendOp getBlendOp(){ return Material::LerpAlpha; }

   virtual Resources getResources( const MaterialFeatureData &fd );

   // Sets textures and texture flags for current pass
   virtual void setTexData( Material::StageData &stageDat,
                            const MaterialFeatureData &fd,
                            RenderPassData &passData,
                            U32 &texIndex );
                            
   virtual String getName()
   {
      return "Tonemap";
   }

   virtual U32 getOutputTargets( const MaterialFeatureData &fd ) const;

   static const char* toneMapSamplerName;
};


/// Baked lighting stored on the vertex color
class VertLit : public ShaderFeatureCommon
{
public:
   virtual void processVert( Vector<ShaderComponent*> &componentList,
                             const MaterialFeatureData &fd );

   virtual void processPix( Vector<ShaderComponent*> &componentList, 
                            const MaterialFeatureData &fd );

   virtual Material::BlendOp getBlendOp(){ return Material::None; }
   
   virtual String getName()
   {
      return "Vert Lit";
   }

   virtual U32 getOutputTargets( const MaterialFeatureData &fd ) const;
};


/// Detail map
class DetailFeat : public ShaderFeatureCommon
{
public:
   virtual void processVert( Vector<ShaderComponent*> &componentList,
                             const MaterialFeatureData &fd );

   virtual void processPix( Vector<ShaderComponent*> &componentList, 
                            const MaterialFeatureData &fd );

   virtual Resources getResources( const MaterialFeatureData &fd );

   virtual Material::BlendOp getBlendOp(){ return Material::Mul; }

   // Sets textures and texture flags for current pass
   virtual void setTexData( Material::StageData &stageDat,
                            const MaterialFeatureData &fd,
                            RenderPassData &passData,
                            U32 &texIndex );

   virtual String getName()
   {
      return "Detail";
   }

   static const char* detailMapSamplerName;
};


/// Reflect Cubemap
class ReflectCubeFeat : public ShaderFeatureCommon
{
public:
   virtual void processVert( Vector<ShaderComponent*> &componentList,
                             const MaterialFeatureData &fd );

   virtual void processPix( Vector<ShaderComponent*> &componentList, 
                            const MaterialFeatureData &fd );

   virtual Resources getResources( const MaterialFeatureData &fd );

   // Sets textures and texture flags for current pass
   virtual void setTexData( Material::StageData &stageDat,
                            const MaterialFeatureData &fd,
                            RenderPassData &passData,
                            U32 &texIndex );

   virtual String getName()
   {
      return "Reflect Cube";
   }

   static const char* cubeMapSamplerName;
   static const char* glossMapSamplerName;
};


/// Fog
class FogFeat : public ShaderFeatureCommon
{
protected:

   ShaderIncludeDependency mFogDep;

public:
   FogFeat();

   virtual void processVert( Vector<ShaderComponent*> &componentList,
                             const MaterialFeatureData &fd );

   virtual void processPix( Vector<ShaderComponent*> &componentList, 
                            const MaterialFeatureData &fd );

   virtual Resources getResources( const MaterialFeatureData &fd );

   virtual Material::BlendOp getBlendOp() { return Material::LerpAlpha; }

   virtual String getName()
   {
      return "Fog";
   }
};


/// Tex Anim
class TexAnim : public ShaderFeatureCommon
{
public:
   virtual Material::BlendOp getBlendOp() { return Material::None; }

   virtual String getName()
   {
      return "Texture Animation";
   }
};


/// Visibility
class VisibilityFeat : public ShaderFeatureCommon
{
protected:

   ShaderIncludeDependency mTorqueDep;

public:

   VisibilityFeat();

   virtual void processVert( Vector<ShaderComponent*> &componentList,
                             const MaterialFeatureData &fd );

   virtual void processPix( Vector<ShaderComponent*> &componentList, 
                            const MaterialFeatureData &fd );

   virtual Resources getResources( const MaterialFeatureData &fd );

   virtual Material::BlendOp getBlendOp() { return Material::None; }

   virtual String getName()
   {
      return "Visibility";
   }
};


///
class AlphaTest : public ShaderFeatureCommon
{
public:
   virtual void processPix(   Vector<ShaderComponent*> &componentList, 
                              const MaterialFeatureData &fd );

   virtual Material::BlendOp getBlendOp() { return Material::None; }

   virtual String getName()
   {
      return "Alpha Test";
   }
};


/// Special feature used to mask out the RGB color for
/// non-glow passes of glow materials.
/// @see RenderGlowMgr
class GlowMask : public ShaderFeatureCommon
{
public:
   virtual void processPix(   Vector<ShaderComponent*> &componentList, 
                              const MaterialFeatureData &fd );

   virtual Material::BlendOp getBlendOp() { return Material::None; }

   virtual String getName()
   {
      return "Glow Mask";
   }
};

/// This should be the final feature on most pixel shaders which
/// encodes the color for the current HDR target format.
/// @see HDRPostFx
/// @see LightManager
/// @see torque.
class HDROut : public ShaderFeatureCommon
{
protected:

   ShaderIncludeDependency mTorqueDep;

public:

   HDROut();

   virtual void processPix(   Vector<ShaderComponent*> &componentList, 
                              const MaterialFeatureData &fd );

   virtual Material::BlendOp getBlendOp() { return Material::None; }

   virtual String getName() { return "HDR Output"; }
};

///
class FoliageFeature : public ShaderFeatureCommon
{
protected:

   ShaderIncludeDependency mDep;

public:

   FoliageFeature();

   virtual void processVert( Vector<ShaderComponent*> &componentList,
                             const MaterialFeatureData &fd );

   virtual void processPix( Vector<ShaderComponent*> &componentList,
                           const MaterialFeatureData &fd );   

   virtual String getName()
   {
      return "Foliage Feature";
   }

   virtual void determineFeature( Material *material, 
                                  const GFXVertexFormat *vertexFormat,
                                  U32 stageNum,
                                  const FeatureType &type,
                                  const FeatureSet &features,
                                  MaterialFeatureData *outFeatureData );

   virtual ShaderFeatureConstHandles* createConstHandles( GFXShader *shader, SimObject *userObject );   
};

class ParticleNormalFeature : public ShaderFeatureCommon
{
public:

   virtual void processVert( Vector<ShaderComponent*> &componentList,
      const MaterialFeatureData &fd );

   virtual String getName()
   {
      return "Particle Normal Generation Feature";
   }

};


/// Special feature for unpacking imposter verts.
/// @see RenderImposterMgr
class ImposterVertFeature : public ShaderFeatureCommon
{
protected:

   ShaderIncludeDependency mDep;

public:

   ImposterVertFeature();

   virtual void processVert(  Vector<ShaderComponent*> &componentList,
                              const MaterialFeatureData &fd );

   virtual void processPix(  Vector<ShaderComponent*> &componentList,
                             const MaterialFeatureData &fd );

   virtual String getName() { return "Imposter Vert"; }

   virtual void determineFeature( Material *material, 
                                  const GFXVertexFormat *vertexFormat,
                                  U32 stageNum,
                                  const FeatureType &type,
                                  const FeatureSet &features,
                                  MaterialFeatureData *outFeatureData );
};


#endif // _SHADERGEN_HLSL_SHADERFEATUREHLSL_H_
