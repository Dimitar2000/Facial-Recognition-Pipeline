#pragma once

#include <string>

#include "fr_pipeline.hpp"
#include "gst_wrappers/gst_element_lm.hpp"

class WebcamFRPipeline : public FRPipeline
{
    public:
        WebcamFRPipeline(double target_fps,
                         std::string face_dataset_file_path,
                         std::string yunet_model_file_path,
                         std::string sface_model_file_path);

        ~WebcamFRPipeline() override = default;

        virtual GstElementLM get_source() override;
};