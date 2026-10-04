#include "fr_pipeline.hpp"

#include <algorithm>
#include <cstdlib>
#include <glib-object.h>
#include <glib.h>
#include <gst/gst.h>
#include <gst/gstclock.h>
#include <gst/gstelement.h>
#include <gst/gstevent.h>
#include <gst/gstmessage.h>
#include <gst/gstpad.h>
#include <gst/gststructure.h>
#include <gst/gstutils.h>
#include <iostream>
#include <stdexcept>

#include "elements/fr_element.hpp"
#include "gst_wrappers/gst_element_lm.hpp"

const char * FRPipeline::EL_PIPELINE                  = "pipeline";
const char * FRPipeline::EL_VIDEO_CONVERT_FROM_SOURCE = "video_convert_from_source";
const char * FRPipeline::EL_FR_ELEMENT                = "facial_recognition_element";
const char * FRPipeline::EL_FR_SKIP_QUEUE             = "fr_skip_queue";
const char * FRPipeline::EL_FRAMERATE                 = "framerate";
const char * FRPipeline::EL_FRAMERATE_FILTER          = "capsfilter_fps";
const char * FRPipeline::EL_FR_META_VISUALIZER        = "fr_meta_visualizer";
const char * FRPipeline::EL_VIDEO_CONVERT_TO_SINK     = "video_convert_to_sink";
const char * FRPipeline::EL_SINK                      = "sink";

const char * FRPipeline::MSG_RECONFIGURE_FPS = "reconfigure-fps";

FRPipeline::FRPipeline(double target_fps,
                       std::string face_dataset_file_path,
                       std::string yunet_model_file_path,
                       std::string sface_model_file_path)
    : target_fps(target_fps),
      probe_data({{0, 0, 0}, 0})
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

    pipeline.add_to_pipeline(video_convert);
    pipeline.add_to_pipeline(fr_element);
    pipeline.add_to_pipeline(fr_meta_visualizer);
    pipeline.add_to_pipeline(video_convert2);
    pipeline.add_to_pipeline(sink);

    pipeline.link_elements(EL_VIDEO_CONVERT_FROM_SOURCE, EL_FR_ELEMENT);
    pipeline.link_elements(EL_FR_ELEMENT, EL_FR_META_VISUALIZER);
    pipeline.link_elements(EL_FR_META_VISUALIZER, EL_VIDEO_CONVERT_TO_SINK);
    pipeline.link_elements(EL_VIDEO_CONVERT_TO_SINK, EL_SINK);

    attach_fr_measurement_probes();
}

void FRPipeline::attach_fr_measurement_probes()
{
    GstElementLM fr_element = pipeline.get_by_name(EL_FR_ELEMENT);

    // Install a dropping probe to stop the pipeline after N frames
    GstPadLM sink_pad(gst_element_get_static_pad(fr_element.get(), "sink"));
    GstPadLM src_pad(gst_element_get_static_pad(fr_element.get(), "src"));

    gst_pad_add_probe(sink_pad.get(),
                      GST_PAD_PROBE_TYPE_BUFFER,
                      FRPipeline::fr_measure_probe_entry_cb,
                      &this->probe_data,
                      NULL);
   
    gst_pad_add_probe(src_pad.get(),
                      GST_PAD_PROBE_TYPE_BUFFER,
                      FRPipeline::fr_measure_probe_exit_cb,
                      &this->probe_data,
                      NULL);
}

void FRPipeline::attach_source_caps_event_probe()
{
    GstElementLM source = get_source();
    GstPadLM src_pad = gst_element_get_static_pad(source.get(), "src");

    gst_pad_add_probe(src_pad.get(),
                      GST_PAD_PROBE_TYPE_EVENT_DOWNSTREAM,
                      FRPipeline::src_caps_event_probe_cb,
                      NULL,
                      NULL);
}


