/*============================================================================*/
/*
  Copyright (C) 2008 by Vinnie Falco, this file is part of VFLib.
  See the file GNU_GPL_v2.txt for full licensing terms.

  This program is free software; you can redistribute it and/or modify it
  under the terms of the GNU General Public License as published by the Free
  Software Foundation; either version 2 of the License, or (at your option)
  any later version.

  This program is distributed in the hope that it will be useful, but WITHOUT
  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
  FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
  details.

  You should have received a copy of the GNU General Public License along with
  this program; if not, write to the Free Software Foundation, Inc., 51
  Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/
/*============================================================================*/

#ifndef VF_SEMAPHORE_VFHEADER
#define VF_SEMAPHORE_VFHEADER

/*============================================================================*/
/**
  @ingroup vf_core

  @brief A semaphore.

  This implementation is not the best.
*/
class Semaphore
{
public:
  Semaphore (int initialCount = 0);

  ~Semaphore ();

  void signal (int amount = 1);

  /** Returns true if the resource was acquired without a timeout.
  */
  bool wait (int timeoutMilliseconds = -1);

private:
  class WaitingThread : public List <WaitingThread>::Node
  {
  public:
    WaitingThread ()
      : m_event (false) // auto-reset
      , m_signaled (false)
    {
    }

    WaitableEvent m_event;
    bool volatile m_signaled;
  };

  int m_counter;
  CriticalSection m_mutex;
  List <WaitingThread> m_waitingThreads;
};

#endif
