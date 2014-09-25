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
#include "shaderGen/HLSL/shaderFeatureHLSL.h"
#include "shaderGen/shaderGen.h"
#include "lighting/advanced/HLSL/gBufferConditionerHLSL.h"

#include "shaderGen/langElement.h"
#include "shaderGen/shaderOp.h"
#include "shaderGen/shaderGenVars.h"
#include "gfx/gfxDevice.h"
#include "materials/matInstance.h"
#include "materials/processedMaterial.h"
#include "materials/materialFeatureTypes.h"
#include "core/util/autoPtr.h"

#include "lighting/advanced/advancedLightBinManager.h"

// martinJ
#include "renderInstance/renderPrePassMgr.h"
#include "scene/reflectionManager.h"


#include "ts/tsShape.h"



const char *ShaderFeatureCommon::smHalfFloatName;
const char *ShaderFeatureCommon::smHalfVector2Name;
const char *ShaderFeatureCommon::smHalfVector3Name;
const char *ShaderFeatureCommon::smHalfVector4Name;
const char *ShaderFeatureCommon::smVector2Name;
const char *ShaderFeatureCommon::smVector3Name;
const char *ShaderFeatureCommon::smVector4Name;
const char *ShaderFeatureCommon::smMatrix3x3Name;
const char *ShaderFeatureCommon::smMatrix4x3Name;
const char *ShaderFeatureCommon::smMatrix4x4Name;
const char *ShaderFeatureCommon::smTexture2DFunction;
const char *ShaderFeatureCommon::smTexture2DLodFunction;
const char *ShaderFeatureCommon::smTextureCubeFunction;


GFXAdapterType ShaderFeatureCommon::smCurrentAdapterType = NullDevice;


GFXAdapterType ShaderFeatureCommon::getCurrentAdapterType()
{
	return smCurrentAdapterType;
}

void ShaderFeatureCommon::setCurrentAdapterType(GFXAdapterType type)
{
	if (smCurrentAdapterType == type)
		return;

	smCurrentAdapterType = type;

	if (smCurrentAdapterType == OpenGL)
	{
        smHalfFloatName = "float";
        smHalfVector2Name = "vec2";
        smHalfVector3Name = "vec3";
        smHalfVector4Name = "vec4";
        smVector2Name = "vec2";
        smVector3Name = "vec3";
        smVector4Name = "vec4";
        smMatrix3x3Name = "mat3";
        smMatrix4x3Name = "mat4x3";
        smMatrix4x4Name = "mat4";
        smTexture2DFunction = "texture2D";
        smTexture2DLodFunction = "textureLod";
        smTextureCubeFunction = "textureCube";
	}
	else if (smCurrentAdapterType == Direct3D9)
	{
        smHalfFloatName = "half";
        smHalfVector2Name = "half2";
        smHalfVector3Name = "half3";
        smHalfVector4Name = "half4";
        smVector2Name = "float2";
        smVector3Name = "float3";
        smVector4Name = "float4";
        smMatrix3x3Name = "float3x3";
        smMatrix4x3Name = "float4x3";
        smMatrix4x4Name = "float4x4";
        smTexture2DFunction = "tex2D";
        smTexture2DLodFunction = "tex2DLod";
        smTextureCubeFunction = "texCUBE";
	}
}

LangElement * ShaderFeatureCommon::setupTexSpaceMat( Vector<ShaderComponent*> &, // componentList
                                                   Var **texSpaceMat )
{
   Var *N = (Var*) LangElement::find( "inNormal" );
   if (!N) N = (Var*) LangElement::find( "normal" );
   Var *B = (Var*) LangElement::find( "B" );
   Var *T = (Var*) LangElement::find( "T" );

   Var *tangentW = (Var*) LangElement::find( "tangentW" );
   
   // setup matrix var
   *texSpaceMat = new Var;
   (*texSpaceMat)->setType( getMatrix3x3Name() );
   (*texSpaceMat)->setName( "objToTangentSpace" );

   MultiLine *meta = new MultiLine;
   meta->addStatement( new GenOp( "   @;\r\n", new DecOp( *texSpaceMat ) ) );

   // Protect against missing normal and tangent.
   if ( !N || !T )
   {
      meta->addStatement( new GenOp( avar("   @[0] = %s( 1, 0, 0 ); @[1] = %s( 0, 1, 0 ); @[2] = %s( 0, 0, 1 );\r\n", getVector3Name(), getVector3Name(), getVector3Name()), 
         *texSpaceMat, *texSpaceMat, *texSpaceMat ) );
      return meta;
   }
   
   if (GFX->getAdapterType() == OpenGL)
   {
     // Recreate the binormal if we don't have one.
     if ( !B )
     {
       B = new Var;
       B->setType( "vec3" );
       B->setName( "B" );
       meta->addStatement( new GenOp( "   @ = cross( @, normalize(@) );\r\n", new DecOp( B ), T, N ) );
     }

     meta->addStatement( new GenOp( "   @[0] = vec3(@.x, @.x, normalize(@).x);\r\n", *texSpaceMat, T, B, N ) );
     meta->addStatement( new GenOp( "   @[1] = vec3(@.y, @.y, normalize(@).y);\r\n", *texSpaceMat, T, B, N ) );
     meta->addStatement( new GenOp( "   @[2] = vec3(@.z, @.z, normalize(@).z);\r\n", *texSpaceMat, T, B, N ) );
   }
   else
   {
     meta->addStatement( new GenOp( "   @[0] = @;\r\n", *texSpaceMat, T ) );
     if ( B )
       meta->addStatement( new GenOp( "   @[1] = @;\r\n", *texSpaceMat, B ) );
     else
     {
       if(dStricmp((char*)T->type, getVector4Name()) == 0)
         meta->addStatement( new GenOp( "   @[1] = cross( @, normalize(@) ) * @.w;\r\n", *texSpaceMat, T, N, T ) );
       else if(tangentW)
         meta->addStatement( new GenOp( "   @[1] = cross( @, normalize(@) ) * @;\r\n", *texSpaceMat, T, N, tangentW ) );
       else
         meta->addStatement( new GenOp( "   @[1] = cross( @, normalize(@) );\r\n", *texSpaceMat, T, N ) );
     }
     meta->addStatement( new GenOp( "   @[2] = normalize(@);\r\n", *texSpaceMat, N ) );
   }

   return meta;
}

LangElement* ShaderFeatureCommon::assignColor( LangElement *elem, 
                                             Material::BlendOp blend, 
                                             LangElement *lerpElem, 
                                             ShaderFeature::OutputTarget outputTarget )
{

   // search for color var
   Var *color = (Var*) LangElement::find( getOutputTargetVarName(outputTarget) );

   if ( !color )
   {
      // create color var
      color = new Var;
      color->setType( getVector4Name() );
      color->setName( getOutputTargetVarName(outputTarget) );
      if (GFX->getAdapterType() == OpenGL)
      {
        return new GenOp( "@ = @", new DecOp(color), elem );
      }
      else
      {
        color->setStructName( "OUT" );
        return new GenOp( "@ = @", color, elem );
      }
   }

   LangElement *assign;

   switch ( blend )
   {
      case Material::Add:
         assign = new GenOp( "@ += @", color, elem );
         break;

      case Material::Sub:
         assign = new GenOp( "@ -= @", color, elem );
         break;

      case Material::Mul:
         assign = new GenOp( "@ *= @", color, elem );
         break;

      case Material::AddAlpha:
         assign = new GenOp( "@ += @ * @.a", color, elem, elem );
         break;

      case Material::LerpAlpha:
         if ( !lerpElem )
            lerpElem = elem;
         assign = new GenOp( "@.rgb = lerp( @.rgb, (@).rgb, (@).a )", color, color, elem, lerpElem );
         break;
      
      case Material::ToneMap:
         assign = new GenOp( "@ = 1.0 - exp(-1.0 * @ * @)", color, color, elem );
         break;
         
      default:
         AssertFatal(false, "Unrecognized color blendOp");
         // Fallthru

      case Material::None:
         assign = new GenOp( "@ = @", color, elem );
         break;    
   }
  
   return assign;
}


LangElement *ShaderFeatureCommon::expandNormalMap(   LangElement *sampleNormalOp, 
                                                   LangElement *normalDecl, 
                                                   LangElement *normalVar, 
                                                   const MaterialFeatureData &fd )
{
   MultiLine *meta = new MultiLine;

   if ( fd.features.hasFeature( MFT_IsDXTnm, getProcessIndex() ) )
   {
      if ( fd.features[MFT_ImposterVert] )
      {
         // The imposter system uses object space normals and
         // encodes them with the z axis in the alpha component.
         meta->addStatement( new GenOp( avar("   @ = %s( normalize( @.xyw * 2.0 - 1.0 ), 0.0 ); // Obj DXTnm\r\n", getVector4Name()), normalDecl, sampleNormalOp ) );
      }
      else
      {
         // DXT Swizzle trick
         meta->addStatement( new GenOp( avar("   @ = %s( @.ag * 2.0 - 1.0, 0.0, 0.0 ); // DXTnm\r\n", getVector4Name()), normalDecl, sampleNormalOp ) );
         meta->addStatement( new GenOp( "   @.z = sqrt( 1.0 - dot( @.xy, @.xy ) ); // DXTnm\r\n", normalVar, normalVar, normalVar ) );
      }
   }
   else
   {
      meta->addStatement( new GenOp( "   @ = @;\r\n", normalDecl, sampleNormalOp ) );
      meta->addStatement( new GenOp( "   @.xyz = @.xyz * 2.0 - 1.0;\r\n", normalVar, normalVar ) );
   }

   return meta;
}

ShaderFeatureCommon::ShaderFeatureCommon()
{
   output = NULL;
}

Var * ShaderFeatureCommon::getVertTexCoord( const String &name )
{
   Var *inTex = NULL;

   for( U32 i=0; i<LangElement::elementList.size(); i++ )
   {
      if( !dStrcmp( (char*)LangElement::elementList[i]->name, name.c_str() ) )
      {
         inTex = dynamic_cast<Var*>( LangElement::elementList[i] );
         if ( inTex )
         {
            // NOTE: This used to do this check...
            //
            // dStrcmp( (char*)inTex->structName, "IN" )
            //
            // ... to ensure that the var was from the input
            // vertex structure, but this kept some features
            // ( ie. imposter vert ) from decoding their own
            // coords for other features to use.
            //
            // If we run into issues with collisions between
            // IN vars and local vars we may need to revise.
            
            break;
         }
      }
   }

   return inTex;
}

Var* ShaderFeatureCommon::getOutObjToTangentSpace(   Vector<ShaderComponent*> &componentList,
                                                   MultiLine *meta,
                                                   const MaterialFeatureData &fd )
{
   Var *outObjToTangentSpace = (Var*)LangElement::find( "objToTangentSpace" );
   if ( !outObjToTangentSpace )
      meta->addStatement( setupTexSpaceMat( componentList, &outObjToTangentSpace ) );

   return outObjToTangentSpace;
}

Var* ShaderFeatureCommon::getOutWorldToTangent(   Vector<ShaderComponent*> &componentList,
                                                MultiLine *meta,
                                                const MaterialFeatureData &fd )
{
   Var *outWorldToTangent = (Var*)LangElement::find( "outWorldToTangent" );
   if ( outWorldToTangent )
      return outWorldToTangent;
      
   Var *worldToTangent = (Var*)LangElement::find( "worldToTangent" );
   if ( !worldToTangent )
   {
      Var *texSpaceMat = getOutObjToTangentSpace( componentList, meta, fd );

      if (!fd.features[MFT_ParticleNormal] )
      {
         // turn obj->tangent into world->tangent
         worldToTangent = new Var;
         worldToTangent->setType( smMatrix3x3Name );
         worldToTangent->setName( "worldToTangent" );
         LangElement *worldToTangentDecl = new DecOp( worldToTangent );

         // Get the world->obj transform
         Var *worldToObj = (Var*)LangElement::find( "worldToObj" );
         if ( !worldToObj )
         {
            worldToObj = new Var;
            worldToObj->setName( "worldToObj" );

            if ( fd.features[MFT_UseInstancing] ) 
            {
               // We just use transpose to convert the 3x3 portion of
               // the object transform to its inverse.
               worldToObj->setType( getMatrix3x3Name() );
               Var *objTrans = getObjTrans( componentList, true, meta );
               meta->addStatement( new GenOp( avar("   @ = transpose( (%s)@ ); // Instancing!\r\n", smMatrix3x3Name), new DecOp( worldToObj ), objTrans ) );
            }
            else
            {
 
               worldToObj->setType( getMatrix4x4Name() );
               worldToObj->uniform = true;
               worldToObj->constSortPos = cspPrimitive;

               // Opengl doesn't support type casts so we need to initialise a 3x3 version of the matrix for use
               // in the world->tangent transform.
               if ( GFX->getAdapterType() == OpenGL)
               {
                 Var* worldToObj4x4 = worldToObj;
                 worldToObj = new Var("worldToObj3x3", getMatrix3x3Name());
                 DecOp* worldToObjDecOp = new DecOp(worldToObj);
                 meta->addStatement( new GenOp( "   @ = mat3(@[0].xyz, @[1].xyz, @[2].xyz);\r\n", worldToObjDecOp, worldToObj4x4, worldToObj4x4, worldToObj4x4 ) );
               }
            }
         }

         // assign world->tangent transform
         UsedForDirect3D(meta->addStatement( new GenOp("   @ = mul( @, (float3x3)@ );\r\n", worldToTangentDecl, texSpaceMat, worldToObj ) ));
         UsedForOpenGL(meta->addStatement( new GenOp("   @ = @ * @;\r\n", worldToTangentDecl, texSpaceMat, worldToObj ) ));
      }
      else
      {
         // Assume particle normal generation has set this up in the proper space
         worldToTangent = texSpaceMat;
      }
   }

   // send transform to pixel shader
   ShaderConnector *connectComp = dynamic_cast<ShaderConnector *>( componentList[C_CONNECTOR] );

   outWorldToTangent = connectComp->getElement( RT_TEXCOORD, 1, 3 );
   outWorldToTangent->setName( "outWorldToTangent" );
   outWorldToTangent->setStructName( "OUT" );
   outWorldToTangent->setType( smMatrix3x3Name );
   meta->addStatement( new GenOp( "   @ = @;\r\n", outWorldToTangent, worldToTangent ) );

   return outWorldToTangent;
}

