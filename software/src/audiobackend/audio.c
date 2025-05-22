#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "utils/args.h"
#include "common.h"
#include "audiobackend/ring_buffer.h"
#include "audiobackend/audio.h"

static void init_biquad(ma_biquad* biquad);
static void miniaudio_data_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount);
static int  select_device(ma_device_info* infos, ma_uint32 count, bool useDefaults);

/**
 * Initialise an audio engine with the given read and write buffers. The engine
 * will read audio data from the playback buffer, and write mic data to the 
 * capture buffer.
 * 
 * Will fail if an error happens.
 */
void init_audio_engine(audio_engine_t* engine, ring_buffer_t* playback, ring_buffer_t* capture, intercom_conf_t* conf) {
    if (playback == NULL || capture == NULL) {
        error("Ring buffers point to NULL in audio engine");
    }

    ma_result res;

    // Save ring buffers
    engine->playback = playback;
    engine->capture = capture;

    // Initialise biquad filter
    init_biquad(&engine->biquad);

    // Do device search & user selection
    if ((res = ma_context_init(NULL, 0, NULL, &engine->context)) != MA_SUCCESS) {
        ma_error("Failed to initialise miniaudio context", res);
    }

    ma_device_info* pPlaybackInfos;
    ma_device_info* pCaptureInfos;
    ma_uint32 playbackCount;
    ma_uint32 captureCount;

    // TODO : ma_context_enumerate_devices() may be more efficient here
    if ((res = ma_context_get_devices(&engine->context, &pPlaybackInfos, &playbackCount, &pCaptureInfos, &captureCount)) != MA_SUCCESS) {
        ma_error("Failed to get miniaudio devices", res);
    }
    
    // User selections for devices
    ma_uint32 playbackDeviceSelection = select_device(pPlaybackInfos, playbackCount, conf->use_audio_defaults);
    ma_uint32 captureDeviceSelection  = select_device(pCaptureInfos, captureCount, conf->use_audio_defaults);

    // Set the device configurations.
    ma_device_config config = ma_device_config_init(ma_device_type_duplex);

    config.capture.channels  = CHANNELS;
    config.playback.channels = CHANNELS;
    config.capture.format  = FORMAT;
    config.playback.format = FORMAT;

    config.capture.pDeviceID  = &pCaptureInfos[captureDeviceSelection].id;
    config.playback.pDeviceID = &pPlaybackInfos[playbackDeviceSelection].id;

    config.sampleRate   = SAMPLE_RATE;
    config.dataCallback = &miniaudio_data_callback;

    // Set the engine as the user data
    config.pUserData = engine;

    if ((res = ma_device_init(&engine->context, &config, &engine->device)) != MA_SUCCESS) {
        ma_error("Failed to initialise device", res);
    }
}

/**
 * Destroy the given audio engine.
 * 
 * This function does not destroy the ring buffers.
 */
void destroy_audio_engine(audio_engine_t* engine) {
    ma_device_uninit(&engine->device);
    ma_context_uninit(&engine->context);

    destroy_shared_memory(engine, sizeof(*engine));
}

int audio_engine_start(audio_engine_t* engine) {
    ma_result res;

    if ((res = ma_device_start(&engine->device)) != MA_SUCCESS) {
        ma_warn("Failed to start audio device", res);
        return ST_FAIL;
    }

    return ST_GOOD;
}

int audio_engine_stop(audio_engine_t* engine) {
    ma_result res;

    if ((res = ma_device_stop(&engine->device)) != MA_SUCCESS) {
        ma_warn("Failed to stop audio device", res);
        return ST_FAIL;
    }

    return ST_GOOD;
}

