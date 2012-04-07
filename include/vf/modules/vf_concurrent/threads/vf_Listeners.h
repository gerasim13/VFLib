// Copyright (C) 2008 by Vinnie Falco, this file is part of VFLib.
// See the file LICENSE.txt for licensing information.

#ifndef VF_LISTENERS_VFHEADER
#define VF_LISTENERS_VFHEADER

#include "vf_CallQueue.h"
#include "vf_ReadWriteMutex.h"
#include "../memory/vf_AllocatedBy.h"
#include "../memory/vf_FifoFreeStore.h"

//==============================================================================
/** A group of concurrent Listeners.

    A Listener is an object of class type which inherits from a defined
    interface, and registers on a provided instance of Listeners to receive
    asynchronous notifications of changes to concurrent states. Another way of
    defining Listeners, is that it is similar to a Juce ListenerList but with
    the provision that the Listener additional that the Listener specifies the
    CallQueue upon which the notification is made, at the time it registers.

    Listeners makes extensive use of CallQueue for providing the notifications,
    and provides a higher level facility for implementing the concurrent
    synchronization strategy outlined in CallQueue. Therefore, the same notes
    which apply to functors in CallQueue also apply to Listener member
    invocations. Their execution time should be brief, limited in scope to
    updating the recipient's view of a shared state, and use reference counting
    for parameters of class type.
  
    To use this system, first declare your Listener interface:

    @code

    struct Listener
    {
      // Sent on every output block
      virtual void onOutputLevelChanged (const float outputLevel) { }
    };

    @endcode

    Now set up the place where you want to send the notifications. In this
    example, we will set up the audioDeviceIOCallback to notify anyone who is
    interested about changes in the current audio output level. We will use
    this to implement a VU meter:

    @code

    Listeners <Listener> listeners;

    void audioDeviceIOCallback (const float** inputChannelData,
							    int numInputChannels,
							    float** outputChannelData,
							    int numOutputChannels,
							    int numSamples)
    {
      // Process audio data

      // Calculate output level
      float outputLevel = calcOutputLevel ();

      // Notify listeners
      listeners.call (&Listener::onOutputLevelChanged, outputLevel);
    }

    @endcode

    To receive notifications, derive from Listener and then add yourself to the
    Listeners object using the desired CallQueue.

    @code

    // We want notifications on the message thread
    GuiCallQueue fifo;

    struct VUMeter : public Listener, public Component
    {
      VUMeter () : m_outputLevel (0)
      {
        listeners.add (this, fifo);
      }

      ~VUMeter ()
      {
        listeners.remove (this);
      }

      void onOutputLevelChanged (float outputLevel)
      {
        // Update our copy of the output level shared state
        m_outputLevel = outputLevel;
        
        // And repaint
        repaint ();
      }

      float m_outputLevel;
    };

    @endcode

    In this example, the VUMeter constructs with the output level set to zero,
    and must wait for a notification before it shows up to date data. For a
    simple VU meter, this is likely not a problem. But if the shared state
    contains complex information, such as dynamically allocated objects with
    rich data, then we need a more solid system.

    We will add some classes to create a complete robust example of the use of
    Listeners to synchronize shared state:

    @code

    // Handles audio device output.
    class AudioDeviceOutput : public AudioIODeviceCallback
    {
    public:
      struct Listener
      {
        // Sent on every output block.
        virtual void onOutputLevelChanged (float outputLevel) { }
      };

      AudioDeviceOutput () : AudioDeviceOutput ("Audio CallQueue")
      {
      }

      ~AudioDeviceOutput ()
      {
        // Synchronize required since we're using a ManualCallQueue.
        m_fifo.synchronize ();

        m_fifo.close ();
      }

      void addListener (Listener* listener, CallQueue& callQueue)
      {
        // Acquire read access to the shared state.
        ConcurrentState <State>::ReadAccess state (m_state);

        // Add the listener.
        m_listeners.add (listener, callQueue);

        // Queue an update for the listener to receive the initial state.
        m_listeners.queue1 (listener,
                            &Listener::onOutputLevelChanged,
                            state->outputLevel);
      }

      void removeListener (Listener* listener)
      {
        m_listeners.remove (listener);
      }

    protected:
      void audioDeviceIOCallback (const float** inputChannelData,
							      int numInputChannels,
							      float** outputChannelData,
							      int numOutputChannels,
							      int numSamples)
      {
        // Synchronize our call queue. Not needed for this example but
        // included here as a best-practice for audio device I/O callbacks.
        m_fifo.synchronize ();

        // (Process audio data)

        // Calculate output level.
        float newOutputLevel = calcOutputLevel ();

        // Update shared state.
        {
          ConcurrentState <State>::WriteAccess state (m_state);
          
          m_state->outputLevel = newOutputLevel;
        }

        // Notify listeners.
        listeners.call (&Listener::onOutputLevelChanged, newOutputLevel);
      }

    private:
      struct State
      {
        State () : outputLevel (0) { }

        float outputLevel;
      };

      ConcurrentState <State> m_state;

      ManualCallQueue m_fifo;
    };

    @endcode

    Although the rigor demonstrated in the example above is not strictly required
    when the shared state consists only of a single float, it becomes necessary
    when there are dynamically allocated objects with complex interactions in the
    shared state.

    @see CallQueue
*/

