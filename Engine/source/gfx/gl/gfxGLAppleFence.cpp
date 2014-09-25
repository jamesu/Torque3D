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

#include "gfx/gl/gfxGLAppleFence.h"


GFXGLAppleFence::GFXGLAppleFence(GFXDevice* device) : GFXFence(device), mIssued(false)
{
   mHandle = 0;
   glGenFencesAPPLE(1, &mHandle);
}

GFXGLAppleFence::~GFXGLAppleFence()
{
   glDeleteFencesAPPLE(1, &mHandle);
}

void GFXGLAppleFence::issue()
{
   glSetFenceAPPLE(mHandle);
   mIssued = true;
}

GFXFence::FenceStatus GFXGLAppleFence::getStatus() const
{
   if(!mIssued)
      return GFXFence::Unset;
   
   GLboolean res = glTestFenceAPPLE(mHandle);
   return res ? GFXFence::Processed : GFXFence::Pending;
}

void GFXGLAppleFence::block()
{
   if(!mIssued)
      return;
   
   glFinishFenceAPPLE(mHandle);
}

void GFXGLAppleFence::zombify()
{
   glDeleteFencesAPPLE(1, &mHandle);
	mIssued = false;
}

void GFXGLAppleFence::resurrect()
{
   glGenFencesAPPLE(1, &mHandle);
}

const String GFXGLAppleFence::describeSelf() const
{
   return String::ToString("   GL APPLE Fence %s", mIssued ? "Issued" : "Not Issued");
}

//-----------------------------------------------------------------------------

GFXGLSyncFence::GFXGLSyncFence(GFXDevice* device) : GFXFence(device), mIssued(false)
{
   mHandle = 0;
}

GFXGLSyncFence::~GFXGLSyncFence()
{
	if (mIssued)
		glDeleteSync(mHandle);
}

void GFXGLSyncFence::issue()
{
	mHandle = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
   mIssued = true;
}

GFXFence::FenceStatus GFXGLSyncFence::getStatus() const
{
   if(!mIssued)
      return GFXFence::Unset;
	
	GLint result = 0;
	glGetSynciv(mHandle, GL_SYNC_STATUS, sizeof(GLint), NULL, &result);
   return result == GL_SIGNALED ? GFXFence::Processed : GFXFence::Pending;
}

void GFXGLSyncFence::block()
{
   if(!mIssued)
      return;
	
   glClientWaitSync(mHandle, 0, GL_TIMEOUT_IGNORED);
}

void GFXGLSyncFence::zombify()
{
	if (mHandle != 0)
		glDeleteSync(mHandle);
	mIssued = false;
}

void GFXGLSyncFence::resurrect()
{
}

const String GFXGLSyncFence::describeSelf() const
{
   return String::ToString("   GL Fence %s", mIssued ? "Issued" : "Not Issued");
}

