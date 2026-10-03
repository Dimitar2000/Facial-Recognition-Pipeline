#pragma once

#include <gst/gstclock.h>
#include <string>

#include "config_calculator.hpp"
#include "gst_wrappers/gst_element_lm.hpp"

class FRPipeline
{
    public:
        // Run the pipeline 
        void run();

        // Perform warm up with a few frames from the real source.
        // This allows:
        //      - the FR models to load all runtime data - cold start is removed
        //      - the FR stage's latency to be measured for the actual workload
        //      - the optimal skip+queue configuration to be created so that 
        //          minimum amount of frames are skipped
        void warm_up();

        // Extend the pipeline with a post-FR queue and  
        void add_skip_queuing();

        virtual ~FRPipeline() = default;

    protected:
        FRPipeline() = default;

        FRPipeline(double target_fps,
                   std::string face_dataset_file_path,
                   std::string yunet_model_file_path,
                   std::string sface_model_file_path);

        void attach_fr_measurement_probes();

        void configure_skip_queues(double target_fps, 
                                   GstClockTime max_fr_latency);

        static GstPadProbeReturn fr_measure_probe_entry_cb(GstPad *pad,
                                                           GstPadProbeInfo *info,
                                                           gpointer user_data);

        static GstPadProbeReturn fr_measure_probe_exit_cb(GstPad *pad,
                                                          GstPadProbeInfo *info,
                                                          gpointer user_data);

        static GstPadProbeReturn fr_warm_up_drop_probe_cb(GstPad *pad,
                                                          GstPadProbeInfo *info,
                                                          gpointer user_data);

        struct TimeMeasurement {
            GstClockTime base;
            GstClockTime last;
            GstClockTime max;
        };

        ConfigCalculator config_calculator;
        double target_fps;
        GstPipelineLM pipeline;

        // Used to track latency of FR and reconfigure queuing
        //  after warmup and at runtime if the RTS changes
        static const GstClockTime DEFAULT_FR_MAX_LATENCY = 100 * GST_MSECOND;
        static const guint FR_LATENCY_MARGIN_FACTOR = 2;
        TimeMeasurement fr_measurement;

        // Used by the warm up procedure
        static const guint WARMUP_FRAMES = 10;
        guint    warm_up_frame_counter;        

        const char * EL_PIPELINE                  = "pipeline";
        const char * EL_VIDEO_CONVERT_FROM_SOURCE = "video_convert_from_source";
        const char * EL_FR_ELEMENT                = "facial_recognition_element";
        const char * EL_FR_SKIP_QUEUE             = "fr_skip_queue";
        const char * EL_FRAMERATE                 = "framerate";
        const char * EL_FRAMERATE_FILTER          = "capsfilter_fps";
        const char * EL_FR_META_VISUALIZER        = "fr_meta_visualizer";
        const char * EL_VIDEO_CONVERT_TO_SINK     = "video_convert_to_sink";
        const char * EL_SINK                      = "sink";
};