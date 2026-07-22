# asciicast

Play any video as ASCII art directly in your terminal — written in C.

## Requirements

- `gcc`
- `ffmpeg` (includes `ffprobe`) — extract frames & detect frame rate
- `imagemagick` — convert frames to grayscale
- `libjpeg-dev` — read JPEG frames (compile-time)

### Install on Debian/Ubuntu

```sh
sudo apt install gcc ffmpeg imagemagick libjpeg-dev
```

### Install on Arch Linux

```sh
sudo pacman -S gcc ffmpeg imagemagick libjpeg-turbo
```

### Install on Fedora

```sh
sudo dnf install gcc ffmpeg ImageMagick libjpeg-devel
```

## Build

```sh
make
```

## Usage

```sh
./asciicast <video-file>
```

Press Ctrl+C to stop playback.

## How it works

1. Extracts every frame from the video, scaled to your terminal size, using ffmpeg.
2. Converts all frames to grayscale with ImageMagick.
3. A C program reads each grayscale JPEG, maps each pixel to an ASCII character based on brightness, and prints it to the terminal at the correct frame rate.

Temporary frame data is stored in `~/.asciicast/frames/`.

## Examples

```sh
cd asciicast
yt-dlp --merge-output-format mp4 "https://www.youtube.com/watch?v=FtutLA63Cp8" -o BadApple
./asciicast BadApple.mp4
```
