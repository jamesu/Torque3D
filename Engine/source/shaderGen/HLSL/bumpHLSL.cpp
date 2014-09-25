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
#include "shaderGen/HLSL/bumpHLSL.h"

#include "shaderGen/shaderOp.h"
#include "gfx/gfxDevice.h"
#include "materials/matInstance.h"
#include "materials/processedMaterial.h"
#include "materials/materialFeatureTypes.h"
#include "shaderGen/shaderGenVars.h"
#include "shaderGen/shaderGen.h"


const char* BumpFeat::detailBumpMapSamplerName = "detailBumpMap";
const char* BumpFeat::bumpMapSamplerName = "bumpMap";

void BumpFeat::processVert(  Vector<ShaderComponent*> &componentList, 
                                 const MaterialFeatureData &fd )
{
   MultiLine *meta = new MultiLine;
   output = meta;

   const bool useTexAnim = fd.features[MFT_TexAnim];

   // Output the texture coord.
   getOutTexCoord(   "texCoord", 
                     "texCoord", 
                     getVector2Name(), 
                     true, 
                     useTexAnim, 
                     "texMat",
                     meta, 
                     componentList );

   if ( fd.features.hasFeature( MFT_DetailNormalMap ) )
      addOutDetailTexCoord( componentList, 
                            meta,
                            useTexAnim );

   // Also output the worldToTanget transform which
   // we use to create the world space normal.
   getOutWorldToTangent( componentList, meta, fd );
}