Var* ShaderFeatureCommon::getOutViewToTangent( Vector<ShaderComponent*> &componentList,
                                             MultiLine *meta,
                                             const MaterialFeatureData &fd )
{
   Var *outViewToTangent = (Var*)LangElement::find( "outViewToTangent" );
   if ( outViewToTangent )
      return outViewToTangent;

   Var *viewToTangent = (Var*)LangElement::find( "viewToTangent" );
   if ( !viewToTangent )
   {

      Var *texSpaceMat = getOutObjToTangentSpace( componentList, meta, fd );

      if ( !fd.features[MFT_ParticleNormal] )
      {
         // turn obj->tangent into world->tangent
         viewToTangent = new Var;
         viewToTangent->setType( smMatrix3x3Name );
         viewToTangent->setName( "viewToTangent" );
         LangElement *viewToTangentDecl = new DecOp( viewToTangent );

         // Get the view->obj transform
         Var *viewToObj = getInvWorldView( componentList, fd.features[MFT_UseInstancing], meta );

         // assign world->tangent transform
         UsedForDirect3D(meta->addStatement( new GenOp("   @ = mul( @, (float3x3)@ );\r\n", viewToTangentDecl, texSpaceMat, viewToObj ) ));
         UsedForOpenGL(meta->addStatement( new GenOp("   @ = @ * mat3(@);\r\n", viewToTangentDecl, texSpaceMat, viewToObj ) ));
      }
      else
      {
         // Assume particle normal generation has set this up in the proper space
         viewToTangent = texSpaceMat;
      }
   }

   // send transform to pixel shader
   ShaderConnector *connectComp = dynamic_cast<ShaderConnector *>( componentList[C_CONNECTOR] );

   outViewToTangent = connectComp->getElement( RT_TEXCOORD, 1, 3 );
   outViewToTangent->setName( "outViewToTangent" );
   outViewToTangent->setStructName( "OUT" );
   outViewToTangent->setType( smMatrix3x3Name );
   meta->addStatement( new GenOp( "   @ = @;\r\n", outViewToTangent, viewToTangent ) );

   return outViewToTangent;
}

Var* ShaderFeatureCommon::getOutTexCoord(   const char *name,
                                          const char *vertexCoordName,       
                                          const char *type,
                                          bool mapsToSampler,
                                          bool useTexAnim,
                                          const char* texMatrixName,
                                          MultiLine *meta,
                                          Vector<ShaderComponent*> &componentList )
{
   String outTexName = String::ToString( "out_%s", name );
   Var *texCoord = (Var*)LangElement::find( outTexName );
   if ( !texCoord )
   {
      Var *inTex = getVertTexCoord( vertexCoordName );
      AssertFatal( inTex, "ShaderFeatureCommon::getOutTexCoord - Unknown vertex input coord!" );

      ShaderConnector *connectComp = dynamic_cast<ShaderConnector *>( componentList[C_CONNECTOR] );

      texCoord = connectComp->getElement( RT_TEXCOORD );
      texCoord->setName( outTexName );
      texCoord->setStructName( "OUT" );
      texCoord->setType( type );
      texCoord->mapsToSampler = mapsToSampler;

      if ( useTexAnim )
      {
         inTex->setType( getVector4Name() );
         
         // create texture mat var
         Var *texMat = new Var;
         texMat->setType( smMatrix4x4Name );
         texMat->setName( texMatrixName );
         texMat->uniform = true;
         texMat->constSortPos = cspPass;      
         
         // Statement allows for casting of different types which
		 // eliminates vector truncation problems.
		 String statement;
         UsedForDirect3D(statement = String::ToString( "   @ = (%s)mul(@, @);\r\n", type ));
         UsedForOpenGL(statement = "   @ = (@ * @).xy;\r\n");
         meta->addStatement( new GenOp( statement, texCoord, texMat, inTex ) );
      }
      else
      {
         meta->addStatement( new GenOp( "   @ = @;\r\n", texCoord, inTex ) );
      }
   }

   AssertFatal( dStrcmp( type, (const char*)texCoord->type ) == 0, 
      "ShaderFeatureCommon::getOutTexCoord - Type mismatch!" );

   return texCoord;
}

Var* ShaderFeatureCommon::getInTexCoord( const char *name,
                                       const char *type,
                                       bool mapsToSampler,
                                       Vector<ShaderComponent*> &componentList )
{
   const char* texCoordName = name;
   //UsedForOpenGL(const String texCoordName = String::ToString( "out_%s", name );)
   Var* texCoord = (Var*)LangElement::find( texCoordName );
   if ( !texCoord )
   {
      ShaderConnector *connectComp = dynamic_cast<ShaderConnector*>( componentList[C_CONNECTOR] );
      texCoord = connectComp->getElement( RT_TEXCOORD );
      texCoord->setName( texCoordName );
      texCoord->setStructName( "IN" );
      texCoord->setType( type );
      texCoord->mapsToSampler = mapsToSampler;
   }

   AssertFatal( dStrcmp( type, (const char*)texCoord->type ) == 0, 
      "ShaderFeatureCommon::getInTexCoord - Type mismatch!" );

   return texCoord;
}

Var* ShaderFeatureCommon::getInColor( const char *name,
                                    const char *type,
                                    Vector<ShaderComponent*> &componentList )
{
   Var *inColor = (Var*)LangElement::find( name );
   if ( !inColor )
   {
      ShaderConnector *connectComp = dynamic_cast<ShaderConnector*>( componentList[C_CONNECTOR] );
      inColor = connectComp->getElement( RT_COLOR );
      inColor->setName( name );
      inColor->setStructName( "IN" );
      inColor->setType( type );
   }

   AssertFatal( dStrcmp( type, (const char*)inColor->type ) == 0, 
      "ShaderFeatureCommon::getInColor - Type mismatch!" );

   return inColor;
}

Var* ShaderFeatureCommon::addOutVpos( MultiLine *meta,
                                    Vector<ShaderComponent*> &componentList )
{
	return NULL; // Normally defined by implementation
}

Var* ShaderFeatureCommon::getInVpos(  MultiLine *meta,
                                    Vector<ShaderComponent*> &componentList )
{
   Var *inVpos = (Var*)LangElement::find( "vpos" );
   if ( inVpos )
      return inVpos;

   ShaderConnector *connectComp = dynamic_cast<ShaderConnector*>( componentList[C_CONNECTOR] );
   
   inVpos = connectComp->getElement( RT_VPOS );

   UsedForDirect3D({
   inVpos->setName( "vpos" );
   inVpos->setStructName( "IN" );
   inVpos->setType( getVector2Name() );
   });
   
   UsedForOpenGL({
   inVpos->setName( "gl_FragCoord" );
   inVpos->setType( getVector4Name() );
   });

   AssertFatal(inVpos != NULL, "inVPos should be set");
   return inVpos;
}

Var* ShaderFeatureCommon::getInVnormal(  MultiLine *meta,
                                       Vector<ShaderComponent*> &componentList )
{
  Var *inVNormal = (Var*)LangElement::find( "vnormal" );
  if ( inVNormal )
    return inVNormal;

  ShaderConnector *connectComp = dynamic_cast<ShaderConnector*>( componentList[C_CONNECTOR] );

  inVNormal = connectComp->getElement( RT_TEXCOORD );
  inVNormal->setName( "inVNormal" );
  inVNormal->setStructName( "IN" );
  inVNormal->setType( getVector3Name() );

  Var *vnormal = new Var( "vnormal", getVector3Name() );
  meta->addStatement( new GenOp( "   @ = normalize(@);\r\n", new DecOp( vnormal ), inVNormal) );
  return vnormal;
}

Var* ShaderFeatureCommon::getInWorldToTangent( Vector<ShaderComponent*> &componentList )
{
   Var *worldToTangent = (Var*)LangElement::find( "worldToTangent" );
   if ( !worldToTangent )
   {
      ShaderConnector *connectComp = dynamic_cast<ShaderConnector *>( componentList[C_CONNECTOR] );
      worldToTangent = connectComp->getElement( RT_TEXCOORD, 1, 3 );
      worldToTangent->setName( "worldToTangent" );
      worldToTangent->setStructName( "IN" );
      worldToTangent->setType( smMatrix3x3Name );
   }

   return worldToTangent;
}

Var* ShaderFeatureCommon::getInViewToTangent( Vector<ShaderComponent*> &componentList )
{
   const char* elementName = "viewToTangent";

   Var *viewToTangent = (Var*)LangElement::find( elementName );
   if ( !viewToTangent )
   {
      ShaderConnector *connectComp = dynamic_cast<ShaderConnector *>( componentList[C_CONNECTOR] );
      viewToTangent = connectComp->getElement( RT_TEXCOORD, 1, 3 );
      viewToTangent->setName( elementName );
      viewToTangent->setStructName( "IN" );
      viewToTangent->setType( smMatrix3x3Name );
   }

   return viewToTangent;
}

Var* ShaderFeatureCommon::getNormalMapTex()
{
   return findOrCreateSampler( "bumpMap" );
}

// martinJ
Var* ShaderFeatureCommon::findOrCreateSampler( const char* name )
{
  Var *texture = (Var*)LangElement::find( name );
  if ( !texture )
  {
    texture = new Var;
    texture->setType( "sampler2D" );
    texture->setName( name );
    texture->uniform = true;
    texture->sampler = true;
    texture->constNum = Var::getTexUnitNum();
  }

  AssertFatal( strcmp((const char*)(texture->type), "sampler2D") == 0, "Failed to retrieve texture sampler, a variable already exists with a different type!" );

  return texture;
}

/// martinJ - Searches for the existing variable and creates it if it doesn't exist
Var* ShaderFeatureCommon::findOrCreateVar( const char* name, const char* type, bool uniform )
{
  Var *var = (Var*)LangElement::find(name);
  if ( var )
  {
    AssertFatal(strcmp((char*)var->type, type) == 0, "Shader variable type mis-match");
  }
  else
  {
    var = new Var(name, type);
    var->uniform = uniform;

    if (uniform)
    {
      var->constSortPos = cspPass;
    }
  }

  return var;
}

/// martinJ - Searches for the existing connected variable and creates it if it doesn't exist
Var* findOrCreateInput(const char* name, const char* type, const char* inStructureName, ShaderConnector* connectComp)
{
  Var *var = (Var*)LangElement::find(name);
  if (var)
  {
    AssertFatal(strcmp((char*)var->type, type) == 0, "Shader variable type mis-match");
  }
  else
  {
    var = connectComp->getElement( RT_TEXCOORD );
    var->setName(name);
    var->setStructName(inStructureName);
    var->setType(type);
    var->mapsToSampler = true;
  }

  return var;
}

Var* ShaderFeatureCommon::getObjTrans(   Vector<ShaderComponent*> &componentList,                                       
                                       bool useInstancing,
                                       MultiLine *meta )
{
   Var *objTrans = (Var*)LangElement::find( "objTrans" );
   if ( objTrans )
      return objTrans;

   if ( useInstancing )
   {
      ShaderConnector *vertStruct = dynamic_cast<ShaderConnector *>( componentList[C_VERT_STRUCT] );
      Var *instObjTrans = vertStruct->getElement( RT_TEXCOORD, 4, 4 );
      instObjTrans->setStructName( "IN" );
      instObjTrans->setType( getVector4Name() );
      instObjTrans->setName( "inst_objectTrans" );

      mInstancingFormat->addElement( "objTrans", GFXDeclType_Float4, instObjTrans->constNum+0 );
      mInstancingFormat->addElement( "objTrans", GFXDeclType_Float4, instObjTrans->constNum+1 );
      mInstancingFormat->addElement( "objTrans", GFXDeclType_Float4, instObjTrans->constNum+2 );
      mInstancingFormat->addElement( "objTrans", GFXDeclType_Float4, instObjTrans->constNum+3 );

      objTrans = new Var;
      objTrans->setType( getMatrix4x4Name() );
      objTrans->setName( "objTrans" );

      UsedForDirect3D({
         meta->addStatement( new GenOp( "   @ = { // Instancing!\r\n", new DecOp( objTrans ), instObjTrans ) );
         meta->addStatement( new GenOp( "      @[0],\r\n", instObjTrans ) );
         meta->addStatement( new GenOp( "      @[1],\r\n", instObjTrans ) );
         meta->addStatement( new GenOp( "      @[2],\r\n",instObjTrans ) );
         meta->addStatement( new GenOp( "      @[3] };\r\n", instObjTrans ) );
      });

      UsedForOpenGL({
         meta->addStatement( new GenOp( "   @ = mat4x4( // Instancing!\r\n", new DecOp( objTrans ), instObjTrans ) );
         meta->addStatement( new GenOp( "      @[0],\r\n", instObjTrans ) );
         meta->addStatement( new GenOp( "      @[1],\r\n", instObjTrans ) );
         meta->addStatement( new GenOp( "      @[2],\r\n",instObjTrans ) );
         meta->addStatement( new GenOp( "      @[3] );\r\n", instObjTrans ) );
      });
   }
   else
   {
      objTrans = new Var;
      objTrans->setType( smMatrix4x4Name );
      objTrans->setName( "objTrans" );
      objTrans->uniform = true;
      objTrans->constSortPos = cspPrimitive;
   }

   return objTrans;
}

Var* ShaderFeatureCommon::getPrevObjTrans(   Vector<ShaderComponent*> &componentList,                                       
  bool useInstancing,
  MultiLine *meta )
{
  Var *prevObjTrans = (Var*)LangElement::find( "prevObjTrans" );
  if ( prevObjTrans )
    return prevObjTrans;

  if ( useInstancing )
  {
    ShaderConnector *vertStruct = dynamic_cast<ShaderConnector *>( componentList[C_VERT_STRUCT] );
    Var *instPrevObjTrans = vertStruct->getElement( RT_TEXCOORD, 4, 4 );
    instPrevObjTrans->setStructName( "IN" );
    instPrevObjTrans->setName( "inst_prevObjectTrans" );

    mInstancingFormat->addElement( "prevObjTrans", GFXDeclType_Float4, instPrevObjTrans->constNum+0 );
    mInstancingFormat->addElement( "prevObjTrans", GFXDeclType_Float4, instPrevObjTrans->constNum+1 );
    mInstancingFormat->addElement( "prevObjTrans", GFXDeclType_Float4, instPrevObjTrans->constNum+2 );
    mInstancingFormat->addElement( "prevObjTrans", GFXDeclType_Float4, instPrevObjTrans->constNum+3 );

    prevObjTrans = new Var( "prevObjTrans", smMatrix4x4Name );

    UsedForDirect3D({
       meta->addStatement( new GenOp( "   @ = { // Instancing!\r\n", new DecOp( prevObjTrans ), instPrevObjTrans ) );
       meta->addStatement( new GenOp( "      @[0],\r\n", instPrevObjTrans ) );
       meta->addStatement( new GenOp( "      @[1],\r\n", instPrevObjTrans ) );
       meta->addStatement( new GenOp( "      @[2],\r\n", instPrevObjTrans ) );
       meta->addStatement( new GenOp( "      @[3] };\r\n", instPrevObjTrans ) );
    });

    UsedForOpenGL({
       meta->addStatement( new GenOp( "   @ = mat4x4( // Instancing!\r\n", new DecOp( prevObjTrans ), instPrevObjTrans ) );
       meta->addStatement( new GenOp( "      @[0],\r\n", instPrevObjTrans ) );
       meta->addStatement( new GenOp( "      @[1],\r\n", instPrevObjTrans ) );
       meta->addStatement( new GenOp( "      @[2],\r\n", instPrevObjTrans ) );
       meta->addStatement( new GenOp( "      @[3] );\r\n", instPrevObjTrans ) );
    });
  }
  else
  {
    prevObjTrans = new Var( "objTrans", getMatrix4x4Name() );
    prevObjTrans->uniform = true;
    prevObjTrans->constSortPos = cspPrimitive;
  }

  return prevObjTrans;
}

