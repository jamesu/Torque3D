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

#include <Cocoa/Cocoa.h>
#include "windowManager/mac/macWindow.h"
#include "windowManager/mac/macView.h"

#include "console/console.h"

MacWindow::SafariWindowInfo* MacWindow::sSafariWindowInfo = NULL;
MacWindow* MacWindow::sInstance = NULL;

@interface SafariBrowserWindow : NSWindow
{
}
@end

@implementation SafariBrowserWindow

// Windows created with NSBorderlessWindowMask normally can't be key, but we want ours to be
- (BOOL) canBecomeKeyWindow
{
    return YES;
}

@end


MacWindow::MacWindow(U32 windowId, const char* windowText, Point2I clientExtent)
{
   mMouseLocked      = false;
   mShouldMouseLock  = false;
   mTitle            = NULL;
   mMouseCaptured    = false;
   
   mCocoaWindow      = NULL;
   mCursorController = new MacCursorController( this );
   mOwningWindowManager = NULL;
   
   mShouldFullscreen = false;
   mDefaultDisplayMode = NULL;
   
   mSkipMouseEvents = 0;
   
   mDisplay = kCGDirectMainDisplay;
   mMainDisplayBounds = mDisplayBounds = CGDisplayBounds(mDisplay);
   
   mWindowId = windowId;
   _initCocoaWindow(windowText, clientExtent);
   
   appEvent.notify(this, &MacWindow::_onAppEvent);
   
   sInstance = this;
}

MacWindow::~MacWindow()
{
   appEvent.remove(this, &MacWindow::_onAppEvent);

   //ensure our view isn't the delegate
   [NSApp setDelegate:nil];
   
   if( mCocoaWindow )
   {
      NSWindow* window = mCocoaWindow;
      _disassociateCocoaWindow();
      
      [ window close ];
   }
   
   appEvent.trigger(mWindowId, LoseFocus);
   appEvent.trigger(mWindowId, WindowDestroy);
   
   mOwningWindowManager->_removeWindow(this);
   
   setSafariWindow(NULL);
   
   sInstance = NULL;
}

extern "C"
{

void torque_setsafariwindow( NSWindow *window, S32 x, S32 y, S32 width, S32 height)
{
   MacWindow::setSafariWindow(window, x, y, width, height);
}

}

void MacWindow::hideBrowserWindow(bool hide)
{
   if (sSafariWindowInfo && sInstance && sInstance->mCocoaWindow)
   {
      if (hide)
      {
         if (sSafariWindowInfo && sSafariWindowInfo->safariWindow)
            [sSafariWindowInfo->safariWindow removeChildWindow: sInstance->mCocoaWindow];
   
         sInstance->hide();
      }
      else
      {
      
         if (sSafariWindowInfo && sSafariWindowInfo->safariWindow)
            [sSafariWindowInfo->safariWindow addChildWindow: sInstance->mCocoaWindow ordered:NSWindowAbove];
         
         sInstance->show();
      }
   }
}

void MacWindow::setSafariWindow(NSWindow *window, S32 x, S32 y, S32 width, S32 height )
{
   if (!window)
   {
      hideBrowserWindow(true);
   
      if (sSafariWindowInfo)
         delete sSafariWindowInfo;
         
      sSafariWindowInfo = NULL;
      
      return;
   }
   
   if (!sSafariWindowInfo)
   {   
      sSafariWindowInfo = new SafariWindowInfo;
      sSafariWindowInfo->safariWindow = window;
      sSafariWindowInfo->width = width;
      sSafariWindowInfo->height = height;
      sSafariWindowInfo->x = x;
      sSafariWindowInfo->y = y;
      if (sInstance && sInstance->mCocoaWindow)
      {
         [window addChildWindow: sInstance->mCocoaWindow ordered:NSWindowAbove];
         hideBrowserWindow(false);
      }
   }
   else
   {
      sSafariWindowInfo->width = width;
      sSafariWindowInfo->height = height;
      sSafariWindowInfo->x = x;
      sSafariWindowInfo->y = y;   
   }
   
   if (sInstance && sInstance->mCocoaWindow)
   {
      //update position
      
      NSRect frame = [sSafariWindowInfo->safariWindow frame];
      
      NSPoint o = { (float)sSafariWindowInfo->x,  frame.size.height -  sSafariWindowInfo->y  };      
      NSPoint p = [sSafariWindowInfo->safariWindow convertBaseToScreen: o];
            
      NSRect contentRect = NSMakeRect(p.x, p.y - sSafariWindowInfo->height, sSafariWindowInfo->width,sSafariWindowInfo->height);
      
      // we have to set display to NO when resizing otherwise get hangs, perhaps add delegate to SafariBrowserWindow class?
      [sInstance->mCocoaWindow setFrame:contentRect display:NO];
            
   }
   
}