#ifndef DOXYGEN
class ListenersBase
{
public:
  struct ListenersStructureTag { };

  typedef GlobalFifoFreeStore <ListenersStructureTag> AllocatorType;

  typedef GlobalFifoFreeStore <ListenersBase> CallAllocatorType;

  class Call : public ReferenceCountedObject,
               public AllocatedBy <CallAllocatorType>
  {
  public:
    typedef ReferenceCountedObjectPtr <Call> Ptr;
    virtual void operator () (void* const listener) = 0;
  };

private:
  typedef unsigned long timestamp_t;

  class Group;
  typedef List <Group> Groups;

  class Proxy;
  typedef List <Proxy> Proxies;

  class CallWork;
  class GroupWork;
  class GroupWork1;

  // Maintains a list of listeners registered on the same CallQueue
  //
  class Group : public Groups::Node,
                public ReferenceCountedObject,
                public AllocatedBy <AllocatorType>
  {
  public:
    typedef ReferenceCountedObjectPtr <Group> Ptr;

    explicit Group    (CallQueue& callQueue);
    ~Group            ();
    void add          (void* listener, const timestamp_t timestamp,
                       AllocatorType& allocator);
    bool remove       (void* listener);
    bool contains     (void* const listener);
    void call         (Call* const c, const timestamp_t timestamp);
    void queue        (Call* const c, const timestamp_t timestamp);
    void call1        (Call* const c, const timestamp_t timestamp,
                       void* const listener);
    void queue1       (Call* const c, const timestamp_t timestamp,
                       void* const listener);
    void do_call      (Call* const c, const timestamp_t timestamp);
    void do_call1     (Call* const c, const timestamp_t timestamp,
                       void* const listener);

    bool empty        () const { return m_list.empty(); }
    CallQueue& getCallQueue () const { return m_fifo; }

  private:
    struct Entry;
    typedef List <Entry> List;

    CallQueue& m_fifo;
    List m_list;
    void* m_listener;
    CacheLine::Aligned <ReadWriteMutex> m_mutex;
  };

  // A Proxy is keyed to a unique pointer-to-member of a
  // ListenerClass and is used to consolidate multiple unprocessed
  // Calls into a single call to prevent excess messaging. It is up
  // to the user of the class to decide when this behavior is appropriate.
  //
  class Proxy : public Proxies::Node,
                public AllocatedBy <AllocatorType>
  {
  public:
    enum
    {
      maxMemberBytes = 16
    };

    Proxy (void const* const member, const size_t bytes);
    ~Proxy ();

    void add    (Group* group, AllocatorType& allocator);
    void remove (Group* group);
    void update (Call* const c, const timestamp_t timestamp);

    bool match  (void const* const member, const size_t bytes) const;

  private:
    class Work;
    struct Entry;
    typedef List <Entry> Entries;
    char m_member [maxMemberBytes];
    const size_t m_bytes;
    Entries m_entries;
  };

public:
  ListenersBase ();
  ~ListenersBase ();

  inline CallAllocatorType& getCallAllocator ()
  {
    return *m_callAllocator;
  }

  void callp        (Call::Ptr c);
  void queuep       (Call::Ptr c);

protected:
  void add_void     (void* const listener, CallQueue& callQueue);
  void remove_void  (void* const listener);
  void call1p_void  (void* const listener, Call* c);
  void queue1p_void (void* const listener, Call* c);
  void updatep      (void const* const member,
                     const size_t bytes, Call::Ptr cp);

private:
  Proxy* find_proxy (const void* member, int bytes);

private:
  Groups m_groups;
  Proxies m_proxies;
  timestamp_t m_timestamp;
  CacheLine::Aligned <ReadWriteMutex> m_groups_mutex;
  CacheLine::Aligned <ReadWriteMutex> m_proxies_mutex;
  AllocatorType::Ptr m_allocator;
  CallAllocatorType::Ptr m_callAllocator;
};

