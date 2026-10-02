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
            GstBusLM bus(gst_element_get_bus (pipeline.get()));
            
            GstMessageLM msg(gst_bus_timed_pop_filtered (bus.get(),
                                                           GST_CLOCK_TIME_NONE, 
                                                           static_cast<GstMessageType>(GST_MESSAGE_ERROR | GST_MESSAGE_EOS)));

            // Parse message
            if (msg.get() != NULL) {
                GError *err;
                gchar *debug_info;
                
                switch (GST_MESSAGE_TYPE (msg.get())) 
                {
                case GST_MESSAGE_ERROR:
                    gst_message_parse_error (msg.get(), &err, &debug_info);
                    g_printerr ("Error received from element %s: %s\n",
                        GST_OBJECT_NAME (msg.get()->src), err->message);
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
            }

            // Reset pipeline
            gst_element_set_state (pipeline.get(), GST_STATE_NULL);
        }

        virtual ~FRPipeline() = default;

    protected:
        FRPipeline() = default;

        FRPipeline(double target_fps,
                   std::string face_dataset_file_path, 
                   std::string yunet_model_file_path, 
                   std::string sface_model_file_path)
            : target_fps(target_fps)
        {
            /* Create the empty pipeline */
            pipeline = GstPipelineLM(gst_pipeline_new ("pipeline"));

            // Create the common elements
            GstElementLM video_convert(gst_element_factory_make ("videoconvert", "video_convert"));
            GstElementLM fr_transformer(gst_element_factory_make ("fr-transformer", "facial-recognition-transformer"));
            GstElementLM video_convert2(gst_element_factory_make ("videoconvert", "video_convert_2"));
            GstElementLM sink(gst_element_factory_make ("autovideosink", "sink"));

            gst_fr_transformer_load_models(GST_FR_TRANSFORMER(fr_transformer.get()),
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
        double target_fps;
        GstPipelineLM pipeline;
};

class WebcamFRPipeline: public FRPipeline
{
    public:
        WebcamFRPipeline(double target_fps,
                         std::string face_dataset_file_path, 
                         std::string yunet_model_file_path, 
                         std::string sface_model_file_path)
        :
            FRPipeline(target_fps, 
                       face_dataset_file_path, 
                       yunet_model_file_path, 
                       sface_model_file_path)
        {
            // Create the elements
            GstElementLM source(gst_element_factory_make ("v4l2src", "source"));
            GstElementLM caps_filter(gst_element_factory_make ("capsfilter", "resolution"));
            GstElementLM mjpeg(gst_element_factory_make ("jpegdec", "jpeg_decoder"));

            // Set required camera frame format and resolution
            GstCapsLM caps = GstCapsLM(gst_caps_new_simple("image/jpeg", 
                                                "width", G_TYPE_INT, 1920,
                                                "height", G_TYPE_INT, 1080,
                                                NULL));
            g_object_set(caps_filter.get(), "caps", caps.get(), NULL);

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
        MP4FRPipeline(double target_fps,
                      std::string face_dataset_file_path, 
                      std::string yunet_model_file_path, 
                      std::string sface_model_file_path,
                      std::string input_mp4_file_path)
        :
            FRPipeline(target_fps,
                       face_dataset_file_path, 
                       yunet_model_file_path, 
                       sface_model_file_path) 
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
            GstPadLM sink_pad = GstPadLM(gst_element_get_static_pad(video_convert, "sink"));
            
            // If our converter is already linked, we have nothing to do here
            if (gst_pad_is_linked(sink_pad.get()))
            {
                return;
            }

            // Get the caps of the new source pad
            GstCapsLM caps = GstCapsLM(gst_pad_get_current_caps(new_pad), true);
          
            try {
                caps = GstCapsLM(gst_pad_get_current_caps(new_pad));
            }
            catch (std::runtime_error& e) {
                g_printerr("Failed to query caps from new pad: %s\n", e.what());
                
                caps = GstCapsLM(gst_pad_query_caps(new_pad, nullptr), true);
            }
            
            if (gst_caps_is_empty(caps.get()) || gst_caps_is_any(caps.get()))
            {
                throw std::runtime_error("New pad does not have required caps");
            }

            // Check if the new pad is a video pad
            const GstStructure *structure = gst_caps_get_structure(caps.get(), 0);
                
            if (!g_str_has_prefix(gst_structure_get_name(structure), "video/"))
            {
                throw std::runtime_error("Decoded pad is not video.\n");
            }

            // Link the new pad to the video converter's sink pad
            GstPadLinkReturn result = gst_pad_link(new_pad, sink_pad.get());

            if (result != GST_PAD_LINK_OK)
            {
                throw std::runtime_error("Could not link decoded video pad " + std::to_string(result) + ").\n");
            }

            g_printerr("Decodebin pad added and linked successfully.\n");
        }
};