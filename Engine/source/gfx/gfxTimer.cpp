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

#include "gfx/gfxTimer.h"
#include "gfx/gfxDevice.h"

#ifdef ENABLE_GPU_TIMERS

GFXTimerManager GFXTimerManager::sm_singleton;

GFXTimerManager::~GFXTimerManager()
{
  destroyTimers();
}

void GFXTimerManager::getTimers(const char* name, GFXTimer** startTimer, GFXTimer** endTimer, U32 frameIndex)
{
  AssertFatal(startTimer, "startTimer must not be null!");
  AssertFatal(endTimer, "endTimer must not be null!");

  if (!name)
  {
    name = "NO_NAME";
  }

  // Timers will only be handed out if the previous batch has completed or requesting on in the active frame.
  if (m_activeTimers.size() == 0 || frameIndex == m_activeFrame)
  {
    m_activeFrame = frameIndex;

    if (m_spareTimers.size() == 0)
    {
      *startTimer = GFX->createTimer();
      if (!*startTimer)
      {
        AssertFatal(startTimer, "Failed to create GFXTimer");
        *startTimer = NULL;
        *endTimer = NULL;
        return;
      }
    }
    else
    {
      *startTimer = m_spareTimers.back();
      m_spareTimers.pop_back();
    }

    if (m_spareTimers.size() == 0)
    {
      *endTimer = GFX->createTimer();
      if (!*endTimer)
      {
        // Add the start timer to the pool so it isn't lost
        m_spareTimers.push_back(*startTimer);
        AssertFatal(startTimer, "Failed to create GFXTimer");
        *startTimer = NULL;
        *endTimer = NULL;
        return;
      }
    }
    else
    {
      *endTimer = m_spareTimers.back();
      m_spareTimers.pop_back();
    }

    // Successfully got 2 timers so reset them and add them to the active list.
    // The timers will be checked again in checkForResults()
    ActiveTimerPair timers;
    timers.m_begin = *startTimer;
    timers.m_end = *endTimer;
    timers.m_name = name;
    timers.m_begin->reset();
    timers.m_end->reset();
    m_activeTimers.push_back(timers);
  }
}

void GFXTimerManager::checkForResults()
{
  bool allTimersFinished = true;

  // Check if all the timers are finished
  for (U32 timerI = 0, timerCount = m_activeTimers.size(); timerI < timerCount; ++timerI)
  {
    ActiveTimerPair& timerTuple = m_activeTimers[timerI];
    if (!timerTuple.m_begin->resultReady() || !timerTuple.m_end->resultReady())
    {
      allTimersFinished = false;
      break;
    }
  }

  if (allTimersFinished)
  {
    long long timerFrequency = GFX->getTimerFrequency();
    m_resultString.reset();
    for (U32 timerI = 0, timerCount = m_activeTimers.size(); timerI < timerCount; ++timerI)
    {
      ActiveTimerPair& timerPair = m_activeTimers[timerI];
      long long start = timerPair.m_begin->getResult();
      long long end = timerPair.m_end->getResult();

      // multiply the result to go from seconds to milliseconds
      float timeTaken = (float)(((end - start) / (double)timerFrequency) * 1000.f);

      // append the result to the string
      m_resultString.format("\n %s %f", timerPair.m_name, timeTaken);

      // Return the timers to the pool for re-use
      m_spareTimers.push_back(timerPair.m_begin);
      m_spareTimers.push_back(timerPair.m_end);
    }

    // All the active timers have been processed so the list can be cleared.
    // This will allow timers to retrieved in a new frame
    m_activeTimers.clear();
  }
}

const char* GFXTimerManager::getResultsString()
{
  return m_resultString.data();
}

void GFXTimerManager::addCallback()
{
  Con::addCommand("getGPUProfileResults",  stringCallback, "", 0, 0);
}

void GFXTimerManager::zombify()
{
}

void GFXTimerManager::resurrect()
{
  // Active timers will have been reset and may not have produced any results. They need to be
  // returned to the spare pool because they may never return true from GFXTimer::resultReady()
  // causing the manager to lock up.
  for (U32 timerI = 0, timerCount = m_activeTimers.size(); timerI < timerCount; ++timerI)
  {
    ActiveTimerPair& timerPair = m_activeTimers[timerI];
    m_spareTimers.push_back(timerPair.m_begin);
    m_spareTimers.push_back(timerPair.m_end);
  }
  m_activeTimers.clear();
}

const String GFXTimerManager::describeSelf() const
{
  return "Timer Manager: Stores GFXTimers and collates results into "
    "a string that can be seen using the script command metrics(\"GPU\")";
}

void GFXTimerManager::destroyTimers()
{
  // make sure all timers are in the spare list
  resurrect();
  AssertFatal(m_activeTimers.size() == 0, "All timers should be in m_spareTimers at this point.");

  for (U32 timerI = 0, timerCount = m_spareTimers.size(); timerI < timerCount; ++timerI)
  {
    m_spareTimers[timerI]->zombify();
    delete m_spareTimers[timerI];
  }
  m_spareTimers.clear();
}

#endif // ENABLE_GPU_TIMERS