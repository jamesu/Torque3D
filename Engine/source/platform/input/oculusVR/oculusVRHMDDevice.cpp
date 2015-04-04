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

#include "platform/input/oculusVR/oculusVRHMDDevice.h"
#include "platform/input/oculusVR/oculusVRDevice.h"
#include "gfx/D3D9/gfxD3D9Device.h"
#include "postFx/postEffectCommon.h"
#include "gui/core/guiCanvas.h"
#include "platform/input/oculusVR/oculusVRUtil.h"

// Use D3D9 for win32
#ifdef TORQUE_OS_WIN
#define OVR_D3D_VERSION 9
#include "..\src\OVR_CAPI_D3D.h"
#endif

//#define OCULUS_DEBUG_FRAME

GFXTextureObject *gLastStereoTexture = NULL;

OculusVRHMDDevice::OculusVRHMDDevice() :
mWindowSize(1280,800)
{
   mIsValid = false;
   mIsSimulation = false;
   mDevice = NULL;
   mSupportedDistortionCaps = 0;
   mCurrentDistortionCaps = 0;
   mCurrentCaps = 0;
   mSupportedCaps = 0;
   mVsync = true;
   mTimewarp = true;
   mRenderConfigurationDirty = true;
   mCurrentPixelDensity = OculusVRDevice::smDesiredPixelDensity;
   mDesiredRenderingMode = GFXDevice::RS_StereoSideBySide;
   mRTFormat = GFXFormatR8G8B8A8;
   mDrawCanvas = NULL;
   mFrameReady = false;
   mConnection = NULL;
}

OculusVRHMDDevice::~OculusVRHMDDevice()
{
   cleanUp();
}

void OculusVRHMDDevice::cleanUp()
{
   onDeviceDestroy();

   if(mDevice)
   {
      ovrHmd_Destroy(mDevice);
      mDevice = NULL;
   }

   mIsValid = false;
}

void OculusVRHMDDevice::set(ovrHmd hmd)
{
   mIsValid = false;
   mIsSimulation = false;
   mRenderConfigurationDirty = true;

   mDevice = hmd;

   mSupportedCaps = hmd->HmdCaps;
   mCurrentCaps = mSupportedCaps & (ovrHmdCap_DynamicPrediction | ovrHmdCap_LowPersistence | (!mVsync ? ovrHmdCap_NoVSync : 0));

   mSupportedDistortionCaps = hmd->DistortionCaps;
   mCurrentDistortionCaps	= mSupportedDistortionCaps & (ovrDistortionCap_Chromatic | /*ovrDistortionCap_TimeWarp | */ovrDistortionCap_Vignette | ovrDistortionCap_Overdrive);
	
   mTimewarp = mSupportedDistortionCaps & ovrDistortionCap_TimeWarp;

   // DeviceInfo
   mProductName = hmd->ProductName;
   mManufacturer = hmd->Manufacturer;
   mVersion = hmd->FirmwareMajor;

   mDisplayDeviceName = hmd->DisplayDeviceName;
   mDisplayId = hmd->DisplayId;

   mDesktopPosition.x = hmd->WindowsPos.x;
   mDesktopPosition.y = hmd->WindowsPos.y;

   mResolution.x = hmd->Resolution.w;
   mResolution.y = hmd->Resolution.h;

   mProfileInterpupillaryDistance = ovrHmd_GetFloat(hmd, OVR_KEY_IPD, OVR_DEFAULT_IPD);
   mLensSeparation = ovrHmd_GetFloat(hmd, "LensSeparation", 0);
	ovrHmd_GetFloatArray(hmd, "ScreenSize", &mScreenSize.x, 2);

   dMemcpy(mCurrentFovPorts, mDevice->DefaultEyeFov, sizeof(mDevice->DefaultEyeFov));

   for (U32 i=0; i<2; i++)
   {
      mCurrentFovPorts[i].UpTan = mDevice->DefaultEyeFov[i].UpTan;
      mCurrentFovPorts[i].DownTan = mDevice->DefaultEyeFov[i].DownTan;
      mCurrentFovPorts[i].LeftTan = mDevice->DefaultEyeFov[i].LeftTan;
      mCurrentFovPorts[i].RightTan = mDevice->DefaultEyeFov[i].RightTan;
   }

   if (mDevice->HmdCaps & ovrHmdCap_ExtendDesktop)
   {
      mWindowSize = Point2I(mDevice->Resolution.w, mDevice->Resolution.h);
   }
   else
   {
      mWindowSize = Point2I(1100, 618);
   }

   mIsValid = true;

   updateCaps();
}

