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

#include "console/consoleInternal.h"
#include "console/stringStack.h"


void ConsoleValueStack::getArgcArgv(StringTableEntry name, U32 *argc, ConsoleValue **in_argv, bool popStackFrame /* = false */)
{
   U32 startStack = mStackFrames[mFrame-1];
   U32 argCount   = getMin(mStackPos - startStack, (U32)MaxArgs - 1);

   *in_argv = mArgv;
   mArgv[0] = name;
   
   for(U32 i = 0; i < argCount; i++)
      mArgv[i+1] = mStack[startStack + i];
   argCount++;
   
   *argc = argCount;

   if(popStackFrame)
      popFrame();
}

ConsoleValueStack::ConsoleValueStack() : 
mFrame(0),
mStackPos(0)
{
}

ConsoleValueStack::~ConsoleValueStack()
{
}

void ConsoleValueStack::push(Dictionary::Entry *variable)
{
	if (mStackPos == ConsoleValueStack::MaxStackDepth) {
		AssertFatal(false, "Console Value Stack is empty");
		return;
	}

	switch (variable->type)
	{
	case Dictionary::Entry::TypeInternalInt:
		mStack[mStackPos++] = (S32)variable->getIntValue();
	case Dictionary::Entry::TypeInternalFloat:
		mStack[mStackPos++] = (F32)variable->getFloatValue();
	default:
		mStack[mStackPos++] = variable->getStringValue();
	}
}

void ConsoleValueStack::push(ConsoleValue &value)
{
	if (mStackPos == ConsoleValueStack::MaxStackDepth) {
		AssertFatal(false, "Console Value Stack is empty");
		return;
	}

	mStack[mStackPos++] = value;
}

static ConsoleValue gNothing(0);
static ConsoleValue gPop(0);
ConsoleValue& ConsoleValueStack::pop()
{
	if (mStackPos == 0) {
		AssertFatal(false, "Console Value Stack is empty");
		return gNothing;
	}

	gPop = mStack[mStackPos-1];
	mStack[mStackPos-1] = 0;
	mStackPos--;
	return gPop;
}

void ConsoleValueStack::pushFrame()
{
	//Con::printf("CSTK pushFrame");
	mStackFrames[mFrame++] = mStackPos;
}

void ConsoleValueStack::popFrame()
{
	//Con::printf("CSTK popFrame");
	if (mFrame == 0)
		return;

	U32 start = mStackFrames[mFrame-1];
	//for (U32 i=start; i<mStackPos; i++) {
		//mStack[i].clear();
	//}
	mStackPos = start;
	mFrame--;
}
