#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "args.h"
#include "common.h"
#include "audiobackend/ring_buffer.h"
#include "audiobackend/audio.h"

static void miniaudio_data_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount);
static int  select_device(ma_device_info* infos, ma_uint32 count, bool useDefaults);

/**
 * Initialise an audio engine with the given read and write buffers. The engine
 * will read audio data from the playback buffer, and write mic data to the 
 * capture buffer.
 * 
 * Will fail if an error happens.
 */
void init_audio_engine(struct audio_engine* engine, struct ring_buffer* playback, struct ring_buffer* capture, struct program_conf* conf) {
    if (playback == NULL || capture == NULL) {
        error("Ring buffers point to NULL in audio engine");
    }

    ma_result res;

    // Save ring buffers
    engine->playback = playback;
    engine->capture = capture;

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
    ma_uint32 playbackDeviceSelection = select_device(pPlaybackInfos, playbackCount, conf->useDefaults);
    ma_uint32 captureDeviceSelection  = select_device(pCaptureInfos, captureCount, conf->useDefaults);

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
void destroy_audio_engine(struct audio_engine* engine) {
    ma_device_uninit(&engine->device);
    ma_context_uninit(&engine->context);
}

int audio_engine_start(struct audio_engine* engine) {
    ma_result res;

    if ((res = ma_device_start(&engine->device)) != MA_SUCCESS) {
        ma_warn("Failed to start audio device", res);
        return ST_FAIL;
    }

    return ST_GOOD;
}

int audio_engine_stop(struct audio_engine* engine) {
    ma_result res;

    if ((res = ma_device_stop(&engine->device)) != MA_SUCCESS) {
        ma_warn("Failed to stop audio device", res);
        return ST_FAIL;
    }

    return ST_GOOD;
}

/**
 * Main miniaudio device data callback.
 * 
 * This function is called from within the miniaudio worker thread.
 */
static void miniaudio_data_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount) {
    // Extract engine from data
    struct audio_engine* engine = (struct audio_engine*) pDevice->pUserData;
    
    size_t sizeBytes;
    size_t expected = frameCount * FRAME_SIZE;
    void* buffer;

    if (pOutput != NULL) {
        sizeBytes = expected;
        if (ring_buffer_acquire_read(engine->playback, &sizeBytes, &buffer) != ST_GOOD) {
            warn("Failed to acquire a read pointer to the ring buffer");
            goto write_playback;
        }

        info("bytes to read : %zu", sizeBytes);

        memcpy(pOutput, buffer, sizeBytes);

        if (ring_buffer_commit_read(engine->playback, sizeBytes) != ST_GOOD) {
            warn("Failed to commit a read to the ring buffer");
            goto write_playback;
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