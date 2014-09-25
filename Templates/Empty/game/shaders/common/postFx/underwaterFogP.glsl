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

#include "../gl/hlslCompat.glsl"
#include "shadergen:/autogenConditioners.h"
#include "../torque.glsl"

//-----------------------------------------------------------------------------
// Defines                                                                  
//-----------------------------------------------------------------------------

// oceanFogData
#define FOG_DENSITY        waterFogData[0]
#define FOG_DENSITY_OFFSET waterFogData[1]
#define WET_DEPTH          waterFogData[2]
#define WET_DARKENING      waterFogData[3]

//-----------------------------------------------------------------------------
// Uniforms                                                                  
//-----------------------------------------------------------------------------

uniform sampler2D prepassTex; 
uniform sampler2D backbuffer;
uniform sampler2D waterDepthGradMap;
uniform vec3    eyePosWorld;
uniform vec3    ambientColor;     
uniform vec4    waterColor;       
uniform vec4    waterFogData;    
uniform vec4    waterFogPlane;    
uniform vec2    nearFar;      
uniform vec4    rtParams0;
uniform float     waterDepthGradMax;

varying vec2 uv0;
varying vec4 rtParams0;


void main()
{    
   float depth = prepassUncondition( prepassTex, uv0 ).w;
   
   // Skip fogging the extreme far plane so that 
   // the canvas clear color always appears.
   //clip( 0.9 - depth );
   
   // We assume that the eye position is below water because
   // otherwise this shader/posteffect should not be active.
   
   depth *= nearFar.y;
   
   vec3 eyeRay = normalize( IN.wsEyeRay );
   
   vec3 rayStart = eyePosWorld;
   vec3 rayEnd = eyePosWorld + ( eyeRay * depth );
   
   vec4 plane = waterFogPlane;
   
   float startSide = dot( plane.xyz, rayStart ) + plane.w;
   if ( startSide > 0 )
   {
      rayStart.z -= ( startSide );
   }

   vec3 hitPos;
   vec3 ray = rayEnd - rayStart;
   float rayLen = length( ray );
   vec3 rayDir = normalize( ray );
   
   float endSide = dot( plane.xyz, rayEnd ) + plane.w;     
   float planeDist;
   
   if ( endSide < -0.005 )    
   {  
      hitPos = rayEnd;
      planeDist = endSide;
   }
   else
   {   
      float den = dot( ray, plane.xyz );
      
      // Parallal to the plane, return the endPnt.
        
      
      float dist = -( dot( plane.xyz, rayStart ) + plane.w ) / den;  
      if ( dist < 0.0 )         
         dist = 0.0;
              
         
      hitPos = lerp( rayStart, rayEnd, dist );
      
      planeDist = dist;
   }
      
   float delta = length( hitPos - rayStart );  
      
   float fogAmt = 1.0 - saturate( exp( -FOG_DENSITY * ( delta - FOG_DENSITY_OFFSET ) ) );   

   // Calculate the water "base" color based on depth.
   vec4 fogColor = waterColor * texture2D( waterDepthGradMap, float2(saturate( delta / waterDepthGradMax ), 0.0f) );      
   // Modulate baseColor by the ambientColor.
   fogColor *= vec4( ambientColor.rgb, 1 );
   
   vec3 inColor = hdrDecode( texture2D( backbuffer, IN.uv0 ).rgb );
   inColor.rgb *= 1.0 - saturate( abs( planeDist ) / WET_DEPTH ) * WET_DARKENING;
   
   vec3 outColor = lerp( inColor, fogColor, fogAmt );
   
   gl_FragColor = vec4( hdrEncode( outColor ), 1 );        
}