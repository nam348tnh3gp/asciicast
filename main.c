#include "asciicast.h"
#include <stdio.h>
#include <string.h>

int main(int argc, char **argv)
{
    int withAudio = 1;
    const char *file = NULL;

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-n") || !strcmp(argv[i], "--no-audio"))
            withAudio = 0;
        else if (!strcmp(argv[i], "-c") || !strcmp(argv[i], "--clear-cache")) {
            clearCache();
            return 0;
        } else if (!file)
            file = argv[i];
        else
            file = NULL, i = argc;
    }

    if (!file) {
        fprintf(stderr, "Usage: %s [-n] [-c] <video-file>\n", argv[0]);
        return 1;
    }

    generateFrames(file);
    generateGrayFrames();
    readGenerateASCII(file, withAudio);

    return 0;
}
