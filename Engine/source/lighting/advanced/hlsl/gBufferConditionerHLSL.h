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

#ifndef _GBUFFER_CONDITIONER_HLSL_H_
#define _GBUFFER_CONDITIONER_HLSL_H_

#ifndef _CONDITIONER_BASE_H_
#include "shaderGen/conditionerFeature.h"
#endif
#ifndef _SHADEROP_H_
#include "shaderGen/shaderOp.h"
#endif


///
class GBufferConditioner : public ConditionerFeature
{
   typedef ConditionerFeature Parent;

public:
   enum NormalStorage
   {
      CartesianXYZ,
      CartesianXY,
      Spherical,
      LambertAzimuthal,
   };

   enum NormalSpace
   {
      WorldSpace,
      ViewSpace,
   };
   
protected:

   NormalStorage mNormalStorageType;
   bool mCanWriteNegativeValues;
   U32 mBitsPerChannel;

public:

   GBufferConditioner( const GFXFormat bufferFormat, const NormalSpace nrmSpace );
   virtual ~GBufferConditioner();


   virtual void processVert( Vector<ShaderComponent*> &componentList, const MaterialFeatureData &fd );
   virtual void processPix( Vector<ShaderComponent*> &componentList, const MaterialFeatureData &fd );
   virtual Resources getResources( const MaterialFeatureData &fd );
   virtual String getName() { return "GBuffer Conditioner"; }

   static const char* prepassSamplerName;

protected:

   virtual Var *printMethodHeader( MethodType methodType, const String &methodName, Stream &stream, MultiLine *meta );

   virtual GenOp* _posnegEncode( GenOp *val );
   virtual GenOp* _posnegDecode( GenOp *val );
   virtual Var* _conditionOutput( Var *unconditionedOutput, MultiLine *meta );
   virtual Var* _unconditionInput( Var *conditionedInput, MultiLine *meta );
};

#endif // _GBUFFER_CONDITIONER_HLSL_H_