Var* ShaderFeatureCommon::getModelView(  Vector<ShaderComponent*> &componentList,                                       
                                       bool useInstancing,
                                       MultiLine *meta )
{
   Var *modelview = (Var*)LangElement::find( "modelview" );
   if ( modelview )
      return modelview;

   if ( useInstancing )
   {
      Var *objTrans = getObjTrans( componentList, useInstancing, meta );

      Var *viewProj = (Var*)LangElement::find( "viewProj" );
      if ( !viewProj )
      {
         viewProj = new Var;
         viewProj->setType( smMatrix4x4Name );
         viewProj->setName( "viewProj" );
         viewProj->uniform = true;
         viewProj->constSortPos = cspPass;        
      }

      modelview = new Var;
      modelview->setType( smMatrix4x4Name );
      modelview->setName( "modelview" );
      UsedForDirect3D(meta->addStatement( new GenOp( "   @ = mul( @, @ ); // Instancing!\r\n", new DecOp( modelview ), viewProj, objTrans ) ));
      UsedForOpenGL(meta->addStatement( new GenOp( "   @ = @ * @; // Instancing!\r\n", new DecOp( modelview ), viewProj, objTrans ) ));
   }
   else
   {
      modelview = new Var;
      modelview->setType( smMatrix4x4Name );
      modelview->setName( "modelview" );
      modelview->uniform = true;
      modelview->constSortPos = cspPrimitive;
   }

   return modelview;
}

Var* ShaderFeatureCommon::getWorldView(  Vector<ShaderComponent*> &componentList,                                       
                                       bool useInstancing,
                                       MultiLine *meta )
{
   Var *worldView = (Var*)LangElement::find( "worldViewOnly" );
   if ( worldView )
      return worldView;

   if ( useInstancing )
   {
      Var *objTrans = getObjTrans( componentList, useInstancing, meta );

      Var *worldToCamera = (Var*)LangElement::find( "worldToCamera" );
      if ( !worldToCamera )
      {
         worldToCamera = new Var;
         worldToCamera->setType( smMatrix4x4Name );
         worldToCamera->setName( "worldToCamera" );
         worldToCamera->uniform = true;
         worldToCamera->constSortPos = cspPass;        
      }

      worldView = new Var;
      worldView->setType( smMatrix4x4Name );
      worldView->setName( "worldViewOnly" );

      UsedForDirect3D(meta->addStatement( new GenOp( "   @ = mul( @, @ ); // Instancing!\r\n", new DecOp( worldView ), worldToCamera, objTrans ) ));
      UsedForOpenGL(meta->addStatement( new GenOp( "   @ = @ * @; // Instancing!\r\n", new DecOp( worldView ), worldToCamera, objTrans ) ));
   }
   else
   {
      worldView = new Var;
      worldView->setType( smMatrix4x4Name );
      worldView->setName( "worldViewOnly" );
      worldView->uniform = true;
      worldView->constSortPos = cspPrimitive;  
   }

   return worldView;
}


Var* ShaderFeatureCommon::getInvWorldView(  Vector<ShaderComponent*> &componentList,                                       
                                          bool useInstancing,
                                          MultiLine *meta )
{
   Var *viewToObj = (Var*)LangElement::find( "viewToObj" );
   if ( viewToObj )
      return viewToObj;

   if ( useInstancing )
   {
      Var *worldView = getWorldView( componentList, useInstancing, meta );

      viewToObj = new Var;
      viewToObj->setType( smMatrix3x3Name );
      viewToObj->setName( "viewToObj" );

      // We just use transpose to convert the 3x3 portion 
      // of the world view transform into its inverse.

      UsedForDirect3D(meta->addStatement( new GenOp( avar("   @ = transpose( (%s)@ ); // Instancing!\r\n", smMatrix3x3Name), new DecOp( viewToObj ), worldView ) ));
      UsedForOpenGL(meta->addStatement( new GenOp( avar("   @ = transpose( %s(@) ); // Instancing!\r\n", smMatrix3x3Name), new DecOp( viewToObj ), worldView ) ));
   }
   else
   {
      viewToObj = new Var;
      viewToObj->setType( smMatrix4x4Name );
      viewToObj->setName( "viewToObj" );
      viewToObj->uniform = true;
      viewToObj->constSortPos = cspPrimitive;
   }

   return viewToObj;
}

void ShaderFeatureCommon::getWsPosition( Vector<ShaderComponent*> &componentList,                                       
                                       bool useInstancing,
                                       MultiLine *meta,
                                       LangElement *wsPosition )
{
   Var *inPosition = (Var*)LangElement::find( "wsPosition" );
   if ( inPosition )
   {
      meta->addStatement( new GenOp( "   @ = @.xyz;\r\n", 
         wsPosition, inPosition ) );
      return;
   }

   // Get the input position.
   inPosition = (Var*)LangElement::find( "inPosition" );
   if ( !inPosition )
      inPosition = (Var*)LangElement::find( "position" );

   AssertFatal( inPosition, "ShaderFeatureCommon::getWsPosition - The vertex position was not found!" );

   Var *objTrans = getObjTrans( componentList, useInstancing, meta );

   UsedForOpenGL(meta->addStatement( new GenOp( avar("   @ = (@ * %s( @.xyz, 1 )).xyz;\r\n", getVector4Name()), wsPosition, objTrans, inPosition ) ));
   UsedForDirect3D(meta->addStatement( new GenOp( avar("   @ = mul( @, %s( @.xyz, 1 ) ).xyz;\r\n", getVector4Name()), wsPosition, objTrans, inPosition ) ));
}

void ShaderFeatureCommon::getVsPosition( Vector<ShaderComponent*> &componentList,                                       
  bool useInstancing,
  MultiLine *meta,
  LangElement *vsPosition )
{
  Var *inPosition = (Var*)LangElement::find( "vsPosition" );
  if ( inPosition )
  {
    meta->addStatement( new GenOp( "   @ = @.xyz;\r\n", 
      vsPosition, inPosition ) );
    return;
  }

  // Get the input position.
  inPosition = (Var*)LangElement::find( "inPosition" );
  if ( !inPosition )
    inPosition = (Var*)LangElement::find( "position" );

  AssertFatal( inPosition, "ShaderFeatureCommon::getVsPosition - The vertex position was not found!" );

  Var *objTrans = getWorldView( componentList, useInstancing, meta );

  UsedForDirect3D(meta->addStatement( new GenOp( avar("   @ = mul( @, %s( @.xyz, 1 ) ).xyz;\r\n", getVector4Name()), vsPosition, objTrans, inPosition ) ));
  UsedForOpenGL(meta->addStatement( new GenOp( avar("   @ = ( @ * %s( @.xyz, 1 ) ).xyz;\r\n", getVector4Name()), vsPosition, objTrans, inPosition ) ));
}

void ShaderFeatureCommon::getVsNormal( Vector<ShaderComponent*> &componentList,                                       
  bool useInstancing,
  MultiLine *meta,
  LangElement *vsNormal )
{
  Var *inNormal = (Var*)LangElement::find( "vsNormal" );
  if ( inNormal )
  {
    meta->addStatement( new GenOp( "   @ = @.xyz;\r\n", 
      vsNormal, inNormal ) );
    return;
  }

  // Get the input position.
  inNormal = (Var*)LangElement::find( "inNormal" );
  if ( !inNormal )
    inNormal = (Var*)LangElement::find( "normal" );

  AssertFatal( inNormal, "ShaderFeatureCommon::getVSNormal - The vertex normal was not found!" );

  Var *objTrans = getWorldView( componentList, useInstancing, meta );

  UsedForDirect3D(meta->addStatement( new GenOp( avar("   @ = mul( @, %s( @.xyz, 0.0f ) ).xyz;\r\n", getVector4Name()), vsNormal, objTrans, inNormal ) ));
  UsedForOpenGL(meta->addStatement( new GenOp( avar("   @ = ( @ * %s( @.xyz, 0.0f ) ).xyz;\r\n", getVector4Name()), vsNormal, objTrans, inNormal ) ));
}

Var* ShaderFeatureCommon::addOutWsPosition( Vector<ShaderComponent*> &componentList,                                       
                                          bool useInstancing,
                                          MultiLine *meta )
{
   Var *outWsPosition = (Var*)LangElement::find( "outWsPosition" );
   if ( !outWsPosition )
   {
      ShaderConnector *connectComp = dynamic_cast<ShaderConnector *>( componentList[C_CONNECTOR] );
      outWsPosition = connectComp->getElement( RT_TEXCOORD );
      outWsPosition->setName( "outWsPosition" );
      outWsPosition->setStructName( "OUT" );
      outWsPosition->setType( getVector3Name() );
      outWsPosition->mapsToSampler = false;

      getWsPosition( componentList, useInstancing, meta, outWsPosition );
   }

   return outWsPosition;
}

Var* ShaderFeatureCommon::addOutVsPosition( Vector<ShaderComponent*> &componentList,                                       
  bool useInstancing,
  MultiLine *meta )
{
  Var *outVsPosition = (Var*)LangElement::find( "outVsPosition" );
  if ( !outVsPosition )
  {
    ShaderConnector *connectComp = dynamic_cast<ShaderConnector *>( componentList[C_CONNECTOR] );
    outVsPosition = connectComp->getElement( RT_TEXCOORD );
    outVsPosition->setName( "outVsPosition" );
    outVsPosition->setStructName( "OUT" );
    outVsPosition->setType( getVector3Name() );
    outVsPosition->mapsToSampler = false;

    getVsPosition( componentList, useInstancing, meta, outVsPosition );
  }

  return outVsPosition;
}

Var* ShaderFeatureCommon::addOutVsNormal(  Vector<ShaderComponent*> &componentList,                                       
  bool useInstancing,
  MultiLine *meta )
{
  Var *outVsNormal = (Var*)LangElement::find( "outVsNormal" );
  if ( !outVsNormal )
  {
    ShaderConnector *connectComp = dynamic_cast<ShaderConnector *>( componentList[C_CONNECTOR] );
    outVsNormal = connectComp->getElement( RT_TEXCOORD );
    outVsNormal->setName( "outVsNormal" );
    outVsNormal->setStructName( "OUT" );
    outVsNormal->setType( getVector3Name() );
    outVsNormal->mapsToSampler = false;

    getVsNormal( componentList, useInstancing, meta, outVsNormal );
  }

  return outVsNormal;
}

Var* ShaderFeatureCommon::getInWsPosition( Vector<ShaderComponent*> &componentList )
{
   Var *wsPosition = (Var*)LangElement::find( "outWsPosition" );
   if ( !wsPosition )
   {
      ShaderConnector *connectComp = dynamic_cast<ShaderConnector *>( componentList[C_CONNECTOR] );
      wsPosition = connectComp->getElement( RT_TEXCOORD );
      wsPosition->setName( "outWsPosition" );
      wsPosition->setStructName( "IN" );
      wsPosition->setType( getVector3Name() );
   }

   return wsPosition;
}

Var* ShaderFeatureCommon::getInVsPosition( Vector<ShaderComponent*> &componentList )
{
  Var *vsPosition = (Var*)LangElement::find( "outVsPosition" );
  if ( !vsPosition )
  {
    ShaderConnector *connectComp = dynamic_cast<ShaderConnector *>( componentList[C_CONNECTOR] );
    vsPosition = connectComp->getElement( RT_TEXCOORD );
    vsPosition->setName( "outVsPosition" );
    vsPosition->setStructName( "IN" );
    vsPosition->setType( getVector3Name() );
  }

  return vsPosition;
}

Var* ShaderFeatureCommon::getWsView( Var *wsPosition, MultiLine *meta )
{
   Var *wsView = (Var*)LangElement::find( "wsView" );
   if ( !wsView )
   {
      wsView = new Var( "wsView", getVector3Name() );

      Var *eyePos = (Var*)LangElement::find( "eyePosWorld" );
      if ( !eyePos )
      {
         eyePos = new Var;
         eyePos->setType( getVector3Name() );
         eyePos->setName( "eyePosWorld" );
         eyePos->uniform = true;
         eyePos->constSortPos = cspPass;
      }

      meta->addStatement( new GenOp( "   @ = normalize( @ - @ );\r\n", 
         new DecOp( wsView ), eyePos, wsPosition ) );
   }

   return wsView;
}

Var* ShaderFeatureCommon::addOutDetailTexCoord(   Vector<ShaderComponent*> &componentList, 
                                                MultiLine *meta,
                                                bool useTexAnim )
{
   // Check if its already added.
   Var *outTex = (Var*)LangElement::find( "detCoord" );
   if ( outTex )
      return outTex;

   // Grab incoming texture coords.
   Var *inTex = getVertTexCoord( "texCoord" );

   // create detail variable
   Var *detScale = new Var;
   detScale->setType( getVector2Name() );
   detScale->setName( "detailScale" );
   detScale->uniform = true;
   detScale->constSortPos = cspPotentialPrimitive;
   
   // grab connector texcoord register
   ShaderConnector *connectComp = dynamic_cast<ShaderConnector *>( componentList[C_CONNECTOR] );
   outTex = connectComp->getElement( RT_TEXCOORD );
   outTex->setName( "detCoord" );
   outTex->setStructName( "OUT" );
   outTex->setType( getVector2Name() );
   outTex->mapsToSampler = true;

   if ( useTexAnim )
   {
      inTex->setType( getVector4Name() );

      // Find or create the texture matrix.
      Var *texMat = (Var*)LangElement::find( "texMat" );
      if ( !texMat )
      {
         texMat = new Var;
         texMat->setType( getMatrix4x4Name() );
         texMat->setName( "texMat" );
         texMat->uniform = true;
         texMat->constSortPos = cspPass;   
      }

      UsedForDirect3D(meta->addStatement( new GenOp( "   @ = mul(@, @) * @;\r\n", outTex, texMat, inTex, detScale ) ));
      UsedForOpenGL(meta->addStatement( new GenOp( "   @ = (@ * @) * @;\r\n", outTex, texMat, inTex, detScale ) ));
   }
   else
   {
      // setup output to mul texCoord by detail scale
      meta->addStatement( new GenOp( "   @ = @ * @;\r\n", outTex, inTex, detScale ) );
   }

   return outTex;
}