#endif

//------------------------------------------------------------------------------

template <class ListenerClass>
class Listeners : public ListenersBase
{
private:
#ifndef DOXYGEN
  template <class Functor>
  class CallType : public Call
  {
  public:
    CallType (const Functor& f) : m_f (f)
    {
    }

    void operator() (void* const listener)
    {
      m_f.operator() (static_cast <ListenerClass*> (listener));
    }

  private:
    Functor m_f;
  };
#endif

public:
  //
  // Add a listener to receive call notifications.
  //
  //  #1 All other functions are blocked during add().
  //  #2 The listener's receipt of every subsequent call() is guaranteed.
  //  #3 Member functions are called on the specified thread queue.
  //  #4 The listener must not already exist in the list.
  //  #5 This can be called from any thread.
  // 
  void add (ListenerClass* const listener, CallQueue& callQueue)
  {
    add_void (listener, callQueue);
  }
  void add (ListenerClass* const listener, CallQueue* callQueue)
  {
    add (listener, *callQueue);
  }

  //
  // Remove a listener from the list
  //
  //  #1 All other functions are blocked during remove().
  //  #2 The listener is guaranteed not to receive calls
  //     after remove() returns.
  //  #3 The listener must exist in the list.
  //  #4 This can be called from any thread.
  //
  // A listener should always be removed before it's corresponding
  //   CallQueue is closed.
  //
  void remove (ListenerClass* const listener)
  {
    remove_void (listener);
  }

  //
  // Call a specified member function on every listener's associated
  // CallQueue with the given functor.
  //
  //  #1 The arguments must match the function signature.
  //  #2 A listener that removes itself afterwards may not get called.
  //  #3 Calls from the same thread always execute in order.
  //  #4 Listener members are always invoked immediately in call() by the
  //     current thread of execution if it matches the thread used
  //     by the listener's thread queue. This happens before call() returns.
  //  #5 A listener can always remove itself even if there are pending calls.
  //

  // Queue a call to a single listener.
  // The CallQueue is processed if called on the associated thread.
  //
  inline void call1p (ListenerClass* const listener, Call::Ptr c)
  {
    call1p_void (listener, c);
  }

  // Queue a call to a single listener.
  //
  inline void queue1p (ListenerClass* const listener, Call::Ptr c)
  {
    queue1p_void (listener, c);
  }

  // Queue a call to all listeners.
  // The CallQueue is processed if called on the associated thread.
  //
  template <class Functor>
  inline void callf (const Functor& f)
  {
    callp (new (getCallAllocator ()) CallType <Functor> (f));
  }

  // Queue a call to all listeners.
  //
  template <class Functor>
  inline void queuef (const Functor& f)
  {
    queuep (new (getCallAllocator ()) CallType <Functor> (f));
  }

  template <class Functor>
  inline void call1f (ListenerClass* const listener, const Functor& f)
  {
    call1p (listener, new (getCallAllocator ()) CallType <Functor> (f));
  }

  template <class Functor>
  inline void queue1f (ListenerClass* const listener, const Functor& f)
  {
    queue1p (listener, new (getCallAllocator ()) CallType <Functor> (f));
  }

  template <class Member, class Functor>
  inline void updatef (Member member, const Functor& f)
  {
    updatep (reinterpret_cast <void*> (&member), sizeof (Member),
             new (getCallAllocator ()) CallType <Functor> (f));
  }

  template <class Mf>
  inline void call (Mf mf)
  { callf (vf::bind (mf, vf::_1)); }

  template <class Mf, typename  T1>
  inline void call (Mf mf,   const T1& t1)
  { callf (vf::bind (mf, vf::_1, t1)); }

  template <class Mf, typename  T1, typename  T2>
  inline void call (Mf mf,   const T1& t1, const T2& t2)
  { callf (vf::bind (mf, vf::_1, t1, t2)); }

  template <class Mf, typename  T1, typename  T2, typename  T3>
  inline void call (Mf mf,   const T1& t1, const T2& t2, const T3& t3)
  { callf (vf::bind (mf, vf::_1, t1, t2, t3)); }