void MacWindow::_initCocoaWindow(const char* windowText, Point2I clientExtent)
{
   // create the window
   NSRect contentRect;
   U32 style;
   
   if (sSafariWindowInfo)
   {
      
      NSRect frame = [sSafariWindowInfo->safariWindow frame];
      
      NSPoint o = { (float)sSafariWindowInfo->x,  frame.size.height -  sSafariWindowInfo->y  };
      
      NSPoint p = [sSafariWindowInfo->safariWindow convertBaseToScreen: o];
      
      contentRect = NSMakeRect(0, 0, sSafariWindowInfo->width,sSafariWindowInfo->height);
      
      style = NSBorderlessWindowMask;
      
      mCocoaWindow = [[SafariBrowserWindow alloc] initWithContentRect:contentRect styleMask:style backing:NSBackingStoreBuffered defer:YES screen:nil];
      
      [mCocoaWindow setFrameTopLeftPoint: p];
      
      [sSafariWindowInfo->safariWindow addChildWindow: mCocoaWindow ordered:NSWindowAbove];
      
      // necessary to accept mouseMoved events
      [mCocoaWindow setAcceptsMouseMovedEvents:YES];
   }
   else
   {
      
      contentRect = NSMakeRect(0,0,clientExtent.x, clientExtent.y);
      
      style = NSTitledWindowMask | NSClosableWindowMask | NSMiniaturizableWindowMask | NSResizableWindowMask;
      
      mCocoaWindow = [[NSWindow alloc] initWithContentRect:contentRect styleMask:style backing:NSBackingStoreBuffered defer:YES screen:nil];
      if(windowText)
         [mCocoaWindow setTitle: [NSString stringWithUTF8String: windowText]];
      
      // necessary to accept mouseMoved events
      [mCocoaWindow setAcceptsMouseMovedEvents:YES];
      
      // correctly position the window on screen
      [mCocoaWindow center];
      
      // create the opengl view. we don't care about its pixel format, because we
      // will be replacing its context with another one.
      GGMacView* view = [[GGMacView alloc] initWithFrame:contentRect pixelFormat:[NSOpenGLView defaultPixelFormat]];
      [view setTorqueWindow:this];
      [mCocoaWindow setContentView:view];
      [mCocoaWindow setDelegate:static_cast<id<NSWindowDelegate> >(view)];
   }
}

void MacWindow::_disassociateCocoaWindow()
{
   if( !mCocoaWindow )
      return;
      
   [mCocoaWindow setContentView:nil];
   [mCocoaWindow setDelegate:nil];   

   if (sSafariWindowInfo)
      [sSafariWindowInfo->safariWindow removeChildWindow: mCocoaWindow];
      
   mCocoaWindow = NULL;
}

void MacWindow::setVideoMode(const GFXVideoMode &mode)
{
   if (mCurrentMode == mode)
      return;
   
   mCurrentMode = mode;
   
   if(mCurrentMode.fullScreen)
   {
      [mCocoaWindow setLevel:NSMainMenuWindowLevel+1];
      
      [mCocoaWindow setOpaque:YES];
      [mCocoaWindow setHidesOnDeactivate:YES];
      
      [mCocoaWindow makeKeyAndOrderFront:mCocoaWindow];
      [mCocoaWindow setStyleMask:NSBorderlessWindowMask];
      
      CGRect bounds = CGDisplayBounds(mDisplay);
      setBounds(RectI(bounds.origin.x, bounds.origin.y, bounds.size.width, bounds.size.height));
      
      // Set backing size
      GLint dim[2];
      dim[0] = mCurrentMode.resolution.x;
      dim[1] = mCurrentMode.resolution.y;
      
      CGLContextObj ctx = CGLGetCurrentContext();
      CGLSetParameter(ctx, kCGLCPSurfaceBackingSize, dim);
      CGLEnable(ctx, kCGLCESurfaceBackingSize);
   }
   else
   {
      [mCocoaWindow setLevel:NSNormalWindowLevel];
      
      [mCocoaWindow setOpaque:NO];
      [mCocoaWindow setHidesOnDeactivate:NO];
      
      [mCocoaWindow setStyleMask:NSTitledWindowMask | NSClosableWindowMask | NSMiniaturizableWindowMask | NSResizableWindowMask];
      mDefaultDisplayMode = NULL;
      
      setSize(mCurrentMode.resolution);
      
      // Clear backing size
      CGLContextObj ctx = CGLGetCurrentContext();
      CGLDisable(ctx, kCGLCESurfaceBackingSize);
   }
   
   if(mTarget.isValid())
      mTarget->resetMode();
}

