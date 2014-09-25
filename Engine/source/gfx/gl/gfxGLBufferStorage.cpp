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

#include "platform/platform.h"
#include "gfx/gl/gfxGLBufferStorage.h"

#include "gfx/gl/gfxGLDevice.h"
#include "gfx/gl/gfxGLEnumTranslate.h"
#include "gfx/gl/gfxGLUtils.h"
#include "gfx/gl/gfxGLVAO.h"

#define FENCE_POINTS 4
#define FENCE_FOR_POS(size, pos) (((pos) * FENCE_POINTS) / size)

#define QUICKSET_GL_BUFFER(binding, buffer) if (binding == GL_ELEMENT_ARRAY_BUFFER) static_cast<GFXGLDevice*>(getOwningDevice())->_setWorkingIndexBuffer(mBuffer); else static_cast<GFXGLDevice*>(getOwningDevice())->_setWorkingVertexBuffer(mBuffer);

#ifdef TORQUE_DEBUG
#define QUICKCHECK_GL_BUFFER(binding, buffer) { GLint tmp; glGetIntegerv(binding == GL_ELEMENT_ARRAY_BUFFER ? GL_ELEMENT_ARRAY_BUFFER_BINDING : GL_ARRAY_BUFFER_BINDING, &tmp); AssertFatal(tmp == buffer, avar("Incorrect buffer bound (%u vs %u)", tmp, buffer)); }
#else
#define QUICKCHECK_GL_BUFFER(binding, buffer)
#endif

bool GFXGLBufferStorage::smAllowBufferStorage = true;
bool GFXGLBufferStorage::smAllowPinnedMemory = true;
bool GFXGLBufferStorage::smAllowArbSync = true;

GFXGLBufferStorage::GFXGLBufferStorage(U32 initialSize,
   GLuint type,
   GLuint usageHint)
{
   mSize = initialSize;
   mBinding = type;
   mUsageHint = usageHint;

   mBuffer = 0;
   mZombieCache = 0;
}

GFXGLBufferStorage::~GFXGLBufferStorage()
{
   cleanup();
}

void GFXGLBufferStorage::cleanup()
{
   if (mBuffer == 0)
      return;

   QUICKSET_GL_BUFFER(mBinding, 0);
   glDeleteBuffers(1, &mBuffer);

   cleanupVAOs();

   mBuffer = 0;
   
   if (mZombieCache)
   {
      delete[] mZombieCache;
      mZombieCache = NULL;
   }
}

void GFXGLBufferStorage::zombify()
{
   if (mBuffer == 0)
      return;
   //Con::printf("Buf[%x] DEL %u", this, mBuffer);

   mZombieCache = new U8[mSize];

   QUICKSET_GL_BUFFER(mBinding, mBuffer);
   glGetBufferSubData(mBinding, 0, mSize, mZombieCache);

   QUICKSET_GL_BUFFER(mBinding, 0);
   glDeleteBuffers(1, &mBuffer);

   cleanupVAOs();

   mBuffer = 0;
} 

void GFXGLBufferStorage::resurrect()
{
   if (mBuffer == 0)
   {
      glGenBuffers(1, &mBuffer);
      //Con::printf("Buf[%x] GEN %u", this, mBuffer);
   }
   
   if( gglHasExtension(EXT_direct_state_access) )
   {
      glNamedBufferDataEXT(mBuffer, mSize, mZombieCache, mUsageHint);
   }
   else
   {
      QUICKSET_GL_BUFFER(mBinding, mBuffer);
      glBufferData(mBinding, mSize, mZombieCache, mUsageHint);
   }
   
   if (mZombieCache)
      delete[] mZombieCache;
   mZombieCache = NULL;
}

void GFXGLBufferStorage::cleanupVAOs()
{
   // Unset buffer if we have the root vao active
   GFXGLVAO *rootVAO = static_cast<GFXGLDevice*>(getOwningDevice())->getRootVAO();
   GLuint currVBO = static_cast<GFXGLDevice*>(getOwningDevice())->_getWorkingVertexBuffer();

   if (mBinding == GL_ARRAY_BUFFER)
   {
      if (currVBO == mBuffer)
      {
         static_cast<GFXGLDevice*>(getOwningDevice())->_setWorkingVertexBuffer(0);
      }
   }
   else
   {
      if (rootVAO && rootVAO->mCurrentPrimitiveBuffer == mBuffer)
         rootVAO->mCurrentPrimitiveBuffer = 0;
   }
}

// Implementations of various buffers
class GFXGLStaticSubBuffer : public GFXGLBufferStorage
{
public:
   GFXGLStaticSubBuffer(U32 initialSize, GLuint type, GLuint usageHint) : GFXGLBufferStorage(initialSize, type, usageHint) { }

