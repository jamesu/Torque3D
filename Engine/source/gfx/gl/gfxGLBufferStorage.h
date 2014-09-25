//-----------------------------------------------------------------------------
// Copyright (c) 2013-2014 Mode 7 Limited
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

#ifndef _GFXGLBUFFERSTORAGE_H_
#define _GFXGLBUFFERSTORAGE_H_

#ifndef _GFXVERTEXBUFFER_H_
#include "gfx/gfxVertexBuffer.h"
#endif
#ifndef T_GL_H
#include "gfx/gl/tGL/tGL.h"
#endif
#ifndef _GFXGLAPPLEFENCE_H_
#include "gfx/gl/gfxGLAppleFence.h"
#endif

class GFXGLBufferStorage : public StrongRefBase, public GFXResource
{
   friend class GFXLVAO;
   
public:
	GFXGLBufferStorage(U32 initialSize,
                      GLuint type,
                      GLuint usageHint);

	virtual ~GFXGLBufferStorage();

   /// Maps a portion of the buffer from start -> start+size. Returns offset in buffer for vertexPtr
	virtual U32 map(U32 start, U32 size, void **vertexPtr) = 0;

   // Unmaps previously mapped portion of buffer
	virtual void unmap() = 0;

   virtual void cleanup();

   // GFXResource interface
   virtual void zombify();
   virtual void resurrect();

   /// Disposes buffer in driver (if applicable)
   virtual void dispose() {;}

   void cleanupVAOs();

   /// Creates a buffer based on current GL state and requirements
   static GFXGLBufferStorage* createBuffer(GFXDevice *device, GLuint type, GFXBufferType usageHint, U32 initialSize);

   inline GLuint getHandle() { return mBuffer; }

   virtual void onFenceMarked(U32 fenceId) {;}
   virtual void onFenceDone(U32 fenceId) {;}

   const String describeSelf() const { return "GLBUFFER"; }

   /// Returns verts we can map in this buffer (i.e. not tied up behind a fence)
   virtual U32 getBytesFree() { return mSize; }

   static bool smAllowBufferStorage;
   static bool smAllowPinnedMemory;
   static bool smAllowArbSync;


protected:
   friend class GFXGLDevice;

	/// GL buffer handle
	GLuint mBuffer;
   
   /// Storage for resurrect & zombify
   U8* mZombieCache;
   U32 mSize;
   GLuint mUsageHint;
   GLuint mBinding;

   /// FrameAllocator mark if required for lock
   U32 mAllocatorMark;

   U32 mLockedOffset;
   U32 mLockedSize;
};

#endif
