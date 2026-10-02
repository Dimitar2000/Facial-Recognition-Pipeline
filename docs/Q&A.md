# Questions
    1. Gstreamer seems to have only a mainstream port for C, not C++, do I have to find proper C++ bindings?
        - No, GStreamer is C, use it, but that doesn't prevent you from using C++ around it.

    2. The requirement for the FR model says "no training required", 
        but it expects me to match against a "set of faces I have curated".
        Don't I then have to train the FR model on the faces I select, or I can 
        use a generic facial recognition model that can match any face? 
        
        - Research a bit on facial recognition models. You can use a pretrained model 
            and simply enroll new faces in it to create your own curated database of faces 
            you care about. This is what we expect.
    
    3. Does the metadata output have to be in the frame itself (bounding boxes) 
        or in the console or in a side debug pane, or whatever combination of these I pick?
        
        - Attach it as metadata that travels along with the frame. This is the genetic pattern. 
            A metadata viewer is a plus, it's a convenient tool that facilitates debugging and development.
    
    4. Should the pipeline keep a constant throughput of ~25 fps and what should be the maximum
        input source resolution for which this is achieved? 
        Or its up to me to make practical design choices in that respect?

        - It's up to you to determine. Try to make it work reliably and predictably. A user won't be happy 
            if their pipeline runs at 30fps most of the time and then stalls to 5fps ot less just from time to time
    
    5. If a frame is dropped (or I simulate one to be), and I keep showing the last frame with a debug message,
        would that be considered proper handling?

        - This is up to you. Pick whichever strategy feels most natural to you.
    
    6. Is it desirable that I implement all image operations like greyscale and classification myself in 
        order to be less dependent on 3rd party libraries in general and in order to redesign them for GPU acceleration?
        Instead of relying on a 3rd party (i.e. OpenCV, Dlib) implementation?

        -  You are free to use OpenCV or other tools, but if you can write these simple operations yourself, 
            and/or hardware accelerated, that is an advantage.
    
    7. Should I prepare a test environment for the final meeting where I can simulate input changes or 
        errors and show how well the pipeline handles them?

        - You don't have to.