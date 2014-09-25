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

#include "windowManager/platformWindow.h"
#include "gfx/gl/gfxGLDevice.h"
#include "gfx/gl/gfxGLWindowTarget.h"
#include "gfx/gl/gfxGLTextureObject.h"
#include "gfx/gl/gfxGLUtils.h"
#include "postFx/postEffect.h"
#include "gfx/gfxDebugEvent.h"

GFX_ImplementTextureProfile( BackBufferDepthProfile,
									 GFXTextureProfile::DiffuseMap,
									 GFXTextureProfile::PreserveSize |
									 GFXTextureProfile::NoMipmap |
									 GFXTextureProfile::ZTarget |
									 GFXTextureProfile::Pooled,
									 GFXTextureProfile::None );

GFXGLWindowTarget::GFXGLWindowTarget(PlatformWindow *win, GFXDevice *d)
: GFXWindowTarget(win), mDevice(d), mContext(NULL)
, mCopyFBO(0), mBackBufferFBO(0)
{
   win->appEvent.notify(this, &GFXGLWindowTarget::_onAppSignal);
}

GFXGLWindowTarget::~GFXGLWindowTarget()
{
   if (mBackBufferFBO)
   {
      glDeleteFramebuffers(1, &mBackBufferFBO);
      mBackBufferFBO = 0;
   }
}

void GFXGLWindowTarget::resetMode()
{
#ifdef TORQUE_OS_WIN32
   if(mWindow->getVideoMode().fullScreen != mWindow->isFullscreen())
#endif
   {
      zombify();
      _teardownCurrentMode();
      _setupNewMode();
   }
}

void GFXGLWindowTarget::_onAppSignal(WindowId wnd, S32 event)
{
   if(event != WindowHidden)
      return;
}

void GFXGLWindowTarget::resolveTo(GFXTextureObject* obj)
{
   AssertFatal(dynamic_cast<GFXGLTextureObject*>(obj), "GFXGLTextureTarget::resolveTo - Incorrect type of texture, expected a GFXGLTextureObject");
   GFXGLTextureObject* glTexture = static_cast<GFXGLTextureObject*>(obj);
	
   if( gglHasExtension(ARB_copy_image) )
   {
      static_cast<GFXGLDevice*>(obj->getOwningDevice())->_setTempBoundTexture(GL_TEXTURE_2D, static_cast<GFXGLTextureObject*>(mBackBufferColorTex.getPointer())->getHandle());
      static_cast<GFXGLDevice*>(obj->getOwningDevice())->_setTempBoundTexture(GL_TEXTURE_2D, glTexture->getHandle());
      
      if(mBackBufferColorTex.getWidth() == glTexture->getWidth()
         && mBackBufferColorTex.getHeight() == glTexture->getHeight()
         && mBackBufferColorTex.getFormat() == glTexture->getFormat())
      {
		 GFXDEBUGEVENT_MARKER( GFXGLWindowTarget_resolveTo, ColorI::RED );
         glCopyImageSubData(
									 static_cast<GFXGLTextureObject*>(mBackBufferColorTex.getPointer())->getHandle(), GL_TEXTURE_2D, 0, 0, 0, 0,
									 glTexture->getHandle(), GL_TEXTURE_2D, 0, 0, 0, 0,
									 getSize().x, getSize().y, 1);
         return;
      }
   }

   GLuint currentFBORead = static_cast<GFXGLDevice*>(getOwningDevice())->_getCurrentFBORead();
   GLuint currentFBOWrite = static_cast<GFXGLDevice*>(getOwningDevice())->_getCurrentFBOWrite();
	
   if(!mCopyFBO)
   {
      glGenFramebuffers(1, &mCopyFBO);
   }
   
   static_cast<GFXGLDevice*>(getOwningDevice())->_setCurrentFBOWrite(mCopyFBO);
   glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, glTexture->getHandle(), 0);
   
   static_cast<GFXGLDevice*>(getOwningDevice())->_setCurrentFBORead(mBackBufferFBO);
   glBlitFramebuffer(0, 0, getSize().x, getSize().y,
								0, 0, glTexture->getWidth(), glTexture->getHeight(), GL_COLOR_BUFFER_BIT, GL_NEAREST);

   static_cast<GFXGLDevice*>(getOwningDevice())->_setCurrentFBORead(currentFBORead);
   static_cast<GFXGLDevice*>(getOwningDevice())->_setCurrentFBOWrite(currentFBOWrite);
}

