#include "webcam_fr_pipeline.hpp"

#include <glib-object.h>

WebcamFRPipeline::WebcamFRPipeline(double target_fps,
                                   std::string face_dataset_file_path,
                                   std::string yunet_model_file_path,
                                   std::string sface_model_file_path)
    : FRPipeline(target_fps,
                 face_dataset_file_path,
                 yunet_model_file_path,
                 sface_model_file_path)
{
    GstElementLM source(gst_element_factory_make("v4l2src", "source"));
    GstElementLM caps_filter(gst_element_factory_make("capsfilter", "resolution"));
    GstElementLM mjpeg(gst_element_factory_make("jpegdec", "jpeg_decoder"));

    GstCapsLM caps(gst_caps_new_simple("image/jpeg",
                                       "width", G_TYPE_INT, 1920,
                                       "height", G_TYPE_INT, 1080,
                                       NULL));
    g_object_set(caps_filter.get(), "caps", caps.get(), NULL);

    pipeline.add_to_pipeline(source);
    pipeline.add_to_pipeline(caps_filter);
    pipeline.add_to_pipeline(mjpeg);

    pipeline.link_elements("source", "resolution");
    pipeline.link_elements("resolution", "jpeg_decoder");
    pipeline.link_elements("jpeg_decoder", EL_VIDEO_CONVERT_FROM_SOURCE);
}