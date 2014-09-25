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

#ifndef _GFXGLAPPLEFENCE_H_
#define _GFXGLAPPLEFENCE_H_

#include "gfx/gfxFence.h"
#include "gfx/gl/tGL/tGL.h"

class GFXGLAppleFence : public GFXFence
{
public:
   GFXGLAppleFence(GFXDevice* device);
   virtual ~GFXGLAppleFence();
   
   // GFXFence interface
   virtual void issue();
   virtual FenceStatus getStatus() const;
   virtual void block();
   
   // GFXResource interface
   virtual void zombify();
   virtual void resurrect();
   virtual const String describeSelf() const;
   
private:
   GLuint mHandle;
   bool mIssued;
};

class GFXGLSyncFence : public GFXFence
{
public:
   GFXGLSyncFence(GFXDevice* device);
   virtual ~GFXGLSyncFence();
   
   // GFXFence interface
   virtual void issue();
   virtual FenceStatus getStatus() const;
   virtual void block();
   
   // GFXResource interface
   virtual void zombify();
   virtual void resurrect();
   virtual const String describeSelf() const;
   
private:
   GLsync mHandle;
   bool mIssued;
};

#endif