void MacWindow::_onAppEvent(WindowId, S32 evt)
{
   if(evt == LoseFocus && isFullscreen())
   {
      mShouldFullscreen = true;
      GFXVideoMode mode = mCurrentMode;
      mode.fullScreen = false;
      setVideoMode(mode);
   }
   
   if(evt == GainFocus && !isFullscreen() && mShouldFullscreen)
   {
      mShouldFullscreen = false;
      GFXVideoMode mode = mCurrentMode;
      mode.fullScreen = true;
      setVideoMode(mode);
   }
}

void MacWindow::_setFullscreen(bool fullScreen)
{
   // NOP
}

void* MacWindow::getPlatformDrawable() const
{
   return [mCocoaWindow contentView];
}

void MacWindow::show()
{
   [mCocoaWindow makeKeyAndOrderFront:nil];
   [mCocoaWindow makeFirstResponder:[mCocoaWindow contentView]];
   appEvent.trigger(getWindowId(), WindowShown);
   appEvent.trigger(getWindowId(), GainFocus);
}

void MacWindow::close()
{
   [mCocoaWindow close];
   appEvent.trigger(mWindowId, LoseFocus);
   appEvent.trigger(mWindowId, WindowDestroy);
   
   mOwningWindowManager->_removeWindow(this);
   
   delete this;
}

void MacWindow::hide()
{
   [mCocoaWindow orderOut:nil];
   appEvent.trigger(getWindowId(), WindowHidden);
}

void MacWindow::setDisplay(CGDirectDisplayID display)
{
   mDisplay = display;
   mDisplayBounds = CGDisplayBounds(mDisplay);
}

PlatformWindow* MacWindow::getNextWindow() const
{
   return mNextWindow;
}

bool MacWindow::setSize(const Point2I &newSize)
{
   if (sSafariWindowInfo)
      return true;

   NSSize newExtent = {(CGFloat)newSize.x, (CGFloat)newSize.y};
   [mCocoaWindow setContentSize:newExtent];
   [mCocoaWindow center];
   return true;
}

void MacWindow::setClientExtent( const Point2I newExtent )
{
   // Set the Client Area Extent (Resolution) of this window
   NSSize newSize = {(CGFloat)newExtent.x, (CGFloat)newExtent.y};
   [mCocoaWindow setContentSize:newSize];
}

const Point2I MacWindow::getRealClientExtent()
{
   return getClientExtent();
}

const Point2I MacWindow::getClientExtent()
{
   if(!mCurrentMode.fullScreen)
   {
      // Get the Client Area Extent (Resolution) of this window
      NSSize size = [[mCocoaWindow contentView] frame].size;
      return Point2I(size.width, size.height);
   }
   else
   {
      return Point2I(mCurrentMode.resolution.x, mCurrentMode.resolution.y);
   }
}

void MacWindow::setBounds( const RectI &newBounds )
{
   NSRect newFrame = NSMakeRect(newBounds.point.x, newBounds.point.y, newBounds.extent.x, newBounds.extent.y);
   [mCocoaWindow setFrame:newFrame display:YES];
}

const RectI MacWindow::getBounds() const
{
   if(!mCurrentMode.fullScreen)
   {
      // Get the position and size (fullscreen windows are always at (0,0)).
      NSRect frame = [mCocoaWindow frame];
      return RectI(frame.origin.x, frame.origin.y, frame.size.width, frame.size.height);
   }
   else
   {
      return RectI(Point2I(0, 0), Point2I(mCurrentMode.resolution.x, mCurrentMode.resolution.y));
   }
}

void MacWindow::setPosition( const Point2I newPosition )
{
   NSScreen *screen = [mCocoaWindow screen];
   NSRect screenFrame = [screen frame];

   NSPoint pos = {(CGFloat)newPosition.x, (CGFloat)(newPosition.y + screenFrame.size.height)};
   [mCocoaWindow setFrameTopLeftPoint: pos];
}

const Point2I MacWindow::getPosition()
{
   NSScreen *screen = [mCocoaWindow screen];
   NSRect screenFrame = [screen frame];
   NSRect frame = [mCocoaWindow frame];

   return Point2I(frame.origin.x, screenFrame.size.height - (frame.origin.y + frame.size.height));
}