void BumpFeat::processPix(   Vector<ShaderComponent*> &componentList, 
                                 const MaterialFeatureData &fd )
{
   MultiLine *meta = new MultiLine;
   output = meta;

   // Get the texture coord.
   Var *texCoord = getInTexCoord( "texCoord", getVector2Name(), true, componentList );

   // Sample the bumpmap.
   Var *bumpMap = getNormalMapTex();
   LangElement *texOp = NULL;

   // Handle atlased textures
   // http://www.infinity-universe.com/Infinity/index.php?option=com_content&task=view&id=65&Itemid=47
   if(fd.features[MFT_NormalMapAtlas])
   {
      // This is a big block of code, so put a comment in the shader code
      meta->addStatement( new GenOp( "   // Atlased texture coordinate calculation (see BumpFeat*LSL for details)\r\n") );

      Var *atlasedTex = new Var;
      atlasedTex->setName("atlasedBumpCoord");
      atlasedTex->setType( getVector2Name() );
      LangElement *atDecl = new DecOp( atlasedTex );

      // Parameters of the texture atlas
      Var *atParams  = new Var;
      atParams->setType( getVector4Name() );
      atParams->setName("bumpAtlasParams");
      atParams->uniform = true;
      atParams->constSortPos = cspPotentialPrimitive;

      // Parameters of the texture (tile) this object is using in the atlas
      Var *tileParams  = new Var;
      tileParams->setType( getVector4Name() );
      tileParams->setName("bumpAtlasTileParams");
      tileParams->uniform = true;
      tileParams->constSortPos = cspPotentialPrimitive;

      const bool is_sm3 = (GFX->getPixelShaderVersion() > 2.0f) || GFX->getAdapterType() == OpenGL;
      if(is_sm3)
      {
         // Figure out the mip level
         meta->addStatement( new GenOp( avar("   %s _dx_bump = ddx(@ * @.z);\r\n", getVector2Name()), texCoord, atParams ) );
         meta->addStatement( new GenOp( avar("   %s _dy_bump = ddy(@ * @.z);\r\n", getVector2Name()), texCoord, atParams ) );
         meta->addStatement( new GenOp( "   float mipLod_bump = 0.5 * log2(max(dot(_dx_bump, _dx_bump), dot(_dy_bump, _dy_bump)));\r\n" ) );
         meta->addStatement( new GenOp( "   mipLod_bump = clamp(mipLod_bump, 0.0, @.w);\r\n", atParams ) );

         // And the size of the mip level
         meta->addStatement( new GenOp( "   float mipPixSz_bump = pow(2.0, @.w - mipLod_bump);\r\n", atParams ) );
         meta->addStatement( new GenOp( avar("   %s mipSz_bump = mipPixSz_bump / @.xy;\r\n", getVector2Name()), atParams ) );
      }
      else
      {
         meta->addStatement(new GenOp(avar("   %s mipSz = %s(1.0, 1.0);\r\n", getVector2Name())));
      }

      // Tiling mode
      if( true ) // Wrap
      {
         UsedForDirect3D(meta->addStatement( new GenOp( "   @ = frac(@);\r\n", atDecl, texCoord ) ));
         UsedForOpenGL(meta->addStatement( new GenOp( "   @ = fract(@);\r\n", atDecl, texCoord ) ));
      }
      else       // Clamp
      {
         meta->addStatement( new GenOp( "   @ = saturate(@);\r\n", atDecl, texCoord ) );
      }

      // Finally scale/offset, and correct for filtering
      meta->addStatement( new GenOp( "   @ = @ * ((mipSz_bump * @.xy - 1.0) / mipSz_bump) + 0.5 / mipSz_bump + @.xy * @.xy;\r\n", 
         atlasedTex, atlasedTex, atParams, atParams, tileParams ) );

      // Add a newline
      meta->addStatement( new GenOp( "\r\n" ) );

      if(is_sm3)
      {
         UsedForDirect3D(texOp = new GenOp( "tex2Dlod(@, float4(@, 0.0, mipLod_bump))", bumpMap, texCoord ));
         UsedForOpenGL(texOp = new GenOp( "texture2DLod(@, @, mipLod_bump)", bumpMap, texCoord ));
      }
      else
      {
         texOp = new GenOp( avar("%s(@, @)", getTexture2DFunction()), bumpMap, texCoord );
      }
   }
   else
   {
      texOp = new GenOp( avar("%s(@, @)", getTexture2DFunction()), bumpMap, texCoord );
   }

   Var *bumpNorm = new Var( "bumpNormal", getVector4Name() );
   meta->addStatement( expandNormalMap( texOp, new DecOp( bumpNorm ), bumpNorm, fd ) );

   // If we have a detail normal map we add the xy coords of
   // it to the base normal map.  This gives us the effect we
   // want with few instructions and minial artifacts.
   if ( fd.features.hasFeature( MFT_DetailNormalMap ) )
   {
      bumpMap = new Var;
      bumpMap->setType( "sampler2D" );
      bumpMap->setName( detailBumpMapSamplerName );
      bumpMap->uniform = true;
      bumpMap->sampler = true;
      bumpMap->constNum = Var::getTexUnitNum();

      texCoord = getInTexCoord( "detCoord", getVector2Name(), true, componentList );
      texOp = new GenOp( avar("%s(@, @)", getTexture2DFunction()), bumpMap, texCoord );

      Var *detailBump = new Var;
      detailBump->setName( "detailBump" );
      detailBump->setType( getVector4Name() );
      meta->addStatement( expandNormalMap( texOp, new DecOp( detailBump ), detailBump, fd ) );

      Var *detailBumpScale = new Var;
      detailBumpScale->setType( "float" );
      detailBumpScale->setName( "detailBumpStrength" );
      detailBumpScale->uniform = true;
      detailBumpScale->constSortPos = cspPass;
      meta->addStatement( new GenOp( "   @.xy += @.xy * @;\r\n", bumpNorm, detailBump, detailBumpScale ) );
   }

   // We transform it into world space by reversing the 
   // multiplication by the worldToTanget transform.
   Var *wsNormal = new Var( "wsNormal", getVector3Name() );
   Var *worldToTanget = getInWorldToTangent( componentList );
   UsedForDirect3D(meta->addStatement( new GenOp( "   @ = normalize( mul( @.xyz, @ ) );\r\n", new DecOp( wsNormal ), bumpNorm, worldToTanget ) ));
   UsedForOpenGL(meta->addStatement( new GenOp( "   @ = normalize( @.xyz * mat3(@) );\r\n", new DecOp( wsNormal ), bumpNorm, worldToTanget ) ));
}

