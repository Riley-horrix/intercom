#include "miniaudio.h"

#include <stdio.h>

int main(int argc, char** argv) {
    if (argc != 2) {
        printf("Specify file path!\n");
        return 1;
    }

    const char* filePath = argv[1];

    ma_result result;
    ma_engine engine;

    result = ma_engine_init(NULL, &engine);
    if (result != MA_SUCCESS) {
        return -1;
    }

    ma_engine_play_sound(&engine, filePath, NULL);

    printf("Press Enter to quit...");
    getchar();

    ma_engine_uninit(&engine);

    return 0;
}