  template <class Mf, typename  T1, typename  T2,
                      typename  T3, typename  T4>
  inline void call (Mf mf,   const T1& t1, const T2& t2,
                      const T3& t3, const T4& t4)
  { callf (vf::bind (mf, vf::_1, t1, t2, t3, t4)); }

  template <class Mf, typename  T1, typename  T2, typename  T3,
                      typename  T4, typename  T5>
  inline void call (Mf mf,   const T1& t1, const T2& t2, const T3& t3,
                      const T4& t4, const T5& t5)
  { callf (vf::bind (mf, vf::_1, t1, t2, t3, t4, t5)); }

  template <class Mf, typename  T1, typename  T2, typename  T3,
                      typename  T4, typename  T5, typename  T6>
  inline void call (Mf mf,   const T1& t1, const T2& t2, const T3& t3,
                      const T4& t4, const T5& t5, const T6& t6)
  { callf (vf::bind (mf, vf::_1, t1, t2, t3, t4, t5, t6)); }

  template <class Mf, typename  T1, typename  T2, typename  T3, typename  T4,
                      typename  T5, typename  T6, typename  T7>
  inline void call (Mf mf,   const T1& t1, const T2& t2, const T3& t3, const T4& t4,
                      const T5& t5, const T6& t6, const T7& t7)
  { callf (vf::bind (mf, vf::_1, t1, t2, t3, t4, t5, t6, t7)); }

  template <class Mf, typename  T1, typename  T2, typename  T3, typename  T4,
                      typename  T5, typename  T6, typename  T7, typename  T8>
  inline void call (Mf mf,   const T1& t1, const T2& t2, const T3& t3, const T4& t4,
                      const T5& t5, const T6& t6, const T7& t7, const T8& t8)
  { callf (vf::bind (mf, vf::_1, t1, t2, t3, t4, t5, t6, t7, t8)); }

  //
  // Queue a call without synchronizing
  //

  template <class Mf>
  inline void queue (Mf mf)
  { queuef (vf::bind (mf, vf::_1)); }

  template <class Mf, typename  T1>
  inline void queue (Mf mf,  const T1& t1)
  { queuef (vf::bind (mf, vf::_1, t1)); }

  template <class Mf, typename  T1, typename  T2>
  inline void queue (Mf mf,  const T1& t1, const T2& t2)
  { queuef (vf::bind (mf, vf::_1, t1, t2)); }

  template <class Mf, typename  T1, typename  T2, typename  T3>
  inline void queue (Mf mf,  const T1& t1, const T2& t2, const T3& t3)
  { queuef (vf::bind (mf, vf::_1, t1, t2, t3)); }

  template <class Mf, typename  T1, typename  T2,
                      typename  T3, typename  T4>
  inline void queue (Mf mf,  const T1& t1, const T2& t2,
                      const T3& t3, const T4& t4)
  { queuef (vf::bind (mf, vf::_1, t1, t2, t3, t4)); }

  template <class Mf, typename  T1, typename  T2, typename  T3,
                      typename  T4, typename  T5>
  inline void queue (Mf mf,  const T1& t1, const T2& t2, const T3& t3,
                      const T4& t4, const T5& t5)
  { queuef (vf::bind (mf, vf::_1, t1, t2, t3, t4, t5)); }

  template <class Mf, typename  T1, typename  T2, typename  T3,
                      typename  T4, typename  T5, typename  T6>
  inline void queue (Mf mf,  const T1& t1, const T2& t2, const T3& t3,
                      const T4& t4, const T5& t5, const T6& t6)
  { queuef (vf::bind (mf, vf::_1, t1, t2, t3, t4, t5, t6)); }

  template <class Mf, typename  T1, typename  T2, typename  T3, typename  T4,
                      typename  T5, typename  T6, typename  T7>
  inline void queue (Mf mf,  const T1& t1, const T2& t2, const T3& t3, const T4& t4,
                      const T5& t5, const T6& t6, const T7& t7)
  { queuef (vf::bind (mf, vf::_1, t1, t2, t3, t4, t5, t6, t7)); }

  template <class Mf, typename  T1, typename  T2, typename  T3, typename  T4,
                      typename  T5, typename  T6, typename  T7, typename  T8>
  inline void queue (Mf mf,  const T1& t1, const T2& t2, const T3& t3, const T4& t4,
                      const T5& t5, const T6& t6, const T7& t7, const T8& t8)
  { queuef (vf::bind (mf, vf::_1, t1, t2, t3, t4, t5, t6, t7, t8)); }

