
#include "../gl/hlslCompat.glsl"
#include "shadergen:/autogenConditioners.h"  
#include "./../gl/torque.glsl"


uniform sampler2D backBuffer;
uniform sampler2D colorCorrectionTex;

uniform float OneOverGamma;

varying vec2 uv0;


void main()
{
    vec4 color = texture2D(backBuffer, uv0.xy);

    // Apply the color correction.
    color.r = texture2D( colorCorrectionTex, vec2(color.r, 0) ).r;
    color.g = texture2D( colorCorrectionTex, vec2(color.g, 0) ).g;
    color.b = texture2D( colorCorrectionTex, vec2(color.b, 0) ).b;

    // Apply gamma correction
    color.rgb = pow( abs(color.rgb), vec3(OneOverGamma));

    gl_FragColor = color;
}