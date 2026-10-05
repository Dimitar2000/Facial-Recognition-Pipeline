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

        // Add control framerate at source to test FPS change behaviour
        void add_virtual_src_fps();

        // Set the virtual src fps to trigger a caps renegotiation downstream
        void set_virtual_src_fps(double fps);

        virtual ~FRPipeline() = default;

    protected:
        FRPipeline() = default;

        FRPipeline(double target_fps,
                   std::string face_dataset_file_path,
                   std::string yunet_model_file_path,
                   std::string sface_model_file_path,
                   bool monitor_src_caps);
        
        virtual GstElementLM get_source() = 0;

        void attach_fr_measurement_probes();

        void attach_source_caps_event_probe();

        void configure_skip_queues(double target_fps, 
                                   GstClockTime max_fr_latency);

        // FR latency measurement and adjusrtment
        static GstPadProbeReturn fr_measure_probe_entry_cb(GstPad *pad,
                                                           GstPadProbeInfo *info,
                                                           gpointer user_data);

        static GstPadProbeReturn fr_measure_probe_exit_cb(GstPad *pad,
                                                          GstPadProbeInfo *info,
                                                          gpointer user_data);

        void adjust_for_fr_max_measure();

        // Warm up
        static GstPadProbeReturn fr_warm_up_drop_probe_cb(GstPad *pad,
                                                          GstPadProbeInfo *info,
                                                          gpointer user_data);

        // Source caps monitoring
        static GstPadProbeReturn src_caps_event_probe_cb(GstPad *pad,
                                                         GstPadProbeInfo *info,
                                                         gpointer user_data);

        void handle_msg_reconfigure_fps(const GstStructure * s);

        struct TimeMeasurement {
            GstClockTime base;
            GstClockTime last;
            GstClockTime max;
        };

        struct ProbeData {
            TimeMeasurement fr_measurement;
            guint           warm_up_frame_counter;
        };

        // Used to track latency of FR and reconfigure queuing
        //  after warmup and at runtime if the RTS changes
        static const GstClockTime DEFAULT_FR_MAX_LATENCY = 100 * GST_MSECOND;
        static constexpr double FR_LATENCY_MARGIN_FACTOR = 1.3;
        
        // Used by the warm up procedure
        static const guint WARMUP_FRAMES            = 10;
        static const guint WARMUP_IGNORE_NO_MEASURE = WARMUP_FRAMES / 2;

        static const char * EL_PIPELINE;
        static const char * EL_SRC_ENDPOINT;
        static const char * EL_VIRT_SRC_FPS_QUEUE;
        static const char * EL_VIRT_SRC_FPS_FRAMERATE;
        static const char * EL_VIRT_SRC_FPS_FILTER;
        static const char * EL_VIDEO_CONVERT_FROM_SOURCE;
        static const char * EL_Q_CONST_FPS;
        static const char * EL_V_CONST_FPS;
        static const char * EL_F_CONST_FPS;
        static const char * EL_BURSTQUEUE;
        static const char * EL_FR_ELEMENT;
        static const char * EL_FR_SKIP_QUEUE;
        static const char * EL_FRAMERATE;
        static const char * EL_FRAMERATE_FILTER;
        static const char * EL_FR_META_VISUALIZER;
        static const char * EL_VIDEO_CONVERT_TO_SINK;
        static const char * EL_SINK;

        static const char * MSG_RECONFIGURE_FPS;

        double           target_fps;
        bool             monitor_src_fps;

        ConfigCalculator config_calculator;
        GstPipelineLM    pipeline;

        ProbeData    probe_data;
        GstClockTime expected_max_fr_latency;
};