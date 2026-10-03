#include "fr_pipeline.hpp"

#include <glib-object.h>
#include <gst/gst.h>
#include <iostream>
#include <utility>

#include "elements/fr_element.hpp"
#include "gst_wrappers/gst_element_lm.hpp"

FRPipeline::FRPipeline(double target_fps,
                       std::string face_dataset_file_path,
                       std::string yunet_model_file_path,
                       std::string sface_model_file_path)
    : target_fps(target_fps)
{
    pipeline = GstPipelineLM(gst_pipeline_new(EL_PIPELINE));

    GstElementLM video_convert(gst_element_factory_make("videoconvert", EL_VIDEO_CONVERT_FROM_SOURCE));
    GstElementLM fr_element(gst_element_factory_make("fr-element", EL_FR_ELEMENT));
    GstElementLM fr_meta_visualizer(gst_element_factory_make("fr-metadata-visualizer", EL_FR_META_VISUALIZER));
    GstElementLM video_convert2(gst_element_factory_make("videoconvert", EL_VIDEO_CONVERT_TO_SINK));
    GstElementLM sink(gst_element_factory_make("autovideosink", EL_SINK));

    gst_fr_element_init_processor(GST_FR_ELEMENT(fr_element.get()),
                                  face_dataset_file_path,
                                  yunet_model_file_path,
                                  sface_model_file_path,
                                  1024);

    pipeline.add_to_pipeline(std::move(video_convert));
    pipeline.add_to_pipeline(std::move(fr_element));
    pipeline.add_to_pipeline(std::move(fr_meta_visualizer));
    pipeline.add_to_pipeline(std::move(video_convert2));
    pipeline.add_to_pipeline(std::move(sink));

    pipeline.link_elements(EL_VIDEO_CONVERT_FROM_SOURCE, EL_FR_ELEMENT);
    pipeline.link_elements(EL_FR_ELEMENT, EL_FR_META_VISUALIZER);
    pipeline.link_elements(EL_FR_META_VISUALIZER, EL_VIDEO_CONVERT_TO_SINK);
    pipeline.link_elements(EL_VIDEO_CONVERT_TO_SINK, EL_SINK);

    if (target_fps != 0.0)
    {
        std::cout << "[Configuration] Extending pipeline with queuing for stable FPS: " << std::endl;
        extend_for_stable_fps();
    }
}

void FRPipeline::extend_for_stable_fps()
{
    auto slots = config_calculator.compute_queue_slots(target_fps,
                                                       1 * GST_MSECOND,
                                                       1 * GST_MSECOND,
                                                       200 * GST_MSECOND);

    std::cout << "[Configuration] Configuring skip queue with " << slots << " slots" << std::endl;

    GstElementLM queue(gst_element_factory_make("queue", EL_FR_SKIP_QUEUE));

    g_object_set(queue.get(),
                 "max-size-buffers", slots,
                 "max-size-bytes", 0,
                 "max-size-time", 0,
                 nullptr);

    std::cout << "[Configuration] Configuring FR element to skip " << slots - 1 << " frames" << std::endl;

    auto *element = GST_FR_ELEMENT(pipeline.get_by_name(EL_FR_ELEMENT));
    gst_fr_element_set_skips(element, slots - 1);

    std::cout << "[Configuration] Configuring framerate pair with FPS = " << target_fps << std::endl;

    GstElementLM videorate(gst_element_factory_make("videorate", EL_FRAMERATE));
    GstElementLM capsfilter(gst_element_factory_make("capsfilter", EL_FRAMERATE_FILTER));

    GstCapsLM caps(gst_caps_new_simple("video/x-raw",
                                       "framerate",
                                       GST_TYPE_FRACTION,
                                       static_cast<guint>(target_fps * 100),
                                       100,
                                       nullptr));

    g_object_set(capsfilter.get(), "caps", caps.get(), nullptr);

    std::cout << "[Configuration] Inserting new elements" << std::endl;

    pipeline.unlink_elements(EL_FR_META_VISUALIZER, EL_VIDEO_CONVERT_TO_SINK);

    pipeline.add_to_pipeline(std::move(queue));
    pipeline.add_to_pipeline(std::move(videorate));
    pipeline.add_to_pipeline(std::move(capsfilter));

    pipeline.link_elements(EL_FR_META_VISUALIZER, EL_FR_SKIP_QUEUE);
    pipeline.link_elements(EL_FR_SKIP_QUEUE, EL_FRAMERATE);
    pipeline.link_elements(EL_FRAMERATE, EL_FRAMERATE_FILTER);
    pipeline.link_elements(EL_FRAMERATE_FILTER, EL_VIDEO_CONVERT_TO_SINK);
}

void FRPipeline::run()
{
    gst_element_set_state(pipeline.get(), GST_STATE_PLAYING);

    GstBusLM bus(gst_element_get_bus(pipeline.get()));
    GstMessageLM msg(gst_bus_timed_pop_filtered(
        bus.get(),
        GST_CLOCK_TIME_NONE,
        static_cast<GstMessageType>(GST_MESSAGE_ERROR | GST_MESSAGE_EOS)));

    if (msg.get() != NULL)
    {
        GError *err;
        gchar *debug_info;

        switch (GST_MESSAGE_TYPE(msg.get()))
        {
        case GST_MESSAGE_ERROR:
            gst_message_parse_error(msg.get(), &err, &debug_info);
            g_printerr("Error received from element %s: %s\n",
                       GST_OBJECT_NAME(msg.get()->src), err->message);
            g_printerr("Debugging information: %s\n",
                       debug_info ? debug_info : "none");
            g_clear_error(&err);
            g_free(debug_info);
            break;
        case GST_MESSAGE_EOS:
            g_print("End-Of-Stream reached.\n");
            break;
        default:
            g_printerr("Unexpected message received.\n");
            break;
        }
    }

    gst_element_set_state(pipeline.get(), GST_STATE_NULL);
}