void OculusVRHMDDevice::createSimulation(SimulationTypes simulationType)
{
   if(simulationType == ST_RIFT_PREVIEW)
   {
      createSimulatedPreviewRift();
   }
}

void OculusVRHMDDevice::createSimulatedPreviewRift()
{
   mIsValid = true;
   mIsSimulation = true;

   mProductName = "Oculus Rift DK1-SLA1";
   mManufacturer = "Oculus VR";
   mVersion = 0;

   mDisplayDeviceName = "";

   mResolution.x = 1280;
   mResolution.y = 800;

   mScreenSize.x = 0.14975999f;
   mScreenSize.y = 0.093599997f;

   mLensSeparation = 0.064000003f;
   mProfileInterpupillaryDistance = 0.064000003f;
   mInterpupillaryDistance = mProfileInterpupillaryDistance;
}

void OculusVRHMDDevice::setIPD(F32 ipd)
{
   mInterpupillaryDistance = ipd;
}

void OculusVRHMDDevice::setOptimalDisplaySize(GuiCanvas *canvas)
{
   if (!mDevice)
      return;

   PlatformWindow *window = canvas->getPlatformWindow();
   GFXTarget *target = window->getGFXTarget();

   if (target && target->getSize() != mWindowSize)
   {
      GFXVideoMode newMode;
      newMode.antialiasLevel = 0;
      newMode.bitDepth = 32;
      newMode.fullScreen = false;
      newMode.refreshRate = 75;
      newMode.resolution = mWindowSize;
      newMode.wideScreen = false;
      window->setVideoMode(newMode);
      //AssertFatal(window->getClientExtent().x == mWindowSize[0] && window->getClientExtent().y == mWindowSize[1], "Window didn't resize to correct dimensions");
   }

   // Need to move window over to the rift side of the desktop
   if (mDevice->HmdCaps & ovrHmdCap_ExtendDesktop)
   {
#ifndef OCULUS_WINDOW_DEBUG
      window->setPosition(getDesktopPosition());
#endif
   }
}

bool OculusVRHMDDevice::isDisplayingWarning()
{
   if (!mIsValid || !mDevice)
      return false;

   ovrHSWDisplayState displayState;
   ovrHmd_GetHSWDisplayState(mDevice, &displayState);

   return displayState.Displayed;
}

void OculusVRHMDDevice::dismissWarning()
{
   if (!mIsValid || !mDevice)
      return;
   ovrHmd_DismissHSWDisplay(mDevice);
}

