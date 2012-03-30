// Copyright (C) 2008 by Vinnie Falco, this file is part of VFLib.
// See the file LICENSE.txt for licensing information.

#ifndef __VF_JUCETHREAD_VFHEADER__
#define __VF_JUCETHREAD_VFHEADER__

#include "vf/modules/vf_core/memory/vf_Atomic.h"
#include "vf/modules/vf_core/functor/vf_Function.h"
#include "vf/modules/vf_core/threads/vf_ThreadBase.h"

namespace detail {

// Stores a back pointer to our object so we can keep
// the derivation from juce::Thread private and still use
// dynamic_cast.
//
class JuceThreadWrapper : public VF_JUCE::Thread
{
public:
  JuceThreadWrapper (String name, ThreadBase& threadBase)
    : VF_JUCE::Thread (name)
    , m_threadBase (threadBase)
  {
  }

  ThreadBase& getThreadBase () const
  {
    return m_threadBase;
  }

private:
  ThreadBase& m_threadBase;
};

}

//
// Thread based on Juce
//
class JuceThread : public ThreadBase
                 , private detail::JuceThreadWrapper
{
public:
  typedef VF_JUCE::Thread::ThreadID id;

public:
  class InterruptionModel
  {
  protected:
    InterruptionModel ();
    bool do_wait ();    // true if interrupted
    bool do_timeout (); // true if interrupted

  public:
    void interrupt (JuceThread& thread);

  protected:
    bool do_interruptionPoint ();

  protected:
    enum
    {
      stateRun,
      stateInterrupt,
      stateReturn,
      stateWait
    };

    Atomic::State m_state;
  };

  class PollingBased : public InterruptionModel
  {
  public:
    bool wait (int milliseconds, JuceThread& thread);
    Interrupted interruptionPoint (JuceThread& thread);
  };

public:
  explicit JuceThread (String name);
  ~JuceThread ();

  void start (Function <void (void)> const& f);

  void join ();

  id getId () const;

  // only valid if the thread is running
  bool isTheCurrentThread () const;

  void setPriority (int priority);

private:
  void run ();

  Function <void (void)> m_function;
  VF_JUCE::WaitableEvent m_runEvent;
  id m_threadId;
};

//------------------------------------------------------------------------------

template <class InterruptionType>
class JuceThreadType : public JuceThread
{
public:
  explicit JuceThreadType (String const& name) : JuceThread (name)
  {
  }

  bool wait (int milliseconds = -1)
  {
    return m_model.wait (milliseconds, *this);
  }

  void interrupt ()
  {
    m_model.interrupt (*this);
  }

  Interrupted interruptionPoint ()
  {
    return m_model.interruptionPoint (*this);
  }

private:
  InterruptionType m_model;
};

//------------------------------------------------------------------------------

namespace CurrentJuceThread {

// Avoid this function because the implementation is slow.
// Use JuceThread::interruptionPoint() instead.
//
extern ThreadBase::Interrupted interruptionPoint ();

inline JuceThread::id getId ()
{
  return VF_JUCE::Thread::getCurrentThreadId ();
}

// [0, 10] where 5 = normal
inline void setPriority (int priority) 
{
  VF_JUCE::Thread::setCurrentThreadPriority (priority);
}

inline void yield ()
{
  VF_JUCE::Thread::yield ();
}

inline void sleep (const int milliseconds)
{
  VF_JUCE::Thread::sleep (milliseconds);
}

}

#endif

