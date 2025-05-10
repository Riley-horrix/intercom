#include <stdio.h>
#include <stdlib.h>
#include "common.h"
#include "ring_buffer.h"
#include "audio.h"

static void miniaudio_data_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount);

/**
 * Initialise an audio engine with the given read and write buffers. The engine
 * will read audio data from the playback buffer, and write mic data to the 
 * capture buffer.
 * 
 * Returns ST_CODE.
 */
int init_audio_engine(struct audio_engine* engine, struct ring_buffer* playback, struct ring_buffer* capture) {
    if (engine->initialised) {
        return ST_GOOD;
    }

    if (playback == NULL || capture == NULL) {
        return ST_INVALID_ARG;
    }

    // Save ring buffers
    engine->playback = playback;
    engine->capture = capture;

    // Do device search
    ma_context context;
    if (ma_context_init(NULL, 0, NULL, &context) != MA_SUCCESS) {
        error("Failed to initialise miniaudio context");
    }

    ma_device_info* pPlaybackInfos;
    ma_uint32 playbackCount;
    ma_device_info* pCaptureInfos;
    ma_uint32 captureCount;
    if (ma_context_get_devices(&context, &pPlaybackInfos, &playbackCount, &pCaptureInfos, &captureCount) != MA_SUCCESS) {
        error("Failed to get miniaudio devices");
    }
    
    int playbackDeviceSelection;
    int captureDeviceSelection;

    // Loop over each device info
    info("Listing available playback devices");
    printf("\n");
    for (ma_uint32 i = 0; i < playbackCount; i++) {
        printf("[%d]: %s%s\n", i, pPlaybackInfos[i].name, pPlaybackInfos[i].isDefault ? " (default)" : "");
    }
    printf("\n");

    prompt("Select playback device : ");
    while (scanf("%d", &playbackDeviceSelection) != 1 || playbackDeviceSelection < 0 || playbackDeviceSelection >= playbackCount) {
        warn("Invalid device selection.");
        prompt("Select playback device : ");
    }

    info("Device selected : %s", pPlaybackInfos[playbackDeviceSelection].name);

    info("Listing available capture devices");
    printf("\n");
    for (ma_uint32 i = 0; i < captureCount; i++) {
        printf("[%d]: %s%s\n", i, pCaptureInfos[i].name, pCaptureInfos[i].isDefault ? " (default)" : "");
    }
    printf("\n");

    prompt("Select capture device : ");
    while (scanf("%d", &captureDeviceSelection) != 1 || captureDeviceSelection < 0 || captureDeviceSelection >= captureCount) {
        warn("Invalid device selection.");
        prompt("Select capture device : ");
    }

    info("Device selected : %s", pCaptureInfos[captureDeviceSelection].name);

    ma_device_config config = ma_device_config_init(ma_device_type_duplex);

    config.capture.channels = CHANNELS;
    config.playback.channels = CHANNELS;
    config.capture.format = FORMAT;
    config.playback.format = FORMAT;

    config.capture.pDeviceID = &pPlaybackInfos[captureDeviceSelection].id;
    config.playback.pDeviceID = &pPlaybackInfos[playbackDeviceSelection].id;

    config.sampleRate        = SAMPLE_RATE;
    config.dataCallback      = &miniaudio_data_callback;
    config.pUserData         = engine;



    engine->initialised = true;
    return ST_GOOD;
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

static void miniaudio_data_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount) {}