//****************************************************************************
// Base Texture
//****************************************************************************

const char* DiffuseMapFeat::diffuseMapSamplerName = "diffuseMap";

void DiffuseMapFeat::processVert( Vector<ShaderComponent*> &componentList, 
                                       const MaterialFeatureData &fd )
{
   MultiLine *meta = new MultiLine;
   getOutTexCoord(   "texCoord",
                     "texCoord", 
                     getVector2Name(), 
                     true, 
                     fd.features[MFT_TexAnim], 
                     "texMat",
                     meta, 
                     componentList );

   output = meta;
}

void DiffuseMapFeat::processPix(   Vector<ShaderComponent*> &componentList, 
                                       const MaterialFeatureData &fd )
{
   // grab connector texcoord register
   Var *inTex = getInTexCoord( "texCoord", getVector2Name(), true, componentList );

   // create texture var
   Var *diffuseMap = new Var;
   diffuseMap->setType( "sampler2D" );
   diffuseMap->setName( diffuseMapSamplerName );
   diffuseMap->uniform = true;
   diffuseMap->sampler = true;
   diffuseMap->constNum = Var::getTexUnitNum();     // used as texture unit num here

   if (  fd.features[MFT_CubeMap] )
   {
      MultiLine * meta = new MultiLine;
      
      // create sample color
      Var *diffColor = new Var;
      diffColor->setType( getVector4Name() );
      diffColor->setName( "diffuseColor" );
      LangElement *colorDecl = new DecOp( diffColor );
   
      meta->addStatement(  new GenOp( "   @ = tex2D(@, @);\r\n", 
                           colorDecl, 
                           diffuseMap, 
                           inTex ) );
      
      meta->addStatement( new GenOp( "   @;\r\n", assignColor( diffColor, Material::Mul ) ) );
      output = meta;
   }
   else if(fd.features[MFT_DiffuseMapAtlas])
   {   
      // Handle atlased textures
      // http://www.infinity-universe.com/Infinity/index.php?option=com_content&task=view&id=65&Itemid=47
      MultiLine * meta = new MultiLine;
      output = meta;

      Var *atlasedTex = new Var;
      atlasedTex->setName("atlasedTexCoord");
      atlasedTex->setType(getVector2Name());
      LangElement *atDecl = new DecOp(atlasedTex);

      // Parameters of the texture atlas
      Var *atParams  = new Var;
      atParams->setType(getVector4Name());
      atParams->setName("diffuseAtlasParams");
      atParams->uniform = true;
      atParams->constSortPos = cspPotentialPrimitive;

      // Parameters of the texture (tile) this object is using in the atlas
      Var *tileParams  = new Var;
      tileParams->setType(getVector4Name());
      tileParams->setName("diffuseAtlasTileParams");
      tileParams->uniform = true;
      tileParams->constSortPos = cspPotentialPrimitive;

      const bool is_sm3 = (GFX->getPixelShaderVersion() > 2.0f);
      if(is_sm3)
      {
         // Figure out the mip level
         meta->addStatement(new GenOp("   float2 _dx = ddx(@ * @.z);\r\n", inTex, atParams));
         meta->addStatement(new GenOp("   float2 _dy = ddy(@ * @.z);\r\n", inTex, atParams));
         meta->addStatement(new GenOp("   float mipLod = 0.5 * log2(max(dot(_dx, _dx), dot(_dy, _dy)));\r\n"));
         meta->addStatement(new GenOp("   mipLod = clamp(mipLod, 0.0, @.w);\r\n", atParams));

         // And the size of the mip level
         meta->addStatement(new GenOp("   float mipPixSz = pow(2.0, @.w - mipLod);\r\n", atParams));
         meta->addStatement(new GenOp("   float2 mipSz = mipPixSz / @.xy;\r\n", atParams));
      }
      else
      {
         meta->addStatement(new GenOp("   float2 mipSz = float2(1.0, 1.0);\r\n"));
      }

      // Tiling mode
      // TODO: Select wrap or clamp somehow
      if( true ) // Wrap
      {
         UsedForDirect3D(meta->addStatement(new GenOp("   @ = frac(@);\r\n", atDecl, inTex)));
         UsedForOpenGL(meta->addStatement(new GenOp("   @ = fract(@);\r\n", atDecl, inTex)));
      }
      else       // Clamp
      {
         meta->addStatement(new GenOp("   @ = saturate(@);\r\n", atDecl, inTex));
      }

      // Finally scale/offset, and correct for filtering
      meta->addStatement(new GenOp("   @ = @ * ((mipSz * @.xy - 1.0) / mipSz) + 0.5 / mipSz + @.xy * @.xy;\r\n", 
         atlasedTex, atlasedTex, atParams, atParams, tileParams));

      // Add a newline
      meta->addStatement(new GenOp( "\r\n"));

      // For the rest of the feature...
      inTex = atlasedTex;

      // create sample color var
      Var *diffColor = new Var;
      diffColor->setType(getVector4Name());
      diffColor->setName("diffuseColor");

      // To dump out UV coords...
//#define DEBUG_ATLASED_UV_COORDS
#ifdef DEBUG_ATLASED_UV_COORDS
      if(!fd.features[MFT_PrePassConditioner])
      {
         meta->addStatement(new GenOp("   @ = float4(@.xy, mipLod / @.w, 1.0);\r\n", new DecOp(diffColor), inTex, atParams));
         meta->addStatement(new GenOp("   @; return OUT;\r\n", assignColor(diffColor, Material::Mul)));
         return;
      }
#endif

      if(is_sm3)
      {
         meta->addStatement(new GenOp( avar("   @ = %s(@, %s(@, 0.0, mipLod));\r\n", getTexture2DLodFunction(), getVector4Name()), 
            new DecOp(diffColor), diffuseMap, inTex));
      }
      else
      {
         meta->addStatement(new GenOp( avar("   @ = %s(@, @);\r\n", getTexture2DFunction()), 
            new DecOp(diffColor), diffuseMap, inTex));
      }

      meta->addStatement(new GenOp( "   @;\r\n", assignColor(diffColor, Material::Mul)));
   }
   else
   {
      LangElement *statement = new GenOp( avar("%s(@, @)", getTexture2DFunction()), diffuseMap, inTex );
      output = new GenOp( "   @;\r\n", assignColor( statement, Material::Mul ) );
   }
   
}

ShaderFeature::Resources DiffuseMapFeat::getResources( const MaterialFeatureData &fd )
{
   Resources res; 
   res.numTex = 1;
   res.numTexReg = 1;

   return res;
}

void DiffuseMapFeat::setTexData(   Material::StageData &stageDat,
                                       const MaterialFeatureData &fd,
                                       RenderPassData &passData,
                                       U32 &texIndex )
{
   GFXTextureObject *tex = stageDat.getTex( MFT_DiffuseMap );
   if ( tex )
   {
      passData.mTexSlot[ texIndex ].samplerName = diffuseMapSamplerName;
      passData.mTexSlot[ texIndex++ ].texObject = tex;
   }
}


//****************************************************************************
// Overlay Texture
//****************************************************************************

const char* OverlayTexFeat::overlayMapSamplerName = "overlayMap";

void OverlayTexFeat::processVert(  Vector<ShaderComponent*> &componentList, 
                                       const MaterialFeatureData &fd )
{
   Var *inTex = getVertTexCoord( "texCoord2" );
   AssertFatal( inTex, "OverlayTexFeat::processVert() - The second UV set was not found!" );

   // grab connector texcoord register
   ShaderConnector *connectComp = dynamic_cast<ShaderConnector *>( componentList[C_CONNECTOR] );
   Var *outTex = connectComp->getElement( RT_TEXCOORD );
   outTex->setName( "outTexCoord2" );
   outTex->setStructName( "OUT" );
   outTex->setType( getVector2Name() );
   outTex->mapsToSampler = true;

   if( fd.features[MFT_TexAnim] )
   {
      inTex->setType( getVector4Name() );

      // Find or create the texture matrix.
      Var *texMat = (Var*)LangElement::find( "texMat" );
      if ( !texMat )
      {
         texMat = new Var;
         texMat->setType( getMatrix4x4Name() );
         texMat->setName( "texMat" );
         texMat->uniform = true;
         texMat->constSortPos = cspPass;   
      }
     
      UsedForDirect3D(output = new GenOp( "   @ = mul(@, @);\r\n", outTex, texMat, inTex ));
      UsedForOpenGL(output = new GenOp( "   @ = (@ * @);\r\n", outTex, texMat, inTex ));
      return;
   }
   
   // setup language elements to output incoming tex coords to output
   output = new GenOp( "   @ = @;\r\n", outTex, inTex );
}

void OverlayTexFeat::processPix(   Vector<ShaderComponent*> &componentList, 
                                       const MaterialFeatureData &fd )
{

   // grab connector texcoord register
   ShaderConnector *connectComp = dynamic_cast<ShaderConnector *>( componentList[C_CONNECTOR] );
   Var *inTex = connectComp->getElement( RT_TEXCOORD );
   inTex->setName( "texCoord2" );
   inTex->setStructName( "IN" );
   inTex->setType( getVector2Name() );
   inTex->mapsToSampler = true;

   // create texture var
   Var *diffuseMap = new Var;
   diffuseMap->setType( "sampler2D" );
   diffuseMap->setName( overlayMapSamplerName );
   diffuseMap->uniform = true;
   diffuseMap->sampler = true;
   diffuseMap->constNum = Var::getTexUnitNum();     // used as texture unit num here

   LangElement *statement = new GenOp( avar("%s(@, @)", getTexture2DFunction()), diffuseMap, inTex );
   output = new GenOp( "   @;\r\n", assignColor( statement, Material::LerpAlpha ) );
}

ShaderFeature::Resources OverlayTexFeat::getResources( const MaterialFeatureData &fd )
{
   Resources res; 
   res.numTex = 1;
   res.numTexReg = 1;
   return res;
}

void OverlayTexFeat::setTexData(   Material::StageData &stageDat,
                                       const MaterialFeatureData &fd,
                                       RenderPassData &passData,
                                       U32 &texIndex )
{
   GFXTextureObject *tex = stageDat.getTex( MFT_OverlayMap );
   if ( tex )
   {
      passData.mTexSlot[ texIndex ].samplerName = overlayMapSamplerName;
      passData.mTexSlot[ texIndex++ ].texObject = tex;
   }
}


//****************************************************************************
// Diffuse color
//****************************************************************************

void DiffuseFeature::processPix(   Vector<ShaderComponent*> &componentList, 
                                       const MaterialFeatureData &fd )
{
   Var *diffuseMaterialColor  = new Var;
   diffuseMaterialColor->setType( getVector4Name() );
   diffuseMaterialColor->setName( "diffuseMaterialColor" );
   diffuseMaterialColor->uniform = true;
   diffuseMaterialColor->constSortPos = cspPotentialPrimitive;

   MultiLine * meta = new MultiLine;
   meta->addStatement( new GenOp( "   @;\r\n", assignColor( diffuseMaterialColor, Material::Mul ) ) );
   output = meta;
}


//****************************************************************************
// Diffuse vertex color
//****************************************************************************

void DiffuseVertColorFeature::processVert(  Vector< ShaderComponent* >& componentList, 
                                                const MaterialFeatureData& fd )
{
   // Create vertex color connector if it doesn't exist.
   
   Var* outColor = dynamic_cast< Var* >( LangElement::find( "vertColor" ) );
   if( !outColor )
   {
      // Search for vert color.
      
      Var* inColor = dynamic_cast< Var* >( LangElement::find( "diffuse" ) );
      if( !inColor )
      {
         output = NULL;
         return;
      }
      
      // Create connector.

      ShaderConnector* connectComp = dynamic_cast< ShaderConnector* >( componentList[ C_CONNECTOR ] );
      AssertFatal( connectComp, "DiffuseVertColorFeatureGLSL::processVert - C_CONNECTOR is not a ShaderConnector" );
      Var* outColor = connectComp->getElement( RT_COLOR );
      outColor->setName( "vertColor" );
      outColor->setStructName( "OUT" );
      outColor->setType( getVector4Name() );

      output = new GenOp( "   @ = @;\r\n", outColor, inColor );
   }
   else
      output = NULL; // Nothing we need to do.
}

void DiffuseVertColorFeature::processPix(   Vector<ShaderComponent*> &componentList, 
                                                const MaterialFeatureData &fd )
{
   Var* vertColor = dynamic_cast< Var* >( LangElement::find( "vertColor" ) );
   if( !vertColor )
   {
      ShaderConnector* connectComp = dynamic_cast< ShaderConnector* >( componentList[ C_CONNECTOR ] );
      AssertFatal( connectComp, "DiffuseVertColorFeatureGLSL::processVert - C_CONNECTOR is not a ShaderConnector" );
      vertColor = connectComp->getElement( RT_COLOR );
      vertColor->setName( "vertColor" );
      vertColor->setStructName( "IN" );
      vertColor->setType( getVector4Name() );
   }
   
   MultiLine* meta = new MultiLine;
   meta->addStatement( new GenOp( "   @;\r\n", assignColor( vertColor, Material::Mul ) ) );
   output = meta;
}


//****************************************************************************
// Lightmap
//****************************************************************************

const char* LightmapFeat::lightMapSamplerName = "lightMap";

void LightmapFeat::processVert( Vector<ShaderComponent*> &componentList, 
                                    const MaterialFeatureData &fd )
{
   // grab tex register from incoming vert
   Var *inTex = getVertTexCoord( "texCoord2" );

   // grab connector texcoord register
   ShaderConnector *connectComp = dynamic_cast<ShaderConnector *>( componentList[C_CONNECTOR] );
   Var *outTex = connectComp->getElement( RT_TEXCOORD );
   outTex->setName( "texCoord2" );
   outTex->setStructName( "OUT" );
   outTex->setType( getVector2Name() );
   outTex->mapsToSampler = true;

   // setup language elements to output incoming tex coords to output
   output = new GenOp( "   @ = @;\r\n", outTex, inTex );
}

