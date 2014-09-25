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

#include "platform/platform.h"
#include "gfx/gl/gfxGLVertexBuffer.h"

#include "gfx/gl/gfxGLDevice.h"
#include "gfx/gl/gfxGLEnumTranslate.h"
#include "gfx/gl/gfxGLUtils.h"
#include "gfx/gl/gfxGLVAO.h"

GFXGLVertexBuffer::GFXGLVertexBuffer(  GFXDevice *device, 
   U32 numVerts, 
   const GFXVertexFormat *vertexFormat, 
   U32 vertexSize, 
   GFXBufferType bufferType )
   :  GFXVertexBuffer( device, numVerts, vertexFormat, vertexSize, bufferType ), 
   mVAO(NULL)
{
   mStorage = mBufferType != GFXBufferTypeVolatile ? GFXGLBufferStorage::createBuffer(mDevice, GL_ARRAY_BUFFER, mBufferType, numVerts * vertexSize) : NULL;
   mClearAtFrameEnd = false;
}

GFXGLVertexBuffer::~GFXGLVertexBuffer()
{
   mStorage = NULL;
   mVAO = NULL;
}

void GFXGLVertexBuffer::lock( U32 vertexStart, U32 vertexEnd, void **vertexPtr )
{
   U32 vertexCount = (vertexStart == 0 && vertexEnd == 0) ? mNumVerts : (vertexEnd - vertexStart);
   U32 lockStart = vertexStart;

   if (mBufferType == GFXBufferTypeVolatile)
   {
      GFXGLVertexBuffer *volatileBuffer = static_cast<GFXGLDevice*>(getOwningDevice())->findVBPool(&mVertexFormat, vertexCount);
      if( !volatileBuffer )
         volatileBuffer = static_cast<GFXGLDevice*>(getOwningDevice())->createVBPool( &mVertexFormat, mVertexSize );

      mStorage = volatileBuffer->mStorage;
      lockStart = mVolatileStart = mStorage->map(0, vertexCount * mVertexSize, vertexPtr) / mVertexSize;
      volatileBuffer->mNumVerts += vertexCount;
   }
   else
   {
      mStorage->map(lockStart, vertexCount * mVertexSize, vertexPtr);
   }

   lockedVertexStart = lockStart;
   lockedVertexEnd   = lockedVertexStart + vertexCount;
}

void GFXGLVertexBuffer::unlock()
{
   mStorage->unmap();

   lockedVertexStart = 0;
   lockedVertexEnd   = 0;
}

void GFXGLVertexBuffer::prepare()
{
   static_cast<GFXGLDevice*>(getOwningDevice())->_setWorkingVertexBuffer(mStorage->getHandle(), false);
}

void GFXGLVertexBuffer::finish()
{
}

void GFXGLVertexBuffer::resurrect()
{
   if (mVAO == NULL)
   {
      if (mBufferType == GFXBufferTypeVolatile)
      {
         mVAO = static_cast<GFXGLDevice*>(getOwningDevice())->getRootVAO();
      }
      else
      {
         mVAO = static_cast<GFXGLDevice*>(getOwningDevice())->allocVAO(this);
      }
   }
}

void GFXGLVertexBuffer::zombify()
{
}

void GFXGLVertexBuffer::onFenceMarked(U32 fenceId)
{
   if (mStorage)
   {
      mStorage->onFenceMarked(fenceId);
   }
}

void GFXGLVertexBuffer::onFenceDone(U32 fenceId)
{
   if (mStorage)
   {
      mStorage->onFenceDone(fenceId);
   }
}

U32 GFXGLVertexBuffer::getStorageVertsFree()
{
   if (mBufferType == GFXBufferTypeVolatile)
   {
      if (mStorage)
      {
         return mStorage->getBytesFree() / mVertexSize;
      }
      else
      {
         return MAX_DYNAMIC_VERTS;
      }
   }
   else
   {
      return mNumVerts;
   }
}

void GFXGLVertexBuffer::dispose()
{
   if (mStorage)
   {
      mStorage->dispose();
   }
}