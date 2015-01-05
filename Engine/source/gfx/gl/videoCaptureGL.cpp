#include "videoCaptureGL.h"
#include "gfx/gl/gfxGLDevice.h"
#include "gfx/gl/gfxGLTextureObject.h"
#include "gfx/gl/gfxGLTextureManager.h"
#include "gfx/gl/gfxGLWindowTarget.h"
#include "gfx/gl/gfxGLUtils.h"

#include "platform/tmm_off.h"

VideoFrameGrabberGL::CaptureResource::~CaptureResource()
{
   vidMemTex.free();
   
   if (vidMemFBO)
      glDeleteFramebuffers(1, &vidMemFBO);
   
   if (sysMemPBO)
      glDeleteBuffers(1, &sysMemPBO);
}

VideoFrameGrabberGL::VideoFrameGrabberGL()
{
   GFXDevice::getDeviceEventSignal().notify( this, &VideoFrameGrabberGL::_handleGFXEvent );   
   mCurrentCapture = 0;
}

VideoFrameGrabberGL::~VideoFrameGrabberGL()
{
   GFXDevice::getDeviceEventSignal().remove( this, &VideoFrameGrabberGL::_handleGFXEvent );
}

 
void VideoFrameGrabberGL::captureBackBuffer()
{
   AssertFatal( mCapture[mCurrentCapture].stage != eInSystemMemory, "Error! Trying to override a capture resource in \"SystemMemory\" stage!" );

   GFXGLDevice *glDevice = static_cast<GFXGLDevice *>(GFX);
   
   GFXGLWindowTarget *target = static_cast<GFXGLWindowTarget*>(glDevice->mWindowRT.getPointer());
   GFXTexHandle &vidMemTex = mCapture[mCurrentCapture].vidMemTex;
   
   // Re-init video memory texture if needed
   if (vidMemTex.isNull() || vidMemTex.getWidthHeight() != mResolution)
      vidMemTex.set(mResolution.x, mResolution.y, GFXFormatR8G8B8X8, &GFXDefaultRenderTargetProfile, avar("%s() - mVidMemTex (line %d)", __FUNCTION__, __LINE__) );
   
   // Setup FBO & PBO if its not there
   if (mCapture[mCurrentCapture].vidMemFBO == 0)
   {
      glGenFramebuffers(1, &mCapture[mCurrentCapture].vidMemFBO);
      glDevice->_setCurrentFBOWrite(mCapture[mCurrentCapture].vidMemFBO);
      glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, static_cast<GFXGLTextureObject*>(vidMemTex.getPointer())->getHandle(), 0);
      
      glGenBuffers(1, &mCapture[mCurrentCapture].sysMemPBO);
      glBindBuffer(GL_PIXEL_PACK_BUFFER, mCapture[mCurrentCapture].sysMemPBO);
      glBufferData(GL_PIXEL_PACK_BUFFER, mResolution.x * mResolution.y * 4, 0, GL_STREAM_READ);
      glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
   }
   else
   {
      glDevice->_setCurrentFBOWrite(mCapture[mCurrentCapture].vidMemFBO);
   }
   
   glDevice->_setCurrentFBORead(target->getBackBufferFBO());
   
   Point2I size = target->getSize();
   glBlitFramebuffer(0, 0, size.x, size.y,
                     0, 0, mResolution.x, mResolution.y, GL_COLOR_BUFFER_BIT, GL_NEAREST);
   
   //CHECK_GL_ERROR();

   // Update the stage
   mCapture[mCurrentCapture].stage = eInVideoMemory;
}

void VideoFrameGrabberGL::makeBitmap()
{    
   // Advance the stages for all resources, except the one used for the last capture
   for (U32 i=0; i<eNumStages; i++)
   {
      if (i == mCurrentCapture)
         continue;

      switch (mCapture[i].stage)
      {         
      case eInVideoMemory:
         copyToSystemMemory(mCapture[i]);
         break;
      case eInSystemMemory:
         copyToBitmap(mCapture[i]);
         break;
      }
   }

   // Change the resource being used for backbuffer captures
   mCurrentCapture = (mCurrentCapture + 1) % eNumStages;

   AssertFatal( mCapture[mCurrentCapture].stage != eInSystemMemory, "Error! A capture resource with an invalid state was picked for making captures!" );
}

void VideoFrameGrabberGL::releaseTextures()
{
   for (U32 i=0; i<eNumStages; i++)
   {
      mCapture[i].vidMemTex.free();
      
      if (mCapture[i].vidMemFBO)
      {
         glDeleteFramebuffers(1, &mCapture[i].vidMemFBO);
         mCapture[i].vidMemFBO = 0;
      }
      
      if (mCapture[i].sysMemPBO)
      {
         glDeleteBuffers(1, &mCapture[i].sysMemPBO);
         mCapture[i].sysMemPBO = 0;
      }
      
      mCapture[i].stage = eReadyToCapture;
   }   
}

void VideoFrameGrabberGL::copyToSystemMemory(CaptureResource &capture)
{
   AssertFatal( capture.stage == eInVideoMemory, "Error! copyToSystemMemory() can only work in resources in 'eInVideoMemory' stage!" );

   GFXTexHandle &vidMemTex = capture.vidMemTex;
   
   // Copy pixels to PBO
   glBindBuffer(GL_PIXEL_PACK_BUFFER, capture.sysMemPBO);
   glReadPixels(0, 0, vidMemTex.getWidth(), vidMemTex.getHeight(), GL_BGRA, GL_UNSIGNED_BYTE, NULL);
   glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
   
   // Change the resource state
   capture.stage = eInSystemMemory;
}

void VideoFrameGrabberGL::copyToBitmap(CaptureResource &capture)
{
   AssertFatal( capture.stage == eInSystemMemory, "Error! copyToBitmap() can only work in resources in 'eInSystemMemory' stage!" );

   GFXTexHandle &vidMemTex = capture.vidMemTex;
   Point2I size = vidMemTex.getWidthHeight();
   
   glBindBuffer(GL_PIXEL_PACK_BUFFER, capture.sysMemPBO);
   U8 *data = (U8*)glMapBuffer(GL_PIXEL_PACK_BUFFER, GL_READ_ONLY);
   if (data)
   {
      // Allocate a GBitmap and copy into it.
      GBitmap *gb = new GBitmap(size.x, size.y);
      
      // We've got the X8 in there so we have to manually copy stuff.
      const U32* src = (const U32*)data;
      U8* dst = gb->getWritableBits();
      for (U32 j=0; j<size.y; j++)
      {
         src = (const U32*)data + ((size.x) * (size.y-j-1));
         S32 pixels = size.x;
         for(S32 i=0; i<pixels; i++)
         {
            U32 px = *src++;
            *dst++ = px >> 16;
            *dst++ = px >> 8;
            *dst++ = px;
         }
      }
      
      // Push this new bitmap
      pushNewBitmap(gb);
      
      glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
   }
   else
   {
      AssertFatal(false, "Pixel data not ready!");
   }
   glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);

   // Change the resource state
   capture.stage = eReadyToCapture;
}

bool VideoFrameGrabberGL::_handleGFXEvent( GFXDevice::GFXDeviceEventType event_ )
{
   switch ( event_ )
   {
      case GFXDevice::deDestroy :
         releaseTextures();
         break;

      default:
         break;
   }

   return true;
}
