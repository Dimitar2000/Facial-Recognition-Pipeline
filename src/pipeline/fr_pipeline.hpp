#pragma once

#include <string>

#include "config_calculator.hpp"
#include "gst_element_lm.hpp"

class FRPipeline
{
    public:
        void run();

        virtual ~FRPipeline() = default;

    protected:
        FRPipeline() = default;

        FRPipeline(double target_fps,
                   std::string face_dataset_file_path,
                   std::string yunet_model_file_path,
                   std::string sface_model_file_path);

    private:
        void extend_for_stable_fps();

    protected:
        ConfigCalculator config_calculator;
        double target_fps;
        GstPipelineLM pipeline;

        const char * EL_PIPELINE                  = "pipeline";
        const char * EL_VIDEO_CONVERT_FROM_SOURCE = "video_convert_from_source";
        const char * EL_FR_ELEMENT                = "facial_recognition_element";
        const char * EL_FR_SKIP_QUEUE             = "fr_skip_queue";
        const char * EL_FRAMERATE                 = "framerate";
        const char * EL_FRAMERATE_FILTER          = "capsfilter_fps";
        const char * EL_VIDEO_CONVERT_TO_SINK     = "video_convert_to_sink";
        const char * EL_SINK                      = "sink";
};