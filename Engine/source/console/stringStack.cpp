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

#include <stdio.h>
#include "console/consoleInternal.h"
#include "console/stringStack.h"


void ConsoleValueStack::getArgcArgv(StringTableEntry name, U32 *argc, ConsoleValueRef **in_argv, bool popStackFrame /* = false */)
{
   U32 startStack = mStackFrames[mFrame-1];
   U32 argCount   = getMin(mStackPos - startStack, (U32)MaxArgs - 1);

   *in_argv = mArgv;
   mArgv[0] = name;
   
   for(U32 i = 0; i < argCount; i++) {
      ConsoleValueRef *ref = &mArgv[i+1];
      ref->value = &mStack[startStack + i];
      ref->stringStackValue = NULL;
   }
   argCount++;
   
   *argc = argCount;

   if(popStackFrame)
      popFrame();
}

void ConsoleValueStack::getArrayElements(Vector<ConsoleValue> &dest)
{
   U32 startStack = mStackFrames[mFrame-1];
   U32 itemCount  = mStackPos - startStack;
   
   U32 startSize = dest.size();
   dest.setSize(dest.size() + itemCount);
   for(U32 i = 0; i < itemCount; i++) {
      mStack[startStack+i].copyInto(dest[startSize + i]);
   }
}

ConsoleValueStack::ConsoleValueStack() : 
mFrame(0),
mStackPos(0)
{
   for (int i=0; i<ConsoleValueStack::MaxStackDepth; i++) {
      mStack[i].init();
      mStack[i].type = ConsoleValue::TypeInternalString;
   }
}

ConsoleValueStack::~ConsoleValueStack()
{
}

void ConsoleValueStack::pushVar(ConsoleValue *variable)
{
   if (mStackPos == ConsoleValueStack::MaxStackDepth) {
      AssertFatal(false, "Console Value Stack is empty");
      return;
   }

   variable->copyInto(mStack[mStackPos++]);
}

ConsoleValue* ConsoleValueStack::pushValue(ConsoleValue &variable)
{
   if (mStackPos == ConsoleValueStack::MaxStackDepth) {
      AssertFatal(false, "Console Value Stack is empty");
      return NULL;
   }

   variable.copyInto(mStack[mStackPos++]);

   return &mStack[mStackPos-1];
}

ConsoleValue *ConsoleValueStack::pushString(const char *value)
{
   if (mStackPos == ConsoleValueStack::MaxStackDepth) {
      AssertFatal(false, "Console Value Stack is empty");
      return NULL;
   }

   //Con::printf("[%i]CSTK pushString %s", mStackPos, value);

   mStack[mStackPos++].setStringValue(value);
   return &mStack[mStackPos-1];
}

ConsoleValue *ConsoleValueStack::pushStackString(const char *value)
{
   if (mStackPos == ConsoleValueStack::MaxStackDepth) {
      AssertFatal(false, "Console Value Stack is empty");
      return NULL;
   }

   //Con::printf("[%i]CSTK pushString %s", mStackPos, value);

   mStack[mStackPos++].setStackStringValue(value);
   return &mStack[mStackPos-1];
}

ConsoleValue *ConsoleValueStack::pushUINT(U32 value)
{
   if (mStackPos == ConsoleValueStack::MaxStackDepth) {
      AssertFatal(false, "Console Value Stack is empty");
      return NULL;
   }

   //Con::printf("[%i]CSTK pushUINT %i", mStackPos, value);

   mStack[mStackPos++].setIntValue(value);
   return &mStack[mStackPos-1];
}

ConsoleValue *ConsoleValueStack::pushFLT(float value)
{
   if (mStackPos == ConsoleValueStack::MaxStackDepth) {
      AssertFatal(false, "Console Value Stack is empty");
      return NULL;
   }

   //Con::printf("[%i]CSTK pushFLT %f", mStackPos, value);

   mStack[mStackPos++].setFloatValue(value);
   return &mStack[mStackPos-1];
}

ConsoleValue *ConsoleValueStack::pushArray(Vector<ConsoleValue> &value)
{
   if (mStackPos == ConsoleValueStack::MaxStackDepth) {
      AssertFatal(false, "Console Value Stack is empty");
      return NULL;
   }

   //Con::printf("[%i]CSTK pushFLT %f", mStackPos, value);

   mStack[mStackPos++].setArrayValue(value);
   return &mStack[mStackPos-1];
}

static ConsoleValue gNothing;

ConsoleValue* ConsoleValueStack::pop()
{
   if (mStackPos == 0) {
      AssertFatal(false, "Console Value Stack is empty");
      return &gNothing;
   }

   return &mStack[--mStackPos];
}

void ConsoleValueStack::pushFrame()
{
   //Con::printf("CSTK pushFrame");
   mStackFrames[mFrame++] = mStackPos;
}

void ConsoleValueStack::resetFrame()
{
   if (mFrame == 0) {
      mStackPos = 0;
      return;
   }

   U32 start = mStackFrames[mFrame-1];
   //for (U32 i=start; i<mStackPos; i++) {
      //mStack[i].clear();
   //}
   mStackPos = start;
   //Con::printf("CSTK resetFrame to %i", mStackPos);
}

void ConsoleValueStack::popFrame()
{
   //Con::printf("CSTK popFrame");
   if (mFrame == 0) {
      // Go back to start
      mStackPos = 0;
      return;
   }

   U32 start = mStackFrames[mFrame-1];
   //for (U32 i=start; i<mStackPos; i++) {
      //mStack[i].clear();
   //}
   mStackPos = start;
   mFrame--;
}