static void init_biquad(ma_biquad* biquad) {

    #if FORMAT != ma_format_s16 && FORMAT != ma_format_f32
        #error "Cannot initialise biquad filter with current format"
    #endif

    // Need to calculate coefficients for a band pass filter
    // https://webaudio.github.io/Audio-EQ-Cookbook/audio-eq-cookbook.html
    // BPF (constant 0 dB peak gain)

    const double samplingFrequency = SAMPLE_RATE;
    const double centreFrequency = 1500;
    const double Q = 0.707;

    const double w0 = 2.0 * M_PI * centreFrequency / samplingFrequency;
    const double a = sin(2.0 * w0) / (2.0 * Q);

    const double a0 = 1.0 + a;
    const double a1 = -2 * cos(w0);
    const double a2 = 1.0 - a;
    
    const double b0 = a;
    const double b1 = 0;
    const double b2 = -a;

    ma_result result;
    ma_biquad_config config = ma_biquad_config_init(FORMAT, CHANNELS, b0, b1, b2, a0, a1, a2);
    
    if ((result = ma_biquad_init(&config, NULL, biquad)) != MA_SUCCESS) {
        ma_error("Failed to initialise biquad filter", result);
    }
}

/**
 * Main miniaudio device data callback.
 * 
 * This function is called from within the miniaudio worker thread.
 */
static void miniaudio_data_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount) {
    // Extract engine from data
    audio_engine_t* engine = (audio_engine_t*)pDevice->pUserData;
    
    ma_result res;
    const size_t expected = frameCount * FRAME_SIZE;
    size_t sizeBytes;
    void* buffer;

    if (pOutput != NULL) {
        // Read audio data from ring buffer
        sizeBytes = expected;
        if (ring_buffer_acquire_read(engine->playback, &sizeBytes, &buffer) != ST_GOOD) {
            warn("Failed to acquire a read pointer to the ring buffer");
            goto write_playback;
        }

        // Apply a band pass filter on the audio data
        if ((res = ma_biquad_process_pcm_frames(&engine->biquad, pOutput, buffer, frameCount)) != MA_SUCCESS) {
            ma_warn("Audio engine failed to apply filter", res);
            memcpy(pOutput, buffer, frameCount);
        }

        if (ring_buffer_commit_read(engine->playback, sizeBytes) != ST_GOOD) {
            warn("Failed to commit a read to the ring buffer");
        }
    }

write_playback:

    if (pInput != NULL) {
        sizeBytes = expected;
        if (ring_buffer_acquire_write(engine->capture, &sizeBytes, &buffer) != ST_GOOD) {
            warn("Failed to acquire a write pointer to the ring buffer");
            return;
        }

        memcpy(buffer, pInput, sizeBytes);

        if (ring_buffer_commit_write(engine->capture, sizeBytes) != ST_GOOD) {
            warn("Failed to commit a write to the ring buffer");
            return;
        }
    }
}

/**
 * Iterate through the device info.
 * 
 * If useDefaults is not set then it will prompt the user for their selected
 * audio device.
 * 
 * Returns selected device index.
 */
static int select_device(ma_device_info* infos, ma_uint32 count, bool useDefaults) {
    ma_uint32 selection = ~0x0;
    // Loop over each device info
    if (!useDefaults) {
        info("Listing available playback devices");
        printf("\n");
    }
    for (ma_uint32 i = 0; i < count; i++) {
        ma_device_info* info = &infos[i];

        if (info->isDefault && useDefaults) {
            info("Device selected : %s", infos[i].name);
            return i;
        }

        if (!useDefaults) {
            printf("[%d]: %s%s\n", i, infos[i].name, infos[i].isDefault ? " (default)" : "");
        }
    }
    printf("\n");

    prompt("Select playback device : ");
    while (scanf("%u", &selection) != 1 || selection >= count) {
        warn("Invalid device selection");

        // Remove possible leftover characters from input buffer
        char ch;
        while ((ch = getchar()) != '\n' && ch != EOF);

        prompt("Select playback device : ");
    }

    info("Device selected : %s", infos[selection].name);
    return (ma_uint32) selection;
}