#include <glib-object.h>
#include <gst/gstelement.h>
#include <gst/gstobject.h>
#include <gst/gstpipeline.h>
#include <gst/gstutils.h>
#include <string>

#include "fr_transformer.hpp"
#include "gst_element_lm.hpp"

class FRPipeline
{
    public:
        void run() 
        {
            // Start pipeline
            gst_element_set_state (pipeline.get(), GST_STATE_PLAYING);
            
            // Wait until error or EOS
            GstBus * bus = gst_element_get_bus (pipeline.get());
            GstMessage * msg = gst_bus_timed_pop_filtered (bus,
                                                           GST_CLOCK_TIME_NONE, 
                                                           static_cast<GstMessageType>(GST_MESSAGE_ERROR | GST_MESSAGE_EOS));

            // Parse message
            if (msg != NULL) {
                GError *err;
                gchar *debug_info;
                
                switch (GST_MESSAGE_TYPE (msg)) 
                {
                case GST_MESSAGE_ERROR:
                    gst_message_parse_error (msg, &err, &debug_info);
                    g_printerr ("Error received from element %s: %s\n",
                        GST_OBJECT_NAME (msg->src), err->message);
                    g_printerr ("Debugging information: %s\n",
                        debug_info ? debug_info : "none");
                    g_clear_error (&err);
                    g_free (debug_info);
                    break;
                case GST_MESSAGE_EOS:
                    g_print ("End-Of-Stream reached.\n");
                    break;
                default:
                    /* We should not reach here because we only asked for ERRORs and EOS */
                    g_printerr ("Unexpected message received.\n");
                    break;
                }
                gst_message_unref (msg);
            }

            // Release running resources
            gst_object_unref (bus);

            // Reset pipeline
            gst_element_set_state (pipeline.get(), GST_STATE_NULL);
        }

        virtual ~FRPipeline() = default;

    protected:
        FRPipeline() = default;

        FRPipeline(std::string face_dataset_file_path, 
                   std::string yunet_model_file_path, 
                   std::string sface_model_file_path)
        {
            /* Create the empty pipeline */
            pipeline = GstPipelineLM(gst_pipeline_new ("pipeline"));

            // Create the common elements
            GstElementLM video_convert(gst_element_factory_make ("videoconvert", "video_convert"));
            GstElementLM fr_transformer(gst_element_factory_make ("fr-transformer", "facial-recognition-transformer"));
            GstElementLM video_convert2(gst_element_factory_make ("videoconvert", "video_convert_2"));
            GstElementLM sink(gst_element_factory_make ("autovideosink", "sink"));

            gst_fr_transformer_set_data(GST_FR_TRANSFORMER(fr_transformer.get()),
                                        face_dataset_file_path,
                                        yunet_model_file_path,
                                        sface_model_file_path);

            pipeline.add_to_pipeline(std::move(video_convert));
            pipeline.add_to_pipeline(std::move(fr_transformer));
            pipeline.add_to_pipeline(std::move(video_convert2));
            pipeline.add_to_pipeline(std::move(sink));

            pipeline.link_elements("video_convert", "facial-recognition-transformer");
            pipeline.link_elements("facial-recognition-transformer", "video_convert_2");
            pipeline.link_elements("video_convert_2", "sink");
        }

    protected:
        GstPipelineLM pipeline;
};

class WebcamFRPipeline: public FRPipeline
{
    public:
        WebcamFRPipeline(std::string face_dataset_file_path, 
                         std::string yunet_model_file_path, 
                         std::string sface_model_file_path)
        :
            FRPipeline(face_dataset_file_path, yunet_model_file_path, sface_model_file_path)
        {
            // Create the elements
            GstElementLM source(gst_element_factory_make ("v4l2src", "source"));
            GstElementLM caps_filter(gst_element_factory_make ("capsfilter", "resolution"));
            GstElementLM mjpeg(gst_element_factory_make ("jpegdec", "jpeg_decoder"));

            // Set required camera frame format and resolution
            GstCaps* caps = gst_caps_new_simple("image/jpeg", 
                                                "width", G_TYPE_INT, 1920,
                                                "height", G_TYPE_INT, 1080,
                                                NULL);
            g_object_set(caps_filter.get(), "caps", caps, NULL);
            gst_caps_unref(caps);

            /* Link all elements that can be automatically linked because they have "Always" pads */
            pipeline.add_to_pipeline(std::move(source));
            pipeline.add_to_pipeline(std::move(caps_filter));
            pipeline.add_to_pipeline(std::move(mjpeg));

            pipeline.link_elements("source", "resolution");
            pipeline.link_elements("resolution", "jpeg_decoder");
            pipeline.link_elements("jpeg_decoder", "video_convert");
        }

        ~WebcamFRPipeline() = default;
};

class MP4FRPipeline: public FRPipeline
{
    public:
        MP4FRPipeline(std::string face_dataset_file_path, 
                      std::string yunet_model_file_path, 
                      std::string sface_model_file_path,
                      std::string input_mp4_file_path)
        :
            FRPipeline(face_dataset_file_path, yunet_model_file_path, sface_model_file_path) 
        {
            // Create the elements
            GstElementLM source(gst_element_factory_make ("filesrc", "source"));
            GstElementLM decoder(gst_element_factory_make ("decodebin", "decoder"));

            // Set input MP4 video file location for streaming 
            g_object_set(source.get(), "location", input_mp4_file_path.c_str(), NULL);

            /* Link all elements that can be automatically linked because they have "Always" pads */
            pipeline.add_to_pipeline(std::move(source));
            pipeline.add_to_pipeline(std::move(decoder));

            pipeline.link_elements("source", "decoder");

            GstElement* video_convert = pipeline.get_by_name("video_convert");

            g_signal_connect(pipeline.get_by_name("decoder"), 
                             "pad-added", 
                             G_CALLBACK(MP4FRPipeline::on_decodebin_pad_added), 
                             video_convert);
        }

        ~MP4FRPipeline() = default;

    private:
        static void on_decodebin_pad_added(GstElement *, GstPad *new_pad, gpointer user_data)
        {
            GstElement *video_convert = GST_ELEMENT(user_data);
            GstPad *sink_pad = gst_element_get_static_pad(video_convert, "sink");
            
            if (gst_pad_is_linked(sink_pad))
            {
                gst_object_unref(sink_pad);
                return;
            }

            GstCaps *caps = gst_pad_get_current_caps(new_pad);
            if (!caps)
            {
                caps = gst_pad_query_caps(new_pad, nullptr);
            }

            bool is_video = false;
            if (caps && !gst_caps_is_empty(caps) && !gst_caps_is_any(caps))
            {
                const GstStructure *structure = gst_caps_get_structure(caps, 0);
                is_video = g_str_has_prefix(gst_structure_get_name(structure), "video/");
            }

            if (is_video)
            {
                GstPadLinkReturn result = gst_pad_link(new_pad, sink_pad);
                if (result != GST_PAD_LINK_OK)
                {
                    g_printerr("Could not link decoded video pad (error %d).\n", result);
                }
            }

            if (caps)
            {
                gst_caps_unref(caps);
            }
            gst_object_unref(sink_pad);
        }
};