void FRPipeline::configure_skip_queues(double target_fps, 
                                       GstClockTime max_fr_latency)
{
    this->expected_max_fr_latency = max_fr_latency * FR_LATENCY_MARGIN_FACTOR;

    auto latency_pipeline_base  = 10 * GST_MSECOND;
    auto latency_queue_overhead = 1 * GST_MSECOND;

    std::cout << "[Pipeline] Calculating required skip queue size\n" 
              << "           Using: \n"
              << "              Target FPS                 : " << target_fps << "\n"
              << "              Latency of Base Pipeline   : " << latency_pipeline_base << "\n"
              << "              Latency of Queue           : " << latency_queue_overhead << "\n"
              << "              Expected Max Latency of FR : " << expected_max_fr_latency << "\n"
              << std::endl;

    auto slots = config_calculator.compute_queue_slots(target_fps,
                                                       latency_pipeline_base,
                                                       latency_queue_overhead,
                                                       expected_max_fr_latency);

    std::cout << "[Pipeline] Configuring skip queue with " << slots << " slots" << std::endl;

    GstElementLM queue            = pipeline.get_by_name(EL_FR_SKIP_QUEUE);
    GstElementLM framerate_filter = pipeline.get_by_name(EL_FRAMERATE_FILTER);
    GstElementLM element          = pipeline.get_by_name(EL_FR_ELEMENT);

    // Set queue size
    g_object_set(queue.get(),
                 "max-size-buffers", slots,
                 "max-size-bytes", 0,
                 "max-size-time", 0,
                 nullptr);

    // Set FR skips
    std::cout << "[Pipeline] Configuring FR element to skip " << slots - 1 << " frames" << std::endl;

    gst_fr_element_set_skips(GST_FR_ELEMENT(element.get()), slots - 1);

    std::cout << "[Pipeline] Configuring framerate pair with FPS = " << target_fps << std::endl;

    // Set required FPS for queue buffers downstream
    GstCapsLM caps(gst_caps_new_simple("video/x-raw",
                                       "framerate",
                                       GST_TYPE_FRACTION,
                                       static_cast<guint>(target_fps * 100),
                                       100,
                                       nullptr));

    g_object_set(framerate_filter.get(), "caps", caps.get(), nullptr);

}

GstPadProbeReturn FRPipeline::fr_measure_probe_entry_cb(GstPad *pad,
                                                        GstPadProbeInfo *info,
                                                        gpointer user_data)
{
    ProbeData *probe_data = reinterpret_cast<ProbeData *>(user_data);

    probe_data->fr_measurement.base = gst_util_get_timestamp();

    return GST_PAD_PROBE_OK;
}

GstPadProbeReturn FRPipeline::fr_measure_probe_exit_cb(GstPad *pad,
                                                       GstPadProbeInfo *info,
                                                       gpointer user_data)
{
    ProbeData *probe_data = reinterpret_cast<ProbeData *>(user_data);
    TimeMeasurement& fr_measurement = probe_data->fr_measurement;

    fr_measurement.last = gst_util_get_timestamp() - fr_measurement.base;
    fr_measurement.max  = std::max(fr_measurement.last, fr_measurement.max);

    return GST_PAD_PROBE_OK;
}

GstPadProbeReturn FRPipeline::fr_warm_up_drop_probe_cb(GstPad *pad,
                                                       GstPadProbeInfo *info,
                                                       gpointer user_data)
{
    std::cout << "[Block Probe] Dropping warm up frame ..." << std::endl;

    if (!(info->type & GST_PAD_PROBE_TYPE_BUFFER))
    {
        return GST_PAD_PROBE_OK;
    }
    
    ProbeData *probe_data = reinterpret_cast<ProbeData *>(user_data);
    guint& counter = probe_data->warm_up_frame_counter;

    // Delete this frame's measurement from the other probes 
    if (counter < FRPipeline::WARMUP_IGNORE_NO_MEASURE)
    {
        probe_data->fr_measurement = {0, 0, 0};
    }

    counter++;

    if (counter >= FRPipeline::WARMUP_FRAMES) {

        std::cout << "[Block Probe] Sending EOS ..." << std::endl;

        GstElementLM pipeline(GST_ELEMENT(
            gst_pad_get_parent_element(pad)
        ));

        gst_element_send_event(
            pipeline.get(),
            gst_event_new_eos()
        );
    }

    return GST_PAD_PROBE_DROP;
}

