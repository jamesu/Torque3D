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
#include "gfx/D3D9/gfxD3D9Device.h"
#include "postFx/postEffectCommon.h"
#include "gui/core/guiCanvas.h"

// Use D3D9 for win32
#ifdef TORQUE_OS_WIN
#define OVR_D3D_VERSION 9
#include "..\src\OVR_CAPI_D3D.h"
#endif

OculusVRHMDDevice::OculusVRHMDDevice()
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
   mConfigurationDirty = true;
   mDesiredPixelDensity = 0.5f;//1.0f;
   mDesiredRenderingMode = GFXDevice::RS_StereoSideBySide;
   mRTFormat = GFXFormatR8G8B8A8;
   mDrawCanvas = NULL;
}

OculusVRHMDDevice::~OculusVRHMDDevice()
{
   cleanUp();
}

void OculusVRHMDDevice::cleanUp()
{
   if(mDevice)
   {
      ovrHmd_Destroy(mDevice);
      mDevice = NULL;
   }

   mIsValid = false;
}

void OculusVRHMDDevice::set(ovrHmd hmd, bool calculateDistortionScale)
{
   mIsValid = false;
   mIsSimulation = false;
   mConfigurationDirty = true;

   mDevice = hmd;

   mSupportedCaps = hmd->HmdCaps;
   mCurrentCaps = mSupportedCaps & (ovrHmdCap_DynamicPrediction | ovrHmdCap_LowPersistence | (!mVsync ? ovrHmdCap_NoVSync : 0));

   mSupportedDistortionCaps = hmd->DistortionCaps;
   mCurrentDistortionCaps	= mSupportedDistortionCaps & (ovrDistortionCap_Chromatic | ovrDistortionCap_TimeWarp | ovrDistortionCap_Vignette | ovrDistortionCap_Overdrive);
	
   mTimewarp = mSupportedDistortionCaps & mTimewarp;

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

   // Calculated values
   calculateValues();

   mIsValid = true;

   updateCaps();
}

void OculusVRHMDDevice::createSimulation(SimulationTypes simulationType, bool calculateDistortionScale)
{
   if(simulationType == ST_RIFT_PREVIEW)
   {
      createSimulatedPreviewRift(calculateDistortionScale);
   }
}

void OculusVRHMDDevice::createSimulatedPreviewRift(bool calculateDistortionScale)
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

   mEyeToScreen = 0.041000001f;
   mLensSeparation = 0.064000003f;
   mProfileInterpupillaryDistance = 0.064000003f;
   mInterpupillaryDistance = mProfileInterpupillaryDistance;

   calculateValues();
}

void OculusVRHMDDevice::setIPD(F32 ipd, bool calculateDistortionScale)
{
   mInterpupillaryDistance = ipd;
   
   //ovrVector3f hmdToEyeViewOffset[2] = { EyeRenderDesc[0].HmdToEyeViewOffset, EyeRenderDesc[1].HmdToEyeViewOffset };
   //ovrHmd_GetEyePoses(Hmd, 0, hmdToEyeViewOffset, EyeRenderPose, &hmdState);
}

void OculusVRHMDDevice::calculateValues()
{
   if (!mIsValid || !mDevice)
      return;

   // Generate render target textures
   GFXD3D9Device *d3d9GFX = dynamic_cast<GFXD3D9Device*>(GFX);
   if (d3d9GFX && mDrawCanvas)
   {
      PlatformWindow *window = mDrawCanvas->getPlatformWindow();
      //clearRenderTargets();

      GFXVideoMode requiredVideoMode;

      ovrD3D9Config cfg;
      cfg.D3D9.Header.API = ovrRenderAPI_D3D9;
      cfg.D3D9.Header.Multisample = 0;
      cfg.D3D9.pDevice = d3d9GFX->getDevice();
      cfg.D3D9.pDevice->GetSwapChain(0, &cfg.D3D9.pSwapChain);

      ovrFovPort eyeFov[2] = {mDevice->DefaultEyeFov[0], mDevice->DefaultEyeFov[1]};

      mRecomendedEyeTargetSize[0] = ovrHmd_GetFovTextureSize(mDevice, ovrEye_Left,  eyeFov[0], mDesiredPixelDensity);
      mRecomendedEyeTargetSize[1] = ovrHmd_GetFovTextureSize(mDevice, ovrEye_Right, eyeFov[1], mDesiredPixelDensity);
      
      // Calculate render target size
      if (mDesiredRenderingMode == GFXDevice::RS_StereoSideBySide)
      {
         Point2I rtSize(
            mRecomendedEyeTargetSize[0].w + mRecomendedEyeTargetSize[1].w,
            mRecomendedEyeTargetSize[0].h > mRecomendedEyeTargetSize[1].h ? mRecomendedEyeTargetSize[0].h : mRecomendedEyeTargetSize[1].h
         );

         GFXFormat targetFormat = GFX->getActiveRenderTarget()->getFormat();
         mRTFormat = targetFormat;

         rtSize = generateRenderTarget(mStereoRT, rtSize);

         // Update window size if it's incorrect
         Point2I extent = mDrawCanvas->getBounds().extent;
         if (extent.x != rtSize.x && extent.y != rtSize.y)
         {
            GFXVideoMode newMode;
            newMode.antialiasLevel = 0;
            newMode.bitDepth = 32;
            newMode.fullScreen = false;
            newMode.refreshRate = 75;
            newMode.resolution = Point2I(rtSize[0], rtSize[1]);
            newMode.wideScreen = false;
            window->setVideoMode(newMode);
         }

         AssertFatal(window->getClientExtent().x == rtSize[0] && window->getClientExtent().y == rtSize[1], "Window didn't resize to correct dimensions");

         cfg.D3D9.Header.BackBufferSize = OVR::Sizei(rtSize.x, rtSize.y);

         mEyeRenderSize[0] = rtSize;
         mEyeRenderSize[1] = rtSize;

         mEyeRT[0] = mStereoRT;
         mEyeViewport[0] = RectI(Point2I(0,0), mEyeRenderSize[0]);
         mEyeRT[1] = mStereoRT;
         mEyeViewport[1] = RectI(Point2I((rtSize.x+1)/2,0), mEyeRenderSize[1]);
      }
      else if (mDesiredRenderingMode == GFXDevice::RS_StereoRenderTargets)
      {
         // TODO
      }
      else
      {
         // No rendering, abort!
         return;
      }

      ovrHmd_AttachToWindow(mDevice, window->getPlatformDrawable(), NULL, NULL);

      // Set texture info

      if (!ovrHmd_ConfigureRendering( mDevice, &cfg.Config, mCurrentDistortionCaps, eyeFov, mEyeRenderDesc ))
      {
         Con::errorf("Couldn't configure oculus rendering!");
         return;
      }
   }

   ovrTrackingState hmdState;
   ovrVector3f hmdToEyeViewOffset[2] = { mEyeRenderDesc[0].HmdToEyeViewOffset, mEyeRenderDesc[1].HmdToEyeViewOffset };
   ovrHmd_GetEyePoses(mDevice, 0, hmdToEyeViewOffset, mCurrentEyePoses, &hmdState);
}

