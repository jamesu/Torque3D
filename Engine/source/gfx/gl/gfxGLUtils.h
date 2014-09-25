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

#ifndef TORQUE_GFX_GL_GFXGLUTILS_H_
#define TORQUE_GFX_GL_GFXGLUTILS_H_

#include "core/util/preprocessorHelpers.h"
#include "gfx/gl/gfxGLEnumTranslate.h"

inline U32 getMaxMipmaps(U32 width, U32 height, U32 depth)
{
   // replicate D3D behaviour
   if (!isPow2(width) && !isPow2(height))
   {
      return 1;
   }
   
   return getMax( getBinLog2(depth), getMax(getBinLog2(width), getBinLog2(height)));
}

inline GLenum minificationFilter(U32 minFilter, U32 mipFilter, U32 mipLevels)
{
   if(mipLevels == 1)
      return GFXGLTextureFilter[minFilter];

   // the compiler should interpret this as array lookups
   switch( minFilter ) 
   {
      case GFXTextureFilterLinear:
         switch( mipFilter ) 
         {
         case GFXTextureFilterLinear:
            return GL_LINEAR_MIPMAP_LINEAR;
         case GFXTextureFilterPoint:
            return GL_LINEAR_MIPMAP_NEAREST;
         default: 
            return GL_LINEAR;
         }
      default:
         switch( mipFilter ) {
      case GFXTextureFilterLinear:
         return GL_NEAREST_MIPMAP_LINEAR;
      case GFXTextureFilterPoint:
         return GL_NEAREST_MIPMAP_NEAREST;
      default:
         return GL_NEAREST;
         }
   }
}


// Check if format is compressed format.
// Even though dxt2/4 are not supported, they are included because they are a compressed format.
// Assert checks on supported formats are done elsewhere.
inline bool isCompressedFormat( GFXFormat format )
{
   bool compressed = false;
   if(format == GFXFormatDXT1 || format == GFXFormatDXT2
		|| format == GFXFormatDXT3
		|| format == GFXFormatDXT4
		|| format == GFXFormatDXT5 )
   {
      compressed = true;
   }
	
   return compressed;
}

//Get the surface size of a compressed mip map level - see ddsLoader.cpp
inline U32 getCompressedSurfaceSize(GFXFormat format,U32 width, U32 height, U32 mipLevel=0 )
{
   if(!isCompressedFormat(format))
      return 0;
	
   // Bump by the mip level.
   height = getMax(U32(1), height >> mipLevel);
   width = getMax(U32(1), width >> mipLevel);
	
   U32 sizeMultiple = 0;
   if(format == GFXFormatDXT1)
      sizeMultiple = 8;
   else
      sizeMultiple = 16;
	
   return getMax(U32(1), width/4) * getMax(U32(1), height/4) * sizeMultiple;
}

/// Simple class which preserves a given GL integer.
/// This class determines the integer to preserve on construction and restores 
/// it on destruction.
class GFXGLPreserveInteger
{
public:
   typedef void(STDCALL *BindFn)(GLenum, GLuint);

   /// Preserve the integer.
   /// @param binding The binding which should be set on destruction.
   /// @param getBinding The parameter to be passed to glGetIntegerv to determine
   /// the integer to be preserved.
   /// @param binder The gl function to call to restore the integer.
   GFXGLPreserveInteger(GLenum binding, GLint getBinding, BindFn binder) :
      mBinding(binding), mPreserved(0), mBinder(binder)
   {
      AssertFatal(mBinder, "GFXGLPreserveInteger - Need a valid binder function");
      glGetIntegerv(getBinding, &mPreserved);
   }
   
   /// Restores the integer.
   ~GFXGLPreserveInteger()
   {
      mBinder(mBinding, mPreserved);
   }

private:
   GLenum mBinding;
   GLint mPreserved;
   BindFn mBinder;
};

// Handy macro for checking the status of a framebuffer.  Framebuffers can fail in
// all sorts of interesting ways, these are just the most common.  Further, no existing GL profiling
// tool catches framebuffer errors when the framebuffer is created, so we actually need this.
#define CHECK_FRAMEBUFFER_STATUS()\
{\
GLenum status;\
status = glCheckFramebufferStatus(GL_FRAMEBUFFER);\
switch(status) {\
case GL_FRAMEBUFFER_COMPLETE:\
break;\
case GL_FRAMEBUFFER_UNSUPPORTED:\
AssertFatal(false, "Unsupported FBO");\
break;\
case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT:\
AssertFatal(false, "Incomplete FBO Attachment");\
break;\
case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT:\
AssertFatal(false, "Incomplete FBO Missing Attachment");\
break;\
case GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER:\
AssertFatal(false, "Incomplete FBO Draw buffer");\
break;\
case GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER:\
AssertFatal(false, "Incomplete FBO Read buffer");\
break;\
default:\
/* programming error; will fail on all hardware */\
AssertFatal(false, "Something really bad happened with an FBO");\
}\
}

class GFXGLVertexDecl : public GFXVertexDecl
{
public:
   typedef struct Element
   {
      GLint attrIndex;
      GLint elementCount; // 1 - 4
      GLenum type; // GL_FLOAT...
      GLboolean normalized;
      GLsizei stride;
      GLvoid *pointerFirst;
      U8 streamIdx;
   } Vertex;
   
   GFXGLVertexDecl() {;}
   virtual ~GFXGLVertexDecl() {;}
   
   Vector<Vertex> mElements;
};

#endif

#ifdef TORQUE_DEBUG_EXTRA

inline void CheckAgainstCode(int code, int glCode, const char* errorName)
{
  if (code == glCode)
  {
    //AssertFatal(0, errorName);
  }
}

#define CHECK_AGAINST_CODE(code, glError) CheckAgainstCode(code, glError, #glError)

inline void CheckGLError()
{
  GLenum errorCode = glGetError();

  CHECK_AGAINST_CODE(errorCode, GL_INVALID_VALUE);
  CHECK_AGAINST_CODE(errorCode, GL_INVALID_ENUM);
  CHECK_AGAINST_CODE(errorCode, GL_INVALID_OPERATION);
  CHECK_AGAINST_CODE(errorCode, GL_STACK_OVERFLOW);
  CHECK_AGAINST_CODE(errorCode, GL_STACK_UNDERFLOW);
  CHECK_AGAINST_CODE(errorCode, GL_OUT_OF_MEMORY);
}

#define CHECK_GL_ERROR() CheckGLError()

#else

#define CHECK_GL_ERROR()

#endif

#include "gfx/screenshot.h"

class ScreenShotGL : public ScreenShot
{
protected:
   
   GBitmap* _captureBackBuffer();
   
};
