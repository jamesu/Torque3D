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
#include "lighting/advanced/hlsl/gBufferConditionerHLSL.h"
#include "shaderGen/shaderGen.h"

#include "shaderGen/featureMgr.h"
#include "gfx/gfxStringEnumTranslate.h"
#include "gfx/gfxDevice.h"
#include "materials/materialFeatureTypes.h"
#include "materials/materialFeatureData.h"
#include "shaderGen/HLSL/shaderFeatureHLSL.h"

const char* GBufferConditioner::prepassSamplerName = "prepassBuffer";

GBufferConditioner::GBufferConditioner( const GFXFormat bufferFormat, const NormalSpace nrmSpace ) : 
      Parent( bufferFormat )
{
   // Figure out how we should store the normal data. These are the defaults.
   mCanWriteNegativeValues = false;
   mNormalStorageType = CartesianXYZ;

   // Note:  We clear to a depth 1 (the w component) so
   // that the unrendered parts of the scene end up 
   // farthest to the camera.
   const NormalStorage &twoCmpNrmStorageType = ( nrmSpace == WorldSpace ? Spherical : LambertAzimuthal );
   switch(bufferFormat)
   {
      case GFXFormatR8G8B8A8:
         mNormalStorageType = twoCmpNrmStorageType;
         mBitsPerChannel = 8;
         break;

      case GFXFormatR16G16B16A16F:
         // Floating point buffers don't need to encode negative values
         mCanWriteNegativeValues = true;
         mNormalStorageType = twoCmpNrmStorageType;
         mBitsPerChannel = 16;
         break;

      // Store a 32bit depth with a sperical normal in the
      // integer 16 format.  This gives us perfect depth 
      // precision and high quality normals within a 64bit
      // buffer format.
      case GFXFormatR16G16B16A16:
         mNormalStorageType = twoCmpNrmStorageType;
         mBitsPerChannel = 16;
         break;

      case GFXFormatR32G32B32A32F:
         mCanWriteNegativeValues = true;
         mNormalStorageType = CartesianXYZ;
         mBitsPerChannel = 32;
         break;

      default:
         AssertFatal(false, "Unsupported G-Buffer format");
   }
}

GBufferConditioner::~GBufferConditioner()
{
}

void GBufferConditioner::processVert( Vector<ShaderComponent*> &componentList, 
                                          const MaterialFeatureData &fd )
{
   // If we have a normal map then that feature will
   // take care of passing gbNormal to the pixel shader.
   if ( fd.features[MFT_NormalMap] )
      return;

   MultiLine *meta = new MultiLine;
   output = meta;

   // grab incoming vert normal
   Var *inNormal = (Var*) LangElement::find( "inNormal" );
   if (!inNormal) inNormal = (Var*) LangElement::find( "normal" );
   AssertFatal( inNormal, "Something went bad with ShaderGen. The normal should be already defined." );

   // grab output for gbuffer normal
   ShaderConnector *connectComp = dynamic_cast<ShaderConnector *>( componentList[C_CONNECTOR] );
   Var *outNormal = connectComp->getElement( RT_TEXCOORD );
   outNormal->setName( "gbNormal" );
   outNormal->setStructName( "OUT" );
   outNormal->setType( ShaderFeatureCommon::getVector3Name() );

   if( !fd.features[MFT_ParticleNormal] )
   {
      // Kick out the view-space normal

      // TODO: Total hack because Conditioner is directly derived
      // from ShaderFeature and not from ShaderFeatureCommon.
      NamedFeature dummy( String::EmptyString );
      dummy.mInstancingFormat = mInstancingFormat;
      Var *worldViewOnly = dummy.getWorldView( componentList, fd.features[MFT_UseInstancing], meta );

      UsedForDirect3D(meta->addStatement(  new GenOp("   @ = mul(@, float4( normalize(@), 0.0 ) ).xyz;\r\n", outNormal, worldViewOnly, inNormal ) ) );
      UsedForOpenGL(meta->addStatement(  new GenOp("   @ = (@ * vec4( normalize(@), 0.0 ) ).xyz;\r\n", outNormal, worldViewOnly, inNormal ) ) );
   }
   else
   {
      // Assume the particle normal generator has already put this in view space
      // and normalized it
      meta->addStatement( new GenOp( "   @ = @;\r\n", outNormal, inNormal ) );
   }
}