GstPadProbeReturn FRPipeline::src_caps_event_probe_cb(GstPad *pad,
                                                      GstPadProbeInfo *info,
                                                      gpointer user_data)
{
    if (GST_PAD_PROBE_INFO_TYPE(info) & GST_PAD_PROBE_TYPE_EVENT_DOWNSTREAM)
    {
        GstEvent *event = GST_PAD_PROBE_INFO_EVENT(info);

        if (GST_EVENT_TYPE(event) == GST_EVENT_CAPS)
        {
            std::cout << "[Caps Probe] Source caps renegotiation message intercepted." << std::endl;

            GstCaps *caps;

            gst_event_parse_caps(event, &caps);

            const GstStructure *s = gst_caps_get_structure(caps, 0);

            if (gst_structure_has_field(s, "framerate")) 
            {
                std::cout << "[Caps Probe] Sending message to reconfigure ..." << std::endl;

                // Get the new fps
                gint fps_numerator = 0;
                gint fps_denominator = 0;

                if (!gst_structure_get_fraction(s, 
                                                "framerate",
                                                &fps_numerator, 
                                                &fps_denominator) 
                    || fps_denominator == 0)
                {
                    throw std::runtime_error("[Caps Probe] Could not read a valid framerate fraction.");
                    return GST_PAD_PROBE_OK;
                }

                double fps = static_cast<double>(fps_numerator) / fps_denominator;

                // Send a message to reconfigure the pipeline
                GstElementLM pipeline(GST_ELEMENT(gst_pad_get_parent_element(pad)));

                GstStructure *s = gst_structure_new(
                    MSG_RECONFIGURE_FPS,
                    "fps", G_TYPE_DOUBLE, fps,
                    NULL
                );

                GstMessage *msg = gst_message_new_application(GST_OBJECT(pipeline.get()), s);

                gst_element_post_message(pipeline.get(), msg);
            }
        }
    }

    return GST_PAD_PROBE_OK;
}

void FRPipeline::warm_up()
{
    GstElementLM fr_element = pipeline.get_by_name(EL_FR_ELEMENT);

    // Install a dropping probe to stop the pipeline after N frames
    GstPadLM src_pad(gst_element_get_static_pad(fr_element.get(), "src"));

    int drop_probe_id = gst_pad_add_probe(src_pad.get(),
                                          GST_PAD_PROBE_TYPE_BUFFER,
                                          FRPipeline::fr_warm_up_drop_probe_cb,
                                          &this->probe_data,
                                          NULL);

    gst_fr_element_set_skips(GST_FR_ELEMENT(fr_element.get()), 0);

    // Run the pipeline until the drop probe sends EOS
    std::cout << "[Pipeline] Starting warm up ..." << std::endl;

    gst_element_set_state(pipeline.get(), GST_STATE_PLAYING);

    GstBusLM bus(gst_element_get_bus(pipeline.get()));
    GstMessageLM msg(gst_bus_timed_pop_filtered(
        bus.get(),
        GST_CLOCK_TIME_NONE,
        static_cast<GstMessageType>(GST_MESSAGE_ERROR | GST_MESSAGE_EOS)));

    if (msg.get() != NULL)
    {
        switch (GST_MESSAGE_TYPE(msg.get()))
        {
        case GST_MESSAGE_ERROR:
            throw std::runtime_error("Warm up received an error!");
            break;

        case GST_MESSAGE_EOS:
            std::cout << "[Pipeline] Warm up finished." << std::endl;
            break;
        
        default:
            g_printerr("Unexpected message received.\n");
            break;
        }
    }

    gst_element_set_state(pipeline.get(), GST_STATE_NULL);

    // Remove the dropping probe
    gst_pad_remove_probe(src_pad.get(),drop_probe_id);
}