ShaderFeature::Resources BumpFeat::getResources( const MaterialFeatureData &fd )
{
   Resources res; 

   // If we have no parallax then we bring on the normal tex.
   if ( !fd.features[MFT_Parallax] )
      res.numTex = 1;

   // Only the parallax or diffuse map will add texture
   // coords other than us.
   if (  !fd.features[MFT_Parallax] &&
         !fd.features[MFT_DiffuseMap] &&
         !fd.features[MFT_OverlayMap] &&
         !fd.features[MFT_DetailMap] )
      res.numTexReg++;

   // We pass the world to tanget space transform.
   res.numTexReg += 3;

   // Do we have detail normal mapping?
   if ( fd.features[MFT_DetailNormalMap] )
   {
      res.numTex++;
      if ( !fd.features[MFT_DetailMap] )
         res.numTexReg++;
   }

   return res;
}

void BumpFeat::setTexData(   Material::StageData &stageDat,
                                 const MaterialFeatureData &fd,
                                 RenderPassData &passData,
                                 U32 &texIndex )
{
   // If we had a parallax feature then it takes
   // care of hooking up the normal map texture.
   if ( fd.features[MFT_Parallax] )
      return;

   if ( fd.features[MFT_NormalMap] )
   {
      passData.mTexType[ texIndex ] = Material::Bump;
      passData.mTexSlot[ texIndex ].samplerName = BumpFeat::bumpMapSamplerName;
      passData.mTexSlot[ texIndex++ ].texObject = stageDat.getTex( MFT_NormalMap );
   }

   if ( fd.features[ MFT_DetailNormalMap ] )
   {
      passData.mTexType[ texIndex ] = Material::DetailBump;
      passData.mTexSlot[ texIndex ].samplerName = BumpFeat::detailBumpMapSamplerName;
      passData.mTexSlot[ texIndex++ ].texObject = stageDat.getTex( MFT_DetailNormalMap );
   }
}


ParallaxFeat::ParallaxFeat()
   : mIncludeDep( "shaders/common/torque.hlsl" )
{
   addDependency( &mIncludeDep );
}

Var* ParallaxFeat::_getUniformVar( const char *name, const char *type, ConstantSortPosition csp )
{
   Var *theVar = (Var*)LangElement::find( name );
   if ( !theVar )
   {
      theVar = new Var;
      theVar->setType( type );
      theVar->setName( name );
      theVar->uniform = true;
      theVar->constSortPos = csp;
   }

   return theVar;
}