void GBufferConditioner::processPix(  Vector<ShaderComponent*> &componentList, 
                                          const MaterialFeatureData &fd )
{     
   // sanity
   AssertFatal( fd.features[MFT_EyeSpaceDepthOut], "No depth-out feature enabled! Bad news!" );

   MultiLine *meta = new MultiLine;

   // grab connector normal
   ShaderConnector *connectComp = dynamic_cast<ShaderConnector *>( componentList[C_CONNECTOR] );
   Var *gbNormal = (Var*) LangElement::find( "gbNormal" );
   if( !gbNormal )
   {
      gbNormal = connectComp->getElement( RT_TEXCOORD );
      gbNormal->setName( "gbNormal" );
      gbNormal->setStructName( "IN" );
      gbNormal->setType( ShaderFeatureCommon::getVector3Name() );
      gbNormal->mapsToSampler = false;
      gbNormal->uniform = false;
   }

   // find depth
   ShaderFeature *depthFeat = FEATUREMGR->getByType( MFT_EyeSpaceDepthOut );
   AssertFatal( depthFeat != NULL, "No eye space depth feature found!" );

   Var *depth = (Var*) LangElement::find(depthFeat->getOutputVarName());
   AssertFatal( depth, "Something went bad with ShaderGen. The depth should be already generated by the EyeSpaceDepthOut feature." );


   Var *unconditionedOut = new Var;
   unconditionedOut->setType(ShaderFeatureCommon::getVector4Name());
   unconditionedOut->setName("normal_depth");

   LangElement *outputDecl = new DecOp( unconditionedOut );

   // If we're doing prepass blending then we need 
   // to steal away the alpha channel before the 
   // conditioner stomps on it.
   Var *alphaVal = NULL;
   if ( fd.features[ MFT_IsTranslucentZWrite ] )
   {
      alphaVal = new Var( "outAlpha", "float" );
      meta->addStatement( new GenOp( "   @ = OUT.col.a; // MFT_IsTranslucentZWrite\r\n", new DecOp( alphaVal ) ) );
   }

   // If using interlaced normals, invert the normal
   if(fd.features[MFT_InterlacedPrePass])
   {
      // NOTE: Its safe to not call ShaderFeatureHLSL::addOutVpos() in the vertex
      // shader as for SM 3.0 nothing is needed there.
      Var *Vpos = ShaderFeatureCommon::getInVpos( meta, componentList );

      Var *iGBNormal = new Var( "interlacedGBNormal", ShaderFeatureCommon::getVector3Name() );
      meta->addStatement(new GenOp(avar("   @ = (frac(@.y * 0.5) < 0.1 ? reflect(@, %s(0.0, -1.0, 0.0)) : @);\r\n", ShaderFeatureCommon::getVector3Name()), new DecOp(iGBNormal), Vpos, gbNormal, gbNormal));
      gbNormal = iGBNormal;
   }

   // NOTE: We renormalize the normal here as they
   // will not stay normalized during interpolation.
   meta->addStatement( new GenOp("   @ = @;", outputDecl, new GenOp( avar("%s(normalize(@), @)", ShaderFeatureCommon::getVector4Name()), gbNormal, depth ) ) );
   meta->addStatement( assignOutput( unconditionedOut ) );

   // If we have an alpha var then we're doing prepass lerp blending.
   if ( alphaVal )
   {
      Var *outColor = (Var*)LangElement::find( getOutputTargetVarName( DefaultTarget ) );
      meta->addStatement( new GenOp( avar("   @.ba = %s( 0, @ ); // MFT_IsTranslucentZWrite\r\n", ShaderFeatureCommon::getVector2Name()), outColor, alphaVal ) );
   }

   output = meta;
}