void FRPipeline::add_skip_queuing()
{
    GstElementLM queue(gst_element_factory_make("queue", EL_FR_SKIP_QUEUE));
    GstElementLM videorate(gst_element_factory_make("videorate", EL_FRAMERATE));
    GstElementLM capsfilter(gst_element_factory_make("capsfilter", EL_FRAMERATE_FILTER));

    std::cout << "[Pipeline] Inserting queueing + framerate elements" << std::endl;

    pipeline.unlink_elements(EL_FR_META_VISUALIZER, EL_VIDEO_CONVERT_TO_SINK);

    pipeline.add_to_pipeline(queue);
    pipeline.add_to_pipeline(videorate);
    pipeline.add_to_pipeline(capsfilter);

    pipeline.link_elements(EL_FR_META_VISUALIZER, EL_FR_SKIP_QUEUE);
    pipeline.link_elements(EL_FR_SKIP_QUEUE, EL_FRAMERATE);
    pipeline.link_elements(EL_FRAMERATE, EL_FRAMERATE_FILTER);
    pipeline.link_elements(EL_FRAMERATE_FILTER, EL_VIDEO_CONVERT_TO_SINK);

    configure_skip_queues(target_fps, probe_data.fr_measurement.max     
                                                        ? probe_data.fr_measurement.max
                                                        : DEFAULT_FR_MAX_LATENCY);
}

void FRPipeline::run()
{
    attach_source_caps_event_probe();
    
    gst_element_set_state(pipeline.get(), GST_STATE_PLAYING);

    bool run = true;
    GstBusLM bus = gst_element_get_bus(pipeline.get());

    while (run)
    {
        GstMessage * m = gst_bus_timed_pop_filtered(bus.get(),
                                                    GST_SECOND,
                                                    static_cast<GstMessageType>(
                                                        GST_MESSAGE_ERROR | 
                                                        GST_MESSAGE_EOS | 
                                                        GST_MESSAGE_APPLICATION));

        // When no new message is received, check if the maximum detected latency has changed
        if (!m)
        {
            std::cout << "[Pipeline] Checking current max measurement ... " << std::endl;

            if (probe_data.fr_measurement.max > expected_max_fr_latency)
            {
                std::cout << "[Pipeline] Reconfiguring for new max FR latency ..." << std::endl;

                configure_skip_queues(target_fps, probe_data.fr_measurement.max);
            }

            continue;
        }

        // Process a new message
        GstMessageLM msg(m);

        switch (GST_MESSAGE_TYPE(msg.get()))
        {
        case GST_MESSAGE_APPLICATION:
        {
            std::cout << "[Pipeline] Received application message." << std::endl;

            const GstStructure *s = gst_message_get_structure(msg.get());

            if (gst_structure_has_name(s, FRPipeline::MSG_RECONFIGURE_FPS)) 
            {
                std::cout << "[Pipeline] Reconfiguring for new fps ..." << std::endl;
    
                double new_fps;

                // Extract the new fps
                gst_structure_get_double(s, "fps", &new_fps);

                // If the new fps is lower than the current Reconfigure the queuing
                target_fps = new_fps;

                configure_skip_queues(target_fps, probe_data.fr_measurement.max
                                                        ? probe_data.fr_measurement.max
                                                        : DEFAULT_FR_MAX_LATENCY);
            }
            break;
        }

        case GST_MESSAGE_EOS:
        {
            g_print("[Pipeline] End-Of-Stream reached.\n");

            run = false;

            break;
        }

        case GST_MESSAGE_ERROR:
        {
            GError *err;
        
            gst_message_parse_error(msg.get(), &err, NULL);

            g_printerr("[Pipeline] Error received from element %s: %s\n", GST_OBJECT_NAME(msg.get()->src), err->message);
            run = false;

            g_clear_error(&err);
            break;
        }

        default:
        {
            throw std::runtime_error("Unexpected message received.");
            break;
        }
        }
    }

    gst_element_set_state(pipeline.get(), GST_STATE_NULL);
}