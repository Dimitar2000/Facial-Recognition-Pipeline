* [x] Preparation
    * [x] Choose source type - MP4
    * [x] Choose OTS model   - YuNet + SFace 
    * [x] Choose C++ inference framework - OpenCV    

* [x] Base product
    * [x] Record database of faces
    * [x] Load source from GStreamer
    * [x] Create custom GStreamer element
    * [x] Get framework
    * [x] Instantiate model
    * [x] Pass each source frame to the model and get result info
    * [x] Display result info in terminal
    * [x] Test on stream

* Achieve a stable static FPS. Strategy: skip (N-1) frames for FR -> queue of size N. 
  Queue size is computed based on provided pipeline + max FR latency.
    
    * [x] Version 1 - use a large constant placeholder for max FR latency
        * [x] Compute configuration for queue and FR components
        * [x] Configure queue and FR components
        * [x] Add videorate with caps filter for target FPS
        * [x] Test how it works for two video resolutions

    * [ ] Version 2 - use wampup to estimate max FR latency 
        * [ ] Create warmup pipeline setup
        * [ ] Run T test frames from real source and compute max latency
        * [ ] Compute configuration for queue and FR components
        * [ ] Remove warmup pipeline setup

    * [ ] Version 3 - use source FPS as cutoff for target FPS

* [ ] Handle changes in source FPS with caps renegotiation

* [ ] Handle changes in source resolution

* [ ] Handle changes in computing power (CPU frequency changes) 

Current Issues/Limitations
---
* Pipeline
    - ~~does not have constant FPS at output~~
    - ~~long FR detection and recognition latencies result in stalling and dropping frames at the output~~
    - initial frames seem to take longer FR time - cold start?

* FR dataset
    - only one face is in the dataset

* Face Detection quality
    - static detection size - different video resolutions result in different amount of image information loss
    - faces are often not detected despite suitable size and lighting conditions

* Runtime Information
    - ~~metadata is attached and not read later~~
    - don't know average and maximum time FR stages take
    - don't know what is the avg processing time of other pipeline elements
    - don't know what is the maximum achievable FPS for a non-queue pipeline using the previous info
    - don't know what is the maximum achievable FPS for a queue buffering pipeline using the previous info
    - ~~don't know when frames are dropped~~
    - ~~don't know if the current frame FR processing could
        stall the pipeline if not dropped.~~

* Static Information 
    - ~~don't know how to use a queue element~~