bool OculusVRHMDDevice::setupTargets()
{
   ovrFovPort eyeFov[2] = {mDevice->DefaultEyeFov[0], mDevice->DefaultEyeFov[1]};

   mRecomendedEyeTargetSize[0] = ovrHmd_GetFovTextureSize(mDevice, ovrEye_Left,  eyeFov[0], mCurrentPixelDensity);
   mRecomendedEyeTargetSize[1] = ovrHmd_GetFovTextureSize(mDevice, ovrEye_Right, eyeFov[1], mCurrentPixelDensity);

   // Calculate render target size
   if (mDesiredRenderingMode == GFXDevice::RS_StereoSideBySide)
   {
      // Setup a single texture, side-by-side viewports
      Point2I rtSize(
         mRecomendedEyeTargetSize[0].w + mRecomendedEyeTargetSize[1].w,
         mRecomendedEyeTargetSize[0].h > mRecomendedEyeTargetSize[1].h ? mRecomendedEyeTargetSize[0].h : mRecomendedEyeTargetSize[1].h
         );

      GFXFormat targetFormat = GFX->getActiveRenderTarget()->getFormat();
      mRTFormat = targetFormat;

      rtSize = generateRenderTarget(mStereoRT, mStereoTexture, mStereoDepthTexture, rtSize);
      
      // Left
      mEyeRenderSize[0] = rtSize;
      mEyeRT[0] = mStereoRT;
      mEyeTexture[0] = mStereoTexture;
      mEyeViewport[0] = RectI(Point2I(0,0), Point2I((mRecomendedEyeTargetSize[0].w+1)/2, mRecomendedEyeTargetSize[0].h));

      // Right
      mEyeRenderSize[1] = rtSize;
      mEyeRT[1] = mStereoRT;
      mEyeTexture[1] = mStereoTexture;
      mEyeViewport[1] = RectI(Point2I((mRecomendedEyeTargetSize[0].w+1)/2,0), Point2I((mRecomendedEyeTargetSize[1].w+1)/2, mRecomendedEyeTargetSize[1].h));

      gLastStereoTexture = mEyeTexture[0];
   }
   else if (mDesiredRenderingMode == GFXDevice::RS_StereoRenderTargets)
   {
      // Setup two targets
      Point2I rtSize;

      GFXFormat targetFormat = GFX->getActiveRenderTarget()->getFormat();
      mRTFormat = targetFormat;

      // Left
      rtSize = generateRenderTarget(mEyeRT[0], mEyeTexture[0], mStereoDepthTexture, Point2I(mRecomendedEyeTargetSize[0].w, mRecomendedEyeTargetSize[0].h));
      mEyeViewport[0] = RectI(Point2I(0,0), Point2I((rtSize.x+1)/2, rtSize.y));

      // Right
      rtSize = generateRenderTarget(mEyeRT[1], mEyeTexture[1], mStereoDepthTexture, Point2I(mRecomendedEyeTargetSize[1].w, mRecomendedEyeTargetSize[1].h));
      mEyeViewport[1] = RectI(Point2I(0,0), Point2I((rtSize.x+1)/2, rtSize.y));

      mStereoRT = NULL;
      mStereoTexture = NULL;

      gLastStereoTexture = mEyeTexture[0];

   }
   else
   {
      // No rendering, abort!
      return false;
   }

   return true;
}

void OculusVRHMDDevice::updateRenderInfo()
{
   // Check console values first
   if (mCurrentPixelDensity != OculusVRDevice::smDesiredPixelDensity)
   {
      mRenderConfigurationDirty = true;
      mCurrentPixelDensity = OculusVRDevice::smDesiredPixelDensity;
   }

   if (!mIsValid || !mDevice || !mRenderConfigurationDirty)
      return;

   if (!mDrawCanvas)
      return;
   
   PlatformWindow *window = mDrawCanvas->getPlatformWindow();
   ovrFovPort eyeFov[2] = {mDevice->DefaultEyeFov[0], mDevice->DefaultEyeFov[1]};

   // Update window size if it's incorrect
   Point2I backbufferSize = mDrawCanvas->getBounds().extent;

   // Reset
   ovrHmd_ConfigureRendering(mDevice, NULL, 0, NULL, NULL);

   // Generate render target textures
   GFXD3D9Device *d3d9GFX = dynamic_cast<GFXD3D9Device*>(GFX);
   if (d3d9GFX)
   {
      ovrD3D9Config cfg;
      cfg.D3D9.Header.API = ovrRenderAPI_D3D9;
      cfg.D3D9.Header.Multisample = 0;
      cfg.D3D9.Header.BackBufferSize = OVR::Sizei(backbufferSize.x, backbufferSize.y);
      cfg.D3D9.pDevice = d3d9GFX->getDevice();
      cfg.D3D9.pDevice->GetSwapChain(0, &cfg.D3D9.pSwapChain);

      // Finally setup!
      if (!setupTargets())
      {
         onDeviceDestroy();
         return;
      }

      ovrHmd_AttachToWindow(mDevice, window->getPlatformDrawable(), NULL, NULL);

      if (!ovrHmd_ConfigureRendering( mDevice, &cfg.Config, mCurrentDistortionCaps, eyeFov, mEyeRenderDesc ))
      {
         Con::errorf("Couldn't configure oculus rendering!");
         return;
      }
   }

   mRenderConfigurationDirty = false;
}