   U32 map( U32 start, U32 count, void **vertexPtr )
   {
      mAllocatorMark = FrameAllocator::getWaterMark();
      *vertexPtr = FrameAllocator::alloc(count);
      
      // Orphan buffer if we're setting the whole thing
      if (start == 0 && count == mSize)
      {
         QUICKSET_GL_BUFFER(mBinding, mBuffer);
         glBufferData(mBinding, count, NULL, mUsageHint);
      }

      mLockedOffset = start;
      mLockedSize   = count;

      return mLockedOffset;
   }

   void unmap()
   {
      if (mAllocatorMark == -1)
         return;

      if (mLockedSize == mSize)
      {
         QUICKCHECK_GL_BUFFER(mBinding, mBuffer);
         glBufferData(mBinding, mLockedSize, FrameAllocator::getWaterMarkPtr(mAllocatorMark), mUsageHint);
      }
      else
      {
         QUICKSET_GL_BUFFER(mBinding, mBuffer);
         glBufferSubData(mBinding, mLockedOffset, mLockedSize, FrameAllocator::getWaterMarkPtr(mAllocatorMark));
      }

      FrameAllocator::setWaterMark(mAllocatorMark);

      mAllocatorMark = -1;
      mLockedSize    = 0;
      mLockedOffset  = 0;
   }
};

class GFXGLStaticMappedOrphanBuffer : public GFXGLBufferStorage
{
public:
   GFXGLStaticMappedOrphanBuffer(U32 initialSize, GLuint type, GLuint usageHint) : GFXGLBufferStorage(initialSize, type, usageHint) { }

   U32 map( U32 start, U32 count, void **vertexPtr )
   {
      QUICKSET_GL_BUFFER(mBinding, mBuffer);

      *vertexPtr = (U8*)glMapBuffer(mBinding, GL_WRITE_ONLY) + start;

      mLockedOffset = start;
      mLockedSize   = count;

      return mLockedOffset;
   }

   void unmap()
   {
      QUICKCHECK_GL_BUFFER(mBinding, mBuffer);
      glUnmapBuffer(mBinding);

      mLockedSize   = 0;
      mLockedOffset = 0;
   }
};

// NOTE: streaming buffers have a limitation: start position needs to be 0

class GFXGLStreamingMappedOrphanBuffer : public GFXGLBufferStorage
{
public:
   GFXGLStreamingMappedOrphanBuffer(U32 initialSize, GLuint type, GLuint usageHint) : GFXGLBufferStorage(initialSize, type, usageHint), mIterator(0), mNeedDispose(false) { }

   U32 mIterator;
   bool mNeedDispose;

   void dispose()
   {
      mNeedDispose = true;
      mIterator = 0;
   }

   U32 map( U32 start, U32 count, void **vertexPtr )
   {
      QUICKSET_GL_BUFFER(mBinding, mBuffer);

      // Orphan if applicable
      if (mNeedDispose)
      {
         glBufferData(mBinding, mSize, NULL, mUsageHint);
         mIterator = 0;
         mNeedDispose = false;
      }

      if (mIterator + count > mSize)
      {
         AssertFatal(false, "GFXGLStreamingMappedRangeOrphanBuffer ran out of verts");
      }

      *vertexPtr = (U8*)glMapBuffer(mBinding, GL_WRITE_ONLY) + mIterator;

      mLockedOffset = mIterator;
      mLockedSize   = count;
      mIterator    += count;

      return mLockedOffset;
   }

   void unmap()
   {
      QUICKCHECK_GL_BUFFER(mBinding, mBuffer);
      glUnmapBuffer(mBinding);

      mLockedSize   = 0;
      mLockedOffset = 0;
   }

   U32 getBytesFree()
   {
      return mSize - mIterator;
   }
};

class GFXGLStreamingMappedRangeOrphanBuffer : public GFXGLBufferStorage
{
public:
   GFXGLStreamingMappedRangeOrphanBuffer(U32 initialSize, GLuint type, GLuint usageHint) : GFXGLBufferStorage(initialSize, type, usageHint), mIterator(0), mNeedDispose(false) { }

   U32 mIterator;
   bool mNeedDispose;

   void dispose()
   {
      mNeedDispose = true;
      mIterator = 0;
   }

