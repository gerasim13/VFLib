// Copyright (C) 2008 by Vinnie Falco, this file is part of VFLib.
// See the file LICENSE.txt for licensing information.

#ifndef VF_CONCURRENTOBJECT_VFHEADER
#define VF_CONCURRENTOBJECT_VFHEADER

// A copy of juce::ReferenceCountedObject,
// with the following features:
//
// - Shorter name
//
// - Derived class may override the behavior of destruction.
//
// - Default behavior performs the delete on a separate thread.
//

class ConcurrentObject : Uncopyable
{
public:
  inline void incReferenceCount() noexcept
  {
    m_refs.addref ();
  }

  inline void decReferenceCount() noexcept
  {
    vfassert (m_refs.is_signaled ());

    if (m_refs.release ())
      destroySharedObject ();
  }

  // Caller must synchronize.
  inline bool isBeingReferenced () const
  {
    return m_refs.is_signaled ();
  }

protected:
  ConcurrentObject();

  virtual ~ConcurrentObject();

  // default implementation performs the delete
  // on a separate, provided thread that cleans up
  // after itself on exit.
  //
  virtual void destroySharedObject ();

protected:
  class Deleter;

private:
  AtomicCounter m_refs;
};

#endif
