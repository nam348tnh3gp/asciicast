#ifndef ASCIICAST_H
#define ASCIICAST_H

void generateFrames(const char *videoFile);
void generateGrayFrames(void);
void readGenerateASCII(const char *videoFile, int withAudio);

/* Deletes the whole ~/.asciicast/cache directory (used by -c/--clear-cache). */
void clearCache(void);

#endif
