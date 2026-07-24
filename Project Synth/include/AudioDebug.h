#ifndef AUDIODEBUG_H
#define AUDIODEBUG_H

// Set to 1 to re-enable verbose logging in the audio-generation hot path
// (Wave, ADSR, Voice, State::render_voices/generate_audio). Off by
// default: these calls run at audio sample rate (up to 44100 * active
// voice count per second), and Serial I/O in that path can stall the
// Bluetooth A2DP callback, causing audible glitches.
#define AUDIO_DEBUG 0

#endif // AUDIODEBUG_H
