* [ ] Preparation
    * [ ] Choose source type
    * [ ] Choose OTS model
    * [x] Choose C++ inference framework - OpenCV    

* [ ] Base product
    * [ ] Record database of faces
    * [ ] Load source from GStreamer
    * [x] Create custom GStreamer element
    * [ ] Get source stream metadata
    * [x] Get framework
    * [ ] Instantiate model
        - [ ] Add model parameters file
        - [ ] Add face database
        - [ ] Pass both to model on creation
    * [ ] Pass each source frame to the model and get result info
    * [ ] Display result info in terminal
    * [ ] Test on stream

* [ ] Create test setup

* [ ] Achieve a stable static RTS
    * Strategy 1 - static configuration of queue + skip frames based on max FR latency
        * [ ] Compute max latency of FR before playing
        * [ ] Compute configuration for queue and FR components
        * [ ] Configure queue and FR components
        * [ ] Test how it works for two video resolutions
 
Current Issues/Limitations
---
* Pipeline
    - does not have constant FPS at output
    - initial frames seem to take longer FR time - cold start?
    - long FR detection and recognition latencies result in stalling and dropping frames at the output

* FR dataset
    - only one face is in the dataset

* FR quality
    - static detection size - different video resolutions result in different amount of image information loss
    - faces are often not detected despite suitable size and lighting conditions

* Runtime Information
    - metadata is attached and not read later
    - don't know average and maximum time FR stages take
    - don't know what is the avg processing time of other pipeline elements
    - don't know what is the maximum achievable FPS for a non-queue pipeline using the previous info
    - don't know what is the maximum achievable FPS for a queue buffering pipeline using the previous info
    - don't know when frames are dropped
    - don't know if the current frame FR processing could
        stall the pipeline if not dropped.

* Static Information 
    - don't know how to use a queue element
