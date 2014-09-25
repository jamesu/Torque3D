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


#ifndef __brutal_Bundle__gfxGLVAO__
#define __brutal_Bundle__gfxGLVAO__

#ifndef _GFXVERTEXBUFFER_H_
#include "gfx/gfxVertexBuffer.h"
#endif

#ifndef _GFXGLPRIMITIVEBUFFER_H_
#include "gfx/gl/gfxGLPrimitiveBuffer.h"
#endif

/*
 e.g.
 
 GFXGLVAO vao;
 
 vao.setVertexDecl(decl);
 vao.setVertexStream(0, vertBuffer);
 vao.setPrimitiveBuffer(primBuffer);
 vao.updateState();
 
 */

class GFXGLVAO : public StrongRefBase, public GFXResource
{
public:
   
   GLuint mVAOId;
   
   GLuint mCurrentVertexBuffer[GFXDevice::VERTEX_STREAM_COUNT];
   GLuint mCurrentVertexBufferOffset[GFXDevice::VERTEX_STREAM_COUNT];
   U32 mCurrentVB_Divisor[GFXDevice::VERTEX_STREAM_COUNT];
   bool mVertexBufferDirty[GFXDevice::VERTEX_STREAM_COUNT];
   
   bool mDivisorDirty[GFXDevice::VERTEX_STREAM_COUNT];
   
   GLuint mCurrentPrimitiveBuffer;
   bool mPrimitiveBufferDirty;
   
   const GFXGLVertexDecl *mCurrentVertexDecl;
   bool mVertexDeclDirty;
   
   BitSet32 mVertexAttributesSet;   // attributes which are active
   
   bool mVertexAttributesDirty[GFXGLDevice::GL_NumVertexAttributes];
   
   GFXGLVAO() : mVAOId(0)
   {
      for (int i=0; i<GFXDevice::VERTEX_STREAM_COUNT; i++)
      {
         mCurrentVertexBuffer[i] = 0;
         mCurrentVB_Divisor[i] = 0;
         mCurrentVertexBufferOffset[i] = 0;
         mDivisorDirty[i] = false;
      }
      mCurrentPrimitiveBuffer = 0;
      mCurrentVertexDecl = 0;
      dMemset(mVertexBufferDirty, 0xFF, sizeof(mVertexBufferDirty));
      mPrimitiveBufferDirty = true;
      mVertexDeclDirty = true;
      mVertexAttributesSet.clear();
   }
   
   ~GFXGLVAO()
   {
      //zombify();
      
      if (mVAOId != 0)
      {
         if (gglHasExtension(APPLE_vertex_array_object)) glDeleteVertexArraysAPPLE(1, &mVAOId);
         else if (gglHasExtension(ARB_vertex_array_object)) glDeleteVertexArrays(1, &mVAOId);
      }
   }
   
   GLuint getHandle()
   {
      return mVAOId;
   }
   
   virtual const String describeSelf() const
   {
      return "VAO";
   }
   
   virtual void zombify()
   {
      //Con::printf("!!VAO<%x> %u ZOMBIFY\n", this, getHandle());
      /*
      if (mVAOId != 0)
      {
         if (gglHasExtension(APPLE_vertex_array_object)) glDeleteVertexArraysAPPLE(1, &mVAOId);
         else if (gglHasExtension(ARB_vertex_array_object)) glDeleteVertexArrays(1, &mVAOId);
      }
      mVAOId = 0;
      
      for (int i=0; i<GFXDevice::VERTEX_STREAM_COUNT; i++)
      {
         mCurrentVertexBuffer[i] = 0;
         mCurrentVB_Divisor[i] = 0;
         mCurrentVertexBufferOffset[i] = 0;
         mDivisorDirty[i] = true;
      }
      mCurrentPrimitiveBuffer = 0;
      mCurrentVertexDecl = 0;
      dMemset(mVertexBufferDirty, 0xFF, sizeof(mVertexBufferDirty));
      mPrimitiveBufferDirty = true;
      mVertexDeclDirty = true;
      mVertexAttributesSet.clear();*/
   }
   
   virtual void resurrect()
   {
      if (mVAOId == 0)
      {
         if (gglHasExtension(APPLE_vertex_array_object))
         {
            glGenVertexArraysAPPLE(1, &mVAOId);
         }
         else if (gglHasExtension(ARB_vertex_array_object))
         {
             glGenVertexArrays(1, &mVAOId);
         }
      }
      
      //Con::printf("!!VAO<%x> %u RESURRECT VBO == %u\n", this, getHandle(), mCurrentVertexBufferId);
   }
   
   virtual void setVertexDecl( const GFXGLVertexDecl *decl )
   {
      if (mCurrentVertexDecl != decl)
      {
         mCurrentVertexDecl = decl;
         mVertexDeclDirty = true;
      }
   }
   
