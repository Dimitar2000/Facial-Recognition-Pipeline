# Overview

A C++-based GStreamer pipeline that performs facial recognition on an input real-time stream while trying to keep a stable target FPS.

The stream can be either MP4 video file or computer camera. For MP4 files, a reference face dataset is provided with face images for a few actors from the movie Jurassic World. For the camera, no reference dataset is provided, since it
was mainly used to test the behaviour of the pipeline under continuously changing source FPS.

The primary goal of the pipeline is to provide a stable FPS to the user, which can be either a user requested FPS in the range (1, 30), or the FPS requested by the stream source. As a result, the pipeline (re-)configures itself to limit the workload for the FR stage to the maximum possible that can still satisfy the target FPS.

The FR stage works in a two-step process on a frame using two standard OpenCV models - YuNet and SFace. First, YuNet does face detection and collects a set of discovered faces. SFace then maps each face to a face embedding, which is compared to a set of reference embeddings from a supplied dataset. The detected face is identified as the face with maximum reference matches.

Currently, the project uses only the CPU to execute the two OpenCV models. As a result the FR stage is usually configured to skip at least a few frames in order to satisfy the requested FPS.

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

## FR
- Basic workflow is:
```
    frame -> resize -> YuNet -> detections -> scale back -> SFace -> embeddings -> find match
```
- In order to limit the maximum time FR can take to process a frame regardless of input resolution, the input frame is scaled down to the same aspect ratio and a maximum dimension of 1024 (i.e. 1024x700 or 700x1024, depending on the input resolution).
- The frame is not scaled if it is smaller than the scaled version (i.e. 700x500).
- A downside of this approach is that the larger the input resolution is the more information is lost during resizing. In particular, a 4K video is shrinked ~16 times. So the smaller a face is in the original frame, the smaller the chance it will get detected after scaling.
    - However, testing showed this approach still allow small faces in general to be detected. 
    - Furthermore, OpenCV's YuNet was found to be even more unpredictable in terms of when a face will be detected. Sometimes, even when natural lighting is present and the face is 25% of the frame, it does not get detected.

## GStreamer Main Pipeline Architecture
- The pipeline is divided into two main parts - source part and FR part.
    - The source part is composed of the necessary GStreamer elements to work with the given source type correctly (i.e. video decoder for `decodebin`, camera resolution control for `v4l2src`, etc.).
    
    - The FR part is where the main pipeline logic happens. It is composed of the following sequentially linked elements:
        - `identity` to allow the next element's source pad to be monitored for events without being dependent on dynamic pads on the source side
        - `videoconvert` to provide RGB color scheme
        - `FR element` that does FR on an input frame and attaches metadata, or skips it
        - `FR visualizer` that parses and shows the attached metadata
        - `queue` - used to buffer skipped frames and detach FR from downstream
        - `videorate` + `capsfilter` - configured to the required FPS to provide a steady stream to the sink
        - `videoconvert` to match the sink video format
        - `fpsdisplaysink` to output the frames to display with the calculated achieved FPS and monitor frame dropping (which should be minimum)

## GStreamer Extended Architecture - Source FPS
- If the pipeline is configured to emulate changes in the source fps, an additional stage of components is added between `identity` and `videoconvert`:
    - `queue` + `videorate` + `capsfilter` - configured to supply the desired source FPS and trigger caps renegotiation downstream

## Achieving stable FPS

The main idea behind the approach for stable FPS is as follows:
- Target FPS requires FR to process frames in <= TIME
- FR needs can't process so fast - needs maximum TIME_FR_MAX > TIME
- If sufficiently many frames are skipped and queued for handling by `videorate`
    FR will have enough time to process the next frame even when it cannot satisfy the budget for the target FPS. 
- Ex: TIME = 50 ms, TIME_FR_MAX = 100 ms, so skip frames 0-1 and handle frame 2

