#include <iostream>
#include <exception>
#include <memory>
#include <stdexcept>
#include <string>
#include <gst/gst.h>

#include "pipeline/elements/fr_element.hpp"
#include "pipeline/elements/fr_metadata_visualizer.hpp"
#include "pipeline/fr_pipeline.hpp"
#include "pipeline/mp4_fr_pipeline.hpp"
#include "pipeline/webcam_fr_pipeline.hpp"

#define DEBUG

const std::string HELP_MESSAGE = "Not enough arguments. Format is: \n"
                                 "  <app> webcam <fps> <input-face-embeddings-file-path> <face-detection-yunet-file-path> <face-recog-sface-file-path>\n"
                                 "  <app> mp4    <fps> <input-face-embeddings-file-path> <face-detection-yunet-file-path> <face-recog-sface-file-path> <input-mp4-file-path>\n";

int main(int argc, char *argv[])
{
    std::cout << "===================" << std::endl;

    // Initialize GStreamer
    gst_init (&argc, &argv);
    
    // Register custom components
    if (!gst_element_register(nullptr, "fr-element", GST_RANK_NONE, GST_TYPE_FR_ELEMENT))
    {
        std::cerr << "Failed to register fr-element" << std::endl;
        return 1;
    }

    if (!gst_element_register(nullptr, "fr-metadata-visualizer", GST_RANK_NONE, GST_TYPE_FR_METADATA_VISUALIZER))
    {
        std::cerr << "Failed to register fr-metadata-visualizer element" << std::endl;
        return 1;
    }
    
    // Check and parse arguments
    if (argc < 6)
    {
        std::cerr << HELP_MESSAGE << std::endl;
        return 1;
    }
    
    std::string source_type            = argv[1];
    std::string target_fps_s           = argv[2];
    double      target_fps;
    std::string face_dataset_file_path = argv[3];
    std::string yunet_model_file_path  = argv[4];
    std::string sface_model_file_path  = argv[5];
    std::string input_mp4_file_path;

    try 
    {
        target_fps = std::stod(target_fps_s);
    }
    catch (std::exception e)
    {
        std::cerr << "Invalid FPS value - not a number - <" << target_fps_s << ">" << std::endl;
        return 1;
    }

    if (target_fps < 0.0) 
    {
        std::cerr << "Invalid FPS value - negative value - <" << target_fps_s << ">" << std::endl;
        return 1;
    }

    if (target_fps > 30.0)
    {
        std::cerr << "Invalid FPS value - unsupported - <" << target_fps_s << ">" << std::endl;
        return 1;            
    }

    if (source_type == "mp4")
    {
        if (argc < 7)
        {
            std::cerr << HELP_MESSAGE << std::endl;
            return 1;
        }
        else
        {
            input_mp4_file_path = argv[6];
        }
    }
    
    // Create the requested pipeline
    std::unique_ptr<FRPipeline> pipeline;
    
    try
    {
        if (source_type == "webcam")
        {
            std::cout << "Source type is: Camera" << std::endl;
            
            pipeline = std::make_unique<WebcamFRPipeline>(target_fps,
                                                          face_dataset_file_path, 
                                                          yunet_model_file_path, 
                                                          sface_model_file_path);
        }
        else if (source_type == "mp4")
        {
            std::cout << "Source type is: MP4 video file" << std::endl;
               
            pipeline = std::make_unique<MP4FRPipeline>(target_fps,
                                                       face_dataset_file_path, 
                                                       yunet_model_file_path, 
                                                       sface_model_file_path, 
                                                       input_mp4_file_path);
        }
        else
        {
            std::cerr << "Invalid source type provided - <" << source_type << ">" << std::endl;
            return 1;
        }
    }
    catch (std::runtime_error e)
    {
        std::cerr << "Pipeline could not be created.\n" 
                  << "Reason: " << e.what() << std::endl;
        return 1;
    }

    // Run the pipeline
    try 
    {
        pipeline->warm_up();
        pipeline->add_skip_queuing();
        pipeline->run();
    }
    catch(std::runtime_error e)
    {
        std::cerr << "Pipeline run stopped!\n" 
                  << "Reason: " << e.what() << std::endl;
    }
    
    return 0;
}