void ParallaxFeat::processVert( Vector<ShaderComponent*> &componentList, 
                                    const MaterialFeatureData &fd )
{
   AssertFatal( GFX->getPixelShaderVersion() >= 2.0, 
      "ParallaxFeat::processVert - We don't support SM 1.x!" );

   MultiLine *meta = new MultiLine;

   // Add the texture coords.
   getOutTexCoord(   "texCoord", 
                     "texCoord", 
                     getVector2Name(), 
                     true, 
                     fd.features[MFT_TexAnim], 
                     "texMat",
                     meta, 
                     componentList );

   // Grab the input position.
   Var *inPos = (Var*)LangElement::find( "inPosition" );
   if ( !inPos )
      inPos = (Var*)LangElement::find( "position" );

   // Get the object space eye position and the 
   // object to tangent space transform.
   Var *eyePos = _getUniformVar( "eyePos", getVector3Name(), cspPrimitive );
   Var *objToTangentSpace = getOutObjToTangentSpace( componentList, meta, fd );

   // Now send the negative view vector in tangent space to the pixel shader.
   ShaderConnector *connectComp = dynamic_cast<ShaderConnector *>( componentList[C_CONNECTOR] );
   Var *outNegViewTS = connectComp->getElement( RT_TEXCOORD );
   outNegViewTS->setName( "outNegViewTS" );
   outNegViewTS->setStructName( "OUT" );
   outNegViewTS->setType( getVector3Name() );
   meta->addStatement( new GenOp( avar("   @ = mul( @, %s( @.xyz - @ ) );\r\n", getVector3Name()), 
      outNegViewTS, objToTangentSpace, inPos, eyePos ) );

   // TODO: I'm at a loss at why i need to flip the binormal/y coord
   // to get a good view vector for parallax. Lighting works properly
   // with the TS matrix as is... but parallax does not.
   //
   // Someone figure this out!
   //
   meta->addStatement( new GenOp( "   @.y = -@.y;\r\n", outNegViewTS, outNegViewTS ) );  

   // If we have texture anim matrix the tangent
   // space view vector may need to be rotated.
   Var *texMat = (Var*)LangElement::find( "texMat" );
   if ( texMat )
   {
      meta->addStatement( new GenOp( avar("   @ = mul(@, %s(@,0)).xyz;\r\n", getVector4Name()),
         outNegViewTS, texMat, outNegViewTS ) );
   }

   output = meta;
}

void ParallaxFeat::processPix(  Vector<ShaderComponent*> &componentList, 
                                    const MaterialFeatureData &fd )
{
   AssertFatal( GFX->getPixelShaderVersion() >= 2.0, 
      "ParallaxFeat::processPix - We don't support SM 1.x!" );

   MultiLine *meta = new MultiLine;

   // Order matters... get this first!
   Var *texCoord = getInTexCoord( "texCoord", getVector2Name(), true, componentList );

   ShaderConnector *connectComp = dynamic_cast<ShaderConnector *>( componentList[C_CONNECTOR] );

   // We need the negative tangent space view vector
   // as in parallax mapping we step towards the camera.
   Var *negViewTS = (Var*)LangElement::find( "negViewTS" );
   if ( !negViewTS )
   {
      Var *inNegViewTS = (Var*)LangElement::find( "outNegViewTS" );
      if ( !inNegViewTS )
      {
         inNegViewTS = connectComp->getElement( RT_TEXCOORD );
         inNegViewTS->setName( "outNegViewTS" );
         inNegViewTS->setStructName( "IN" );
         inNegViewTS->setType( getVector3Name() );
      }

      negViewTS = new Var( "negViewTS", getVector3Name() );
      meta->addStatement( new GenOp( "   @ = normalize( @ );\r\n", new DecOp( negViewTS ), inNegViewTS ) );
   }

   // Get the rest of our inputs.
   Var *parallaxInfo = _getUniformVar( "parallaxInfo", "float", cspPotentialPrimitive );
   Var *normalMap = getNormalMapTex();

   // Call the library function to do the rest.
   meta->addStatement( new GenOp( "   @.xy += parallaxOffset( @, @.xy, @, @ );\r\n", 
      texCoord, normalMap, texCoord, negViewTS, parallaxInfo ) );

   // TODO: Fix second UV maybe?

   output = meta;
}

ShaderFeature::Resources ParallaxFeat::getResources( const MaterialFeatureData &fd )
{
   AssertFatal( GFX->getPixelShaderVersion() >= 2.0, 
      "ParallaxFeat::getResources - We don't support SM 1.x!" );

   Resources res;

   // We add the outViewTS to the outputstructure.
   res.numTexReg = 1;

   // If this isn't a prepass then we will be
   // creating the normal map here.
   if ( !fd.features.hasFeature( MFT_PrePassConditioner ) )
      res.numTex = 1;

   return res;
}

