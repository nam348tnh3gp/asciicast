#define _GNU_SOURCE
#include "asciicast.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <time.h>
#include <fcntl.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <jpeglib.h>

#define MAX_PATH 4096

static const char ascii[16] = {
    ' ', ' ', ' ', ' ',
    '-', '*', '~', '+',
    '1', ';', 'O', 'o',
    '&', '%', '%', '%'
};

/*
 * Audio: a helper process runs  ffmpeg -> pipe -> player  and copies the PCM
 * itself. The pipe to the player is shrunk to 4KB, so the write below only
 * gets through once the player is really pulling data. At that moment the
 * helper tells the parent, and the parent starts the video clock. This way
 * PulseAudio / ffmpeg / pacat startup time is never added to the video.
 *
 * Player priority: pacat (PulseAudio, works on Termux), aplay (ALSA), ffplay.
 */
static const char playerScript[] =
    "if command -v pacat >/dev/null 2>&1; then\n"
    "  if command -v pulseaudio >/dev/null 2>&1; then\n"
    "    pulseaudio --check >/dev/null 2>&1 ||\n"
    "      pulseaudio --start --exit-idle-time=-1 >/dev/null 2>&1\n"
    "  fi\n"
    "  exec pacat --raw --format=s16le --rate=44100 --channels=2 --latency-msec=30\n"
    "elif command -v aplay >/dev/null 2>&1; then\n"
    "  exec aplay -q -t raw -f S16_LE -r 44100 -c 2 --buffer-time=30000\n"
    "elif command -v ffplay >/dev/null 2>&1; then\n"
    "  exec ffplay -nodisp -autoexit -loglevel quiet -fflags nobuffer "
    "-flags low_delay -f s16le -ar 44100 -ac 2 -i -\n"
    "fi\n"
    "exit 1\n";

static pid_t audioPid = -1;

static void stopAudio(void)
{
    if (audioPid > 0) {
        kill(-audioPid, SIGTERM);
        waitpid(audioPid, NULL, 0);
        audioPid = -1;
    }
}

static void onSigint(int sig)
{
    (void)sig;
    stopAudio();
    printf("\e[?25h\n");
    _exit(130);
}

static void audioMain(const char *file, int syncfd)
{
    int dec[2], out[2];
    pid_t ff, pl;
    char buf[4096];
    size_t total = 0, threshold = 65536 + 8192;
    int signaled = 0, devnull;
    ssize_t r;

    devnull = open("/dev/null", O_RDWR);
    if (pipe(dec) || pipe(out))
        _exit(1);

#if defined(F_SETPIPE_SZ) && defined(F_GETPIPE_SZ)
    fcntl(out[1], F_SETPIPE_SZ, 4096);
    r = fcntl(out[1], F_GETPIPE_SZ);
    if (r > 0)
        threshold = (size_t)r + 8192;
#endif

    ff = fork();
    if (ff == 0) {
        dup2(dec[1], 1);
        dup2(devnull, 0);
        dup2(devnull, 2);
        close(dec[0]); close(dec[1]); close(out[0]); close(out[1]);
        execlp("ffmpeg", "ffmpeg", "-nostdin", "-v", "quiet", "-i", file,
               "-vn", "-f", "s16le", "-ar", "44100", "-ac", "2", "-",
               (char *)NULL);
        _exit(127);
    }

    pl = fork();
    if (pl == 0) {
        dup2(out[0], 0);
        dup2(devnull, 1);
        dup2(devnull, 2);
        close(dec[0]); close(dec[1]); close(out[0]); close(out[1]);
        /* `sh` via PATH: Termux has no /bin/sh */
        execlp("sh", "sh", "-c", playerScript, (char *)NULL);
        _exit(127);
    }

    close(dec[1]);
    close(out[0]);

    while ((r = read(dec[0], buf, sizeof buf)) > 0) {
        ssize_t off = 0;

        while (off < r) {
            ssize_t w = write(out[1], buf + off, (size_t)(r - off));
            if (w <= 0)
                goto done;
            off += w;
        }
        total += (size_t)r;
        if (!signaled && total >= threshold) {
            if (write(syncfd, "1", 1) < 0) {}
            signaled = 1;
        }
    }
done:
    close(out[1]);
    waitpid(pl, NULL, 0);
    waitpid(ff, NULL, 0);
}

static void startAudio(const char *file)
{
    int sp[2];
    pid_t pid;

    if (pipe(sp))
        return;

    pid = fork();
    if (pid == 0) {
        setpgid(0, 0);
        close(sp[0]);
        audioMain(file, sp[1]);
        _exit(0);
    }
    close(sp[1]);

    if (pid > 0) {
        struct pollfd p;
        char c;

        setpgid(pid, pid);
        audioPid = pid;

        /* wait (max 5s) until the player is really consuming audio */
        p.fd = sp[0];
        p.events = POLLIN;
        p.revents = 0;
        if (poll(&p, 1, 5000) > 0 && (p.revents & POLLIN)) {
            if (read(sp[0], &c, 1) < 0) {}
        }
    }
    close(sp[0]);
}

