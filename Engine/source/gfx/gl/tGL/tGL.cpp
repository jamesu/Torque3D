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

#include "tGL.h"
#include "tCoreGL.h"

#include "core/strings/stringFunctions.h"
#include "console/console.h"

#ifdef USE_SDL_OPENGL
   // Bind core stuff using SDL
   #define GL_GROUP_BEGIN(name)
   #define GL_GROUP_END()
   #define GL_FUNCTION(fn_name, fn_return, fn_args) \
         fn_return stub_##fn_name fn_args { Con::printf("Called %x " #fn_name, fn_name); }
   #include "gfx/gl/tGL/bindings/glcfn.h"
   #undef GL_FUNCTION
   #undef GL_GROUP_BEGIN
   #undef GL_GROUP_END
#endif

namespace GL
{
   void gglPerformBinds()
   {
#ifdef USE_SDL_OPENGL
   // Bind core stuff using SDL
   #define GL_GROUP_BEGIN(name)
   #define GL_GROUP_END()
   #define GL_FUNCTION(fn_name, fn_return, fn_args) \
         *((void**)&fn_name) = SDL_GL_GetProcAddress(#fn_name);
   #include "gfx/gl/tGL/bindings/glcfn.h"
   #undef GL_FUNCTION
   #undef GL_GROUP_BEGIN
   #undef GL_GROUP_END
#endif
   }

   void gglPerformExtensionBinds(void *context)
   {
      GLenum err = glewInit();
      AssertFatal(GLEW_OK == err, avar("Error: %s\n", glewGetErrorString(err)) );	
   }
}