void LightmapFeat::processPix(  Vector<ShaderComponent*> &componentList, 
                                    const MaterialFeatureData &fd )
{
   // grab connector texcoord register
   ShaderConnector *connectComp = dynamic_cast<ShaderConnector *>( componentList[C_CONNECTOR] );
   Var *inTex = connectComp->getElement( RT_TEXCOORD );
   inTex->setName( "texCoord2" );
   inTex->setStructName( "IN" );
   inTex->setType( getVector2Name() );
   inTex->mapsToSampler = true;

   // create texture var
   Var *lightMap = new Var;
   lightMap->setType( "sampler2D" );
   lightMap->setName( lightMapSamplerName );
   lightMap->uniform = true;
   lightMap->sampler = true;
   lightMap->constNum = Var::getTexUnitNum();     // used as texture unit num here

   
   // argh, pixel specular should prob use this too
   if( fd.features[MFT_NormalMap] )
   {
      Var *lmColor = new Var;
      lmColor->setName( "lmColor" );
      lmColor->setType( getVector4Name() );
      LangElement *lmColorDecl = new DecOp( lmColor );
      
      output = new GenOp( avar("   @ = %s(@, @);\r\n", getTexture2DFunction()), lmColorDecl, lightMap, inTex );
      return;
   }

   // Add realtime lighting, if it is available
   LangElement *statement = NULL;
   if( fd.features[MFT_RTLighting] )
   {
      // Advanced lighting is the only dynamic lighting supported right now
      Var *inColor = (Var*) LangElement::find( "d_lightcolor" );
      if(inColor != NULL)
      {
         // Find out if RTLighting should be added or substituted
         bool bPreProcessedLighting = false;
         AdvancedLightBinManager *lightBin;
         if ( Sim::findObject( "AL_LightBinMgr", lightBin ) )
            bPreProcessedLighting = lightBin->MRTLightmapsDuringPrePass();

         // Lightmap has already been included in the advanced light bin, so
         // no need to do any sampling or anything
         if(bPreProcessedLighting)
            statement = new GenOp( avar("%s(@, 1.0)", getVector4Name()), inColor );
         else
            statement = new GenOp( avar("%s(@, @) + %s(@.rgb, 0.0)", getTexture2DFunction(), getVector4Name()), lightMap, inTex, inColor );
      }
   }
   
   // If we still don't have it... then just sample the lightmap.   
   if ( !statement )
      statement = new GenOp( avar("%s(@, @)", getTexture2DFunction()), lightMap, inTex );

   // Assign to proper render target
   MultiLine *meta = new MultiLine;
   if( fd.features[MFT_LightbufferMRT] )
   {
      meta->addStatement( new GenOp( "   @;\r\n", assignColor( statement, Material::None, NULL, ShaderFeature::RenderTarget1 ) ) );
      meta->addStatement( new GenOp( "   @.a = 0.0001;\r\n", LangElement::find( getOutputTargetVarName(ShaderFeature::RenderTarget1) ) ) );
   }
   else
      meta->addStatement( new GenOp( "   @;\r\n", assignColor( statement, Material::Mul ) ) );

   output = meta;
}

ShaderFeature::Resources LightmapFeat::getResources( const MaterialFeatureData &fd )
{
   Resources res; 
   res.numTex = 1;
   res.numTexReg = 1;

   return res;
}

void LightmapFeat::setTexData(  Material::StageData &stageDat,
                                    const MaterialFeatureData &fd,
                                    RenderPassData &passData,
                                    U32 &texIndex )
{
   GFXTextureObject *tex = stageDat.getTex( MFT_LightMap );
   if ( tex )
      passData.mTexSlot[ texIndex++ ].texObject = tex;
   else
      passData.mTexType[ texIndex++ ] = Material::Lightmap;
}

U32 LightmapFeat::getOutputTargets( const MaterialFeatureData &fd ) const
{
   return fd.features[MFT_LightbufferMRT] ? ShaderFeature::RenderTarget1 : ShaderFeature::DefaultTarget;
}

//****************************************************************************
// Tonemap
//****************************************************************************

const char* TonemapFeat::toneMapSamplerName = "toneMap";

void TonemapFeat::processVert( Vector<ShaderComponent*> &componentList, 
                                    const MaterialFeatureData &fd )
{
   // Grab the connector
   ShaderConnector *connectComp = dynamic_cast<ShaderConnector *>( componentList[C_CONNECTOR] );

   // Set up the second set of texCoords
   Var *inTex2 = getVertTexCoord( "texCoord2" );

   if ( inTex2 )
   {
      Var *outTex2 = connectComp->getElement( RT_TEXCOORD );
      outTex2->setName( "texCoord2" );
      UsedForDirect3D(outTex2->setStructName( "OUT" ));
      outTex2->setType( getVector2Name() );
      outTex2->mapsToSampler = true;

      output = new GenOp( "   @ = @;\r\n", outTex2, inTex2 );
   }
}

void TonemapFeat::processPix(  Vector<ShaderComponent*> &componentList, 
                                    const MaterialFeatureData &fd )
{
   // Grab connector
   ShaderConnector *connectComp = dynamic_cast<ShaderConnector *>( componentList[C_CONNECTOR] );

   Var *inTex2 = connectComp->getElement( RT_TEXCOORD );
   inTex2->setName( "texCoord2" );
   UsedForDirect3D(inTex2->setStructName( "IN" ));
   inTex2->setType( getVector2Name() );
   inTex2->mapsToSampler = true;

   // create texture var
   Var *toneMap = new Var;
   toneMap->setType( "sampler2D" );
   toneMap->setName( toneMapSamplerName );
   toneMap->uniform = true;
   toneMap->sampler = true;
   toneMap->constNum = Var::getTexUnitNum();     // used as texture unit num here

   MultiLine * meta = new MultiLine;

   // First get the toneMap color
   Var *toneMapColor = new Var;
   toneMapColor->setType( getVector4Name() );
   toneMapColor->setName( "toneMapColor" );
   LangElement *toneMapColorDecl = new DecOp( toneMapColor );

   meta->addStatement( new GenOp( avar("   @ = %s(@, @);\r\n", getTexture2DFunction()), toneMapColorDecl, toneMap, inTex2 ) );

   // We do a different calculation if there is a diffuse map or not
   Material::BlendOp blendOp = Material::Mul;
   if ( fd.features[MFT_DiffuseMap] )
   {
      // Reverse the tonemap
      meta->addStatement( new GenOp( "   @ = -1.0f * log(1.0f - @);\r\n", toneMapColor, toneMapColor ) );

      // Re-tonemap with the current color factored in
      blendOp = Material::ToneMap;
   }

   // Find out if RTLighting should be added
   bool bPreProcessedLighting = false;
   AdvancedLightBinManager *lightBin;
   if ( Sim::findObject( "AL_LightBinMgr", lightBin ) )
      bPreProcessedLighting = lightBin->MRTLightmapsDuringPrePass();

   // Add in the realtime lighting contribution
   if ( fd.features[MFT_RTLighting] )
   {
      // Right now, only Advanced Lighting is supported
      Var *inColor = (Var*) LangElement::find( "d_lightcolor" );
      if(inColor != NULL)
      {
         // Assign value in d_lightcolor to toneMapColor if it exists. This is
         // the dynamic light buffer, and it already has the tonemap included
         if(bPreProcessedLighting)
            meta->addStatement( new GenOp( "   @.rgb = @;\r\n", toneMapColor, inColor ) );
         else
            meta->addStatement( new GenOp( "   @.rgb += @.rgb;\r\n", toneMapColor, inColor ) );
      }
   }

   // Assign to proper render target
   if( fd.features[MFT_LightbufferMRT] )
   {
      meta->addStatement( new GenOp( "   @;\r\n", assignColor( toneMapColor, Material::None, NULL, ShaderFeature::RenderTarget1 ) ) );
      meta->addStatement( new GenOp( "   @.a = 0.0001;\r\n", LangElement::find( getOutputTargetVarName(ShaderFeature::RenderTarget1) ) ) );
   }
   else
      meta->addStatement( new GenOp( "   @;\r\n", assignColor( toneMapColor, blendOp ) ) );
  
   output = meta;
}

ShaderFeature::Resources TonemapFeat::getResources( const MaterialFeatureData &fd )
{
   Resources res; 
   res.numTex = 1;
   res.numTexReg = 1;

   return res;
}

void TonemapFeat::setTexData(  Material::StageData &stageDat,
                                    const MaterialFeatureData &fd,
                                    RenderPassData &passData,
                                    U32 &texIndex )
{
   GFXTextureObject *tex = stageDat.getTex( MFT_ToneMap );
   if ( tex )
   {
      passData.mTexType[ texIndex ] = Material::ToneMapTex;
      passData.mTexSlot[ texIndex++ ].texObject = tex;
   }
}

U32 TonemapFeat::getOutputTargets( const MaterialFeatureData &fd ) const
{
   return fd.features[MFT_LightbufferMRT] ? ShaderFeature::RenderTarget1 : ShaderFeature::DefaultTarget;
}

//****************************************************************************
// pureLIGHT Lighting
//****************************************************************************

void VertLit::processVert(   Vector<ShaderComponent*> &componentList, 
                                 const MaterialFeatureData &fd )
{
   // If we have a lightMap or toneMap then our lighting will be
   // handled by the MFT_LightMap or MFT_ToneNamp feature instead
   if ( fd.features[MFT_LightMap] || fd.features[MFT_ToneMap] )
   {
      output = NULL;
      return;
   }

   // Create vertex color connector if it doesn't exist.
   
   Var* outColor = dynamic_cast< Var* >( LangElement::find( "vertColor" ) );
   if( !outColor )
   {
      // Search for vert color
      Var *inColor = (Var*) LangElement::find( "diffuse" );   

      // If there isn't a vertex color then we can't do anything
      if( !inColor )
      {
         output = NULL;
         return;
      }

      // Grab the connector color
      ShaderConnector *connectComp = dynamic_cast<ShaderConnector *>( componentList[C_CONNECTOR] );
      Var *outColor = connectComp->getElement( RT_COLOR );
      outColor->setName( "vertColor" );
      outColor->setStructName( "OUT" );
      outColor->setType( getVector4Name() );

      output = new GenOp( "   @ = @;\r\n", outColor, inColor );
   }
   else
      output = NULL; // Nothing we need to do.
}

void VertLit::processPix(   Vector<ShaderComponent*> &componentList, 
                                       const MaterialFeatureData &fd )
{
   // If we have a lightMap or toneMap then our lighting will be
   // handled by the MFT_LightMap or MFT_ToneNamp feature instead
   if ( fd.features[MFT_LightMap] || fd.features[MFT_ToneMap] )
   {
      output = NULL;
      return;
   }
   
   // Grab the connector color register
   Var* vertColor = dynamic_cast< Var* >( LangElement::find( "vertColor" ) );
   if( !vertColor )
   {
      ShaderConnector* connectComp = dynamic_cast< ShaderConnector* >( componentList[ C_CONNECTOR ] );
      AssertFatal( connectComp, "VertLitGLSL::processVert - C_CONNECTOR is not a ShaderConnector" );
      vertColor = connectComp->getElement( RT_COLOR );
      vertColor->setName( "vertColor" );
      vertColor->setStructName( "IN" );
      vertColor->setType( getVector4Name() );
   }

   MultiLine * meta = new MultiLine;

   // Defaults (no diffuse map)
   Material::BlendOp blendOp = Material::Mul;
   LangElement *outColor = vertColor;

   // We do a different calculation if there is a diffuse map or not
   if ( fd.features[MFT_DiffuseMap] || fd.features[MFT_VertLitTone] )
   {
      Var * finalVertColor = new Var;
      finalVertColor->setName( "finalVertColor" );
      finalVertColor->setType( getVector4Name() );
      LangElement *finalVertColorDecl = new DecOp( finalVertColor );
      
      // Reverse the tonemap
      meta->addStatement( new GenOp( "   @ = -1.0f * log(1.0f - @);\r\n", finalVertColorDecl, vertColor ) );

      // Set the blend op to tonemap
      blendOp = Material::ToneMap;
      outColor = finalVertColor;
   }

   // Add in the realtime lighting contribution, if applicable
   if ( fd.features[MFT_RTLighting] )
   {
      Var *rtLightingColor = (Var*) LangElement::find( "d_lightcolor" );
      if(rtLightingColor != NULL)
      {
         bool bPreProcessedLighting = false;
         AdvancedLightBinManager *lightBin;
         if ( Sim::findObject( "AL_LightBinMgr", lightBin ) )
            bPreProcessedLighting = lightBin->MRTLightmapsDuringPrePass();

         // Assign value in d_lightcolor to toneMapColor if it exists. This is
         // the dynamic light buffer, and it already has the baked-vertex-color 
         // included in it
         if(bPreProcessedLighting)
            outColor = new GenOp( avar("%s(@.rgb, 1.0)", getVector4Name()), rtLightingColor );
         else
            outColor = new GenOp( avar("%s(@.rgb + @.rgb, 1.0)", getVector4Name()), rtLightingColor, outColor );
      }
   }

   // Output the color
   if ( fd.features[MFT_LightbufferMRT] )
   {
      meta->addStatement( new GenOp( "   @;\r\n", assignColor( outColor, Material::None, NULL, ShaderFeature::RenderTarget1 ) ) );
      meta->addStatement( new GenOp( "   @.a = 0.0001;\r\n", LangElement::find( getOutputTargetVarName(ShaderFeature::RenderTarget1) ) ) );
   }
   else
      meta->addStatement( new GenOp( "   @;\r\n", assignColor( outColor, blendOp ) ) );

   output = meta;
}

U32 VertLit::getOutputTargets( const MaterialFeatureData &fd ) const
{
   return fd.features[MFT_LightbufferMRT] ? ShaderFeature::RenderTarget1 : ShaderFeature::DefaultTarget;
}

//****************************************************************************
// Detail map
//****************************************************************************

const char* DetailFeat::detailMapSamplerName = "detailMap";

void DetailFeat::processVert(   Vector<ShaderComponent*> &componentList, 
                                    const MaterialFeatureData &fd )
{
   MultiLine *meta = new MultiLine;
   addOutDetailTexCoord( componentList, 
                         meta,
                         fd.features[MFT_TexAnim] );
   output = meta;
}

void DetailFeat::processPix( Vector<ShaderComponent*> &componentList, 
                                 const MaterialFeatureData &fd )
{
   // Get the detail texture coord.
   Var *inTex = getInTexCoord( "detCoord", getVector2Name(), true, componentList );

   // create texture var
   Var *detailMap = new Var;
   detailMap->setType( "sampler2D" );
   detailMap->setName( "detailMap" );
   detailMap->uniform = true;
   detailMap->sampler = true;
   detailMap->constNum = Var::getTexUnitNum();     // used as texture unit num here

   // We're doing the standard greyscale detail map
   // technique which can darken and lighten the 
   // diffuse texture.

   // TODO: We could add a feature to toggle between this
   // and a simple multiplication with the detail map.

   LangElement *statement = new GenOp( avar("( %s(@, @) * 2.0 ) - 1.0", getTexture2DFunction()), detailMap, inTex );
   output = new GenOp( "   @;\r\n", assignColor( statement, Material::Add ) );
}

ShaderFeature::Resources DetailFeat::getResources( const MaterialFeatureData &fd )
{
   Resources res; 
   res.numTex = 1;
   res.numTexReg = 1;

   return res;
}

