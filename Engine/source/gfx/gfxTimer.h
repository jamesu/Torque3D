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

#ifndef _GFXTIMER_H_
#define _GFXTIMER_H_

#include "core/util/tDictionary.h"
#include "console/console.h"
#include "gfx/gfxResource.h"
#include "torqueConfig.h"

// GPU Timers shouldn't be included in shipping builds as they're used for profiling builds
#ifndef TORQUE_SHIPPING

#define ENABLE_GPU_TIMERSx

#endif // TORQUE_SHIPPING

#ifdef ENABLE_GPU_TIMERS

/// martinJ - A GFXTimer issues stamps to the GPU allowing asynchronous profiling. The SCOPED_GFX_TIMER
/// macro can be used to manage the GPU time spent executing all commands generated in a scope block.
class GFXTimer : public GFXResource
{
public:
  /// Adds a time stamp to the GPU which can be read back in the future
  virtual void issueMarker() = 0;

  /// True if the marker time has returned from the GPU. Use getResult() to view the timing value.
  virtual bool resultReady() = 0;

  /// Retrieves the GPU time of that last call of issueMarker(). If resultsReady() returns false
  /// then this value isn't valid.
  virtual long long getResult() = 0;

  /// Resets all internal data so the timer is ready to issue a new marker
  virtual void reset() = 0;
};

/// The timer manager stores all gpu profiling results.
class GFXTimerManager : public GFXResource
{
private:
  struct ActiveTimerPair
  {
    GFXTimer* m_begin;
    GFXTimer* m_end;
    const char* m_name;
  };

  typedef Vector<ActiveTimerPair> TimerStore;

public:
  ~GFXTimerManager();

  /// Checks if all timers are complete and the results string can be updated.
  /// Should be called once a frame
  void checkForResults();

  /// Returns a string that contains all profiling results. Designed to be used in the metrics script function.
  /// e.g. metrics("GPU")
  const char* getResultsString();

  /// Adds the script callback "string getGPUProfileResults()" so that profile results can be accesed through script
  void addCallback();

  /// Gets a start/end pair of timers to capture a time period on the GPU. Timers won't not be
  /// returned when requesting in a frame and a previous set of timers have not finished
  void getTimers(const char* name, GFXTimer** startTimer, GFXTimer** endTimer, U32 frameIndex);

  /// Singleton access
  static GFXTimerManager& getInstance()
  {
    return sm_singleton;
  }

  /// GFXResource methods
  virtual void zombify();
  virtual void resurrect();
  virtual const String describeSelf() const;

  void destroyTimers();

private:
  GFXTimerManager() :
    m_activeFrame(0)
  {
  }

  static const char* stringCallback(SimObject *obj, S32 argc, const char *argv[])
  {
    GFXTimerManager& manager = GFXTimerManager::getInstance();
    return manager.getResultsString();
  }

  // A pool of timers 
  Vector<GFXTimer*> m_spareTimers;
  TimerStore m_activeTimers;

  // The results of all timers are stored in this string which is available through
  // script via getGPUProfileResults()
  StringBuilder m_resultString;

  // The frame timers are been processed for. Requests for timers in a later frame
  // while there are active timers will be ignored
  U32 m_activeFrame;


  static GFXTimerManager sm_singleton;
};

/// ScopedGPUTimerBlock
///
/// Utility class that times the GPU duration of all calls made within the block.
class ScopedGPUTimerBlock
{
public:
  ScopedGPUTimerBlock(GFXTimer* beginTimer, GFXTimer* endTimer)
  {
    m_beginTimer = beginTimer;
    m_endTimer = endTimer;

    // ScopedGPUTimerBlock needs to work when either the begin or end timer are null. Set both to null
    // if either is so that all code only deals with either both existing or neither.
    if (!m_beginTimer || !m_endTimer)
    {
      m_beginTimer = 0;
      m_endTimer = 0;
    }
    else
    {
      m_beginTimer->issueMarker();
    }
  }

  ~ScopedGPUTimerBlock()
  {
    if (m_endTimer)
    {
      m_endTimer->issueMarker();
    }
  }

private:
  GFXTimer* m_beginTimer;
  GFXTimer* m_endTimer;
};

/// The SCOPED_GFX_TIMER macro can be used to manage the GPU time spent executing all commands generated in a scope block.
#define  SCOPED_GFX_TIMER(Name) \
  GFXTimer* startTimer = 0; \
  GFXTimer* endTimer = 0; \
  GFXTimerManager::getInstance().getTimers(Name, &startTimer, &endTimer, gClientSceneGraph->getFrameNum()); \
  ScopedGPUTimerBlock gpuTimer(startTimer, endTimer);


#else // ENABLE_GPU_TIMERS

#define SCOPED_GFX_TIMER(Name)

#endif // ENABLE_GPU_TIMERS

#endif // _GFXTIMER_H_
