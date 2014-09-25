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

uniform sampler2D diffuseMap;

varying vec2 uv0;
varying vec2 uv1;
varying vec2 uv2;
varying vec2 uv3;

varying vec2 uv4;
varying vec2 uv5;
varying vec2 uv6;
varying vec2 uv7;

void main()
{
   vec4 kernel = vec4( 0.175, 0.275, 0.375, 0.475 ) * 0.5f;

   vec4 OUT = vec4(0);
   OUT += texture2D( diffuseMap, uv0 ) * kernel.x;
   OUT += texture2D( diffuseMap, uv1 ) * kernel.y;
   OUT += texture2D( diffuseMap, uv2 ) * kernel.z;
   OUT += texture2D( diffuseMap, uv3 ) * kernel.w;

   OUT += texture2D( diffuseMap, uv4 ) * kernel.x;
   OUT += texture2D( diffuseMap, uv5 ) * kernel.y;
   OUT += texture2D( diffuseMap, uv6 ) * kernel.z;
   OUT += texture2D( diffuseMap, uv7 ) * kernel.w;

   // Calculate a lumenance value in the alpha so we
   // can use alpha test to save fillrate.
   vec3 rgb2lum = vec3( 0.30, 0.59, 0.11 );
   OUT.a = dot( OUT.rgb, rgb2lum );

   if (dot( OUT.rgb, rgb2lum ) >= 1)
      discard;

   gl_FragColor = OUT;
}