   U32 map( U32 start, U32 count, void **vertexPtr)
   {
      QUICKSET_GL_BUFFER(mBinding, mBuffer);

      // Orphan if applicable
      if (mNeedDispose)
      {
         glBufferData(mBinding, mSize, NULL, mUsageHint);
         mIterator = 0;
         mNeedDispose = false;
      }

      if (mIterator + count > mSize)
      {
         AssertFatal(false, "GFXGLStreamingMappedRangeOrphanBuffer ran out of verts");
      }

      *vertexPtr = (U8*)glMapBufferRange(mBinding, mIterator, count, GL_MAP_WRITE_BIT | GL_MAP_FLUSH_EXPLICIT_BIT | GL_MAP_UNSYNCHRONIZED_BIT);

      mLockedOffset = mIterator;
      mLockedSize   = count;
      mIterator    += count;

      return mLockedOffset;
   }

   void unmap()
   {
      QUICKCHECK_GL_BUFFER(mBinding, mBuffer);

      glFlushMappedBufferRange(mBinding, 0, mLockedSize); 
      glUnmapBuffer(mBinding);

      mLockedSize   = 0;
      mLockedOffset = 0;
  }

   U32 getBytesFree()
   {
      return mSize - mIterator;
   }
};


class GFXGLStreamingPinnedMemoryBuffer : public GFXGLBufferStorage
{
public:
   GFXGLStreamingPinnedMemoryBuffer(U32 initialSize, GLuint type, GLuint usageHint) : GFXGLBufferStorage(initialSize, type, usageHint), mIterator(0) { }

   /// For streaming buffers
   U32 mFencePos[GFX_MAX_FORWARD_FRAMES];
   U32 mIterator;
   U32 mIteratorEnd;

   virtual ~GFXGLStreamingPinnedMemoryBuffer()
   {
      cleanup();
   }

   void cleanup()
   {
      if (mBuffer != 0)
      {
          cleanupVAOs();

          QUICKSET_GL_BUFFER(mBinding, 0);
          glDeleteBuffers(1, &mBuffer);
          mBuffer = 0;
      }

      dMemset(mFencePos, '\0', sizeof(mFencePos));
      mIteratorEnd = 0;

      if (mZombieCache)
      {
         dFree_aligned(mZombieCache);
         mZombieCache = NULL;
      }
   } 

   void resurrect()
   {
      if (mBuffer == 0)
      {
         glGenBuffers(1, &mBuffer);
      }

      if (mZombieCache == NULL)
      {
         mZombieCache = (U8*)dMalloc_aligned(mSize, 4096);
      }

      dMemset(mFencePos, '\0', sizeof(mFencePos));
      mIteratorEnd = 0;

      glBindBuffer(GL_EXTERNAL_VIRTUAL_MEMORY_BUFFER_AMD, mBuffer);
		glBufferData(GL_EXTERNAL_VIRTUAL_MEMORY_BUFFER_AMD, mSize, mZombieCache, GL_STREAM_COPY);
		glBindBuffer(GL_EXTERNAL_VIRTUAL_MEMORY_BUFFER_AMD, 0);
   }

   U32 map( U32 start, U32 count, void **vertexPtr)
   {
      if (mIterator + count > mSize)
      {
         AssertFatal(mIteratorEnd >= count, "Wrapover but no space left!");
         mIterator = 0;
      }

      *vertexPtr = mZombieCache + mIterator;

      mLockedOffset = mIterator;
      mLockedSize   = count;
      mIterator    += count;

      return mLockedOffset;
   }

   void unmap()
   {
      mLockedSize   = 0;
      mLockedOffset = 0;
   }

   void onFenceMarked(U32 fenceId)
   {
      mFencePos[fenceId] = mIterator;
   }

   void onFenceDone(U32 fenceId)
   {
      mIteratorEnd = mFencePos[fenceId];
   }

   U32 getBytesFree()
   {
      return getMax((S32)mIteratorEnd, (S32)mSize - (S32)mIterator);
   }
};


class GFXGLStreamingStorageBuffer : public GFXGLBufferStorage
{
public:
   GFXGLStreamingStorageBuffer(U32 initialSize, GLuint type, GLuint usageHint) : GFXGLBufferStorage(initialSize, type, usageHint), mIterator(0), mIteratorEnd(0) { }

   /// For streaming buffers
   U32 mFencePos[GFX_MAX_FORWARD_FRAMES];
   U32 mIterator;
   U32 mIteratorEnd;

   virtual ~GFXGLStreamingStorageBuffer()
   {
      cleanup();
   }

   void cleanup()
   {
      if (mZombieCache)
      {
         QUICKSET_GL_BUFFER(mBinding, mBuffer);
         glUnmapBuffer(mBinding);
         mZombieCache = NULL;
      }

      GFXGLBufferStorage::cleanup();
   } 

   void resurrect()
   {
      if (mBuffer == 0)
      {
         glGenBuffers(1, &mBuffer);
      }

      QUICKSET_GL_BUFFER(mBinding, mBuffer);
      dMemset(mFencePos, '\0', sizeof(mFencePos));
      mIteratorEnd = 0;

      glBufferStorage(mBinding, mSize, NULL, GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT);
      mZombieCache = (U8*)glMapBufferRange(mBinding, 0, mSize, GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT);
   }

