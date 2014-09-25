float4 main( float2 texCoord_in : TEXCOORD0,
             uniform sampler2D diffuseMap : register(S0) ) : COLOR0
{
   return tex2D(diffuseMap, texCoord_in);
}