ShaderFeature::Resources GBufferConditioner::getResources( const MaterialFeatureData &fd )
{
   Resources res;

   // Passing from VS->PS:
   // - world space normal (gbNormal)
   res.numTexReg = 1;

   return res; 
}

Var* GBufferConditioner::printMethodHeader( MethodType methodType, const String &methodName, Stream &stream, MultiLine *meta )
{
   const bool isCondition = ( methodType == ConditionerFeature::ConditionMethod );

   Var *retVal = NULL;

   // The uncondition method inputs are changed
   if( isCondition )
      retVal = Parent::printMethodHeader( methodType, methodName, stream, meta );
   else
   {
      Var *methodVar = new Var;
      methodVar->setName(methodName);
      methodVar->setType(ShaderFeatureCommon::getVector4Name());
      DecOp *methodDecl = new DecOp(methodVar);

      Var *prepassSampler = new Var;
      prepassSampler->setName("prepassSamplerVar");
      prepassSampler->setType("sampler2D");
      DecOp *prepassSamplerDecl = new DecOp(prepassSampler);

      Var *screenUV = new Var;
      screenUV->setName("screenUVVar");
      screenUV->setType(ShaderFeatureCommon::getVector2Name());
      DecOp *screenUVDecl = new DecOp(screenUV);

      Var *bufferSample = new Var;
      bufferSample->setName("bufferSample");
      bufferSample->setType(ShaderFeatureCommon::getVector4Name());
      DecOp *bufferSampleDecl = new DecOp(bufferSample); 

      meta->addStatement( new GenOp( "@(@, @)\r\n", methodDecl, prepassSamplerDecl, screenUVDecl ) );

      meta->addStatement( new GenOp( "{\r\n" ) );

      meta->addStatement( new GenOp( "   // Sampler g-buffer\r\n" ) );

#ifdef TORQUE_OS_XENON
      meta->addStatement( new GenOp( "   @;\r\n", bufferSampleDecl ) );
      meta->addStatement( new GenOp( "   asm { tfetch2D @, @, @, MagFilter = point, MinFilter = point, MipFilter = point };\r\n", bufferSample, screenUV, prepassSampler ) );
#else
      // The gbuffer has no mipmaps, so use tex2dlod when 
      // possible so that the shader compiler can optimize.
      if (GFX->getAdapterType() == OpenGL)
      {
        meta->addStatement( new GenOp( "   #if TORQUE_SM >= 30\r\n" ) );
        meta->addStatement( new GenOp( "      @ = texture2DLod(@, @, 0);\r\n", bufferSampleDecl, prepassSampler, screenUV ) );
        meta->addStatement( new GenOp( "   #else\r\n" ) );
        meta->addStatement( new GenOp( "      @ = texture2D(@, @);\r\n", bufferSampleDecl, prepassSampler, screenUV ) );
        meta->addStatement( new GenOp( "   #endif\r\n\r\n" ) );
      }
      else
      {
        meta->addStatement( new GenOp( "   #if TORQUE_SM >= 30\r\n" ) );
        meta->addStatement( new GenOp( "      @ = tex2Dlod(@, float4(@,0,0));\r\n", bufferSampleDecl, prepassSampler, screenUV ) );
        meta->addStatement( new GenOp( "   #else\r\n" ) );
        meta->addStatement( new GenOp( "      @ = tex2D(@, @);\r\n", bufferSampleDecl, prepassSampler, screenUV ) );
        meta->addStatement( new GenOp( "   #endif\r\n\r\n" ) );
      }
#endif

      // We don't use this way of passing var's around, so this should cause a crash
      // if something uses this improperly
      retVal = bufferSample;
   }

   return retVal;
}