void DetailFeat::setTexData( Material::StageData &stageDat,
                                 const MaterialFeatureData &fd,
                                 RenderPassData &passData,
                                 U32 &texIndex )
{
   GFXTextureObject *tex = stageDat.getTex( MFT_DetailMap );
   if ( tex )
      passData.mTexSlot[ texIndex++ ].texObject = tex;
}


//****************************************************************************
// Vertex position
//****************************************************************************

void VertPosition::determineFeature(  Material *material,
                                          const GFXVertexFormat *vertexFormat,
                                          U32 stageNum,
                                          const FeatureType &type,
                                          const FeatureSet &features,
                                          MaterialFeatureData *outFeatureData )
{
   // This feature is always on!
   outFeatureData->features.addFeature( type );
}

void VertPosition::processVert( Vector<ShaderComponent*> &componentList, 
                                    const MaterialFeatureData &fd )
{
   // First check for an input position from a previous feature
   // then look for the default vertex position.
   Var *inPosition = (Var*)LangElement::find( "inPosition" );
   if ( !inPosition )
      inPosition = (Var*)LangElement::find( "position" );
   
   AssertFatal(inPosition != NULL, "No position!");

   // grab connector position

   Var *outPosition = NULL;

   UsedForOpenGL({
   ShaderConnector *connectComp = dynamic_cast<ShaderConnector *>( componentList[C_CONNECTOR] );
   outPosition = new Var;
   outPosition->setName( "gl_Position" );
   outPosition->setType( getVector4Name() );
   });

   UsedForDirect3D({
   ShaderConnector *connectComp = dynamic_cast<ShaderConnector *>( componentList[C_CONNECTOR] );
   outPosition = connectComp->getElement( RT_POSITION );
   outPosition->setName( "hpos" );
   outPosition->setStructName("OUT");
   });

   MultiLine *meta = new MultiLine;

   Var *modelview = getModelView( componentList, fd.features[MFT_UseInstancing], meta );
   
   AssertFatal(modelview != NULL, "No modelview!");

   UsedForDirect3D(meta->addStatement( new GenOp( "   @ = mul(@, float4(@.xyz,1));\r\n", outPosition, modelview, inPosition ) ));
   UsedForOpenGL(meta->addStatement( new GenOp( "   @ = (@ * vec4(@.xyz,1));\r\n", outPosition, modelview, inPosition ) ));
   
   output = meta;
}


//****************************************************************************
// Reflect Cubemap
//****************************************************************************

const char* ReflectCubeFeat::cubeMapSamplerName = "cubeMap";
const char* ReflectCubeFeat::glossMapSamplerName = "glossMap";

void ReflectCubeFeat::processVert( Vector<ShaderComponent*> &componentList, 
                                       const MaterialFeatureData &fd )
{
   // search for vert normal
   Var *inNormal = (Var*)LangElement::find( "inNormal" );
   if (!inNormal) inNormal = (Var*) LangElement::find( "normal" );
   if ( !inNormal )
      return;

   MultiLine * meta = new MultiLine;

   // If a base or bump tex is present in the material, but not in the
   // current pass - we need to add one to the current pass to use
   // its alpha channel as a gloss map.  Here we just need the tex coords.
   if( !fd.features[MFT_DiffuseMap] &&
       !fd.features[MFT_NormalMap] )
   {
      if( fd.materialFeatures[MFT_DiffuseMap] ||
          fd.materialFeatures[MFT_NormalMap] )
      {
         // find incoming texture var
         Var *inTex = getVertTexCoord( "texCoord" );

         // grab connector texcoord register
         ShaderConnector *connectComp = dynamic_cast<ShaderConnector *>( componentList[C_CONNECTOR] );
         Var *outTex = connectComp->getElement( RT_TEXCOORD );
         outTex->setName( "texCoord" );
         outTex->setStructName( "OUT" );
         outTex->setType( getVector2Name() );
         outTex->mapsToSampler = true;

         // setup language elements to output incoming tex coords to output
         meta->addStatement( new GenOp( "   @ = @;\r\n", outTex, inTex ) );
      }
   }

   // create cubeTrans
   Var *cubeTrans = new Var;
   cubeTrans->setType( getMatrix3x3Name() );
   cubeTrans->setName( "cubeTrans" );
   cubeTrans->uniform = true;
   cubeTrans->constSortPos = cspPrimitive;   

   // create cubeEye position
   Var *cubeEyePos = new Var;
   cubeEyePos->setType( getVector3Name() );
   cubeEyePos->setName( "cubeEyePos" );
   cubeEyePos->uniform = true;
   cubeEyePos->constSortPos = cspPrimitive;   

   // cube vert position
   Var * cubeVertPos = new Var;
   cubeVertPos->setName( "cubeVertPos" );
   cubeVertPos->setType( getVector3Name() );
   LangElement *cubeVertPosDecl = new DecOp( cubeVertPos );

   UsedForOpenGL(meta->addStatement( new GenOp( "   @ = (@ * @).xyz;\r\n", cubeVertPosDecl, cubeTrans, LangElement::find( "position" ) ) ));
   UsedForDirect3D(meta->addStatement( new GenOp( "   @ = mul(@, @).xyz;\r\n", cubeVertPosDecl, cubeTrans, LangElement::find( "position" ) ) ));

   // cube normal
   Var * cubeNormal = new Var;
   cubeNormal->setName( "cubeNormal" );
   cubeNormal->setType( getVector3Name() );
   LangElement *cubeNormDecl = new DecOp( cubeNormal );

   if (GFX->getAdapterType() == OpenGL)
   {
     meta->addStatement( new GenOp( "   @ = @;\r\n", cubeNormDecl, inNormal));
     meta->addStatement( new GenOp( "   if (dot(@, @) > 0.0f)\r\n", cubeNormal, cubeNormal));
     meta->addStatement( new GenOp( "   {\r\n"));
     meta->addStatement( new GenOp( "      @ = normalize( (@ * normalize(@)).xyz );\r\n", cubeNormal, cubeTrans, cubeNormal ) );
     meta->addStatement( new GenOp( "   }\r\n"));
   }
   else
   {
     meta->addStatement( new GenOp( "   @ = normalize( mul(@, normalize(@)).xyz );\r\n", cubeNormDecl, cubeTrans, inNormal ) );
   }

   // eye to vert
   Var * eyeToVert = new Var;
   eyeToVert->setName( "eyeToVert" );
   eyeToVert->setType( getVector3Name() );
   LangElement *e2vDecl = new DecOp( eyeToVert );

   meta->addStatement( new GenOp( "   @ = @ - @;\r\n", 
                       e2vDecl, cubeVertPos, cubeEyePos ) );

   // grab connector texcoord register
   ShaderConnector *connectComp = dynamic_cast<ShaderConnector *>( componentList[C_CONNECTOR] );
   Var *reflectVec = connectComp->getElement( RT_TEXCOORD );
   reflectVec->setName( "reflectVec" );
   reflectVec->setStructName( "OUT" );
   reflectVec->setType( getVector3Name() );
   reflectVec->mapsToSampler = true;

   meta->addStatement( new GenOp( "   @ = reflect(@, @);\r\n", reflectVec, eyeToVert, cubeNormal ) );

   output = meta;
}

void ReflectCubeFeat::processPix(  Vector<ShaderComponent*> &componentList, 
                                       const MaterialFeatureData &fd )
{
   MultiLine * meta = new MultiLine;
   Var *glossColor = NULL;
   
   // If a base or bump tex is present in the material, but not in the
   // current pass - we need to add one to the current pass to use
   // its alpha channel as a gloss map.
   if( !fd.features[MFT_DiffuseMap] &&
       !fd.features[MFT_NormalMap] )
   {
      if( fd.materialFeatures[MFT_DiffuseMap] ||
          fd.materialFeatures[MFT_NormalMap] )
      {
         // grab connector texcoord register
         Var *inTex = getInTexCoord( "texCoord", getVector2Name(), true, componentList );
      
         // create texture var
         Var *newMap = new Var;
         newMap->setType( "sampler2D" );
         newMap->setName( glossMapSamplerName );
         newMap->uniform = true;
         newMap->sampler = true;
         newMap->constNum = Var::getTexUnitNum();     // used as texture unit num here
      
         // create sample color
         Var *color = new Var;
         color->setType( getVector4Name() );
         color->setName( "diffuseColor" );
         LangElement *colorDecl = new DecOp( color );

         glossColor = color;
         
         meta->addStatement( new GenOp( avar("   @ = %s( @, @ );\r\n", getTexture2DFunction()), colorDecl, newMap, inTex ) );
      }
   }
   else
   {
      glossColor = (Var*) LangElement::find( "diffuseColor" );
      if( !glossColor )
         glossColor = (Var*) LangElement::find( "bumpNormal" );
   }

   // grab connector texcoord register
   ShaderConnector *connectComp = dynamic_cast<ShaderConnector *>( componentList[C_CONNECTOR] );
   Var *reflectVec = connectComp->getElement( RT_TEXCOORD );
   reflectVec->setName( "reflectVec" );
   reflectVec->setStructName( "IN" );
   reflectVec->setType( getVector3Name() );
   reflectVec->mapsToSampler = true;

   // create cubemap var
   Var *cubeMap = new Var;
   UsedForDirect3D(cubeMap->setType( "samplerCUBE" ));
   UsedForOpenGL(cubeMap->setType( "samplerCube" ));
   cubeMap->setName( cubeMapSamplerName );
   cubeMap->uniform = true;
   cubeMap->sampler = true;
   cubeMap->constNum = Var::getTexUnitNum();     // used as texture unit num here

   // TODO: Restore the lighting attenuation here!
   Var *attn = NULL;
   //if ( fd.materialFeatures[MFT_DynamicLight] )
	   //attn = (Var*)LangElement::find("attn");
   //else 
      if ( fd.materialFeatures[MFT_RTLighting] )
      attn =(Var*)LangElement::find("d_NL_Att");

   LangElement *texCube = new GenOp( avar("%s( @, @ )", getTextureCubeFunction()), cubeMap, reflectVec );
   LangElement *lerpVal = NULL;
   Material::BlendOp blendOp = Material::LerpAlpha;

   // Note that the lerpVal needs to be a float4 so that
   // it will work with the LerpAlpha blend.

   if ( glossColor )
   {
      if ( attn )
         lerpVal = new GenOp( "@ * saturate( @ )", glossColor, attn );
      else
         lerpVal = glossColor;
   }
   else
   {
      UsedForDirect3D(if ( attn )
                      lerpVal = new GenOp( "saturate( @ ).xxxx", attn );
                      else
                      blendOp = Material::Mul;);
      UsedForOpenGL(if ( attn )
                    lerpVal = new GenOp( "vec4(saturate( @ ))", attn );
                    else
                    blendOp = Material::Mul;);
   }

   meta->addStatement( new GenOp( "   @;\r\n", assignColor( texCube, blendOp, lerpVal ) ) );         
   output = meta;
}

ShaderFeature::Resources ReflectCubeFeat::getResources( const MaterialFeatureData &fd )
{
   Resources res; 

   if( fd.features[MFT_DiffuseMap] ||
       fd.features[MFT_NormalMap] )
   {
      res.numTex = 1;
      res.numTexReg = 1;
   }
   else
   {
      res.numTex = 2;
      res.numTexReg = 2;
   }

   return res;
}

void ReflectCubeFeat::setTexData(  Material::StageData &stageDat,
                                       const MaterialFeatureData &stageFeatures,
                                       RenderPassData &passData,
                                       U32 &texIndex )
{
   // set up a gloss map if one is not present in the current pass
   // but is present in the current material stage
   if( !passData.mFeatureData.features[MFT_DiffuseMap] &&
       !passData.mFeatureData.features[MFT_NormalMap] )
   {
      GFXTextureObject *tex = stageDat.getTex( MFT_DetailMap );
      if (  tex &&
            stageFeatures.features[MFT_DiffuseMap] )
      {
         passData.mTexSlot[ texIndex ].samplerName = DiffuseMapFeat::diffuseMapSamplerName;
         passData.mTexSlot[ texIndex++ ].texObject = tex;
      }
      else
      {
         tex = stageDat.getTex( MFT_NormalMap );

         if (  tex &&
               stageFeatures.features[ MFT_NormalMap ] )
         {
            passData.mTexSlot[ texIndex ].samplerName = "bumpMap";
            passData.mTexSlot[ texIndex++ ].texObject = tex;
         }
      }
   }
   
   if( stageDat.getCubemap() )
   {
      passData.mCubeMap = stageDat.getCubemap();
      passData.mTexSlot[texIndex].samplerName = cubeMapSamplerName;
      passData.mTexType[texIndex++] = Material::Cube;
   }
   else
   {
      if( stageFeatures.features[MFT_CubeMap] )
      {
         // assuming here that it is a scenegraph cubemap
         passData.mTexSlot[texIndex].samplerName = cubeMapSamplerName;
         passData.mTexType[texIndex++] = Material::SGCube;
      }
   }

}


//****************************************************************************
// RTLighting
//****************************************************************************

RTLightingFeat::RTLightingFeat()
   : mDep( GFX->getAdapterType() == OpenGL ? "shaders/common/gl/lighting.glsl" : "shaders/common/lighting.hlsl" )
{
   addDependency( &mDep );
}

void RTLightingFeat::processVert(  Vector<ShaderComponent*> &componentList, 
                                       const MaterialFeatureData &fd )
{
   MultiLine *meta = new MultiLine;   

   ShaderConnector *connectComp = dynamic_cast<ShaderConnector *>( componentList[C_CONNECTOR] );

   // Special case for lighting imposters. We dont have a vert normal and may not
   // have a normal map. Generate and pass the normal data the pixel shader needs.
   if ( fd.features[MFT_ImposterVert] )
   {
      if ( !fd.features[MFT_NormalMap] )
      {
         Var *eyePos = (Var*)LangElement::find( "eyePosWorld" );
         if ( !eyePos )
         {
            eyePos = new Var( "eyePosWorld", getVector3Name() );
            eyePos->uniform = true;
            eyePos->constSortPos = cspPass;
         }
          
         Var *inPosition = (Var*)LangElement::find( "position" );

         Var *outNormal = connectComp->getElement( RT_TEXCOORD );
         outNormal->setName( "wsNormal" );
         outNormal->setStructName( "OUT" );
         outNormal->setType( getVector3Name() );
         outNormal->mapsToSampler = false;

         // Transform the normal to world space.
         meta->addStatement( new GenOp( "   @ = normalize( @ - @.xyz );\r\n", outNormal, eyePos, inPosition ) );
      }

      addOutWsPosition( componentList, fd.features[MFT_UseInstancing], meta );

      output = meta;

      return;
   }

   // Find the incoming vertex normal.
   Var *inNormal = (Var*)LangElement::find( "inNormal" );
   if (!inNormal) inNormal = (Var*) LangElement::find( "normal" );

   // Skip out on realtime lighting if we don't have a normal
   // or we're doing some sort of baked lighting.
   if (  !inNormal || 
         fd.features[MFT_LightMap] || 
         fd.features[MFT_ToneMap] || 
         fd.features[MFT_VertLit] )
      return;   

   // If there isn't a normal map then we need to pass
   // the world space normal to the pixel shader ourselves.
   if ( !fd.features[MFT_NormalMap] )
   {
      Var *outNormal = connectComp->getElement( RT_TEXCOORD );
      outNormal->setName( "wsNormal" );
      outNormal->setStructName( "OUT" );
      outNormal->setType( getVector3Name() );
      outNormal->mapsToSampler = false;

      // Get the transform to world space.
      Var *objTrans = getObjTrans( componentList, fd.features[MFT_UseInstancing], meta );

      // Transform the normal to world space.
      UsedForDirect3D(meta->addStatement( new GenOp( "   @ = mul( @, float4( normalize( @ ), 0.0 ) ).xyz;\r\n", outNormal, objTrans, inNormal ) ));
      UsedForOpenGL(meta->addStatement( new GenOp( "   @ = ( @ * vec4( normalize( @ ), 0.0 ) ).xyz;\r\n", outNormal, objTrans, inNormal ) ));
   }

   addOutWsPosition( componentList, fd.features[MFT_UseInstancing], meta );

   output = meta;
}

