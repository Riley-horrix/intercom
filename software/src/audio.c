#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "args.h"
#include "common.h"
#include "ring_buffer.h"
#include "audio.h"

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
    if (engine->initialised) {
        return;
    }

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
    engine->config = ma_device_config_init(ma_device_type_duplex);

    engine->config.capture.channels  = CHANNELS;
    engine->config.playback.channels = CHANNELS;
    engine->config.capture.format  = FORMAT;
    engine->config.playback.format = FORMAT;

    engine->config.capture.pDeviceID  = &pCaptureInfos[captureDeviceSelection].id;
    engine->config.playback.pDeviceID = &pPlaybackInfos[playbackDeviceSelection].id;

    engine->config.sampleRate   = SAMPLE_RATE;
    engine->config.dataCallback = &miniaudio_data_callback;

    // Set the engine as the user data
    engine->config.pUserData    = engine;

    if ((res = ma_device_init(&engine->context, &engine->config, &engine->device)) != MA_SUCCESS) {
        ma_error("Failed to initialise device", res);
    }

    if ((res = ma_device_start(&engine->device)) != MA_SUCCESS) {
        ma_device_uninit(&engine->device);
        ma_error("Failed to start playback device", res);
    }

    engine->initialised = true;
}

/**
 * Destroy the given audio engine.
 * 
 * This function does not destroy the ring buffers.
 */
void destroy_audio_engine(struct audio_engine* engine) {
    if (!engine->initialised) {
        return;
    }
    ma_device_uninit(&engine->device);
    engine->initialised = false;
}

static void miniaudio_data_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount) {
    // Extract engine from data
    struct audio_engine* engine = (struct audio_engine*) pDevice->pUserData;

    if (pOutput != NULL) {
        size_t sizeBytes = frameCount * FRAME_SIZE;
        void* buffer;

        if (ring_buffer_acquire_read(engine->playback, &sizeBytes, &buffer) != ST_GOOD) {
            warn("err 10");
        }

        memcpy(pOutput, buffer, sizeBytes);

        if (ring_buffer_commit_read(engine->playback, sizeBytes) != ST_GOOD) {
            warn("err 11");
        }
    }
    
    if (pInput != NULL) {
        // Dont input for now
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