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

#ifndef _GFXGLVERTEXBUFFER_H_
#define _GFXGLVERTEXBUFFER_H_

#ifndef _GFXVERTEXBUFFER_H_
#include "gfx/gfxVertexBuffer.h"
#endif
#ifndef T_GL_H
#include "gfx/gl/tGL/tGL.h"
#endif
#ifndef _GFXGLBUFFERSTORAGE_H_
#include "gfx/gl/gfxGLBufferStorage.h"
#endif


class GFXGLVAO;

/// This is a vertex buffer which uses GL_ARB_vertex_buffer_object.
class GFXGLVertexBuffer : public GFXVertexBuffer 
{
   friend class GFXLVAO;
   
public:
   GFXGLVertexBuffer() : GFXVertexBuffer(NULL, 0, NULL, 0, GFXBufferTypeDynamic), mClearAtFrameEnd(false) {;}

	GFXGLVertexBuffer(   GFXDevice *device, 
                        U32 numVerts, 
                        const GFXVertexFormat *vertexFormat, 
                        U32 vertexSize, 
                        GFXBufferType bufferType );

	~GFXGLVertexBuffer();

	virtual void lock(U32 vertexStart, U32 vertexEnd, void **vertexPtr);
	virtual void unlock();
	virtual void prepare();
   virtual void finish();

   // GFXResource interface
   virtual void zombify();
   virtual void resurrect();

   virtual void onFenceMarked(U32 fenceId);
   virtual void onFenceDone(U32 fenceId);
   virtual U32 getStorageVertsFree();

   static GFXGLVertexBuffer* createBuffer(GFXDevice *device);

   inline GLuint getHandle() const { return mStorage->getHandle(); }

   void dispose();
   bool mClearAtFrameEnd;
   
protected:
   friend class GFXGLDevice;
   
   StrongRefPtr<GFXGLVAO> mVAO;
   StrongRefPtr<GFXGLBufferStorage> mStorage;
};

#endif