GenOp* GBufferConditioner::_posnegEncode( GenOp *val )
{
   if(mNormalStorageType == LambertAzimuthal)
      return mCanWriteNegativeValues ? val : new GenOp(avar("(%f * (@ + %f))", 1.0f/(M_SQRT2_F * 2.0f), M_SQRT2_F), val);
   else
      return mCanWriteNegativeValues ? val : new GenOp("(0.5 * (@ + 1.0))", val);
}

GenOp* GBufferConditioner::_posnegDecode( GenOp *val )
{
   if(mNormalStorageType == LambertAzimuthal)
      return mCanWriteNegativeValues ? val : new GenOp(avar("(@ * %f - %f)", M_SQRT2_F * 2.0f, M_SQRT2_F), val);
   else
      return mCanWriteNegativeValues ? val : new GenOp("(@ * 2.0 - 1.0)", val);
}

Var* GBufferConditioner::_conditionOutput( Var *unconditionedOutput, MultiLine *meta )
{
   Var *retVar = new Var;
   retVar->setType(ShaderFeatureCommon::getVector4Name());
   retVar->setName("_gbConditionedOutput");
   LangElement *outputDecl = new DecOp( retVar );

   switch(mNormalStorageType)
   {
      case CartesianXYZ:
         meta->addStatement( new GenOp( avar("   // g-buffer conditioner: %s(normal.xyz, depth)\r\n", ShaderFeatureCommon::getVector4Name()) ) );
         meta->addStatement( new GenOp( avar("   @ = %s(@, @.a);\r\n", ShaderFeatureCommon::getVector4Name()) , outputDecl, 
            _posnegEncode(new GenOp("@.xyz", unconditionedOutput)), unconditionedOutput ) );
         break;

      case CartesianXY:
         meta->addStatement( new GenOp( avar("   // g-buffer conditioner: %s(normal.xy, depth Hi + z-sign, depth Lo)\r\n", ShaderFeatureCommon::getVector4Name()) ) );
         meta->addStatement( new GenOp( avar("   @ = $s(@, @.a);", ShaderFeatureCommon::getVector4Name()), outputDecl, 
            _posnegEncode(new GenOp(avar("%s(@.xy, sign(@.z))", ShaderFeatureCommon::getVector3Name()), unconditionedOutput, unconditionedOutput)), unconditionedOutput ) );
         break;

      case Spherical:
        {
           if (GFX->getAdapterType() == OpenGL)
           {
             meta->addStatement( new GenOp( "   // g-buffer conditioner: vec4(normal.theta, normal.phi, depth Hi, depth Lo)\r\n" ) );
             meta->addStatement( new GenOp( "   @ = vec4(@, 0.0, @.a);\r\n", outputDecl, 
                _posnegEncode(new GenOp("vec2(atan(@.y, @.x) / 3.14159265358979323846f, @.z)", unconditionedOutput, unconditionedOutput, unconditionedOutput ) ), 
                unconditionedOutput ) );

             // HACK: This fixes the noise present when using a floating point
             // gbuffer on Geforce cards and the "flat areas unlit" issues.
             //
             // We need work around atan2() above to fix this issue correctly
             // without the extra overhead of this test.
             //
             meta->addStatement( new GenOp( "   if ( abs( dot( @.xyz, vec3( 0.0, 0.0, 1.0 ) ) ) > 0.999f ) @ = vec4( 0, 1 * sign( @.z ), 0, @.a );\r\n", 
                unconditionedOutput, retVar, unconditionedOutput, unconditionedOutput ) );
           }
           else
           {
             meta->addStatement( new GenOp( "   // g-buffer conditioner: float4(normal.theta, normal.phi, depth Hi, depth Lo)\r\n" ) );
             meta->addStatement( new GenOp( "   @ = float4(@, 0.0, @.a);\r\n", outputDecl, 
               _posnegEncode(new GenOp("float2(atan2(@.y, @.x) / 3.14159265358979323846f, @.z)", unconditionedOutput, unconditionedOutput, unconditionedOutput ) ), 
               unconditionedOutput ) );

             // HACK: This fixes the noise present when using a floating point
             // gbuffer on Geforce cards and the "flat areas unlit" issues.
             //
             // We need work around atan2() above to fix this issue correctly
             // without the extra overhead of this test.
             //
             meta->addStatement( new GenOp( "   if ( abs( dot( @.xyz, float3( 0.0, 0.0, 1.0 ) ) ) > 0.999f ) @ = float4( 0, 1 * sign( @.z ), 0, @.a );\r\n", 
               unconditionedOutput, retVar, unconditionedOutput, unconditionedOutput ) );
           }
           break;
        }

      case LambertAzimuthal:
        {
          //http://en.wikipedia.org/wiki/Lambert_azimuthal_equal-area_projection
          //

          if (GFX->getAdapterType() == OpenGL)
          {
            // Note we're casting to half to use partial precision
            // sqrt which is much faster on older Geforces while
            // still being acceptable for normals.
            //
            meta->addStatement( new GenOp( "   // g-buffer conditioner: vec4(normal.X, normal.Y, depth Hi, depth Lo)\r\n" ) );
            meta->addStatement( new GenOp( "   @ = vec4(@, 0.0, @.a);\r\n", outputDecl, 
              _posnegEncode(new GenOp("sqrt(2.0/(1.0 - @.y)) * vec2(@.xz)", unconditionedOutput, unconditionedOutput)), 
              unconditionedOutput ) );
          }
          else
          {
            // Note we're casting to half to use partial precision
            // sqrt which is much faster on older Geforces while
            // still being acceptable for normals.
            //
            meta->addStatement( new GenOp( "   // g-buffer conditioner: float4(normal.X, normal.Y, depth Hi, depth Lo)\r\n" ) );
            meta->addStatement( new GenOp( "   @ = float4(@, 0.0, @.a);\r\n", outputDecl, 
               _posnegEncode(new GenOp("sqrt(half(2.0/(1.0 - @.y))) * half2(@.xz)", unconditionedOutput, unconditionedOutput)), 
               unconditionedOutput ) );
          }
        }
        break;
   }

   // Encode depth into two channels
   if(mNormalStorageType != CartesianXYZ)
   {
      const U64 maxValPerChannel = (U64)1 << mBitsPerChannel;
      const char* fracFunctionName = GFX->getAdapterType() == OpenGL ? "fract" : "frac";
      meta->addStatement( new GenOp( "   \r\n   // Encode depth into hi/lo\r\n" ) );
      meta->addStatement( new GenOp( avar( "   %s _tempDepth = %s(@.a * %s(1.0, %llu.0));\r\n", ShaderFeatureCommon::getVector2Name(), fracFunctionName, ShaderFeatureCommon::getVector2Name(), maxValPerChannel - 1 ), 
         unconditionedOutput ) );
      meta->addStatement( new GenOp( avar( "   @.zw = _tempDepth.xy - %s(_tempDepth.y, _tempDepth.y) * %s(1.0/%llu.0, 0.0);\r\n\r\n", ShaderFeatureCommon::getVector2Name(), ShaderFeatureCommon::getVector2Name(), maxValPerChannel - 1 ), 
         retVar ) );
   }

   AssertFatal( retVar != NULL, avar( "Cannot condition output to buffer format: %s", GFXStringTextureFormat[getBufferFormat()] ) );
   return retVar; 
}

