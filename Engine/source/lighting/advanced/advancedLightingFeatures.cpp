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
#include "lighting/advanced/advancedLightingFeatures.h"
#include "shaderGen/shaderGen.h"

#include "shaderGen/featureMgr.h"
#include "gfx/gfxStringEnumTranslate.h"
#include "materials/materialParameters.h"
#include "materials/materialFeatureTypes.h"
#include "materials/matTextureTarget.h"
#include "materials/materialFeatureData.h"
#include "materials/processedMaterial.h"
#include "gfx/gfxDevice.h"
#include "core/util/safeDelete.h"

#include "lighting/advanced/hlsl/gBufferConditionerHLSL.h"
#include "lighting/advanced/advancedLightingFeatures.h"
#include "lighting/advanced/advancedLightBinManager.h"

#include "renderInstance/renderPrePassMgr.h"


bool AdvancedLightingFeatures::smFeaturesRegistered = false;

void AdvancedLightingFeatures::registerFeatures( const GFXFormat &prepassTargetFormat, const GFXFormat &lightInfoTargetFormat )
{
   AssertFatal( !smFeaturesRegistered, "AdvancedLightingFeatures::registerFeatures() - Features already registered. Bad!" );

   // If we ever need this...
   TORQUE_UNUSED(lightInfoTargetFormat);

   ConditionerFeature *cond = NULL;

   cond = new GBufferConditioner( prepassTargetFormat, GBufferConditioner::ViewSpace );
   FEATUREMGR->registerFeature(MFT_PrePassConditioner, cond);
   FEATUREMGR->registerFeature(MFT_RTLighting, new DeferredRTLightingFeat());
   FEATUREMGR->registerFeature(MFT_NormalMap, new DeferredBumpFeat());
   FEATUREMGR->registerFeature(MFT_PixSpecular, new DeferredPixelSpecular());
   FEATUREMGR->registerFeature(MFT_MinnaertShading, new DeferredMinnaert());
   FEATUREMGR->registerFeature(MFT_SubSurface, new DeferredSubSurface());

   NamedTexTarget *target = NamedTexTarget::find( "prepass" );
   if ( target )
      target->setConditioner( cond );

   smFeaturesRegistered = true;
}

void AdvancedLightingFeatures::unregisterFeatures()
{
   NamedTexTarget *target = NamedTexTarget::find( "prepass" );
   if ( target )
      target->setConditioner( NULL );

   FEATUREMGR->unregisterFeature(MFT_PrePassConditioner);
   FEATUREMGR->unregisterFeature(MFT_RTLighting);
   FEATUREMGR->unregisterFeature(MFT_NormalMap);
   FEATUREMGR->unregisterFeature(MFT_PixSpecular);
   FEATUREMGR->unregisterFeature(MFT_MinnaertShading);
   FEATUREMGR->unregisterFeature(MFT_SubSurface);

   smFeaturesRegistered = false;
}
