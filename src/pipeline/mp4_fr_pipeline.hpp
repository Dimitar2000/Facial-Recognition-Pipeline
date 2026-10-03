#pragma once

#include <gst/gst.h>
#include <string>

#include "fr_pipeline.hpp"

class MP4FRPipeline : public FRPipeline
{
    public:
        MP4FRPipeline(double target_fps,
                      std::string face_dataset_file_path,
                      std::string yunet_model_file_path,
                      std::string sface_model_file_path,
                      std::string input_mp4_file_path);

        ~MP4FRPipeline() override = default;

        virtual GstElementLM get_source() override;

    private:
        static void on_decodebin_pad_added(GstElement *element, GstPad *new_pad, gpointer user_data);
};