inline void GFXGLWindowTarget::_setupAttachments()
{
   GFXDEBUGEVENT_SCOPE( GFXGLWindowTarget_setupAttachments, ColorI::RED );
   
   GFXGLDevice *owner = static_cast<GFXGLDevice*>(getOwningDevice());
   owner->mWindowRT = this;
   
   static_cast<GFXGLDevice*>(getOwningDevice())->_setCurrentFBO(mBackBufferFBO);
   const Point2I dstSize = getSize();
   mBackBufferColorTex.set(dstSize.x, dstSize.y, getFormat(), &PostFxTargetProfile, "backBuffer");
   GFXGLTextureObject *color = static_cast<GFXGLTextureObject*>(mBackBufferColorTex.getPointer());

   mBackBufferDepthTex.set(dstSize.x, dstSize.y, GFXFormatD24S8, &BackBufferDepthProfile, "backBufferDepthStencil");
   GFXGLTextureObject *depth = static_cast<GFXGLTextureObject*>(mBackBufferDepthTex.getPointer());

   GLuint colorHandle = color->getHandle();
   GLuint depthHandle = depth->getHandle();
   
   owner->_setCurrentFBO(mBackBufferFBO);
   glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, color->getHandle(), 0);
   glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, depth->getHandle(), 0);

   CHECK_FRAMEBUFFER_STATUS();

   owner->_setCurrentFBO(mBackBufferFBO);
   glClearColor(1.0, 0.0, 0.0, 1.0);
   glClear(GL_COLOR_BUFFER_BIT);

   //glBindTexture(GL_TEXTURE_2D, color->getHandle());
   glBindTexture(GL_TEXTURE_2D, 0);
}

void GFXGLWindowTarget::makeActive()
{
	GFXDEBUGEVENT_MARKER( GFXGLWindowTarget_makeActive, ColorI::RED );
   if(mBackBufferFBO)
   {
     static_cast<GFXGLDevice*>(getOwningDevice())->_setCurrentFBO(mBackBufferFBO);
   }
   else
   {
      glGenFramebuffers(1, &mBackBufferFBO);
      _setupAttachments();
   }
}

bool GFXGLWindowTarget::present()
{
	GFXDEBUGEVENT_MARKER( GFXGLWindowTarget_present, ColorI::RED );

   GLuint currentFBORead = static_cast<GFXGLDevice*>(getOwningDevice())->_getCurrentFBORead();
   GLuint currentFBOWrite = static_cast<GFXGLDevice*>(getOwningDevice())->_getCurrentFBOWrite();
	
   const Point2I srcSize = mBackBufferColorTex.getWidthHeight();
   const Point2I targetSize = getSize();
   const Point2I dstSize = mWindow->getRealClientExtent(); // jamesu - real window may have a different size

   static_cast<GFXGLDevice*>(getOwningDevice())->_setCurrentFBOWrite(0);
   static_cast<GFXGLDevice*>(getOwningDevice())->_setCurrentFBORead(mBackBufferFBO);
	
   // OpenGL render upside down for make render more similar to DX.
   // Final screen are corrected here
   glBlitFramebuffer(
								0, 0, srcSize.x, srcSize.y,
								0, 0, dstSize.x, dstSize.y, // Y inverted
								GL_COLOR_BUFFER_BIT, GL_NEAREST);
	
   _WindowPresent();
	
   if(srcSize != targetSize || mBackBufferDepthTex.getWidthHeight() != targetSize)
   {
      zombify();
      _setupAttachments();
   }

   //static_cast<GFXGLDevice*>(getOwningDevice())->_setCurrentFBO(mBackBufferFBO);

   static_cast<GFXGLDevice*>(getOwningDevice())->_setCurrentFBORead(currentFBORead);
   static_cast<GFXGLDevice*>(getOwningDevice())->_setCurrentFBOWrite(currentFBOWrite);
	
   return true;
}

void GFXGLWindowTarget::resurrect()
{
}

void GFXGLWindowTarget::zombify()
{
   if(mBackBufferFBO)
   {
      static_cast<GFXGLDevice*>(getOwningDevice())->_setCurrentFBO(0);
      if (mBackBufferFBO)
      {
         glDeleteFramebuffers(1, &mBackBufferFBO);
         mBackBufferFBO = 0;
      }
   }
}
