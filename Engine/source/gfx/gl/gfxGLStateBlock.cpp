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

#include "gfx/gl/gfxGLStateBlock.h"
#include "gfx/gl/gfxGLDevice.h"
#include "gfx/gl/gfxGLEnumTranslate.h"
#include "gfx/gl/gfxGLUtils.h"
#include "gfx/gl/gfxGLTextureObject.h"


GFXGLStateBlock::GFXGLStateBlock(const GFXStateBlockDesc& desc) :
   mDesc(desc),
   mCachedHashValue(desc.getHashValue())
{
}

GFXGLStateBlock::~GFXGLStateBlock()
{
}

/// Returns the hash value of the desc that created this block
U32 GFXGLStateBlock::getHashValue() const
{
   return mCachedHashValue;
}

/// Returns a GFXStateBlockDesc that this block represents
const GFXStateBlockDesc& GFXGLStateBlock::getDesc() const
{
   return mDesc;   
}

/// Called by OpenGL device to active this state block.
/// @param oldState  The current state, used to make sure we don't set redundant states on the device.  Pass NULL to reset all states.
void GFXGLStateBlock::activate(const GFXGLStateBlock* oldState)
{
#define STATE_CHANGE(state) (!oldState || oldState->mDesc.state != mDesc.state)
#define TOGGLE_STATE(state, enum) if(mDesc.state) glEnable(enum); else glDisable(enum)
#define CHECK_TOGGLE_STATE(state, enum) if(!oldState || oldState->mDesc.state != mDesc.state) if(mDesc.state) glEnable(enum); else glDisable(enum)

   // Blending
   // jamesu - note that if blend is disabled, we don't update anything else
   CHECK_TOGGLE_STATE(blendEnable, GL_BLEND);
   if (mDesc.blendEnable)
   {
      if(STATE_CHANGE(blendSrc) || STATE_CHANGE(blendDest))
         glBlendFunc(GFXGLBlend[mDesc.blendSrc], GFXGLBlend[mDesc.blendDest]);
      if(STATE_CHANGE(blendOp))
         glBlendEquation(GFXGLBlendOp[mDesc.blendOp]);
   }

   // Color write masks
   if(STATE_CHANGE(colorWriteRed) || STATE_CHANGE(colorWriteBlue) || STATE_CHANGE(colorWriteGreen) || STATE_CHANGE(colorWriteAlpha))
      glColorMask(mDesc.colorWriteRed, mDesc.colorWriteBlue, mDesc.colorWriteGreen, mDesc.colorWriteAlpha);
   
   // Culling
   if(STATE_CHANGE(cullMode))
   {
      TOGGLE_STATE(cullMode, GL_CULL_FACE);
      if (!oldState || oldState->mDesc.cullMode != mDesc.cullMode)
      {
         glCullFace(GFXGLCullMode[mDesc.cullMode]);
      }
   }

   // Depth
   CHECK_TOGGLE_STATE(zEnable, GL_DEPTH_TEST);
   
   //glDepthRange(0, 0.5);
   
   if(STATE_CHANGE(zFunc))
      glDepthFunc(GFXGLCmpFunc[mDesc.zFunc]);
   
   if(STATE_CHANGE(zBias))
   {
      if (mDesc.zBias == 0)
      {
         glDisable(GL_POLYGON_OFFSET_FILL);
      } else {
         F32 bias = mDesc.zBias * 10000.0f;
         glEnable(GL_POLYGON_OFFSET_FILL);
         glPolygonOffset(bias, bias);
      } 
   }
   
   if(STATE_CHANGE(zWriteEnable))
      glDepthMask(mDesc.zWriteEnable);

   // Stencil
   CHECK_TOGGLE_STATE(stencilEnable, GL_STENCIL_TEST);
   if(STATE_CHANGE(stencilFunc) || STATE_CHANGE(stencilRef) || STATE_CHANGE(stencilMask))
      glStencilFunc(GFXGLCmpFunc[mDesc.stencilFunc], mDesc.stencilRef, mDesc.stencilMask);
   if(STATE_CHANGE(stencilFailOp) || STATE_CHANGE(stencilZFailOp) || STATE_CHANGE(stencilPassOp))
      glStencilOp(GFXGLStencilOp[mDesc.stencilFailOp], GFXGLStencilOp[mDesc.stencilZFailOp], GFXGLStencilOp[mDesc.stencilPassOp]);
   if(STATE_CHANGE(stencilWriteMask))
      glStencilMask(mDesc.stencilWriteMask);

   if(STATE_CHANGE(fillMode))
      glPolygonMode(GL_FRONT_AND_BACK, GFXGLFillMode[mDesc.fillMode]);

#undef CHECK_STATE
#undef TOGGLE_STATE
#undef CHECK_TOGGLE_STATE
   
   if (!mDesc.samplersDefined)
   {
      return;
   }

   // Non per object texture mode states
   for (U32 i = 0; i < getMin(getOwningDevice()->getNumSamplers(), (U32) TEXTURE_STAGE_COUNT); i++)
   {
      GFXGLTextureObject* tex = static_cast<GFXGLTextureObject*>(getOwningDevice()->getCurrentTexture(i));
      const GFXSamplerStateDesc &ssd = mDesc.samplers[i];
	  if (!tex)
		  continue;
      
      // jamesu - no point updating if the texture is dirty
      if (getOwningDevice()->getCurrentTextureDirty(i))
         continue;

	  GFXSamplerStateDesc &texSSD = tex->getSamplerState();

	  #define SSF(state, enum, value, tex) if(!oldState || texSSD.state != mDesc.samplers[i].state) glTexParameteri(tex->getBinding(), enum, value)
	  #define SSW(state, enum, value, tex) if(!oldState || texSSD.state != mDesc.samplers[i].state) glTexParameteri(tex->getBinding(), enum, !tex->mIsNPoT2 ? value : GL_CLAMP_TO_EDGE)

	  // Have we modified any sampler states?
	  if (GFXSamplerStateDesc::isDirty(ssd, texSSD))
	  {
        static_cast<GFXGLDevice*>(getOwningDevice())->_setActiveTexture(GL_TEXTURE0 + i);
        
        if(!oldState || texSSD.minFilter != mDesc.samplers[i].minFilter)
        {
           //Con::warnf("TEXTURE %i [idx %i] changed from MIN_FILTER == %u to %u", tex->getHandle(), i, texSSD.minFilter , mDesc.samplers[i].minFilter);
        }
        
        if (!oldState || texSSD.minFilter != mDesc.samplers[i].minFilter || texSSD.mipFilter != mDesc.samplers[i].mipFilter)
        {
           glTexParameteri(tex->getBinding(), GL_TEXTURE_MIN_FILTER, minificationFilter(ssd.minFilter, ssd.mipFilter, tex->getMipLevels()));
        }
        
		  SSF(magFilter, GL_TEXTURE_MAG_FILTER, GFXGLTextureFilter[ssd.magFilter], tex);
		  SSW(addressModeU, GL_TEXTURE_WRAP_S, GFXGLTextureAddress[ssd.addressModeU], tex);
		  SSW(addressModeV, GL_TEXTURE_WRAP_T, GFXGLTextureAddress[ssd.addressModeV], tex);

		  if( ( !oldState || oldState->mDesc.samplers[i].maxAnisotropy != ssd.maxAnisotropy ) &&
			  static_cast< GFXGLDevice* >( GFX )->supportsAnisotropic() )
			  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, ssd.maxAnisotropy);

		  if( ( !oldState || oldState->mDesc.samplers[i].mipLODBias != ssd.mipLODBias ) )
			  glTexEnvf(GL_TEXTURE_FILTER_CONTROL_EXT, GL_TEXTURE_LOD_BIAS_EXT, ssd.mipLODBias);

		  tex->setSamplerState(ssd);
	  }
   }
#undef SSF
#undef SSW
}
