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

* [ ] Handle source changes
    * [ ] Requirements for handling
    * [ ] Implement

* [ ] Improve pipeline cycle speed
    * [ ] Reimplement image processing steps yourself
    * [ ] Reimplement in CUDA

* [ ] Handle errors
    * [ ] Requirements for handling
    * [ ] Implement

* [ ] Benchmark
