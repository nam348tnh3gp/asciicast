# asciicast
<p align="center">
  <img src="https://github.com/user-attachments/assets/b74d2cb8-9dca-4759-91c3-9da26574dc05" />
</p>

**Play any video as ASCII art directly in your terminal — with synced audio — written in C.**

## Requirements

- `gcc` (or `clang`) and `make`
- `ffmpeg` (includes `ffprobe`) — extract frames, detect frame rate, decode audio
- `imagemagick` — convert frames to grayscale
- `libjpeg-dev` — read JPEG frames (compile-time)
- An audio player, first one found is used: `pacat` (PulseAudio), `aplay` (ALSA) or `ffplay`

## Quick install

```sh
chmod +x install.sh
./install.sh
```

`install.sh` detects Termux, Debian/Ubuntu, Arch and Fedora, installs the dependencies, builds the project and copies `asciicast` into your `PATH`.

| Option      | Effect                                                     |
|-------------|------------------------------------------------------------|
| `--no-deps` | Skip dependency installation, only build and install       |
| `--user`    | Install to `~/.local/bin` instead of `/usr/local/bin`      |

`asciicast` itself also takes flags:

| Flag                  | Effect                                |
|-----------------------|----------------------------------------|
| `-n`, `--no-audio`    | Play without sound                    |
| `-c`, `--clear-cache` | Delete `~/.asciicast/cache` and exit  |

### Termux

```sh
pkg install git
git clone <repo-url> asciicast && cd asciicast
./install.sh
termux-setup-storage   # only if your videos are in shared storage
```

Audio on Termux goes through PulseAudio (`pacat`); the program starts `pulseaudio` automatically if it isn't running.

### Manual install

```sh
# Debian/Ubuntu
sudo apt install gcc make ffmpeg imagemagick libjpeg-dev pulseaudio-utils alsa-utils
# Arch Linux
sudo pacman -S gcc make ffmpeg imagemagick libjpeg-turbo libpulse alsa-utils
# Fedora
sudo dnf install gcc make ffmpeg ImageMagick libjpeg-turbo-devel pulseaudio-utils alsa-utils
# Termux
pkg install clang make findutils ffmpeg imagemagick libjpeg-turbo pulseaudio

make
```

## Usage

```sh
./asciicast [-n] [-c] <video-file>
```

`-n` (or `--no-audio`) plays without sound. `-c` (or `--clear-cache`) deletes the whole frame cache and exits.

Press Ctrl+C to stop playback.

## Audio sync

A helper process runs `ffmpeg -> pipe -> player` (`pacat`, `aplay` or `ffplay`). The pipe to the player is only 4 KB, so the video clock starts at the moment the player actually begins consuming audio — PulseAudio/ffmpeg startup time is never added to the picture. Each frame is then shown until an absolute deadline (`start + frame / fps`) instead of a fixed sleep, so there is no cumulative drift; if the terminal is too slow, late frames are skipped rather than delaying the audio.

To fine-tune on your device, shift the video clock in milliseconds (positive = video later, negative = video earlier):

```sh
ASCIICAST_AUDIO_DELAY_MS=-80 ./asciicast video.mp4
```

## How it works

1. Extracts every frame from the video, scaled to your terminal size, using ffmpeg.
2. Converts all frames to grayscale with ImageMagick.
3. Starts the audio stream, then a C program reads each grayscale JPEG, maps each pixel to an ASCII character based on brightness, and draws it in sync with the audio clock.

Converted frames are cached in `~/.asciicast/cache/<key>/`, so a video is only converted the first time. The cache key is the video path + size + modification time + terminal size (columns x rows), so changing the terminal size or replacing the video converts it again. Only grayscale frames are kept, and a `.done` marker is written when conversion completes — an interrupted conversion is redone automatically.

To free disk space: `rm -rf ~/.asciicast/cache`

## Examples
```sh
cd asciicast
yt-dlp --merge-output-format mp4 "https://www.youtube.com/watch?v=FtutLA63Cp8" -o BadApple
./asciicast BadApple.mp4
```
