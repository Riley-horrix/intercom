#ifndef SRC_AUDIO_H
#define SRC_AUDIO_H

#include <stdbool.h>
#include "miniaudio.h"
#include "args.h"
#include "audiobackend/ring_buffer.h"

// Defines for miniaudio

/**
 * Fail and print an error.
 * 
 * Also prints an error code string for mini_audio, given the ma_result `err`.
 */
#define ma_error(fmt, err, ...) error(fmt ", cause: %s", ma_result_description(err), ##__VA_ARGS__)

/**
 * Print a warning.
 * 
 * Also prints an error code string for mini_audio, given the ma_result `err`.
 */
#define ma_warn(fmt, err, ...) warn(fmt ", cause: %s", ma_result_description(err) , ##__VA_ARGS__)

/**
 * Call the given mini audio function and translate error to ST.
 * 
 * Returns ST_CODE
 */
#define ma_call(f) \
    (f == MA_SUCCESS ? ST_GOOD : ST_FAIL)

#define FORMAT ma_format_s16
#define CHANNELS 1
#define FRAME_SIZE ma_get_bytes_per_frame(FORMAT, CHANNELS)
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
    ma_context context;
    ma_device device;
    ma_biquad biquad;
    struct ring_buffer* playback;
    struct ring_buffer* capture;
};


extern void init_audio_engine(struct audio_engine* engine, struct ring_buffer* playback, struct ring_buffer* capture, struct program_conf* conf);
extern void destroy_audio_engine(struct audio_engine* engine);

extern int audio_engine_start(struct audio_engine* engine);
extern int audio_engine_stop(struct audio_engine* engine);

#endif