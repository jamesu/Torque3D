//-----------------------------------------------------------------------------
// Copyright (c) 2012 GarageGames, LLC
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

#ifndef _VRPostEffect_H_
#define _VRPostEffect_H_

#include "postFx/postEffect.h"

class VRPostEffect : public PostEffect
{
   typedef PostEffect Parent;

protected:

   // Oculus VR HMD index to reference
   S32 mHMDIndex;

   // Oculus VR sensor index to reference
   S32 mSensorIndex;

protected:
   virtual void _setupConstants( const SceneRenderState *state );

public:
   VRPostEffect();
   virtual ~VRPostEffect();

   DECLARE_CONOBJECT(VRPostEffect);

   // SimObject
   virtual bool onAdd();
   virtual void onRemove();
   static void initPersistFields();

   virtual void process(   const SceneRenderState *state, 
                           GFXTexHandle &inOutTex,
                           const RectI *inTexViewport = NULL );
};

#endif   // _VRPostEffect_H_