  // These are for targeting individual listeners.
  // Use carefully!

  template <class Mf>
  inline void call1 (ListenerClass* const listener, Mf mf)
  { call1f (listener, vf::bind (mf, vf::_1)); }

  template <class Mf, typename  T1>
  inline void call1 (ListenerClass* const listener,
               Mf mf, const T1& t1)
  { call1f (listener, vf::bind (mf, vf::_1, t1)); }

  template <class Mf, typename  T1, typename  T2>
  inline void call1 (ListenerClass* const listener,
               Mf mf, const T1& t1, const T2& t2)
  { call1f (listener, vf::bind (mf, vf::_1, t1, t2)); }

  template <class Mf, typename  T1, typename  T2, typename  T3>
  inline void call1 (ListenerClass* const listener,
               Mf mf, const T1& t1, const T2& t2, const T3& t3)
  { call1f (listener, vf::bind (mf, vf::_1, t1, t2, t3)); }

  template <class Mf, typename  T1, typename  T2,
                      typename  T3, typename  T4>
  inline void call1 (ListenerClass* const listener,
               Mf mf, const T1& t1, const T2& t2,
                      const T3& t3, const T4& t4)
  { call1f (listener, vf::bind (mf, vf::_1, t1, t2, t3, t4)); }

  template <class Mf, typename  T1, typename  T2, typename  T3,
                      typename  T4, typename  T5>
  inline void call1 (ListenerClass* const listener,
               Mf mf, const T1& t1, const T2& t2, const T3& t3,
                      const T4& t4, const T5& t5)
  { call1f (listener, vf::bind (mf, vf::_1, t1, t2, t3, t4, t5)); }

  template <class Mf, typename  T1, typename  T2, typename  T3,
                      typename  T4, typename  T5, typename  T6>
  inline void call1 (ListenerClass* const listener,
               Mf mf, const T1& t1, const T2& t2, const T3& t3,
                      const T4& t4, const T5& t5, const T6& t6)
  { call1f (listener, vf::bind (mf, vf::_1, t1, t2, t3, t4, t5, t6)); }

  template <class Mf, typename  T1, typename  T2, typename  T3, typename  T4,
                      typename  T5, typename  T6, typename  T7>
  inline void call1 (ListenerClass* const listener,
               Mf mf, const T1& t1, const T2& t2, const T3& t3, const T4& t4,
                      const T5& t5, const T6& t6, const T7& t7)
  { call1f (listener, vf::bind (mf, vf::_1, t1, t2, t3, t4, t5, t6, t7)); }

  template <class Mf, typename  T1, typename  T2, typename  T3, typename  T4,
                      typename  T5, typename  T6, typename  T7, typename  T8>
  inline void call1 (ListenerClass* const listener,
               Mf mf, const T1& t1, const T2& t2, const T3& t3, const T4& t4,
                      const T5& t5, const T6& t6, const T7& t7, const T8& t8)
  { call1f (listener, vf::bind (mf, vf::_1, t1, t2, t3, t4, t5, t6, t7, t8)); }

  template <class Mf>
  inline void queue1 (ListenerClass* const listener, Mf mf)
  { queue1f (listener, vf::bind (mf, vf::_1)); }

  template <class Mf, typename  T1>
  inline void queue1 (ListenerClass* const listener,
               Mf mf, const T1& t1)
  { queue1f (listener, vf::bind (mf, vf::_1, t1)); }

  template <class Mf, typename  T1, typename  T2>
  inline void queue1 (ListenerClass* const listener,
               Mf mf, const T1& t1, const T2& t2)
  { queue1f (listener, vf::bind (mf, vf::_1, t1, t2)); }

  template <class Mf, typename  T1, typename  T2, typename  T3>
  inline void queue1 (ListenerClass* const listener,
               Mf mf, const T1& t1, const T2& t2, const T3& t3)
  { queue1f (listener, vf::bind (mf, vf::_1, t1, t2, t3)); }

  template <class Mf, typename  T1, typename  T2,
                      typename  T3, typename  T4>
  inline void queue1 (ListenerClass* const listener,
               Mf mf, const T1& t1, const T2& t2,
                      const T3& t3, const T4& t4)
  { queue1f (listener, vf::bind (mf, vf::_1, t1, t2, t3, t4)); }