static double nowSec(void)
{
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec / 1e9;
}

static void sleepUntil(double t)
{
    struct timespec ts;

    ts.tv_sec = (time_t)t;
    ts.tv_nsec = (long)((t - (double)ts.tv_sec) * 1e9);
    clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &ts, NULL);
}

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

/*
 * Cache: frames are stored per video + terminal size in
 * ~/.asciicast/cache/<key>/. A ".done" marker is written once conversion
 * finished, so the next run of the same video skips ffmpeg/ImageMagick.
 */
static char cacheDir[MAX_PATH];

static void setCacheDir(const char *file, int c, int r)
{
    char real[MAX_PATH], key[MAX_PATH + 128];
    struct stat st;
    uint64_t h = 1469598103934665603ULL; /* FNV-1a */

    memset(&st, 0, sizeof st);
    stat(file, &st);
    if (!realpath(file, real))
        snprintf(real, sizeof real, "%s", file);
    snprintf(key, sizeof key, "%s|%lld|%lld|%dx%d", real,
             (long long)st.st_size, (long long)st.st_mtime, c, r);
    for (const char *p = key; *p; p++) {
        h ^= (unsigned char)*p;
        h *= 1099511628211ULL;
    }
    snprintf(cacheDir, sizeof cacheDir, "%s/.asciicast/cache/%016llx",
             getenv("HOME"), (unsigned long long)h);
}

void clearCache(void)
{
    char cmd[MAX_PATH];

    snprintf(cmd, sizeof cmd, "rm -rf \"%s/.asciicast/cache\"", getenv("HOME"));
    system(cmd);
}

static int isCached(void)
{
    char path[MAX_PATH];

    snprintf(path, sizeof path, "%s/.done", cacheDir);
    return access(path, F_OK) == 0;
}

static FILE *openImage(size_t n)
{
    char path[MAX_PATH];
    sprintf(path, "%s/frames/gray/%04zu.jpg", cacheDir, n);
    return fopen(path, "rb");
}

static size_t getNumberOfFrames(void)
{
    FILE *fp;
    char cmd[MAX_PATH], buf[10];
    size_t n;

    sprintf(cmd, "ls \"%s/frames/gray\" | wc -l", cacheDir);
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
    setCacheDir(file, c, r);
    if (isCached())
        return;

    /* drop any half-finished conversion from an interrupted run */
    sprintf(cmd, "rm -rf \"%s\" && mkdir -p \"%s/frames\" && ffmpeg -i %s -vf scale=%d:%d -start_number 0 \"%s/frames/%%04d.jpg\"",
            cacheDir, cacheDir, file, c, r, cacheDir);
    system(cmd);
}

void generateGrayFrames(void)
{
    char cmd[MAX_PATH];

    if (isCached())
        return;

    /*
     * Long videos have tens of thousands of frames, so the file list must not
     * be passed as one glob (\"Argument list too long\"): find|xargs runs
     * ImageMagick in batches. Keep only the gray frames, then mark the cache
     * as complete.
     */
    sprintf(cmd, "mkdir -p \"%s/frames/gray\" && "
                 "find \"%s/frames\" -maxdepth 1 -name '*.jpg' -print0 | "
                 "xargs -0 magick mogrify -path \"%s/frames/gray\" -colorspace Gray && "
                 "find \"%s/frames\" -maxdepth 1 -name '*.jpg' -delete && "
                 "touch \"%s/.done\"",
            cacheDir, cacheDir, cacheDir, cacheDir, cacheDir);
    system(cmd);
}

void readGenerateASCII(const char *file, int withAudio)
{
    uint8_t *pixels = NULL;
    size_t n = getNumberOfFrames();
    double fps = getVideoFrameRate(file);
    double delayMs = getenv("ASCIICAST_AUDIO_DELAY_MS") ?
                     atof(getenv("ASCIICAST_AUDIO_DELAY_MS")) : 0.0;
    double t0;
    struct jpeg_decompress_struct cinfo;
    struct jpeg_error_mgr jerr;

    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_decompress(&cinfo);

    setvbuf(stdout, NULL, _IOFBF, 1 << 16);
    signal(SIGINT, onSigint);
    if (withAudio)
        startAudio(file);
    t0 = nowSec() + delayMs / 1000.0;

    for (size_t i = 0; i < n; i++) {
        /* video is behind the audio clock: skip this frame to stay in sync */
        if (nowSec() > t0 + (double)(i + 1) / fps)
            continue;

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

        fflush(stdout);
        sleepUntil(t0 + (double)(i + 1) / fps);
        printf("\e[1;1H\e[2J");
        jpeg_finish_decompress(&cinfo);
        fclose(image);
    }
    jpeg_destroy_decompress(&cinfo);
    stopAudio();
}