### Calculation of Skips  
- The calculation of necessary FR skips is done by accounting for
    - basic GStreamer pipeline latency (static pessimistic estimate of 10 ms)
    - queue overhead (static pessimistic estimate of 1 ms)
    - *FR max measured latency \* margin_factor*
    - target FPS

### Configuration
- The computed FR skips are used to configure the FR element and the main `queue` after it, so that its buffer capacity is *skips + 1*. This makes sure the FR is not blocked if it finishes a frame faster than expected.

- The `capsfilter` afterwards is set to the target FPS so that `videorate` knows not to drop buffered queue frames.

### Reconfiguration at Runtime
The configuration can be done anew while the pipeline is running if one of the following happens:
- Scenario 1: The source FPS changes with caps renegotiation
- Scenario 2: The maximum FR latency increases
    - A frame is received that requires more work (more faces, large face, lower lighting, YuNet's face detection implementation is susceptible to all these)
    - System computational throughout decreases - CPU frequency changes
- ~~Scenario 3~~: The source buffers' PTS indicate a variable/decreased average FPS

#### Handling Scenario 1: CAPS Event Probe
- If the pipeline should adapt to FPS changes at the source with caps renegotiation, a probe is attached to the first `videoconvert`.
- If the prove intercepts a CAPS renegotiation, it sends a message on the pipeline bus with the new FPS that is being negotiated.
- The message is read on the bus and the configuration is redone with the new source FPS as the target FPS. This means the `capsfilter` will now match the new source FPS.

#### Handling Scenario 2: FR Latency Measurement Probes
- Two probes are always attached to FR element
- Together they measure the time each frame takes to pass through FR
- The maximum measurement is always recorded and updated if necessary.
- The main thread then periodically checks the current maximum measurement and reconfigures if it is more than the last used one.
- The check is currently set to 1 second, but could be lower to make sure the pipeline adapts to new FR maximums faster.

## Dealing with FR cold start + estimating initial FR speed
- In order to account for cold start in the FR models execution and get an initial estimate of the FR speed, the pipeline performs a warm up stage
- The warm up runs a few frames from the source to the FR element source pad.
- A temporary probe drops them.
- The first half's FR measurements are deleted - cold start overhead.
- The second half's FR measurement maximum is recorded and then used as an initial prediction of the maximum FR latency necessary to configure the FR skips + queue.
- After warm up finishes the drop probe is removed and the pipeline is started again.

## Code Structure

### Directory Structure
* `src/fr` - contains actual FR frame processing logic - currently only CPU
* `src/gst_wrappers` - helper wrappers for lifetime management of GStreamer objects/elements references. Main goal is to call `unref` automatically when the wrapper object goes out of scope if an exception is throws.
* `src/util` - auxiliary logic - file reading, time measurements for debugging
* `src/pipeline` - contains the pipeline-related logic
    * main directory - pipeline architectures + (re-)configuration logic
    * `src/pipeline/metadata` - custom metadata for FR output frames
    * `src/pipeline/elements` - custom pipeline elements - FR and Metadata Visualizer

### Class Structure

## Code Design

### No mutexes
- There are multiple places where multiple threads operate with the same data. However, none of them was found to require synchronization with mutexes.
    - FR measurement - the FR start and end probes always have deterministic synchronized order since they run on the same thread
    - FR max measurement - the main thread reads max measurement periodically but max measurement does not have to be synchronized with any other data shared with the probes so reading it without a mutex is okay
    - All Gstreamer functions are supposed to be thread safe, like configuring capsfilter or sending a message on the bus

### Bus Timeout instead of msg for new FR max measurement
- The periodic checking of FR max latency measurement is done with a reading timeout for new messages on the bus. Since at runtime we don't often receive messages, this should achieve a stable check frequency.
- The check is currently set to 1 second, but could be lower to make sure the pipeline adapts to new FR maximums faster.
- A better approach for triggering reconfiguration when FR max latency increases is for the probe to send a message. This approach can be implemented in the future.