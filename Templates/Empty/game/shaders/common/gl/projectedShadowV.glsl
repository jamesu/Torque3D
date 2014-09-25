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

attribute vec4 vPosition;
attribute vec3 vNormal;
attribute vec2 vTangent;
attribute vec4 vColor;
attribute vec2 vTexCoord0;

varying vec2 texCoord;
varying vec4 color;
varying float fade;

uniform mat4 modelview;
uniform float shadowLength;
uniform vec3 shadowCasterPosition;

void main()
{
   gl_Position = modelview * vec4(vPosition.xyz, 1.0);
   
   color = vColor;
   texCoord = vTexCoord0.st;
   
   float fromCasterDist = length(vPosition.xyz - shadowCasterPosition) - shadowLength;
   fade = 1.0 - clamp( fromCasterDist / shadowLength , 0.0, 1.0 );
}
