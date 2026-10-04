#include "mp4_fr_pipeline.hpp"

#include <gst/gst.h>
#include <stdexcept>
#include <string>


MP4FRPipeline::MP4FRPipeline(double target_fps,
                             std::string face_dataset_file_path,
                             std::string yunet_model_file_path,
                             std::string sface_model_file_path,
                             std::string input_mp4_file_path)
    : FRPipeline(target_fps,
                 face_dataset_file_path,
                 yunet_model_file_path,
                 sface_model_file_path)
{
    GstElementLM source(gst_element_factory_make("filesrc", "source"));
    GstElementLM decoder(gst_element_factory_make("decodebin", "decoder"));

    g_object_set(source.get(), "location", input_mp4_file_path.c_str(), NULL);

    pipeline.add_to_pipeline(source);
    pipeline.add_to_pipeline(decoder);

    pipeline.link_elements("source", "decoder");

    GstElementLM video_convert = pipeline.get_by_name(EL_SRC_ENDPOINT);

    g_signal_connect(pipeline.get_by_name("decoder").get(),
                     "pad-added",
                     G_CALLBACK(MP4FRPipeline::on_decodebin_pad_added),
                     video_convert.get());
}

GstElementLM MP4FRPipeline::get_source()
{
    return pipeline.get_by_name("source");
}

void MP4FRPipeline::on_decodebin_pad_added(GstElement *, GstPad *new_pad, gpointer user_data)
{
    GstElement *video_convert = GST_ELEMENT(user_data);
    GstPadLM sink_pad(gst_element_get_static_pad(video_convert, "sink"));

    if (gst_pad_is_linked(sink_pad.get()))
    {
        return;
    }

    GstCapsLM caps;

    try
    {
        caps = GstCapsLM(gst_pad_get_current_caps(new_pad));
    }
    catch (std::runtime_error& error)
    {
        g_printerr("Failed to query caps from new pad: %s\n", error.what());
        caps = GstCapsLM(gst_pad_query_caps(new_pad, nullptr), true);
    }

    if (gst_caps_is_empty(caps.get()) || gst_caps_is_any(caps.get()))
    {
        throw std::runtime_error("New pad does not have required caps");
    }

    const GstStructure *structure = gst_caps_get_structure(caps.get(), 0);

    if (!g_str_has_prefix(gst_structure_get_name(structure), "video/"))
    {
        throw std::runtime_error("Decoded pad is not video.\n");
    }

    GstPadLinkReturn result = gst_pad_link(new_pad, sink_pad.get());

    if (result != GST_PAD_LINK_OK)
    {
        throw std::runtime_error("Could not link decoded video pad " + std::to_string(result) + ").\n");
    }

    g_printerr("[MP4 Pipeline] Decodebin pad added and linked successfully.\n");
}