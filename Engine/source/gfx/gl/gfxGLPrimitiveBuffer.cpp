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

#include "gfx/gl/gfxGLDevice.h"
#include "gfx/gl/gfxGLPrimitiveBuffer.h"
#include "gfx/gl/gfxGLVertexBuffer.h"
#include "gfx/gl/gfxGLEnumTranslate.h"

#include "gfx/gl/tGL/tGL.h"
#include "gfx/gl/gfxGLUtils.h"

GFXGLPrimitiveBuffer::GFXGLPrimitiveBuffer(GFXDevice *device, U32 indexCount, U32 primitiveCount, GFXBufferType bufferType) :
GFXPrimitiveBuffer(device, indexCount, primitiveCount, bufferType)
{
   mStorage = mBufferType != GFXBufferTypeVolatile ? GFXGLBufferStorage::createBuffer(mDevice, GL_ELEMENT_ARRAY_BUFFER, bufferType, indexCount * sizeof(U16)) : NULL;
}

GFXGLPrimitiveBuffer::~GFXGLPrimitiveBuffer()
{
   mStorage = NULL;
}

void GFXGLPrimitiveBuffer::lock(U32 indexStart, U32 indexEnd, void **indexPtr)
{
   U32 indexCount = (indexStart == 0 && indexEnd == 0) ? mIndexCount : (indexEnd - indexStart);
   U32 lockStart = indexStart * sizeof(U16);

   if (mBufferType == GFXBufferTypeVolatile)
   {
      GFXGLPrimitiveBuffer *volatileBuffer = static_cast<GFXGLDevice*>(getOwningDevice())->findPBPool(indexCount);
      if( !volatileBuffer )
         volatileBuffer = static_cast<GFXGLDevice*>(getOwningDevice())->createPBPool();

      mStorage = volatileBuffer->mStorage;
      mVolatileStart = lockStart = mStorage->map(0, indexCount * sizeof(U16), indexPtr) / sizeof(U16);
      volatileBuffer->mIndexCount += indexCount;
   }
   else
   {
      mStorage->map(indexStart * sizeof(U16), indexCount * sizeof(U16), indexPtr);
   }
}

void GFXGLPrimitiveBuffer::unlock()
{
   mStorage->unmap();
}

void GFXGLPrimitiveBuffer::prepare()
{
   static_cast<GFXGLDevice*>(getOwningDevice())->_setWorkingIndexBuffer(mStorage->getHandle(), false);
}

void GFXGLPrimitiveBuffer::finish()
{
   static_cast<GFXGLDevice*>(getOwningDevice())->_setWorkingIndexBuffer(0, false);
}

void GFXGLPrimitiveBuffer::zombify()
{
}

void GFXGLPrimitiveBuffer::resurrect()
{
}

void GFXGLPrimitiveBuffer::onFenceMarked(U32 fenceId)
{
   if (mStorage)
   {
      mStorage->onFenceMarked(fenceId);
   }
}

void GFXGLPrimitiveBuffer::onFenceDone(U32 fenceId)
{
   if (mStorage)
   {
      mStorage->onFenceDone(fenceId);
   }
}

U32 GFXGLPrimitiveBuffer::getStorageIndicesFree()
{
   if (mBufferType == GFXBufferTypeVolatile)
   {
      if (mStorage)
      {
         return mStorage->getBytesFree() / sizeof(U16);
      }
      else
      {
         return MAX_DYNAMIC_VERTS;
      }
   }
   else
   {
      return mIndexCount;
   }
}

void GFXGLPrimitiveBuffer::dispose()
{
   if (mStorage)
   {
      mStorage->dispose();
   }
}