Point2I OculusVRHMDDevice::generateRenderTarget(GFXTexHandle &dest, Point2I desiredSize)
{
    // Texture size that we already have might be big enough.
    Point2I newRTSize;

    if (dest.isNull())
    {
       newRTSize = desiredSize;
    }
    else
    {
       Point2I currentSize = dest.getWidthHeight();
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

    // Does that require actual reallocation?
    if (!dest.getPointer() || dest.getWidthHeight() != newRTSize)
    {
      dest.set(newRTSize.x, newRTSize.y, mRTFormat, &VRTextureProfile,  avar( "%s() - (line %d)", __FUNCTION__, __LINE__ ) );
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
   if (!mIsValid || !mDevice || !mDrawCanvas || sInFrame)
      return;

   sInFrame = true;

   if (mConfigurationDirty)
   {
      calculateValues();
   }
   ovrHmd_BeginFrame(mDevice, 0);

   sInFrame = false;
}

void OculusVRHMDDevice::onEndFrame()
{
   if (!mIsValid || !mDevice || !mDrawCanvas || sInFrame)
      return;

   ovrVector2i Pos;
   ovrSizei    Size;

   Point2I eyeSize;

   // Resolve backbuffer to the stereo RT, assuming texture is correct dimensions
   // TODO: handle this better
   if (mStereoRT.getPointer())
   {
      GFXTarget *target = mDrawCanvas->getPlatformWindow()->getGFXTarget();
      const Point2I &targetSize = target->getSize();
      GFXFormat targetFormat = target->getFormat();

      AssertFatal(targetSize == mStereoRT.getWidthHeight() && targetFormat == mStereoRT.getFormat(), "Render Target Setup Incorrectly!");

      // TOFIX target->resolveTo( mStereoRT );

      // Just to be on the safe side...
      mEyeRT[0] = mStereoRT;
      mEyeRT[1] = mStereoRT;
   }

   GFXD3D9Device *d3d9GFX = dynamic_cast<GFXD3D9Device*>(GFX);
   if (d3d9GFX && mEyeRT[0].getPointer())
   {
      // Left
      ovrD3D9Texture eyeTextures[2];
      eyeSize = mEyeRT[0].getWidthHeight();
      eyeTextures[0].D3D9.Header.API = ovrRenderAPI_D3D9;
      eyeTextures[0].D3D9.Header.RenderViewport.Pos.x = mEyeViewport[0].point.x;
      eyeTextures[0].D3D9.Header.RenderViewport.Pos.y = mEyeViewport[0].point.x;
      eyeTextures[0].D3D9.Header.RenderViewport.Size.w = mEyeViewport[0].extent.x;
      eyeTextures[0].D3D9.Header.RenderViewport.Size.h = mEyeViewport[0].extent.x;
      eyeTextures[0].D3D9.Header.TextureSize.w = eyeSize.x;
      eyeTextures[0].D3D9.Header.TextureSize.h = eyeSize.y;
      eyeTextures[0].D3D9.pTexture = mEyeRT[0].getPointer() ? static_cast<GFXD3D9TextureObject*>(mEyeRT[0].getPointer())->get2DTex() : NULL;

      // Right
      eyeSize = mEyeRT[1].getWidthHeight();
      eyeTextures[1].D3D9.Header.API = ovrRenderAPI_D3D9;
      eyeTextures[1].D3D9.Header.RenderViewport.Pos.x = mEyeViewport[1].point.x;
      eyeTextures[1].D3D9.Header.RenderViewport.Pos.y = mEyeViewport[1].point.x;
      eyeTextures[1].D3D9.Header.RenderViewport.Size.w = mEyeViewport[1].extent.x;
      eyeTextures[1].D3D9.Header.RenderViewport.Size.h = mEyeViewport[1].extent.x;
      eyeTextures[1].D3D9.Header.TextureSize.w = eyeSize.x;
      eyeTextures[1].D3D9.Header.TextureSize.h = eyeSize.y;
      eyeTextures[1].D3D9.pTexture = mEyeRT[0].getPointer() ? static_cast<GFXD3D9TextureObject*>(mEyeRT[1].getPointer())->get2DTex() : NULL;

      // Submit!
      GFX->disableShaders();
      ovrHmd_EndFrame(mDevice, mCurrentEyePoses, (ovrTexture*)(&eyeTextures[0]));
   }
}

