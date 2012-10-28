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

#ifndef _RENDERPASSSTATETOKEN_H_
#define _RENDERPASSSTATETOKEN_H_

#ifndef _RENDERBINMANAGER_H_
#include "renderInstance/renderBinManager.h"
#endif

class RenderPassStateBin;


class RenderPassStateToken : public SimObject
{
   typedef SimObject Parent;

public:
   DECLARE_CONOBJECT(RenderPassStateToken);

   static void initPersistFields();

   // These must be re-implemented, and will assert if called on the base class.
   // They just can't be pure-virtual, due to SimObject.
   virtual void process(SceneRenderState *state, RenderPassStateBin *callingBin);
   virtual void reset();
   virtual void enable(bool enabled = true);
   virtual bool isEnabled() const;
};

//------------------------------------------------------------------------------

class RenderPassStateBin : public RenderBinManager
{
   typedef RenderBinManager Parent;

protected:
   SimObjectPtr< RenderPassStateToken > mStateToken;
   
   static bool _setStateToken( void *object, const char *index, ConsoleValueRef data )
   {
      RenderPassStateToken* stateToken;
      Sim::findObject( data->getStringValue(), stateToken );
      reinterpret_cast< RenderPassStateBin* >( object )->mStateToken = stateToken;
      return false;
   }
   static ConsoleValue* _getStateToken( void *object, ConsoleValueRef data )
   {
      RenderPassStateBin* bin = reinterpret_cast< RenderPassStateBin* >( object );
      if( bin->mStateToken.isValid() )
         return Con::getReturnValue(bin->mStateToken->getIdString());
      else
         return Con::getReturnValue("0");
   }
   
public:
   DECLARE_CONOBJECT(RenderPassStateBin);
   static void initPersistFields();

   RenderPassStateBin();
   virtual ~RenderPassStateBin();

   void render(SceneRenderState *state);
   void clear();
   void sort();
};

#endif // _RENDERPASSSTATETOKEN_H_
