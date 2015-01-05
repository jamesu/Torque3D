#ifndef _VIDEOCAPTURE_H_
#include "gfx/video/videoCapture.h"
#endif

#ifndef _GFXDEVICE_H_
#include "gfx/gfxDevice.h"
#endif

#ifndef _GFXGLDEVICE_H_
#include "gfx/gl/gfxGLDevice.h"
#endif


class VideoFrameGrabberGL : public VideoFrameGrabber
{
protected:   
   enum CaptureStage {
      eReadyToCapture,         
      eInVideoMemory,       
      eInSystemMemory,
      eNumStages
   };

   // Contains all elements involved in single frame capture and
   // is used to spread the multiple "stages" needed to capture a bitmap
   // over various frames to keep GPU resources from locking the CPU.
   struct CaptureResource {
      GLuint vidMemFBO;       // Video memory FBO
      GFXTexHandle vidMemTex; // Video memory texture
      GLuint sysMemPBO;       // System memory PBO
      CaptureStage stage;     // This resource's capture stage

      CaptureResource() : stage(eReadyToCapture), vidMemFBO(0), sysMemPBO(0)  {};
      ~CaptureResource();
   };

   // Capture resource array. One item for each capture pipeline stage
   CaptureResource mCapture[eNumStages];

   // Current capture index
   S32 mCurrentCapture;
   
   // Copies a capture's video memory content to system memory
   void copyToSystemMemory(CaptureResource &capture);

   // Copies a capture's syste memory content to a new bitmap
   void copyToBitmap(CaptureResource &capture);

   bool _handleGFXEvent(GFXDevice::GFXDeviceEventType event);
      
   //------------------------------------------------
   // Overloaded from VideoFrameGrabber
   //------------------------------------------------
   void captureBackBuffer();
   void makeBitmap();
   void releaseTextures();

public:
   VideoFrameGrabberGL();
   ~VideoFrameGrabberGL();
};