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

#include "./../../../gl/torque.glsl"
#include "shadergen:/autogenConditioners.h"

// These are set by the game engine.  
// The render target size is one-quarter the scene rendering size.  
uniform sampler2D colorSampler;  
uniform sampler2D depthSampler;  
uniform vec2 dofEqWorld;  
uniform float depthOffset;
uniform vec2 targetSize;
uniform float maxWorldCoC;
uniform vec2 oneOverTargetSize;
uniform vec2 texSize0;

// Inputs
varying vec4 position;
varying vec2 tcColor0;
varying vec2 tcDepth0;
varying vec2 tcDepth1;
varying vec2 tcDepth2;
varying vec2 tcDepth3;


void main()
{
   vec2 dofRowDelta = vec2( 0, 0.25 / targetSize.y );
   
   // "rowOfs" reduces how many moves PS2.0 uses to emulate swizzling.  
   vec2 rowOfs[4];
   rowOfs[0] = vec2(0.0f);  
   rowOfs[1] = dofRowDelta.xy;  
   rowOfs[2] = dofRowDelta.xy * 2;  
   rowOfs[3] = dofRowDelta.xy * 3; 
   
   
   // 4 samples should be sufficient, but 16 gives better quality
#if 0
   float mul = 1.0f;
   vec2 texOffset = (1.5f / texSize0) * mul;

   // Use bilinear filtering to average 4 color samples for free.  
   float3 color = texture2D( colorSampler, tcColor0.xy + vec2(0.0f, 0.0f) ).rgb;  
   color       += texture2D( colorSampler, tcColor0.xy + vec2(texOffset.x, 0.0f) ).rgb;  
   color       += texture2D( colorSampler, tcColor0.xy + vec2(texOffset.x, texOffset.y) ).rgb;  
   color       += texture2D( colorSampler, tcColor0.xy + vec2(0.0f, texOffset.y) ).rgb;  
   color *= 0.25f;
#else
   vec3 color = vec3(0.0f);
   for (int y = 0; y < 4; ++y)
   {
     vec2 offset;
     offset.y = y * (1.0f / texSize0.y);
     for (int x = 0; x < 4; ++x)
     {
       offset.x = x * (1.0f / texSize0.x);
       color += texture2D(colorSampler, tcColor0.xy + offset).rgb;
     }
   }
   color /= 16.0f;
#endif

   // Process 4 samples at a time to use vector hardware efficiently.  
   // The CoC will be 1 if the depth is negative, so use "min" to pick  
   // between "sceneCoc" and "viewCoc".  
   
   vec4 coc;
   for ( int i = 0; i < 4; i++ )
   {
      vec4 depth;
      depth.x = prepassUncondition( depthSampler, tcDepth0.xy + rowOfs[i] ).w;
      depth.y = prepassUncondition( depthSampler, tcDepth1.xy + rowOfs[i] ).w;
      depth.z = prepassUncondition( depthSampler, tcDepth2.xy + rowOfs[i] ).w;
      depth.w = prepassUncondition( depthSampler, tcDepth3.xy + rowOfs[i] ).w;

      depth *= dofEqWorld.x;
      depth += dofEqWorld.y;

      float maxRowCoc = max( max( depth.x, depth.y ), max( depth.z, depth.w ) );
      coc[i] = depth.x;
   }   
   
   float maxCoc = max( max( coc.x, coc.y ), max( coc.z, coc.w ) );  
   maxCoc = clamp(maxCoc, 0.0f, maxWorldCoC);

   gl_FragColor = vec4(color.rgb, maxCoc); 
}
