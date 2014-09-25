attribute vec4 vPosition;
attribute vec2 vTexCoord0;

uniform mat4 modelview;
varying vec2 texCoord;

void main()
{
   gl_Position = modelview * vPosition;
   texCoord = vTexCoord0.st;
}