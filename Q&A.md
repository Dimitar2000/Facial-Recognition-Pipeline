# Questions
    1. Gstreamer seems to have only a mainstream port for C, not C++, do I have to find proper C++ bindings?
    
    2. The requirement for the FR model says "no training required", 
        but it expects me to match against a "set of faces I have curated".
        Don't I then have to train the FR model on the faces I select, or I can 
        use a generic facial recognition model that can match any face? 
    
    3. Does the metadata output have to be in the frame itself (bounding boxes) 
        or in the console or in a side debug pane, or whatever combination of these I pick?
    
    4. Should the pipeline keep a constant throughput of ~25 fps and what should be the maximum
        input source resolution for which this is achieved? 
        Or its up to me to make practical design choices in that respect?
    
    5. If a frame is dropped (or I simulate one to be), and I keep showing the last frame with a debug message,
        would that be considered proper handling?
    
    6. Is it desirable that I implement all image operations like greyscale and classification myself in 
        order to be less dependent on 3rd party libraries in general and in order to redesign them for GPU acceleration?
        Instead of relying on a 3rd party (i.e. OpenCV, Dlib) implementation?
    
    7. Should I prepare a test environment for the final meeting where I can simulate input changes or 
        errors and show how well the pipeline handles them?