Point2I OculusVRHMDDevice::generateRenderTarget(GFXTextureTargetRef &target, GFXTexHandle &texture, GFXTexHandle &depth, Point2I desiredSize)
{
    // Texture size that we already have might be big enough.
    Point2I newRTSize;
    bool newRT = false;
    
    if (!target.getPointer())
    {
       target = GFX->allocRenderToTextureTarget();
       newRTSize = desiredSize;
       newRT = true;
    }
    else
    {
       Point2I currentSize = target->getSize();
       newRTSize = currentSize;
    }

    // %50 linear growth each time is a nice balance between being too greedy
    // for a 2D surface and too slow to prevent fragmentation.
    while ( newRTSize.x < desiredSize.x )
    {
        newRTSize.x += newRTSize.x/2;
    }
    while ( newRTSize.y < desiredSize.y )
    {
        newRTSize.y += newRTSize.y/2;
    }

    // Put some sane limits on it. 4k x 4k is fine for most modern video cards.
    // Nobody should be messing around with surfaces smaller than 4k pixels these days.
    newRTSize.setMin(Point2I(4096, 4096));
    newRTSize.setMax(Point2I(64, 64));

    // Stereo RT needs to be the same size as the recommended RT
    if ( newRT || texture.getWidthHeight() != newRTSize )
    {
       texture.set( newRTSize.x, newRTSize.y, mRTFormat, &VRTextureProfile,  avar( "%s() - (line %d)", __FUNCTION__, __LINE__ ) );
       target->attachTexture( GFXTextureTarget::Color0, texture );
       Con::printf("generateRenderTarget generated %x", texture.getPointer());
    }

    if ( depth.getWidthHeight() != newRTSize )
    {
       depth.set( newRTSize.x, newRTSize.y, GFXFormatD24S8, &VRDepthProfile, avar( "%s() - (line %d)", __FUNCTION__, __LINE__ ) );
       target->attachTexture( GFXTextureTarget::DepthStencil, depth );
       Con::printf("generateRenderTarget generated depth %x", depth.getPointer());
    }

    return newRTSize;
}

void OculusVRHMDDevice::clearRenderTargets()
{
   mStereoRT = NULL;
   mEyeRT[0] = NULL;
   mEyeRT[1] = NULL;
}

void OculusVRHMDDevice::updateCaps()
{
   if (!mIsValid || !mDevice)
      return;

   mCurrentDistortionCaps = 0;
   mCurrentCaps = 0;
   
   // Distortion
   if (mTimewarp) mCurrentDistortionCaps |= ovrDistortionCap_TimeWarp;

   // Device
   if (!mVsync) mCurrentCaps |= ovrHmdCap_NoVSync;
   
   ovrHmd_SetEnabledCaps(mDevice, mCurrentCaps);
}

static bool sInFrame = false; // protects against recursive onStartFrame calls

void OculusVRHMDDevice::onStartFrame()
{
   if (!mIsValid || !mDevice || !mDrawCanvas || sInFrame || mFrameReady)
      return;

   sInFrame = true;
   
#ifndef OCULUS_DEBUG_FRAME
   ovrHmd_BeginFrame(mDevice, 0);
#endif

   ovrVector3f hmdToEyeViewOffset[2] = { mEyeRenderDesc[0].HmdToEyeViewOffset, mEyeRenderDesc[1].HmdToEyeViewOffset };
   ovrHmd_GetEyePoses(mDevice, 0, hmdToEyeViewOffset, mCurrentEyePoses, &mLastTrackingState);

   sInFrame = false;
   mFrameReady = true;
}