   U32 map( U32 start, U32 count, void **vertexPtr)
   {
      if (mIterator + count > mSize)
      {
         AssertFatal(mIteratorEnd >= count, "Wrapover but no space left!");
         mIterator = 0;
      }

      *vertexPtr = mZombieCache + mIterator;

      mLockedOffset = mIterator;
      mLockedSize   = count;
      mIterator    += count;

      return mLockedOffset;
   }

   void unmap()
   {
      mLockedSize   = 0;
      mLockedOffset = 0;
   }

   void onFenceMarked(U32 fenceId)
   {
      mFencePos[fenceId] = mIterator;
   }

   void onFenceDone(U32 fenceId)
   {
      mIteratorEnd = mFencePos[fenceId];
   }

   U32 getBytesFree()
   {
      return getMax((S32)mIteratorEnd, (S32)mSize - (S32)mIterator);
   }
};

class GFXGLStreamingMappedRangeOrphanBufferApple : public GFXGLBufferStorage
{
public:
   GFXGLStreamingMappedRangeOrphanBufferApple(U32 initialSize, GLuint type, GLuint usageHint) : GFXGLBufferStorage(initialSize, type, usageHint), mIterator(0), mNeedDispose(false) { }
   
   U32 mIterator;
   bool mNeedDispose;

   void dispose()
   {
      mNeedDispose = true;
      mIterator = 0;
   }
   
   U32 map( U32 start, U32 count, void **vertexPtr)
   {
      QUICKSET_GL_BUFFER(mBinding, mBuffer);
      
      // Orphan if applicable
      if (mNeedDispose)
      {
         glBufferData(mBinding, mSize, NULL, mUsageHint);
         mIterator = 0;
         mNeedDispose = false;
      }

      // Orphan if applicable
      if (mIterator + count > mSize)
      {
         AssertFatal(false, "GFXGLStreamingMappedRangeOrphanBufferApple ran out of verts");
      }
      
      *vertexPtr = (U8*)glMapBuffer(mBinding, GL_WRITE_ONLY) + mIterator;
      
      mLockedOffset = mIterator;
      mLockedSize   = count;
      mIterator    += count;
      
      return mLockedOffset;
   }
   
   void unmap()
   {
      QUICKCHECK_GL_BUFFER(mBinding, mBuffer);
      
      glFlushMappedBufferRangeAPPLE(mBinding, mLockedOffset, mLockedSize);
      glUnmapBuffer(mBinding);
      
      mLockedSize   = 0;
      mLockedOffset = 0;
   }
   
   virtual void resurrect()
   {
      GFXGLBufferStorage::resurrect();
      glBufferParameteriAPPLE(GL_ARRAY_BUFFER, GL_BUFFER_FLUSHING_UNMAP_APPLE, GL_FALSE);
      glBufferParameteriAPPLE(GL_ARRAY_BUFFER, GL_BUFFER_SERIALIZED_MODIFY_APPLE, GL_FALSE);
   }
   
   U32 getBytesFree()
   {
      return mSize - mIterator;
   }
};

GFXGLBufferStorage* GFXGLBufferStorage::createBuffer(GFXDevice *device, GLuint type, GFXBufferType usageHint, U32 initialSize)
{
   GFXGLBufferStorage *storage = NULL;
   
   if (usageHint == GFXBufferTypeVolatile)
   {
      if (smAllowBufferStorage && glBufferStorage)//gglHasExtension(ARB_buffer_storage))
         storage = new GFXGLStreamingStorageBuffer(initialSize, type, GL_DYNAMIC_DRAW);
      else if (smAllowPinnedMemory)
         storage = new GFXGLStreamingPinnedMemoryBuffer(initialSize, type, GL_DYNAMIC_DRAW);
      else if (smAllowArbSync && glMapBufferRange)
         storage = new GFXGLStreamingMappedRangeOrphanBuffer(initialSize, type, GL_DYNAMIC_DRAW);
      else if (glFlushMappedBufferRangeAPPLE)
         storage = new GFXGLStreamingMappedRangeOrphanBufferApple(initialSize, type, GL_DYNAMIC_DRAW);
      else
         storage = new GFXGLStreamingMappedOrphanBuffer(initialSize,  type, GFXGLBufferType[usageHint]);
   }
   else
   {
      storage = new GFXGLStaticSubBuffer(initialSize,  type, GFXGLBufferType[usageHint]);
   }
   
   storage->registerResourceWithDevice(device);
   storage->resurrect();

   return storage;
}
