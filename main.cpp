#include <glib-object.h>
#include <gst/gstelementfactory.h>
#include <gst/gstobject.h>
#include <iostream>

#include <gst/gst.h>
#include <gst/audio/audio.h>
#include <opencv2/objdetect/face.hpp>

#include "debug.h"
#include "fr_transformer.hpp"

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

bool create_webcam_pipeline(std::string face_dataset_file_path, std::string yunet_model_file_path, std::string sface_model_file_path)
{
  GstElement *pipeline, *source, *caps_filter, *mjpeg, *video_convert, *fr_transformer, *video_convert2, *sink;
  GstBus *bus;
  GstMessage *msg;

  /* Create the elements */
  source          = gst_element_factory_make ("v4l2src", "source");
  caps_filter     = gst_element_factory_make ("capsfilter", "resolution");
  mjpeg           = gst_element_factory_make ("jpegdec", "jpeg decoder");
  video_convert   = gst_element_factory_make ("videoconvert", "video_convert");
  fr_transformer  = gst_element_factory_make ("fr-transformer", "facial-recognition-transformer");
  video_convert2  = gst_element_factory_make ("videoconvert", "video_convert_2");
  sink            = gst_element_factory_make ("autovideosink", "sink");

  /* Create the empty pipeline */
  pipeline = gst_pipeline_new ("webcam-pipeline");

  if (!pipeline || !source || !caps_filter || !mjpeg || !video_convert || !fr_transformer || !video_convert2 || !sink)
  {
    g_printerr ("Not all elements could be created.\n");
    return 1;
  }

  GstCaps* caps = gst_caps_new_simple("image/jpeg", 
                                      "width", G_TYPE_INT, 1920,
                                      "height", G_TYPE_INT, 1080,
                                      NULL);

  g_object_set(caps_filter, "caps", caps, NULL);
  gst_caps_unref(caps);

  gst_fr_transformer_set_data(GST_FR_TRANSFORMER(fr_transformer),
                              face_dataset_file_path,
                              yunet_model_file_path,
                              sface_model_file_path);

  /* Link all elements that can be automatically linked because they have "Always" pads */
  gst_bin_add_many (GST_BIN (pipeline), source, caps_filter, mjpeg, video_convert, fr_transformer, video_convert2, sink, NULL);
  
  if (gst_element_link_many (source, caps_filter, mjpeg, video_convert, fr_transformer, video_convert2, sink, NULL) != TRUE) {
    g_printerr ("Elements could not be linked.\n");
    gst_object_unref (pipeline);
    return 1;
  }

  /* Start playing the pipeline */
  gst_element_set_state (pipeline, GST_STATE_PLAYING);

    /* Wait until error or EOS */
  bus = gst_element_get_bus (pipeline);
  msg = gst_bus_timed_pop_filtered (bus, GST_CLOCK_TIME_NONE, static_cast<GstMessageType>(GST_MESSAGE_ERROR | GST_MESSAGE_EOS));

  /* Parse message */
  if (msg != NULL) {
    GError *err;
    gchar *debug_info;

    switch (GST_MESSAGE_TYPE (msg)) {
      case GST_MESSAGE_ERROR:
        gst_message_parse_error (msg, &err, &debug_info);
        g_printerr ("Error received from element %s: %s\n",
            GST_OBJECT_NAME (msg->src->name), err->message);
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

  /* Free resources */
  gst_object_unref (bus);
  gst_element_set_state (pipeline, GST_STATE_NULL);
  gst_object_unref (pipeline);
  return 0;
}

bool create_mp4_pipeline( std::string face_dataset_file_path, std::string yunet_model_file_path, std::string sface_model_file_path, std::string input_mp4_file_path)
{
  GstElement *pipeline, *source, *decoder, *video_convert, *fr_transformer, *video_convert2, *sink;
  GstBus *bus;
  GstMessage *msg;

  /* Create the elements */
  source          = gst_element_factory_make ("filesrc", "source");
  decoder         = gst_element_factory_make ("decodebin", "decoder");
  video_convert   = gst_element_factory_make ("videoconvert", "video_convert");
  fr_transformer  = gst_element_factory_make ("fr-transformer", "facial-recognition-transformer");
  video_convert2  = gst_element_factory_make ("videoconvert", "video_convert_2");
  sink            = gst_element_factory_make ("autovideosink", "sink");

  /* Create the empty pipeline */
  pipeline = gst_pipeline_new ("mp4-pipeline");

  if (!pipeline || !source || !decoder || !video_convert || !fr_transformer || !video_convert2 || !sink)
  {
    g_printerr ("Not all elements could be created.\n");
    return 1;
  }

  // Configure elements
  g_object_set(source, "location", input_mp4_file_path.c_str(), NULL);

  gst_fr_transformer_set_data(GST_FR_TRANSFORMER(fr_transformer),
                              face_dataset_file_path,
                              yunet_model_file_path,
                              sface_model_file_path);

  /* Link all elements that can be automatically linked because they have "Always" pads */
  gst_bin_add_many (GST_BIN (pipeline), source, decoder, video_convert, fr_transformer, video_convert2, sink, NULL);

  if (gst_element_link(source, decoder) != TRUE ||
      gst_element_link_many(video_convert, fr_transformer, video_convert2, sink, NULL) != TRUE) {
    g_printerr ("Elements could not be linked.\n");
    gst_object_unref (pipeline);
    return 1;
  }
  g_signal_connect(decoder, "pad-added", G_CALLBACK(on_decodebin_pad_added), video_convert);

  /* Start playing the pipeline */
  gst_element_set_state (pipeline, GST_STATE_PLAYING);

    /* Wait until error or EOS */
  bus = gst_element_get_bus (pipeline);
  msg = gst_bus_timed_pop_filtered (bus, GST_CLOCK_TIME_NONE, static_cast<GstMessageType>(GST_MESSAGE_ERROR | GST_MESSAGE_EOS));

  /* Parse message */
  if (msg != NULL) {
    GError *err;
    gchar *debug_info;

    switch (GST_MESSAGE_TYPE (msg)) {
      case GST_MESSAGE_ERROR:
        gst_message_parse_error (msg, &err, &debug_info);
        g_printerr ("Error received from element %s: %s\n",
            GST_OBJECT_NAME (msg->src->name), err->message);
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

  /* Free resources */
  gst_object_unref (bus);
  gst_element_set_state (pipeline, GST_STATE_NULL);
  gst_object_unref (pipeline);
  return 0;
}

int main(int argc, char *argv[])
{
  // Initialize GStreamer
  gst_init (&argc, &argv);

  // Register custom components
  if (!gst_element_register(nullptr, "fr-transformer", GST_RANK_NONE, GST_TYPE_FR_TRANSFORMER))
  {
    std::cerr << "Failed to register myprocess element" << std::endl;
    return 1;
  }

  // Parse arguments
  if (argc < 5)
  {
    std::cerr << "Not enough arguments. Format is: \n"
              << "  <app> webcam <input-face-embeddings-file-path> <face-detection-yunet-file-path> <face-recog-sface-file-path>\n"
              << "  <app> mp4    <input-face-embeddings-file-path> <face-detection-yunet-file-path> <face-recog-sface-file-path> <input-mp4-file-path>" << std::endl;
    return 1;
  }

  std::string source_type            = argv[1];
  std::string face_dataset_file_path = argv[2];
  std::string yunet_model_file_path  = argv[3];
  std::string sface_model_file_path  = argv[4];

  bool error = false;

  if (source_type == "webcam")
  {
    std::cout << "Source type is: Camera" << std::endl;

    error = create_webcam_pipeline(face_dataset_file_path, yunet_model_file_path, sface_model_file_path);
  }
  else if (source_type == "mp4")
  {
    std::cout << "Source type is: MP4 video file" << std::endl;

    // Parse arguments
    if (argc < 6)
    {
      std::cerr << "Not enough arguments. Format is <app> mp4 <input-face-embeddings-file-path> <face-detection-yunet-file-path> <face-recog-sface-file-path> <input-mp4-file-path>" << std::endl;
      return 1;
    }

    std::string input_mp4_file_path = argv[5];

    error = create_mp4_pipeline(face_dataset_file_path, yunet_model_file_path, sface_model_file_path, input_mp4_file_path);
  }
  else
  {
    std::cerr << "Invalid source type provided - <" << source_type << ">" << std::endl;
    return 1;
  }

  if (error)
  {
    std::cerr << "Pipeline could not be created." << std::endl;
    return 1;
  }

  return 0;
}