Var* GBufferConditioner::_unconditionInput( Var *conditionedInput, MultiLine *meta )
{
   Var *retVar = new Var;
   retVar->setType(ShaderFeatureCommon::getVector4Name());
   retVar->setName("_gbUnconditionedInput");
   LangElement *outputDecl = new DecOp( retVar );

   switch(mNormalStorageType)
   {
      case CartesianXYZ:
         meta->addStatement( new GenOp( avar("   // g-buffer unconditioner: %s(normal.xyz, depth)\r\n", ShaderFeatureCommon::getVector4Name()) ) );
         meta->addStatement( new GenOp( avar("   @ = %s(@, @.a);\r\n", ShaderFeatureCommon::getVector2Name()), outputDecl, 
            _posnegDecode(new GenOp("@.xyz", conditionedInput)), conditionedInput ) );
         break;

      case CartesianXY:
         meta->addStatement( new GenOp( avar("   // g-buffer unconditioner: %s(normal.xy, depth Hi + z-sign, depth Lo)\r\n", ShaderFeatureCommon::getVector4Name()) ) );
         meta->addStatement( new GenOp( avar("   @ = %s(@, @.a);\r\n", ShaderFeatureCommon::getVector4Name()), outputDecl, 
            _posnegDecode(new GenOp("@.xyz", conditionedInput)), conditionedInput ) );
         meta->addStatement( new GenOp( "   @.z *= sqrt(1.0 - dot(@.xy, @.xy));\r\n", retVar, retVar, retVar ) );
         break;

      case Spherical:
         meta->addStatement( new GenOp( avar("   // g-buffer unconditioner: %s(normal.theta, normal.phi, depth Hi, depth Lo)\r\n", ShaderFeatureCommon::getVector4Name()) ) );
         meta->addStatement( new GenOp( avar("   %s spGPUAngles = @;\r\n", ShaderFeatureCommon::getVector2Name()), _posnegDecode(new GenOp("@.xy", conditionedInput)) ) );
         meta->addStatement( new GenOp( avar("   %s sincosTheta;\r\n", ShaderFeatureCommon::getVector2Name()) ) );
         meta->addStatement( new GenOp( "   sincos(spGPUAngles.x * 3.14159265358979323846f, sincosTheta.x, sincosTheta.y);\r\n" ) );
         meta->addStatement( new GenOp( avar("   %s sincosPhi = float2(sqrt(1.0 - spGPUAngles.y * spGPUAngles.y), spGPUAngles.y);\r\n", ShaderFeatureCommon::getVector2Name()) ) );
         meta->addStatement( new GenOp( avar("   @ = %s(sincosTheta.y * sincosPhi.x, sincosTheta.x * sincosPhi.x, sincosPhi.y, @.a);\r\n", ShaderFeatureCommon::getVector4Name()) , outputDecl, conditionedInput ) );
         break;

      case LambertAzimuthal:
         // Note we're casting to half to use partial precision
         // sqrt which is much faster on older Geforces while
         // still being acceptable for normals.
         //      
         meta->addStatement( new GenOp( avar("   // g-buffer unconditioner: %s(normal.X, normal.Y, depth Hi, depth Lo)\r\n", ShaderFeatureCommon::getVector4Name()) ) );
         GenOp *op = _posnegDecode(new GenOp("@.xy", conditionedInput));
         meta->addStatement( new GenOp( avar("   %s _inpXY = @;\r\n", ShaderFeatureCommon::getVector2Name()),  op) );
         meta->addStatement( new GenOp( "   float _xySQ = dot(_inpXY, _inpXY);\r\n" ) );
         meta->addStatement( new GenOp( avar("   @ = %s( sqrt(%s(1.0 - (_xySQ / 4.0))) * _inpXY, -1.0 + (_xySQ / 2.0), @.a).xzyw;\r\n", ShaderFeatureCommon::getHalfVector4Name(), ShaderFeatureCommon::getHalfFloatName()), outputDecl, conditionedInput ) );
         break;
   }

   // Recover depth from encoding
   if(mNormalStorageType != CartesianXYZ)
   {
      const U64 maxValPerChannel = (U64)1 << mBitsPerChannel;
      meta->addStatement( new GenOp( "   \r\n   // Decode depth\r\n" ) );
      meta->addStatement( new GenOp( avar( "   @.w = dot( @.zw, %s(1.0, 1.0/%llu.0));\r\n", ShaderFeatureCommon::getVector2Name(), maxValPerChannel - 1 ), 
         retVar, conditionedInput ) );
   }


   AssertFatal( retVar != NULL, avar( "Cannot uncondition input from buffer format: %s", GFXStringTextureFormat[getBufferFormat()] ) );
   return retVar; 
}

