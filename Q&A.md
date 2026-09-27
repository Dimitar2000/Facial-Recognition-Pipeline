# Questions
    1. Gstreamer seems to have only a mainstream port for C, not C++, do I have to find proper C++ bindings?
    
    2. The requirement for the FR model says "no training required", 
        but it expects me to match against a "set of faces I have curated".
        Don't I then have to train the FR model on the faces I select, or I can 
        use a generic facial recognition model that can match any face? 
    
    3. Does the metadata output have to be in the frame itself (bounding boxes) 
        or in the console or in a side debug pane, or whatever combination of these I pick?
    
    4. What is the maximum source FPS the pipeline should support 
        (I expect <= 30, streaming my webcam seems to be limited to 30 fps, 
            RTSP streams online are also listed with this value)?
    
    5. If a frame is dropped, and I keep showing the last frame with a debug message,
        would that be considered proper handling?

    6. Using my webcam as a source seems to require a different GStreamer element type
        per OS? In order to achieve more platform independent implementation, can I just
        detect the platform OS at runtime and select the proper element factory, or it is
        more preferable to avoid connecting to the camera directly and expose it through
        a local RTSP server that is platform-independent (i.e. in Python).

    7. It is preferable to use as few 3rd party libs for the core functions as I can,
        but can I assume that does not include the FR model and inference framework like OpenCV and GLib?
    
    8. Should I prepare a test environment for the final meeting where I can simulate input changes or 
        errors and show how well the pipeline handles them?