void OculusVRHMDDevice::onEndFrame()
{
   if (!mIsValid || !mDevice || !mDrawCanvas || sInFrame || !mFrameReady)
      return;

   Point2I eyeSize;
   GFXTarget *windowTarget = mDrawCanvas->getPlatformWindow()->getGFXTarget();

#ifndef OCULUS_DEBUG_FRAME
   GFXD3D9Device *d3d9GFX = dynamic_cast<GFXD3D9Device*>(GFX);
   if (d3d9GFX && mEyeRT[0].getPointer())
   {
      // Left
      ovrD3D9Texture eyeTextures[2];
      eyeSize = mEyeTexture[0].getWidthHeight();
      eyeTextures[0].D3D9.Header.API = ovrRenderAPI_D3D9;
      eyeTextures[0].D3D9.Header.RenderViewport.Pos.x = mEyeViewport[0].point.x;
      eyeTextures[0].D3D9.Header.RenderViewport.Pos.y = mEyeViewport[0].point.y;
      eyeTextures[0].D3D9.Header.RenderViewport.Size.w = mEyeViewport[0].extent.x;
      eyeTextures[0].D3D9.Header.RenderViewport.Size.h = mEyeViewport[0].extent.y;
      eyeTextures[0].D3D9.Header.TextureSize.w = eyeSize.x;
      eyeTextures[0].D3D9.Header.TextureSize.h = eyeSize.y;
      eyeTextures[0].D3D9.pTexture = mEyeRT[0].getPointer() ? static_cast<GFXD3D9TextureObject*>(mEyeTexture[0].getPointer())->get2DTex() : NULL;

      // Right
      eyeSize = mEyeTexture[1].getWidthHeight();
      eyeTextures[1].D3D9.Header.API = ovrRenderAPI_D3D9;
      eyeTextures[1].D3D9.Header.RenderViewport.Pos.x = mEyeViewport[1].point.x;
      eyeTextures[1].D3D9.Header.RenderViewport.Pos.y = mEyeViewport[1].point.y;
      eyeTextures[1].D3D9.Header.RenderViewport.Size.w = mEyeViewport[1].extent.x;
      eyeTextures[1].D3D9.Header.RenderViewport.Size.h = mEyeViewport[1].extent.y;
      eyeTextures[1].D3D9.Header.TextureSize.w = eyeSize.x;
      eyeTextures[1].D3D9.Header.TextureSize.h = eyeSize.y;
      eyeTextures[1].D3D9.pTexture = mEyeRT[0].getPointer() ? static_cast<GFXD3D9TextureObject*>(mEyeTexture[1].getPointer())->get2DTex() : NULL;

      // Submit!
      GFX->disableShaders();

      GFX->setActiveRenderTarget(windowTarget);
      GFX->clear(GFXClearZBuffer | GFXClearStencil | GFXClearTarget, ColorI(255,0,0), 1.0f, 0);
      ovrHmd_EndFrame(mDevice, mCurrentEyePoses, (ovrTexture*)(&eyeTextures[0]));
   }
#endif

   mFrameReady = false;
}

void OculusVRHMDDevice::getFrameEyePose(DisplayPose *outPose, U32 eyeId) const
{
   // Directly set the rotation and position from the eye transforms
   ovrPosef pose = mCurrentEyePoses[eyeId];
   OVR::Quatf orientation = pose.Orientation;
   const OVR::Vector3f position = pose.Position;

   EulerF rotEuler;
   OculusVRUtil::convertRotation(orientation, rotEuler);

   outPose->orientation = rotEuler;
   outPose->position = Point3F(-position.x, position.z, -position.y);
}

void OculusVRHMDDevice::onDeviceDestroy()
{
   if (!mIsValid || !mDevice)
      return;

   if (mStereoRT.getPointer())
   {
      mStereoRT->zombify();
   }

   if (mEyeRT[1].getPointer() && mEyeRT[1] != mStereoRT)
   {
      mEyeRT[0]->zombify();
      mEyeRT[1]->zombify();
   }

   mStereoRT = NULL;
   mStereoTexture = NULL;
   mStereoDepthTexture = NULL;

   mEyeTexture[0] = NULL;
   mEyeDepthTexture[0] = NULL;
   mEyeTexture[1] = NULL;
   mEyeDepthTexture[1] = NULL;
   mEyeRT[0] = NULL;
   mEyeRT[1] = NULL;

   mRenderConfigurationDirty = true;
   
   ovrHmd_ConfigureRendering(mDevice, NULL, 0, NULL, NULL);
}