   virtual void setVertexStream( U32 stream, GLuint buffer, U32 offset )
   {
      if (mCurrentVertexBuffer[stream] != buffer || mCurrentVertexBufferOffset[stream] != offset)
      {
         mCurrentVertexBuffer[stream] = buffer;
         mCurrentVertexBufferOffset[stream] = offset;
         mVertexBufferDirty[stream] = true;
      }
   }
   
   virtual void setVertexStreamFrequency( U32 stream, U32 frequency )
   {
      if (stream == 0)
         return;

      if (mCurrentVB_Divisor[stream] != frequency)
      {
         mCurrentVB_Divisor[stream] = frequency; // non instanced, is vertex buffer
         mDivisorDirty[stream] = true;
      }
   }
	
	virtual void setPrimitiveBuffer( GLuint newBuffer )
   {
      if (mCurrentPrimitiveBuffer != newBuffer)
      {
         mCurrentPrimitiveBuffer = newBuffer;
         mPrimitiveBufferDirty = true;
      }
   }
   
   void updateState(bool force=false)
   {
      {
         // Now update state
         // TODO: multiple streams
         bool declDirty = (mVertexBufferDirty[0] || mVertexBufferDirty[1] || mDivisorDirty[0] || mDivisorDirty[1] || mVertexDeclDirty || force);
         
         if (declDirty)
         {
            bool enabledAttributes[GFXGLDevice::GL_NumVertexAttributes];
            dMemset(enabledAttributes, '\0', sizeof(enabledAttributes));
            
            if (mCurrentVertexDecl)
            {
               // Loop thru the vertex format elements adding the array state...
               for ( U32 i=0; i < mCurrentVertexDecl->mElements.size(); i++ )
               {
                  const GFXGLVertexDecl::Element &e = mCurrentVertexDecl->mElements[i];
                  
                  if (!mVertexAttributesSet.test(1 << e.attrIndex) || force)
                  {
                     glEnableVertexAttribArray(e.attrIndex);
                     enabledAttributes[e.attrIndex] = true;
                     mVertexAttributesSet.set(1 << e.attrIndex);
                  }
                  
                  enabledAttributes[e.attrIndex] = true;
                  
                  //Con::printf("!!VAO[%u] set buffer to %u", getHandle(),  mCurrentVertexBuffer[e.streamIdx]);
                  static_cast<GFXGLDevice*>(getOwningDevice())->_setWorkingVertexBuffer(mCurrentVertexBuffer[e.streamIdx]);

                  U32 streamOffset = mCurrentVertexBufferOffset[e.streamIdx];

                  glVertexAttribPointer(
                                        e.attrIndex,                         // attribute
                                        e.elementCount,                      // number of elements per vertex, here (r,g,b)
                                        e.type,                              // the type of each element
                                        e.normalized,                        // take our values as-is
                                        mCurrentVertexDecl->mElements[i].stride,// e.stride,                            // no extra data between each position
                                        (U8*)e.pointerFirst + streamOffset   // offset of first element
                                        );

                  //Con::printf("  Stream[%u:%u] buf == %u offset == %u divisor [%u]", e.attrIndex, e.streamIdx, mCurrentVertexBuffer[e.streamIdx], (U8*)e.pointerFirst + streamOffset, mCurrentVB_Divisor[e.streamIdx]);
                  
                  if (glVertexAttribDivisor && (mCurrentVB_Divisor[e.streamIdx] != 0 || mDivisorDirty[e.streamIdx]))
                  {
                     glVertexAttribDivisor( e.attrIndex, mCurrentVB_Divisor[e.streamIdx] );
                  }
                  else if (gglHasExtension(ARB_instanced_arrays) && (mCurrentVB_Divisor[e.streamIdx] != 0  || mDivisorDirty[e.streamIdx]))
                  {
                     glVertexAttribDivisorARB( e.attrIndex, mCurrentVB_Divisor[e.streamIdx] );
                  }
               }
            }

            dMemset(mDivisorDirty, '\0', sizeof(mDivisorDirty));
         
            // Disable unused attributes
            for (int i=0; i<GFXGLDevice::GL_NumVertexAttributes; i++)
            {
               if (!enabledAttributes[i] && (mVertexAttributesSet.test(i<<i) || force))
               {
                  glDisableVertexAttribArray(i);
                  mVertexAttributesSet.clear(1 << i);
               }
            }
            
            mVertexDeclDirty = false;
            mVertexBufferDirty[0] = mVertexBufferDirty[1] = false;
         }
         
         // Bind correct primitive buffer
         if (mPrimitiveBufferDirty || force)
         {
            //Con::printf("!!VAO[%u] set primBuf to %u", getHandle(), mCurrentPrimitiveBuffer);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mCurrentPrimitiveBuffer);
            mPrimitiveBufferDirty = false;
         }
      }
   }
};

#endif /* defined(__brutal_Bundle__gfxGLVAO__) */
