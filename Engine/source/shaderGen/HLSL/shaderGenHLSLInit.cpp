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

#include "shaderGen/shaderGen.h"
#include "shaderGen/HLSL/shaderGenHLSL.h"
#include "shaderGen/featureMgr.h"
#include "shaderGen/HLSL/bumpHLSL.h"
#include "shaderGen/HLSL/pixSpecularHLSL.h"
#include "shaderGen/HLSL/depthHLSL.h"
#include "shaderGen/HLSL/paraboloidHLSL.h"
#include "materials/materialFeatureTypes.h"
#include "core/module.h"

static ShaderGen::ShaderGenInitDelegate sInitDelegate;

void _initShaderGenHLSL( ShaderGen *shaderGen )
{
   shaderGen->setPrinter( new ShaderGenPrinterHLSL );
   shaderGen->setComponentFactory( new ShaderGenComponentFactoryHLSL );
   shaderGen->setFileEnding( "hlsl" );

   ShaderFeatureCommon::setCurrentAdapterType(Direct3D9);

   FEATUREMGR->registerFeature( MFT_VertTransform, new VertPosition );
   FEATUREMGR->registerFeature( MFT_RTLighting, new RTLightingFeat );
   FEATUREMGR->registerFeature( MFT_IsDXTnm, new NamedFeature( "DXTnm" ) );
   FEATUREMGR->registerFeature( MFT_TexAnim, new TexAnim );
   FEATUREMGR->registerFeature( MFT_DiffuseMap, new DiffuseMapFeat );
   FEATUREMGR->registerFeature( MFT_OverlayMap, new OverlayTexFeat );
   FEATUREMGR->registerFeature( MFT_DiffuseColor, new DiffuseFeature );
   FEATUREMGR->registerFeature( MFT_DiffuseVertColor, new DiffuseVertColorFeature );
   FEATUREMGR->registerFeature( MFT_AlphaTest, new AlphaTest );
   FEATUREMGR->registerFeature( MFT_GlowMask, new GlowMask );
   FEATUREMGR->registerFeature( MFT_LightMap, new LightmapFeat );
   FEATUREMGR->registerFeature( MFT_ToneMap, new TonemapFeat );
   FEATUREMGR->registerFeature( MFT_VertLit, new VertLit );
   FEATUREMGR->registerFeature( MFT_Parallax, new ParallaxFeat );
   FEATUREMGR->registerFeature( MFT_NormalMap, new BumpFeat );
   FEATUREMGR->registerFeature( MFT_DetailNormalMap, new NamedFeature( "Detail Normal Map" ) );
   FEATUREMGR->registerFeature( MFT_DetailMap, new DetailFeat );
   FEATUREMGR->registerFeature( MFT_CubeMap, new ReflectCubeFeat );
   FEATUREMGR->registerFeature( MFT_PixSpecular, new PixelSpecular );
   FEATUREMGR->registerFeature( MFT_IsTranslucent, new NamedFeature( "Translucent" ) );
   FEATUREMGR->registerFeature( MFT_IsTranslucentZWrite, new NamedFeature( "Translucent ZWrite" ) );
   FEATUREMGR->registerFeature( MFT_Visibility, new VisibilityFeat );
   FEATUREMGR->registerFeature( MFT_Fog, new FogFeat );
   FEATUREMGR->registerFeature( MFT_SpecularMap, new SpecularMap );
   FEATUREMGR->registerFeature( MFT_GlossMap, new NamedFeature( "Gloss Map" ) );
   FEATUREMGR->registerFeature( MFT_LightbufferMRT, new NamedFeature( "Lightbuffer MRT" ) );
   FEATUREMGR->registerFeature( MFT_RenderTarget1_Zero, new RenderTargetZero( ShaderFeature::RenderTarget1 ) );

   FEATUREMGR->registerFeature( MFT_DiffuseMapAtlas, new NamedFeature( "Diffuse Map Atlas" ) );
   FEATUREMGR->registerFeature( MFT_NormalMapAtlas, new NamedFeature( "Normal Map Atlas" ) );

   FEATUREMGR->registerFeature( MFT_NormalsOut, new NormalsOutFeat );
   
   FEATUREMGR->registerFeature( MFT_DepthOut, new DepthOut );
   FEATUREMGR->registerFeature( MFT_EyeSpaceDepthOut, new EyeSpaceDepthOut() );

   FEATUREMGR->registerFeature( MFT_HDROut, new HDROut );

   FEATUREMGR->registerFeature( MFT_ParaboloidVertTransform, new ParaboloidVertTransform );
   FEATUREMGR->registerFeature( MFT_IsSinglePassParaboloid, new NamedFeature( "Single Pass Paraboloid" ) );
   FEATUREMGR->registerFeature( MFT_UseInstancing, new NamedFeature( "Hardware Instancing" ) );

   FEATUREMGR->registerFeature( MFT_Foliage, new FoliageFeature );

   FEATUREMGR->registerFeature( MFT_ParticleNormal, new ParticleNormalFeature );

   FEATUREMGR->registerFeature( MFT_InterlacedPrePass, new NamedFeature( "Interlaced Pre Pass" ) );

   FEATUREMGR->registerFeature( MFT_ForwardShading, new NamedFeature( "Forward Shaded Material" ) );

   FEATUREMGR->registerFeature( MFT_ImposterVert, new ImposterVertFeature );
}

MODULE_BEGIN( ShaderGenHLSL )

   MODULE_INIT_AFTER( ShaderGen )
   MODULE_INIT_AFTER( ShaderGenFeatureMgr )
   
   MODULE_INIT
   {
      sInitDelegate.bind(_initShaderGenHLSL);
      SHADERGEN->registerInitDelegate(Direct3D9, sInitDelegate);
      SHADERGEN->registerInitDelegate(Direct3D9_360, sInitDelegate);
   }
   
MODULE_END;