void MacWindow::centerWindow()
{
   [mCocoaWindow center];
}

Point2I MacWindow::clientToScreen( const Point2I& pos )
{
   NSPoint p = { (CGFloat)pos.x, (CGFloat)pos.y };
   
   p = [ mCocoaWindow convertBaseToScreen: p ];
   
   if (mCurrentMode.fullScreen)
   {
      p.x *= mDisplayBounds.size.width / (F32)mCurrentMode.resolution.x;
      p.y *= mDisplayBounds.size.height / (F32)mCurrentMode.resolution.y;
   }
   
   return Point2I( p.x, p.y );
}

Point2I MacWindow::screenToClient( const Point2I& pos )
{
   NSPoint p = { (CGFloat)pos.x, (CGFloat)pos.y };
   
   p = [ mCocoaWindow convertScreenToBase: p ];
   
   if (mCurrentMode.fullScreen)
   {
      p.x *= (F32)mCurrentMode.resolution.x / mDisplayBounds.size.width;
      p.y *= (F32)mCurrentMode.resolution.y / mDisplayBounds.size.height;
   }
   
   return Point2I( p.x, p.y );
}

bool MacWindow::isFocused()
{
   return [mCocoaWindow isKeyWindow];
}

bool MacWindow::isOpen()
{
   // Maybe check if _window != NULL ?
   return true;
}

bool MacWindow::isVisible()
{
   return !isMinimized() && ([mCocoaWindow isVisible] == YES);
}
   
void MacWindow::setFocus()
{
   [mCocoaWindow makeKeyAndOrderFront:nil];
}

void MacWindow::signalGainFocus()
{
   if(isFocused())
      [[mCocoaWindow delegate] performSelector:@selector(signalGainFocus)];
}

void MacWindow::minimize()
{
   if(!isVisible())
      return;
      
   [mCocoaWindow miniaturize:nil];
   appEvent.trigger(getWindowId(), WindowHidden);
}

void MacWindow::maximize()
{
   if(!isVisible())
      return;
   
   // GFX2_RENDER_MERGE 
   //[mCocoaWindow miniaturize:nil];
   //appEvent.trigger(getWindowId(), WindowHidden);
}

void MacWindow::restore()
{
   if(!isMinimized())
      return;
   
   [mCocoaWindow deminiaturize:nil];
   appEvent.trigger(getWindowId(), WindowShown);
}

bool MacWindow::isMinimized()
{
   return [mCocoaWindow isMiniaturized] == YES;
}

bool MacWindow::isMaximized()
{
   return false;
}

void MacWindow::clearFocus()
{
   // Clear the focus state for this Window.  
   // If the Window does not have focus, nothing happens.
   // If the Window has focus, it relinquishes it's focus to the Operating System
   
   // TODO: find out if we can do anything correct here. We are instructed *not* to call [NSWindow resignKeyWindow], and we don't necessarily have another window to assign as key.
}

bool MacWindow::setCaption(const char* windowText)
{
   mTitle = windowText;
   [mCocoaWindow setTitle:[NSString stringWithUTF8String:mTitle]];
   return true;
}

void MacWindow::_doMouseLockNow()
{
   if(!isVisible())
      return;
      
   if(mShouldMouseLock == mMouseLocked && mMouseLocked != isCursorVisible())
      return;
   
   if(mShouldMouseLock)
      _dissociateMouse();
   else
      _associateMouse();
   
   // hide the cursor if we're locking, show it if we're unlocking
   setCursorVisible(!shouldLockMouse());

   mMouseLocked = mShouldMouseLock;

   return;
}

void MacWindow::_associateMouse()
{
   CGAssociateMouseAndMouseCursorPosition(true);
}

void MacWindow::_dissociateMouse()
{
   _centerMouse();
   CGAssociateMouseAndMouseCursorPosition(false);
}

void MacWindow::_centerMouse()
{
   NSRect frame = [mCocoaWindow frame];
   
   // Deal with the y flip (really fun when more than one monitor is involved)
   F32 offsetY = mMainDisplayBounds.size.height - mDisplayBounds.size.height;
   frame.origin.y = (mDisplayBounds.size.height + offsetY) - (S32)frame.origin.y - (S32)frame.size.height;
   mCursorController->setCursorPosition(frame.origin.x + frame.size.width / 2, frame.origin.y + frame.size.height / 2);
}