void ParallaxFeat::setTexData(  Material::StageData &stageDat,
                                    const MaterialFeatureData &fd,
                                    RenderPassData &passData,
                                    U32 &texIndex )
{
   AssertFatal( GFX->getPixelShaderVersion() >= 2.0, 
      "ParallaxFeat::setTexData - We don't support SM 1.x!" );

   GFXTextureObject *tex = stageDat.getTex( MFT_NormalMap );
   if ( tex )
   {
      passData.mTexType[ texIndex ] = Material::Bump;
      passData.mTexSlot[ texIndex ].samplerName = BumpFeat::bumpMapSamplerName;
      passData.mTexSlot[ texIndex++ ].texObject = tex;
   }
}


void NormalsOutFeat::processVert(  Vector<ShaderComponent*> &componentList, 
                                       const MaterialFeatureData &fd )
{
   // If we have normal maps then we can count
   // on it to generate the world space normal.
   if ( fd.features[MFT_NormalMap] )
      return;

   MultiLine *meta = new MultiLine;
   output = meta;

   ShaderConnector *connectComp = dynamic_cast<ShaderConnector *>( componentList[C_CONNECTOR] );

   Var *outNormal = connectComp->getElement( RT_TEXCOORD );
   outNormal->setName( "wsNormal" );
   outNormal->setStructName( "OUT" );
   outNormal->setType( getVector3Name() );
   outNormal->mapsToSampler = false;

   // Find the incoming vertex normal.
   Var *inNormal = (Var*)LangElement::find( "inNormal" );
   if (!inNormal) inNormal = (Var*) LangElement::find( "normal" );
   if ( inNormal )
   {
      // Transform the normal to world space.
      Var *objTrans = getObjTrans( componentList, fd.features[MFT_UseInstancing], meta );
      meta->addStatement( new GenOp( "   @ = mul( @, normalize( @ ) );\r\n", outNormal, objTrans, inNormal ) );
   }
   else
   {
      // If we don't have a vertex normal... just pass the
      // camera facing normal to the pixel shader.
      meta->addStatement( new GenOp( avar("   @ = %s( 0.0, 0.0, 1.0 );\r\n", getVector3Name()), outNormal ) );
   }
}

void NormalsOutFeat::processPix(   Vector<ShaderComponent*> &componentList, 
                                       const MaterialFeatureData &fd )
{
   MultiLine *meta = new MultiLine;
   output = meta;

   Var *wsNormal = (Var*)LangElement::find( "wsNormal" );
   if ( !wsNormal )
   {
      ShaderConnector *connectComp = dynamic_cast<ShaderConnector *>( componentList[C_CONNECTOR] );
		
		Var *wsNormal = (Var*)LangElement::find( "wsNormal" );
		if ( !wsNormal )
		{
			wsNormal = connectComp->getElement( RT_TEXCOORD );
			wsNormal->setName( "wsNormal" );
			wsNormal->setStructName( "IN" );
			wsNormal->setType( getVector3Name() );
		}

      // If we loaded the normal its our responsibility
      // to normalize it... the interpolators won't.
      //
      // Note we cast to half here to get partial precision
      // optimized code which is an acceptable loss of
      // precision for normals and performs much better
      // on older Geforce cards.
      //
      meta->addStatement( new GenOp( avar("   @ = normalize( %s( @ ) );\r\n", getHalfVector3Name()), wsNormal, wsNormal ) );      
   }

   LangElement *normalOut;
   Var *outColor = (Var*)LangElement::find( "col" );
   if ( outColor && !fd.features[MFT_AlphaTest] )
   {
      normalOut = new GenOp( avar("%s( ( -@ + 1 ) * 0.5, @.a )", getVector4Name()), wsNormal, outColor );
   }
   else
   {
      normalOut = new GenOp( avar("%s( ( -@ + 1 ) * 0.5, 1 )", getVector4Name()), wsNormal );
   }

   meta->addStatement( new GenOp( "   @;\r\n", 
      assignColor( normalOut, Material::None ) ) );
}