void RTLightingFeat::processPix(   Vector<ShaderComponent*> &componentList, 
                                       const MaterialFeatureData &fd )
{
   // Skip out on realtime lighting if we don't have a normal
   // or we're doing some sort of baked lighting.
   //
   // TODO: We can totally detect for this in the material
   // feature setup... we should move it out of here!
   //
   if ( fd.features[MFT_LightMap] || fd.features[MFT_ToneMap] || fd.features[MFT_VertLit] )
      return;
  
   ShaderConnector *connectComp = dynamic_cast<ShaderConnector *>( componentList[C_CONNECTOR] );

   MultiLine *meta = new MultiLine;

   // Look for a wsNormal or grab it from the connector.
   Var *wsNormal = (Var*)LangElement::find( "wsNormal" );
   if ( !wsNormal )
   {
		wsNormal = connectComp->getElement( RT_TEXCOORD );
      wsNormal->setName( "wsNormal" );
      wsNormal->setStructName( "IN" );
      wsNormal->setType( getVector3Name() );

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

   // Now the wsPosition and wsView.
   Var *wsPosition = getInWsPosition( componentList );
   Var *wsView = getWsView( wsPosition, meta );

   // Create temporaries to hold results of lighting.
   Var *rtShading = new Var( "rtShading", getVector4Name() );
   Var *specular = new Var( "specular", getVector4Name() );
   meta->addStatement( new GenOp( "   @; @;\r\n", 
      new DecOp( rtShading ), new DecOp( specular ) ) );   

   // Look for a light mask generated from a previous
   // feature (this is done for BL terrain lightmaps).
   LangElement *lightMask = LangElement::find( "lightMask" );
   if ( !lightMask )
      lightMask = new GenOp( avar("%s( 1, 1, 1, 1 )", getVector4Name()) );

   // Get all the light constants.
   Var *inLightPos  = new Var( "inLightPos", getVector4Name() );
   inLightPos->uniform = true;
   inLightPos->arraySize = 3;
   inLightPos->constSortPos = cspPotentialPrimitive;

   Var *inLightInvRadiusSq  = new Var( "inLightInvRadiusSq", getVector4Name() );
   inLightInvRadiusSq->uniform = true;
   inLightInvRadiusSq->constSortPos = cspPotentialPrimitive;

   Var *inLightColor  = new Var( "inLightColor", getVector4Name() );
   inLightColor->uniform = true;
   inLightColor->arraySize = 4;
   inLightColor->constSortPos = cspPotentialPrimitive;

   Var *inLightSpotDir  = new Var( "inLightSpotDir", getVector4Name() );
   inLightSpotDir->uniform = true;
   inLightSpotDir->arraySize = 3;
   inLightSpotDir->constSortPos = cspPotentialPrimitive;

   Var *inLightSpotAngle  = new Var( "inLightSpotAngle", getVector4Name() );
   inLightSpotAngle->uniform = true;
   inLightSpotAngle->constSortPos = cspPotentialPrimitive;

   Var *lightSpotFalloff  = new Var( "inLightSpotFalloff", getVector4Name() );
   lightSpotFalloff->uniform = true;
   lightSpotFalloff->constSortPos = cspPotentialPrimitive;

   Var *specularPower  = new Var( "specularPower", "float" );
   specularPower->uniform = true;
   specularPower->constSortPos = cspPotentialPrimitive;

   Var *specularColor = (Var*)LangElement::find( "specularColor" );
   if ( !specularColor )
   {
      specularColor  = new Var( "specularColor", getVector4Name() );
      specularColor->uniform = true;
      specularColor->constSortPos = cspPotentialPrimitive;
   }

   Var *ambient  = new Var( "ambient", getVector4Name() );
   ambient->uniform = true;
   ambient->constSortPos = cspPass;

   // Calculate the diffuse shading and specular powers.
   meta->addStatement( new GenOp( "   compute4Lights( @, @, @, @,\r\n"
                                  "      @, @, @, @, @, @, @, @,\r\n"
                                  "      @, @ );\r\n", 
      wsView, wsPosition, wsNormal, lightMask,
      inLightPos, inLightInvRadiusSq, inLightColor, inLightSpotDir, inLightSpotAngle, lightSpotFalloff, specularPower, specularColor,
      rtShading, specular ) );

   // Apply the lighting to the diffuse color.
   LangElement *lighting = new GenOp( avar("%s( @.rgb + @.rgb, 1 )", getVector4Name()), rtShading, ambient );
   meta->addStatement( new GenOp( "   @;\r\n", assignColor( lighting, Material::Mul ) ) );
   output = meta;  
}

ShaderFeature::Resources RTLightingFeat::getResources( const MaterialFeatureData &fd )
{
   Resources res;

   // These features disable realtime lighting.
   if (  !fd.features[MFT_LightMap] && 
         !fd.features[MFT_ToneMap] &&
         !fd.features[MFT_VertLit] )
   {
      // If enabled we pass the position.
      res.numTexReg = 1;

      // If there isn't a bump map then we pass the
      // world space normal as well.
      if ( !fd.features[MFT_NormalMap] )
         res.numTexReg++;
   }

   return res;
}


//****************************************************************************
// Fog
//****************************************************************************

FogFeat::FogFeat()
   : mFogDep( GFX->getAdapterType() == OpenGL ? "shaders/common/gl/torque.glsl" : "shaders/common/torque.hlsl" )
{
   addDependency( &mFogDep );
}

void FogFeat::processVert(   Vector<ShaderComponent*> &componentList, 
                                 const MaterialFeatureData &fd )
{
   MultiLine *meta = new MultiLine;

   const bool vertexFog = Con::getBoolVariable( "$useVertexFog", false );
   if ( vertexFog || false )//GFX->getPixelShaderVersion() < 3.0 )
   {
      // Grab the eye position.
      Var *eyePos = (Var*)LangElement::find( "eyePosWorld" );
      if ( !eyePos )
      {
         eyePos = new Var( "eyePosWorld", getVector3Name() );
         eyePos->uniform = true;
         eyePos->constSortPos = cspPass;
      }

      Var *fogData = new Var( "fogData", getVector3Name() );
      fogData->uniform = true;
      fogData->constSortPos = cspPass;   

      Var *wsPosition = new Var( "fogPos", getVector3Name() );
      getWsPosition( componentList, 
                     fd.features[MFT_UseInstancing], 
                     meta,
                     new DecOp( wsPosition ) );

      // We pass the fog amount to the pixel shader.
      ShaderConnector *connectComp = dynamic_cast<ShaderConnector *>( componentList[C_CONNECTOR] );
      Var *fogAmount = connectComp->getElement( RT_TEXCOORD );
      fogAmount->setName( "fogAmount" );
      fogAmount->setStructName( "OUT" );
      fogAmount->setType( "float" );
      fogAmount->mapsToSampler = false;

      meta->addStatement( new GenOp( "   @ = saturate( computeSceneFog( @, @, @.r, @.g, @.b ) );\r\n", 
         fogAmount, eyePos, wsPosition, fogData, fogData, fogData ) );
   }
   else
   {
      // We fog in world space... make sure the world space
      // position is passed to the pixel shader.  This is
      // often already passed for lighting, so it takes up
      // no extra output registers.
      addOutWsPosition( componentList, fd.features[MFT_UseInstancing], meta );
   }

   output = meta;
}

void FogFeat::processPix( Vector<ShaderComponent*> &componentList, 
                              const MaterialFeatureData &fd )
{
   MultiLine *meta = new MultiLine;

   Var *fogColor = new Var;
   fogColor->setType( getVector4Name() );
   fogColor->setName( "fogColor" );
   fogColor->uniform = true;
   fogColor->constSortPos = cspPass;

   // Get the out color.
   Var *color = (Var*) LangElement::find( "col" );
   if ( !color )
   {
      color = new Var;
      color->setType( "fragout" );
      color->setName( "col" );
      color->setStructName( "OUT" );
   }

   Var *fogAmount;

   const bool vertexFog = Con::getBoolVariable( "$useVertexFog", false );
   if ( vertexFog || false)//GFX->getPixelShaderVersion() < 3.0 )
   {
      // Per-vertex.... just get the fog amount.
      ShaderConnector *connectComp = dynamic_cast<ShaderConnector *>( componentList[C_CONNECTOR] );
      fogAmount = connectComp->getElement( RT_TEXCOORD );
      fogAmount->setName( "fogAmount" );
      fogAmount->setStructName( "IN" );
      fogAmount->setType( "float" );
   }
   else
   {
      Var *wsPosition = getInWsPosition( componentList );

      // grab the eye position
      Var *eyePos = (Var*)LangElement::find( "eyePosWorld" );
      if ( !eyePos )
      {
         eyePos = new Var( "eyePosWorld", getVector3Name() );
         eyePos->uniform = true;
         eyePos->constSortPos = cspPass;
      }

      Var *fogData = new Var( "fogData", getVector3Name() );
      fogData->uniform = true;
      fogData->constSortPos = cspPass;

      if (fd.features.hasFeature(MFT_IsTranslucent))
      {
        /// Diffuse intensity
        Var* colorIntensity = new Var( "colorIntensity", "float" );
        meta->addStatement( new GenOp( avar("   @ = min(dot(%s(1.0f, 1.0f, 1.0f), @.rgb), 1.0f);\r\n", getVector3Name()), new DecOp(colorIntensity), color));

        /// Get the fog amount.
        fogAmount = new Var( "fogAmount", "float" );
        meta->addStatement( new GenOp( "   @ = (1.0f - saturate( computeSceneFog( @, @, @.r, @.g, @.b ) ) ) * @;\r\n\n", 
           new DecOp( fogAmount ), eyePos, wsPosition, fogData, fogData, fogData, colorIntensity) );
      }
      else
      {
        // 1 - (1 - fogCont) * (1 - OUT.col.rgb)

        /// Get the fog amount.
        fogAmount = new Var( "fogAmount", "float" );
        meta->addStatement( new GenOp( "   @ = 1.0f - saturate( computeSceneFog( @, @, @.r, @.g, @.b) );\r\n\n", 
          new DecOp( fogAmount ), eyePos, wsPosition, fogData, fogData, fogData) );
      }
   }

   Var* fogContribution = new Var( "fogContribution", getVector3Name() );

   // Blend between the source color and fog color using a "screen" blend mode.
   meta->addStatement( new GenOp( "   // Perform a screen blend. Result = 1.0f - (1 - fog) * (1 - color)\r\n"));
   meta->addStatement( new GenOp( "   @ = 1.0f - ((1.0f - (@.rgb * @)) * (1.0f - @.rgb));\r\n", new DecOp(fogContribution), fogColor, fogAmount, color) );
   meta->addStatement( new GenOp( "   @.rgb = @;\r\n", color, fogContribution ) );

   output = meta;
}

ShaderFeature::Resources FogFeat::getResources( const MaterialFeatureData &fd )
{
   Resources res;
   res.numTexReg = 1;
   return res;
}


//****************************************************************************
// Visibility
//****************************************************************************

VisibilityFeat::VisibilityFeat()
   : mTorqueDep( GFX->getAdapterType() == OpenGL ? "shaders/common/gl/torque.glsl" : "shaders/common/torque.hlsl" )
{
   addDependency( &mTorqueDep );
}

void VisibilityFeat::processVert( Vector<ShaderComponent*> &componentList, 
                                      const MaterialFeatureData &fd )
{
   MultiLine *meta = new MultiLine;
   output = meta;

   if ( fd.features[ MFT_UseInstancing ] )
   {
      // We pass the visibility to the pixel shader via
      // another output register.
      //
      // TODO: We should see if we can share this register
      // with some other common instanced data.
      //
      ShaderConnector *conn = dynamic_cast<ShaderConnector *>( componentList[C_CONNECTOR] );
      Var *outVisibility = conn->getElement( RT_TEXCOORD );
      outVisibility->setStructName( "OUT" );
      outVisibility->setName( "visibility" );
      outVisibility->setType( "float" );

      ShaderConnector *vertStruct = dynamic_cast<ShaderConnector *>( componentList[C_VERT_STRUCT] );
      Var *instVisibility = vertStruct->getElement( RT_TEXCOORD, 1 );
      instVisibility->setStructName( "IN" );
      instVisibility->setName( "inst_visibility" );
      instVisibility->setType( "float" );
      mInstancingFormat->addElement( "visibility", GFXDeclType_Float, instVisibility->constNum );

      meta->addStatement( new GenOp( "   @ = @; // Instancing!\r\n", outVisibility, instVisibility ) );
   }

   if ( fd.features[ MFT_IsTranslucent ] )
      return;

   addOutVpos( meta, componentList );
}

void VisibilityFeat::processPix(   Vector<ShaderComponent*> &componentList, 
                                       const MaterialFeatureData &fd )
{
   // Get the visibility constant.
   Var *visibility = NULL;
   if ( fd.features[ MFT_UseInstancing ] )
      visibility = getInTexCoord( "visibility", "float", false, componentList );
   else
   {
      visibility = (Var*)LangElement::find( "visibility" );

      if ( !visibility )
      {
         visibility = new Var();
         visibility->setType( "float" );
         visibility->setName( "visibility" );
         visibility->uniform = true;
         visibility->constSortPos = cspPotentialPrimitive;  
      }
   }

   MultiLine *meta = new MultiLine;
   output = meta;

   // Translucent objects do a simple alpha fade.
   if ( fd.features[ MFT_IsTranslucent ] )
   {
      Var *color = (Var*)LangElement::find( "col" );      
      meta->addStatement( new GenOp( "   @.a *= @;\r\n", color, visibility ) );
      return;
   }

   // Everything else does a fizzle.
   Var *vPos = getInVpos( meta, componentList );
   meta->addStatement( new GenOp( "   fizzle( @, @ );\r\n", vPos, visibility ) );
}

ShaderFeature::Resources VisibilityFeat::getResources( const MaterialFeatureData &fd )
{
   Resources res; 

   // TODO: Fix for instancing.
   
   if ( !fd.features[ MFT_IsTranslucent ] )
      res.numTexReg = 1;

   return res;
}

//****************************************************************************
// AlphaTest
//****************************************************************************

void AlphaTest::processPix(  Vector<ShaderComponent*> &componentList,
                                 const MaterialFeatureData &fd )
{
   // If we're below SM3 and don't have a depth output
   // feature then don't waste an instruction here.
   if ( GFX->getPixelShaderVersion() < 3.0 &&
        !fd.features[ MFT_EyeSpaceDepthOut ]  &&
        !fd.features[ MFT_DepthOut ] )
   {
      output = NULL;
      return;
   }

   // If we don't have a color var then we cannot do an alpha test.
   Var *color = (Var*)LangElement::find( "col" );
   if ( !color )
   {
      output = NULL;
      return;
   }

   // Now grab the alpha test value.
   Var *alphaTestVal  = new Var;
   alphaTestVal->setType( "float" );
   alphaTestVal->setName( "alphaTestValue" );
   alphaTestVal->uniform = true;
   alphaTestVal->constSortPos = cspPotentialPrimitive;

   // Do the clip.
   UsedForOpenGL(output = new GenOp( "   if (@.a < @)\r\n   {\r\n     discard;\r\n   }\r\n", color, alphaTestVal));
   UsedForDirect3D(output = new GenOp( "   clip( @.a - @ );\r\n", color, alphaTestVal ));
}


//****************************************************************************
// GlowMask
//****************************************************************************

void GlowMask::processPix(   Vector<ShaderComponent*> &componentList,
                                 const MaterialFeatureData &fd )
{
   output = NULL;

   // Get the output color... and make it black to mask out 
   // glow passes rendered before us.
   //
   // The shader compiler will optimize out all the other
   // code above that doesn't contribute to the alpha mask.
   Var *color = (Var*)LangElement::find( "col" );
   if ( color )
      output = new GenOp( avar("   @.rgb = %s(0, 0, 0);\r\n", getVector3Name()), color );
}


//****************************************************************************
// RenderTargetZero
//****************************************************************************

void RenderTargetZero::processPix( Vector<ShaderComponent*> &componentList, const MaterialFeatureData &fd )
{
   // Do not actually assign zero, but instead a number so close to zero it may as well be zero.
   // This will prevent a divide by zero causing an FP special on float render targets
   output = new GenOp( "   @;\r\n", assignColor( new GenOp( "0.00001" ), Material::None, NULL, mOutputTargetMask ) );
}


//****************************************************************************
// HDR Output
//****************************************************************************

HDROut::HDROut()
   : mTorqueDep( GFX->getAdapterType() == OpenGL ? "shaders/common/gl/torque.glsl" : "shaders/common/torque.hlsl" )
{
   addDependency( &mTorqueDep );
}

void HDROut::processPix(  Vector<ShaderComponent*> &componentList,
                              const MaterialFeatureData &fd )
{
   // Let the helper function do the work.
   Var *color = (Var*)LangElement::find( "col" );
   if ( color )
      output = new GenOp( "   @ = hdrEncode( @ );\r\n", color, color );
}

//****************************************************************************
// FoliageFeature
//****************************************************************************

#include "T3D/fx/groundCover.h"

FoliageFeature::FoliageFeature()
: mDep( GFX->getAdapterType() == OpenGL ? "shaders/common/gl/torque.glsl" : "shaders/common/torque.hlsl" )
{
   addDependency( &mDep );
}

void FoliageFeature::processVert( Vector<ShaderComponent*> &componentList, 
                                      const MaterialFeatureData &fd )
{      
   // Get the input variables we need.

   Var *inPosition = (Var*)LangElement::find( "inPosition" );
   if ( !inPosition )
      inPosition = (Var*)LangElement::find( "position" );
      
   Var *inColor = (Var*)LangElement::find( "diffuse" );   
   
   Var *inParams = (Var*)LangElement::find( "texCoord" );   

   MultiLine *meta = new MultiLine;

   // Declare the normal and tangent variables since they do not exist
   // in this vert type, but we do need to set them up for others.

   Var *normal = (Var*)LangElement::find( "inNormal" );
   if (!normal) normal = (Var*) LangElement::find( "normal" );
   AssertFatal( normal, "FoliageFeature requires vert normal!" );   

   Var *tangent = new Var;
   tangent->setType( getVector3Name() );
   tangent->setName( "T" );
   LangElement *tangentDec = new DecOp( tangent );
   meta->addStatement( new GenOp( "   @;\n", tangentDec ) );         

   // We add a float foliageFade to the OUT structure.
   ShaderConnector *connectComp = dynamic_cast<ShaderConnector *>( componentList[C_CONNECTOR] );
   Var *fade = connectComp->getElement( RT_TEXCOORD );
   fade->setName( "foliageFade" );
   fade->setStructName( "OUT" );
   fade->setType( "float" );

   // grab the eye position
   Var *eyePos = (Var*)LangElement::find( "eyePosWorld" );
   if ( !eyePos )
   {
      eyePos = new Var( "eyePosWorld", getVector3Name() );
      eyePos->uniform = true;
      eyePos->constSortPos = cspPass;
   }

   // All actual work is offloaded to this method.
   meta->addStatement( new GenOp( "   foliageProcessVert( @, @, @, @, @, @ );\r\n", inPosition, inColor, inParams, normal, tangent, eyePos ) );   

   // Assign to foliageFade. InColor.a was set to the correct value inside foliageProcessVert.
   meta->addStatement( new GenOp( "   @ = @.a;\r\n", fade, inColor ) );

   output = meta;
}

void FoliageFeature::processPix( Vector<ShaderComponent*> &componentList, 
                                     const MaterialFeatureData &fd )
{
   // Find / create IN.foliageFade
   ShaderConnector *connectComp = dynamic_cast<ShaderConnector *>( componentList[C_CONNECTOR] );
   Var *fade = connectComp->getElement( RT_TEXCOORD );
   fade->setName( "foliageFade" );
   fade->setStructName( "IN" );
   fade->setType( "float" );
      
   // Find / create visibility
   Var *visibility = (Var*) LangElement::find( "visibility" );
   if ( !visibility )
   {
      visibility = new Var();
      visibility->setType( "float" );
      visibility->setName( "visibility" );
      visibility->uniform = true;
      visibility->constSortPos = cspPotentialPrimitive;  
   }      

   MultiLine *meta = new MultiLine;

   // Multiply foliageFade into visibility.
   meta->addStatement( new GenOp( "   @ *= @;\r\n", visibility, fade ) );

   output = meta;
}

void FoliageFeature::determineFeature( Material *material, const GFXVertexFormat *vertexFormat, U32 stageNum, const FeatureType &type, const FeatureSet &features, MaterialFeatureData *outFeatureData )
{      
   // This isn't really necessary since the outFeatureData will be filtered after
   // this call.
   if ( features.hasFeature( MFT_Foliage  ) )
      outFeatureData->features.addFeature( type );
}


ShaderFeatureConstHandles* FoliageFeature::createConstHandles( GFXShader *shader, SimObject *userObject )
{
   GroundCover *gcover = dynamic_cast< GroundCover* >( userObject );
   AssertFatal( gcover != NULL, "FoliageFeature::createConstHandles - userObject was not valid!" );

   GroundCoverShaderConstHandles *handles = new GroundCoverShaderConstHandles();
   handles->mGroundCover = gcover;

   handles->init( shader );
   
   return handles;
}


void ParticleNormalFeature::processVert(Vector<ShaderComponent*> &componentList, const MaterialFeatureData &fd)
{
   MultiLine *meta = new MultiLine;
   output = meta;

   // Calculate normal and tangent values since we want to keep particle verts
   // as light-weight as possible

   Var *normal = (Var*) LangElement::find("normal");
   if(normal == NULL)
   {
      normal = new Var;
      normal->setType( getVector3Name() );
      normal->setName( "normal" );

      // These values are not accidental. It is slightly adjusted from facing straight into the
      // screen because there is a discontinuity at (0, 1, 0) for gbuffer encoding. Do not
      // cause this value to be (0, -1, 0) or interlaced normals will be discontinuous.
      // [11/23/2009 Pat]
      meta->addStatement(new GenOp(avar("   @ = %s(0.0, -0.97, 0.14);\r\n", ShaderFeatureCommon::getVector3Name()), new DecOp(normal)));
   }

   Var *T = (Var*) LangElement::find( "T" );
   if(T == NULL)
   {
      T = new Var;
      T->setType( getVector3Name() );
      T->setName( "T" );
      meta->addStatement(new GenOp(avar("   @ = %s(0.0, 0.0, -1.0);\r\n", ShaderFeatureCommon::getVector3Name()), new DecOp(T)));
   }
}

//****************************************************************************
// ImposterVertFeature
//****************************************************************************

ImposterVertFeature::ImposterVertFeature()
   :  mDep( GFX->getAdapterType() == OpenGL ? "shaders/common/gl/torque.glsl" : "shaders/common/torque.hlsl" )
{
   addDependency( &mDep );
}

void ImposterVertFeature::processVert(   Vector<ShaderComponent*> &componentList, 
                                             const MaterialFeatureData &fd )
{      
   MultiLine *meta = new MultiLine;
   output = meta;

   // Get the input vertex variables.   
   Var *inPosition = (Var*)LangElement::find( "position" );
   Var *inMiscParams = (Var*)LangElement::find( "tcImposterParams" );   
   Var *inUpVec = (Var*)LangElement::find( "tcImposterUpVec" );   
   Var *inRightVec = (Var*)LangElement::find( "tcImposterRightVec" );

   // Get the input shader constants.
   Var *imposterLimits  = new Var;
   imposterLimits->setType( getVector4Name() );
   imposterLimits->setName( "imposterLimits" );
   imposterLimits->uniform = true;
   imposterLimits->constSortPos = cspPotentialPrimitive;

   Var *imposterUVs  = new Var;
   imposterUVs->setType( getVector4Name() );
   imposterUVs->setName( "imposterUVs" );
   imposterUVs->arraySize = 64; // See imposter.
   imposterUVs->uniform = true;
   imposterUVs->constSortPos = cspPotentialPrimitive;

   Var *eyePos = (Var*)LangElement::find( "eyePosWorld" );
   if ( !eyePos )
   {
      eyePos = new Var( "eyePosWorld", getVector3Name() );
      eyePos->uniform = true;
      eyePos->constSortPos = cspPass;
   }

   // Declare the outputs from this feature.
   Var *outInPosition = new Var;
   outInPosition->setType( getVector3Name() );
   outInPosition->setName( "inPosition" );
   meta->addStatement( new GenOp( "   @;\r\n", new DecOp( outInPosition ) ) );         

   Var *outTexCoord = new Var;
   outTexCoord->setType( getVector2Name() );
   outTexCoord->setName( "texCoord" );
   meta->addStatement( new GenOp( "   @;\r\n", new DecOp( outTexCoord ) ) );         

   Var *outWorldToTangent = new Var;
   outWorldToTangent->setType( getMatrix3x3Name() );
   outWorldToTangent->setName( "worldToTangent" );
   meta->addStatement( new GenOp( "   @;\r\n", new DecOp( outWorldToTangent ) ) );         

   // Add imposterFade to the OUT structure.
   ShaderConnector *connectComp = dynamic_cast<ShaderConnector *>( componentList[C_CONNECTOR] );
   Var *outFade = connectComp->getElement( RT_TEXCOORD );
   outFade->setName( "imposterFade" );
   outFade->setStructName( "OUT" );
   outFade->setType( "float" ); 

   // Assign OUT.imposterFade
   meta->addStatement( new GenOp( "   @ = @.y;\r\n", outFade, inMiscParams ) );

   // All actual work is done in this method.
   meta->addStatement( new GenOp( "   imposter_v( @.xyz, @.w, @.x * length(@), normalize(@), normalize(@), @.y, @.x, @.z, @.w, @, @, @, @, @ );\r\n",

                        inPosition,
                        inPosition,

                        inMiscParams,
                        inRightVec,

                        inUpVec,
                        inRightVec,

                        imposterLimits,
                        imposterLimits,
                        imposterLimits,
                        imposterLimits,

                        eyePos,
                        imposterUVs,

                        outInPosition, 
                        outTexCoord,
                        outWorldToTangent ) );

   // Copy the position to wsPosition for use in shaders 
   // down stream instead of looking for objTrans.
   Var *wsPosition = new Var;
   wsPosition->setType( getVector3Name() );
   wsPosition->setName( "wsPosition" );
   meta->addStatement( new GenOp( "   @ = @.xyz;\r\n", new DecOp( wsPosition ), outInPosition ) ); 

   // If we new viewToTangent... its the same as the
   // world to tangent for an imposter.
   Var *viewToTangent = new Var;
   viewToTangent->setType( getMatrix3x3Name() );
   viewToTangent->setName( "viewToTangent" );
   meta->addStatement( new GenOp( "   @ = @;\r\n", new DecOp( viewToTangent ), outWorldToTangent ) );       
}

void ImposterVertFeature::processPix( Vector<ShaderComponent*> &componentList,
                                          const MaterialFeatureData &fd )
{
   // Find / create IN.imposterFade
   ShaderConnector *connectComp = dynamic_cast<ShaderConnector *>( componentList[C_CONNECTOR] );
   Var *fade = connectComp->getElement( RT_TEXCOORD );
   fade->setName( "imposterFade" );
   fade->setStructName( "IN" );
   fade->setType( "float" );
      
   // Find / create visibility
   Var *visibility = (Var*) LangElement::find( "visibility" );
   if ( !visibility )
   {
      visibility = new Var();
      visibility->setType( "float" );
      visibility->setName( "visibility" );
      visibility->uniform = true;
      visibility->constSortPos = cspPotentialPrimitive;  
   }      

   MultiLine *meta = new MultiLine;

   // Multiply foliageFade into visibility.
   meta->addStatement( new GenOp( "   @ *= @;\r\n", visibility, fade ) );

   output = meta;
}

void ImposterVertFeature::determineFeature( Material *material, 
                                                const GFXVertexFormat *vertexFormat, 
                                                U32 stageNum, 
                                                const FeatureType &type, 
                                                const FeatureSet &features, 
                                                MaterialFeatureData *outFeatureData )
{      
   if ( features.hasFeature( MFT_ImposterVert ) )
      outFeatureData->features.addFeature( MFT_ImposterVert );
}

