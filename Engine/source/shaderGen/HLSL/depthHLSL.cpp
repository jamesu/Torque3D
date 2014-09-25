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

#include "platform/platform.h"
#include "shaderGen/HLSL/depthHLSL.h"
#include "shaderGen/shaderGen.h"

#include "materials/materialFeatureTypes.h"
#include "materials/materialFeatureData.h"


void EyeSpaceDepthOut::processVert(   Vector<ShaderComponent*> &componentList, 
                                          const MaterialFeatureData &fd )
{
	
	MultiLine *meta = new MultiLine;
   output = meta;

	 // grab output
   ShaderConnector *connectComp = dynamic_cast<ShaderConnector *>( componentList[C_CONNECTOR] );
   Var *outWSEyeVec = connectComp->getElement( RT_TEXCOORD );
   outWSEyeVec->setName( "outWSEyeVec" );
   outWSEyeVec->setStructName( "OUT" );
	
	
   // grab incoming vert position
   Var *wsPosition = new Var( "depthPos", getVector3Name() );
   getWsPosition( componentList, fd.features[MFT_UseInstancing], meta, new DecOp( wsPosition ) );

   Var *eyePos = (Var*)LangElement::find( "eyePosWorld" );
   if( !eyePos )
   {
      eyePos = new Var;
      eyePos->setType(getVector3Name());
      eyePos->setName("eyePosWorld");
      eyePos->uniform = true;
      eyePos->constSortPos = cspPass;
   }

   GenOp* vector4GenOp = new GenOp( ShaderFeatureCommon::getVector4Name() );
   meta->addStatement( new GenOp( "   @ = @( @.xyz - @, 1 );\r\n", outWSEyeVec, vector4GenOp, wsPosition, eyePos ) );
}

void EyeSpaceDepthOut::processPix( Vector<ShaderComponent*> &componentList, 
                                       const MaterialFeatureData &fd )
{      
   MultiLine *meta = new MultiLine;

   // grab connector position
   ShaderConnector *connectComp = dynamic_cast<ShaderConnector *>( componentList[C_CONNECTOR] );
   Var *wsEyeVec = connectComp->getElement( RT_TEXCOORD );
   wsEyeVec->setName( "outWSEyeVec" );
   wsEyeVec->setStructName( "IN" );
   wsEyeVec->setType( getVector4Name() );
   wsEyeVec->mapsToSampler = false;
   wsEyeVec->uniform = false;

   // get shader constants
   Var *vEye = new Var;
   vEye->setType(getVector3Name());
   vEye->setName("vEye");
   vEye->uniform = true;
   vEye->constSortPos = cspPass;

   // Expose the depth to the depth format feature
   Var *depthOut = new Var;
   depthOut->setType("float");
   depthOut->setName(getOutputVarName());

   LangElement *depthOutDecl = new DecOp( depthOut );

   meta->addStatement( new GenOp( "#ifndef CUBE_SHADOW_MAP\r\n" ) );
   meta->addStatement( new GenOp( "   @ = dot(@, (@.xyz / @.w));\r\n", depthOutDecl, vEye, wsEyeVec, wsEyeVec ) );
   meta->addStatement( new GenOp( "#else\r\n" ) );

   Var *farDist = (Var*)Var::find( "oneOverFarplane" );
   if ( !farDist )
   {
      farDist = new Var;
      farDist->setType("float4");
      farDist->setName("oneOverFarplane");
      farDist->uniform = true;
      farDist->constSortPos = cspPass;
   }

   meta->addStatement( new GenOp( "   @ = length( @.xyz / @.w ) * @.x;\r\n", depthOutDecl, wsEyeVec, wsEyeVec, farDist ) );      
   meta->addStatement( new GenOp( "#endif\r\n" ) );

   // If there isn't an output conditioner for the pre-pass, than just write
   // out the depth to rgba and return.
   if( !fd.features[MFT_PrePassConditioner] )
   {
      GenOp* vector4GenOp = new GenOp( ShaderFeatureCommon::getVector4Name() );
      meta->addStatement( new GenOp( "   @;\r\n", assignColor( new GenOp( "@(@, @, @, @)", vector4GenOp, depthOut, depthOut, depthOut, depthOut ), Material::None ) ) );
   }
   
   output = meta;
}

ShaderFeature::Resources EyeSpaceDepthOut::getResources( const MaterialFeatureData &fd )
{
   Resources temp;

   // Passing from VS->PS:
   // - world space position (wsPos)
   temp.numTexReg = 1; 

   return temp; 
}


void DepthOut::processVert(  Vector<ShaderComponent*> &componentList, 
                                 const MaterialFeatureData &fd )
{
  ShaderConnector *connectComp = dynamic_cast<ShaderConnector *>( componentList[C_CONNECTOR] );

  // Grab our output depth.
  Var *outDepth = connectComp->getElement( RT_TEXCOORD );
  outDepth->setName( "depth" );
  outDepth->setStructName( "OUT" );
  outDepth->setType( "float" );

  UsedForDirect3D(Var *outPosition = (Var*)LangElement::find("hpos"); output = new GenOp( "   @ = @.z / @.w;\r\n", outDepth, outPosition, outPosition ));
  UsedForOpenGL(output = new GenOp( "   @ = gl_Position.z / gl_Position.w;\r\n", outDepth ));
}

void DepthOut::processPix(   Vector<ShaderComponent*> &componentList, 
                                 const MaterialFeatureData &fd )
{
   ShaderConnector *connectComp = dynamic_cast<ShaderConnector *>( componentList[C_CONNECTOR] );

   // grab connector position
   Var *depthVar = connectComp->getElement( RT_TEXCOORD );
   depthVar->setName( "outDepth" );
   depthVar->setStructName( "IN" );
   depthVar->setType( "float" );
   depthVar->mapsToSampler = false;
   depthVar->uniform = false;

   LangElement *depthOut = new GenOp( avar("%s( @, @ * @, 0, 1 )", getVector4Name()), depthVar, depthVar, depthVar );

   output = new GenOp( "   @;\r\n", assignColor( depthOut, Material::None ) );
}

ShaderFeature::Resources DepthOut::getResources( const MaterialFeatureData &fd )
{
   // We pass the depth to the pixel shader.
   Resources temp;
   temp.numTexReg = 1; 

   return temp; 
}
