#ifndef SRC_AUDIO_H
#define SRC_AUDIO_H

#include <stdbool.h>
#include "ring_buffer.h"
#include "miniaudio.h"

// Defines for miniaudio

#define FORMAT ma_format_s16
#define CHANNELS 1
#define FRAME_SIZE ma_get_bytes_per_frame(FORMAT, CHANNELS) // Must update with FORMAT
#define SAMPLE_RATE ma_standard_sample_rate_48000

/**
 * The audio engine is a wrapper around the audio library.
 * 
 * On initialisation, it will provide an interface for selecting which audio
 * device should be used, and do all of the setup related to the low level
 * driver.
 * 
 * When started, it will read data from the device's microphone into a shared
 * buffer, when this is complete it will wake up any threads that are currently
 * waiting on the audio engine's read buffer. At this time it will also check
 * to see if there is any data in the write buffer, and if there is, will write
 * it to the audio device.
 */

struct audio_engine {
    ma_device device;
    struct ring_buffer* playback;
    struct ring_buffer* capture;
    bool initialised;
};


extern int  init_audio_engine(struct audio_engine* engine, struct ring_buffer* playback, struct ring_buffer* capture);
extern void destroy_audio_engine(struct audio_engine* engine);

#endif