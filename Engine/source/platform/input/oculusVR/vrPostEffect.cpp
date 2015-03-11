//-----------------------------------------------------------------------------
// Copyright (c) 2012 GarageGames, LLC
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

#include "platform/input/oculusVR/vrPostEffect.h"

#include "console/consoleTypes.h"
#include "console/engineAPI.h"
#include "gfx/gfxDevice.h"
#include "platform/input/oculusVR/oculusVRDevice.h"

extern bool gEditingMission;

ConsoleDocClass( VRPostEffect, 
   "@brief A fullscreen shader effect used with the Oculus Rift.\n\n"

   "@section PFXTextureIdentifiers\n\n"   

   "@ingroup Rendering\n"
);

IMPLEMENT_CONOBJECT(VRPostEffect);

VRPostEffect::VRPostEffect() 
   :  PostEffect()
{
   mHMDIndex = 0;
   mSensorIndex = 0;
}

VRPostEffect::~VRPostEffect()
{
}

void VRPostEffect::initPersistFields()
{
   addField( "hmdIndex", TypeS32, Offset( mHMDIndex, VRPostEffect ), 
      "Oculus VR HMD index to reference." );

   addField( "sensorIndex", TypeS32, Offset( mSensorIndex, VRPostEffect ), 
      "Oculus VR sensor index to reference." );

   Parent::initPersistFields();
}

bool VRPostEffect::onAdd()
{
   if( !Parent::onAdd() )
      return false;

   return true;
}

void VRPostEffect::onRemove()
{
   Parent::onRemove();
}

void VRPostEffect::_setupConstants( const SceneRenderState *state )
{
   // Test if setup is required before calling the parent method as the parent method
   // will set up the shader constants buffer for us.
   bool setupRequired = mShaderConsts.isNull();

   Parent::_setupConstants(state);

   const Point2I &resolution = GFX->getActiveRenderTarget()->getSize();
   F32 widthScale = 0.5f;
   F32 heightScale = 1.0f;
   F32 aspectRatio = (resolution.x * 0.5f) / resolution.y;

   // Set up the HMD dependant shader constants
   if(ManagedSingleton<OculusVRDevice>::instanceOrNull() && OCULUSVRDEV->getHMDDevice(mHMDIndex))
   {
      const OculusVRHMDDevice* hmd = OCULUSVRDEV->getHMDDevice(mHMDIndex);
   }
   else
   {
   }
}

void VRPostEffect::process(const SceneRenderState *state, GFXTexHandle &inOutTex, const RectI *inTexViewport)
{
   // Don't draw the post effect if the editor is active
   if(gEditingMission)
      return;

   Parent::process(state, inOutTex, inTexViewport);
}
