# FBTools

![GitHub Actions Workflow Status](https://img.shields.io/github/actions/workflow/status/GlitchedCode922/fbtools/ci.yml?branch=main&logo=githubactions)
![GitHub Issues](https://img.shields.io/github/issues/GlitchedCode922/fbtools?logo=github)
![GitHub License](https://img.shields.io/github/license/GlitchedCode922/fbtools)
![GitHub Pull Requests](https://img.shields.io/github/issues-pr/GlitchedCode922/fbtools?label=pull%20requests)

Tools for the Linux framebuffer

## .fbimg file format

This is a simple image format. It consists of a 16 byte header and raw pixel data. The header contains:

* Not null-terminated string "FBIMG" (magic number, 5 bytes)
* Width and height as 32-bit unsigned integers, in little-endian
* "RGB" or "BGR" (color channels)

**Note:** The pixel data must be `width * height * 3` bytes long and in the order specified in the last 3 bytes of the header.

## Features
* Framebuffer image drawing tool
* Custom image format (.fbimg)
* Conversion tools for `fbimg <-> png`
* A daemon for taking screenshots of the framebuffer by pressing `PrintScreen` or `F5`
* Painting application for .fbimg files (early development)
* Library for scaling images with bilinear interpolation

## Usage

```
fbimg image.fbimg # Draw an image to the framebuffer

png2fbimg input.png output.fbimg # Convert .png to .fbimg

fbimg2png input.fbimg output.png # Convert .fbimg to .png

paint # Open the paint application
paint image.fbimg # Modify an existing image
# The painting app has a solid filled circle brush.
# You can set its color by typing #rrggbb and size by typing an integer.
# Use dq to discard changes and quit and sq or Ctrl+C to save and quit.
# If no filename is provided, it will save to paint.fbimg.

screenshotd /dev/input/keyboard_event # Starts the screenshot daemon. This command will save screenshots to /tmp.
```

## Status

This project is in early development.
Some of the features to be added are:
* Support for .png files without conversion
* Better edge case handling
* Undo/redo on painting app

---

**This project uses [LodePNG](https://github.com/lvandeve/lodepng)**
