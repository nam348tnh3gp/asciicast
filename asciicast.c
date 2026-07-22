#include "asciicast.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <jpeglib.h>

#define MAX_PATH 4096

static const char ascii[16] = {
    ' ', ' ', ' ', ' ',
    '-', '*', '~', '+',
    '1', ';', 'O', 'o',
    '&', '%', '%', '%'
};

static void getTerminalSize(int *c, int *r)
{
    struct winsize w;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
    *c = w.ws_col;
    *r = w.ws_row;
}

static double getVideoFrameRate(const char *file)
{
    FILE *fp;
    char cmd[MAX_PATH], buf[64];
    int num, den;

    sprintf(cmd, "ffprobe -v error -select_streams v:0 -show_entries stream=r_frame_rate -of default=noprint_wrappers=1:nokey=1 %s", file);
    fp = popen(cmd, "r");
    fgets(buf, sizeof buf, fp);
    pclose(fp);

    if (sscanf(buf, "%d/%d", &num, &den) == 2 && den)
        return (double)num / den;

    sscanf(buf, "%d", &num);
    return (double)num;
}

static FILE *openImage(size_t n)
{
    char path[MAX_PATH];
    sprintf(path, "%s/.asciicast/frames/gray/%04zu.jpg", getenv("HOME"), n);
    return fopen(path, "rb");
}

static size_t getNumberOfFrames(void)
{
    FILE *fp;
    char cmd[MAX_PATH], buf[10];
    size_t n;

    sprintf(cmd, "/bin/ls %s/.asciicast/frames/gray/ | wc -l", getenv("HOME"));
    fp = popen(cmd, "r");
    fgets(buf, sizeof buf, fp);
    pclose(fp);
    sscanf(buf, "%zu", &n);
    return n;
}

void generateFrames(const char *file)
{
    char cmd[MAX_PATH];
    int c, r;

    getTerminalSize(&c, &r);
    sprintf(cmd, "mkdir -p %s/.asciicast/frames && ffmpeg -i %s -vf scale=%d:%d %s/.asciicast/frames/%%04d.jpg",
            getenv("HOME"), file, c, r, getenv("HOME"));
    system(cmd);
}

void generateGrayFrames(void)
{
    char cmd[MAX_PATH];
    sprintf(cmd, "mkdir -p %s/.asciicast/frames/gray && magick convert %s/.asciicast/frames/*.jpg -colorspace Gray %s/.asciicast/frames/gray/%%04d.jpg",
            getenv("HOME"), getenv("HOME"), getenv("HOME"));
    system(cmd);
}

void readGenerateASCII(const char *file)
{
    uint8_t *pixels = NULL;
    size_t n = getNumberOfFrames();
    double fps = getVideoFrameRate(file);
    useconds_t delay = (useconds_t)(1000000.0 / fps);
    struct jpeg_decompress_struct cinfo;
    struct jpeg_error_mgr jerr;

    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_decompress(&cinfo);

    for (size_t i = 0; i < n; i++) {
        FILE *image = openImage(i);
        jpeg_stdio_src(&cinfo, image);
        jpeg_read_header(&cinfo, 1);
        jpeg_start_decompress(&cinfo);

        char line[cinfo.image_width + 1];
        if (!pixels)
            pixels = malloc(cinfo.image_width);

        while (cinfo.output_scanline < cinfo.image_height) {
            jpeg_read_scanlines(&cinfo, &pixels, 1);
            for (size_t j = 0; j < cinfo.image_width; j++)
                line[j] = ascii[pixels[j] / 17];
            line[cinfo.image_width] = '\0';
            puts(line);
        }

        usleep(delay);
        printf("\e[1;1H\e[2J");
        jpeg_finish_decompress(&cinfo);
        fclose(image);
    }
    jpeg_destroy_decompress(&cinfo);
}
