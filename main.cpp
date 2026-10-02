#include <exception>
#include <glib-object.h>
#include <gst/gstelementfactory.h>
#include <gst/gstobject.h>
#include <iostream>

#include <gst/gst.h>
#include <gst/audio/audio.h>
#include <memory>
#include <opencv2/objdetect/face.hpp>

#include "fr_transformer.hpp"
#include "fr_pipeline.hpp"

const std::string HELP_MESSAGE = "Not enough arguments. Format is: \n"
                                 "  <app> webcam <input-face-embeddings-file-path> <face-detection-yunet-file-path> <face-recog-sface-file-path>\n"
                                 "  <app> mp4    <input-face-embeddings-file-path> <face-detection-yunet-file-path> <face-recog-sface-file-path> <input-mp4-file-path>\n";

int main(int argc, char *argv[])
{
    // Initialize GStreamer
    gst_init (&argc, &argv);
    
    // Register custom components
    if (!gst_element_register(nullptr, "fr-transformer", GST_RANK_NONE, GST_TYPE_FR_TRANSFORMER))
    {
        std::cerr << "Failed to register myprocess element" << std::endl;
        return 1;
    }
    
    // Check and parse arguments
    if (argc < 5)
    {
        std::cerr << HELP_MESSAGE << std::endl;
        return 1;
    }
    
    std::string source_type            = argv[1];
    std::string face_dataset_file_path = argv[2];
    std::string yunet_model_file_path  = argv[3];
    std::string sface_model_file_path  = argv[4];
    std::string input_mp4_file_path;

    if (source_type == "mp4")
    {
        if (argc < 6)
        {
            std::cerr << HELP_MESSAGE << std::endl;
            return 1;
        }
        else
        {
            input_mp4_file_path = argv[5];
        }
    }
    
    // Create the requested pipeline
    std::unique_ptr<FRPipeline> pipeline;
    
    try
    {
        if (source_type == "webcam")
        {
            std::cout << "Source type is: Camera" << std::endl;
            
            pipeline = std::make_unique<WebcamFRPipeline>(face_dataset_file_path, yunet_model_file_path, sface_model_file_path);
        }
        else if (source_type == "mp4")
        {
            std::cout << "Source type is: MP4 video file" << std::endl;
               
            pipeline = std::make_unique<MP4FRPipeline>(face_dataset_file_path, yunet_model_file_path, sface_model_file_path, input_mp4_file_path);
        }
        else
        {
            std::cerr << "Invalid source type provided - <" << source_type << ">" << std::endl;
            return 1;
        }
    }
    catch (std::exception e)
    {
        std::cerr << "Pipeline could not be created." << std::endl;
        return 1;
    }

    // Run the pipeline
    try 
    {
        pipeline->run();
    }
    catch(std::exception e)
    {
        std::cerr << "Pipeline run stopped!\n" 
                  << "Reason: " << e.what() << std::endl;
    }
    
    return 0;
}