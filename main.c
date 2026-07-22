#include "asciicast.h"

int main(int argc, char **argv)
{
    if (argc != 2)
        return 1;

    generateFrames(argv[1]);
    generateGrayFrames();
    readGenerateASCII(argv[1]);

    return 0;
}
