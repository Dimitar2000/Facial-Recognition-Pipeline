# Overview

# Requirements

## Tools
- CMake: 3.31.6
- Make: 4.4.1
- GStreamer: 1.26.2
- pkg-config: 1.8.1
- g++: 14.2.0

## Language Versions
- Python: 3.13
- C++17

## Python Packages
- `numpy`: 2.2.4
- `opencv-python`: 5.0.0.93
- `pathlib`: 1.0.1

## C++ Libraries
- `gstreamer-1.0`: 1.26.2
- `gstreamer-video-1.0`: 1.26.2
- `opencv`: 4.10.0

# Usage

## Computing the facial dataset embeddings

The embeddings for the used facial dataset are already provided.
If you want to regenerate them, use:

```
    rm dataset/compute_embeddings
    make compute_embeddings
```

## Building

To build in debug/release mode, run:
```
    make build type=Debug|Release
```

## Running with MP4

There are a few provided videos under `videos/`.
You have a few ways to configuring running a video.

- Run with a specified target FPS in the range `(1, 30)`:

```
    make run_mp4 fps=<FPS> video=<file_path>
```

- Run with the FPS the video negotiates:
```
    make run_mp4 monitor-src-caps=yes video=<file_path>
```

- Run with initial target FPS and emulation of changes to the video FPS:
```
    make run_mp4 fps=<FPS> emulate=yes video=<file_path>
```

## Running with Webcam

**Important** For the camera, make sure there is enough natural lighting when you use it. Otherwise its effective FPS drops significantly and the pipeline 
will not be able to provide the requested FPS.

- Run with a specified target FPS in the range `(1, 30)`:

```
    make run_webcam fps=<FPS>
```

- Run with the FPS the camera negotiates:
```
    make run_webcam monitor-src-caps=yes
```

# Design

## Functionality

- 

## Code Structure

## Code Design