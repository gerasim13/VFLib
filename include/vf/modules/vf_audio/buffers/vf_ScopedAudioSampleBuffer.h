//==============================================================================
//
// Copyright (C) 2008 by Vinnie Falco, this file is part of VFLib.
// See the file GNU_GPL_v2.txt for full licensing terms.
// 
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 2 of the License, or (at your option)
// any later version.
// 
// This program is distributed in the hope that it will be useful, but WITHOUT
// ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
// FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
// details.
// 
// You should have received a copy of the GNU General Public License along with
// this program; if not, write to the Free Software Foundation, Inc., 51
// Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
// 
//==============================================================================

#ifndef VF_SCOPEDAUDIOSAMPLEBUFFER_VFHEADER
#define VF_SCOPEDAUDIOSAMPLEBUFFER_VFHEADER

//==============================================================================
/**
    Scoped temporary audio buffer from a pool.

    This utility class allows scoped lifetime management for acquiring temporary
    audio buffers from the AudioBufferPool. It is easy to use:

    @code

    AudioBufferPoolType <CriticalSection> pool;

    // Request a stereo buffer with room for 1024 samples.
    //
    ScopedAudioSampleBuffer buffer (pool, 2, 1024);

    // `buffer` is released when it goes out of scope.

    @endcode

    ScopedAudioSampleBuffer is freely convertible to AudioSampleBuffer* so it
    can be used anywhere a pointer to AudioSampleBuffer is expected. The
    dereference and pointer to member operators are similarly overloaded to
    support transparent usage of the underlying AudioSampleBuffer:

    @code

    AudioBufferPoolType <CriticalSection> pool;

    ScopedAudioSampleBuffer buffer (pool, 2, 1024);

    // Call a member of AudioSampleBuffer
    //
    buffer->clear ();

    @endcode

    Note that changing the size of a buffer is undefined.

    \ingroup vf_audio
*/
class ScopedAudioSampleBuffer
  // NO IDEA why the leak checking fails
  // : LeakChecked <ScopedAudioSampleBuffer>, Uncopyable
{
public:
  /** Acquire a ScopedAudioSampleBuffer from a pool.

      @param numChannels  The number of channels requested.
      @param numSamples   The number of samples per channel requested.
  */
  ScopedAudioSampleBuffer (AudioBufferPool& pool,
	int numChannels,
	int numSamples)
	: m_pool (pool)
	, m_buffer (pool.requestBuffer (numChannels, numSamples))
  {
  }

  /** Destroy the ScopedAudioSampleBuffer, releasing the buffer */
  ~ScopedAudioSampleBuffer ()
  {
	m_pool.releaseBuffer (m_buffer);
  }

  /** Return the object as a AudioSampleBuffer pointer */
  VF_JUCE::AudioSampleBuffer* getBuffer ()
  {
	return m_buffer;
  }

  /** Return the object as a AudioSampleBuffer pointer */
  VF_JUCE::AudioSampleBuffer* operator-> ()
  {
	return getBuffer();
  }

  /** Return the object as a reference to AudioSampleBuffer */
  VF_JUCE::AudioSampleBuffer& operator* ()
  {
	return *getBuffer();
  }

  /** Conversion operator for pointer to AudioSampleBuffer */
  operator VF_JUCE::AudioSampleBuffer* ()
  {
	return getBuffer();
  }

private:
  AudioBufferPool& m_pool;
  AudioBufferPool::Buffer* const m_buffer;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ScopedAudioSampleBuffer);
};

#endif