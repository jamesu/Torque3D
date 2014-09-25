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

#include "gfx/D3D9/gfxD3D9Timer.h"
#include "gfx/D3D9/gfxD3D9Device.h"

#ifdef ENABLE_GPU_TIMERS

GFXD3D9Timer::GFXD3D9Timer() :
  m_timer(0),
  m_result(0),
  m_waitingForResult(false),
  m_resultsReady(false)
{
  reset();
}

void GFXD3D9Timer::issueMarker()
{
  if (!m_waitingForResult)
  {
    m_timer->Issue(D3DISSUE_END);
    m_waitingForResult = true;
  }
}

bool GFXD3D9Timer::resultReady()
{
  if (!m_resultsReady && m_waitingForResult)
  {
    HRESULT hr = m_timer->GetData(&m_result, sizeof(m_result), 0);
    if (hr == S_OK)
    { 
      m_resultsReady = true;
      m_waitingForResult = false;
    }
  }

  return m_resultsReady;
}

long long GFXD3D9Timer::getResult()
{
  if (m_resultsReady)
  {
    return m_result;
  }

  return 0;
}

void GFXD3D9Timer::reset()
{
  m_resultsReady = false;
  m_waitingForResult = false;
  m_result = 0;
}

void GFXD3D9Timer::zombify()
{
  if (m_timer)
  {
    m_timer->Release();
    m_timer = NULL;
  }
  reset();
}

void GFXD3D9Timer::resurrect()
{
  IDirect3DDevice9* device = static_cast<GFXD3D9Device *>(getOwningDevice())->getDevice();
  D3D9Assert(device->CreateQuery(D3DQUERYTYPE_TIMESTAMP, &m_timer), "Failed to create time stamp query");
}

const String GFXD3D9Timer::describeSelf() const
{
  return "D3D query wrapper";
}

#endif // ENABLE_GPU_TIMERS