  template <class Mf, typename  T1, typename  T2, typename  T3,
                      typename  T4, typename  T5>
  inline void queue1 (ListenerClass* const listener,
               Mf mf, const T1& t1, const T2& t2, const T3& t3,
                      const T4& t4, const T5& t5)
  { queue1f (listener, vf::bind (mf, vf::_1, t1, t2, t3, t4, t5)); }

  template <class Mf, typename  T1, typename  T2, typename  T3,
                      typename  T4, typename  T5, typename  T6>
  inline void queue1 (ListenerClass* const listener,
               Mf mf, const T1& t1, const T2& t2, const T3& t3,
                      const T4& t4, const T5& t5, const T6& t6)
  { queue1f (listener, vf::bind (mf, vf::_1, t1, t2, t3, t4, t5, t6)); }

  template <class Mf, typename  T1, typename  T2, typename  T3, typename  T4,
                      typename  T5, typename  T6, typename  T7>
  inline void queue1 (ListenerClass* const listener,
               Mf mf, const T1& t1, const T2& t2, const T3& t3, const T4& t4,
                      const T5& t5, const T6& t6, const T7& t7)
  { queue1f (listener, vf::bind (mf, vf::_1, t1, t2, t3, t4, t5, t6, t7)); }

  template <class Mf, typename  T1, typename  T2, typename  T3, typename  T4,
                      typename  T5, typename  T6, typename  T7, typename  T8>
  inline void queue1 (ListenerClass* const listener,
               Mf mf, const T1& t1, const T2& t2, const T3& t3, const T4& t4,
                      const T5& t5, const T6& t6, const T7& t7, const T8& t8)
  { queue1f (listener, vf::bind (mf, vf::_1, t1, t2, t3, t4, t5, t6, t7, t8)); }

  //
  // update()
  //
  // Like call(), but if there is a previous unprocessed call for
  // the same member f, the previous call is replaced. It is
  // up to the caller to determine if this behavior is desired.
  //

  template <class Mf>
  inline void update (Mf mf)
  { updatef (mf, vf::bind (mf, vf::_1)); }

  template <class Mf, typename  T1>
  inline void update (Mf mf,  const T1& t1)
  { updatef (mf, vf::bind (mf, vf::_1, t1)); }

  template <class Mf, typename  T1, typename  T2>
  inline void update (Mf mf, const T1& t1, const T2& t2)
  { updatef (mf, vf::bind (mf, vf::_1, t1, t2)); }

  template <class Mf, typename  T1, typename  T2, typename  T3>
  inline void update (Mf mf, const T1& t1, const T2& t2, const T3& t3)
  { updatef (mf, vf::bind (mf, vf::_1, t1, t2, t3)); }

  template <class Mf, typename  T1, typename  T2,
                      typename  T3, typename  T4>
  inline void update (Mf mf, const T1& t1, const T2& t2,
                      const T3& t3, const T4& t4)
  { updatef (mf, vf::bind (mf, vf::_1, t1, t2, t3, t4)); }

  template <class Mf, typename  T1, typename  T2, typename  T3,
                      typename  T4, typename  T5>
  inline void update (Mf mf, const T1& t1, const T2& t2, const T3& t3,
                      const T4& t4, const T5& t5)
  { updatef (mf, vf::bind (mf, vf::_1, t1, t2, t3, t4, t5)); }

  template <class Mf, typename  T1, typename  T2, typename  T3,
                      typename  T4, typename  T5, typename  T6>
  inline void update (Mf mf, const T1& t1, const T2& t2, const T3& t3,
                      const T4& t4, const T5& t5, const T6& t6)
  { updatef (mf, vf::bind (mf, vf::_1, t1, t2, t3, t4, t5, t6)); }

  template <class Mf, typename  T1, typename  T2, typename  T3, typename  T4,
                      typename  T5, typename  T6, typename  T7>
  inline void update (Mf mf, const T1& t1, const T2& t2, const T3& t3, const T4& t4,
                      const T5& t5, const T6& t6, const T7& t7)
  { updatef (mf, vf::bind (mf, vf::_1, t1, t2, t3, t4, t5, t6, t7)); }

  template <class Mf, typename  T1, typename  T2, typename  T3, typename  T4,
                      typename  T5, typename  T6, typename  T7, typename  T8>
  inline void update (Mf mf, const T1& t1, const T2& t2, const T3& t3, const T4& t4,
                      const T5& t5, const T6& t6, const T7& t7, const T8& t8)
  { updatef (mf, vf::bind (mf, vf::_1, t1, t2, t3, t4, t5, t6, t7, t8)); }
};

#endif
