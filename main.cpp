#include <exception>
#include <glib-object.h>
#include <gst/gstelementfactory.h>
#include <gst/gstobject.h>
#include <iostream>

#include <gst/gst.h>
#include <gst/audio/audio.h>
#include <opencv2/objdetect/face.hpp>
#include <vector>

#include "debug.h"
#include "fr_transformer.hpp"
#include "yaml_util.hpp"

bool create_webcam_pipeline(std::string face_dataset_data_path)
{
  GstElement *pipeline, *source, *caps_filter, *mjpeg, *video_convert, *fr_transformer, *video_convert2, *sink;
  GstBus *bus;
  GstMessage *msg;

  std::vector<FaceEmbeddings> face_dataset;
  
  face_dataset = parse_yaml_embeddings(face_dataset_data_path);

  if (face_dataset.empty())
  {
    std::cerr << "No embeddings were loaded!" << std::endl;
    return 1;
  }

  auto face_detector = cv::FaceDetectorYN::create(
        "../models/face_detection_yunet_2023mar.onnx",
        "",
        cv::Size(320, 320)
  );

  auto face_recogniser = cv::FaceRecognizerSF::create(
        "../models/face_recognition_sface_2021dec.onnx",
        ""
  );

  std::cout << "Loading facial recognition models was successful!" << std::endl;

  /* Create the elements */
  source          = gst_element_factory_make ("v4l2src", "source");
  caps_filter     = gst_element_factory_make ("capsfilter", "resolution");
  mjpeg           = gst_element_factory_make ("jpegdec", "jpeg decoder");
  video_convert   = gst_element_factory_make ("videoconvert", "video_convert");
  fr_transformer  = gst_element_factory_make ("fr-transformer", "facial-recognition-transformer");
  video_convert2  = gst_element_factory_make ("videoconvert", "video_convert_2");
  sink            = gst_element_factory_make ("autovideosink", "sink");

  /* Create the empty pipeline */
  pipeline = gst_pipeline_new ("test-pipeline");

  if (!pipeline || !source || !caps_filter || !mjpeg || !video_convert || !fr_transformer || !video_convert2 || !sink)
  {
    g_printerr ("Not all elements could be created.\n");
    return -1;
  }

  GstCaps* caps = gst_caps_new_simple("image/jpeg", 
                                      "width", G_TYPE_INT, 1920,
                                      "height", G_TYPE_INT, 1080,
                                      NULL);

  g_object_set(caps_filter, "caps", caps, NULL);
  gst_caps_unref(caps);  

  gst_fr_transformer_set_data(
    GST_FR_TRANSFORMER(fr_transformer),
    std::move(face_detector),
    std::move(face_recogniser),
    std::move(face_dataset)
  );

  /* Link all elements that can be automatically linked because they have "Always" pads */
  gst_bin_add_many (GST_BIN (pipeline), source, caps_filter, mjpeg, video_convert, fr_transformer, video_convert2, sink, NULL);
  
  if (gst_element_link_many (source, caps_filter, mjpeg, video_convert, fr_transformer, video_convert2, sink, NULL) != TRUE) {
    g_printerr ("Elements could not be linked.\n");
    gst_object_unref (pipeline);
    return -1;
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
  if (argc < 3)
  {
    std::cerr << "Not enough arguments. Format is <app> <source-type> <input-face-embeddings-file>" << std::endl;
    return 1;
  }

  std::string source_type            = argv[1];
  std::string face_dataset_file_path = argv[2];

  if (source_type == "webcam")
  {
    std::cout << "Source type is: Camera" << std::endl;

    bool error = create_webcam_pipeline(face_dataset_file_path);

    if (error)
    {
      std::cerr << "Pipeline could not be created!" << std::endl;
      return 1;
    }
  }
  else if (source_type == "mp4-video")
  {
    std::cout << "Source type is: MP4 video file" << std::endl;

    // TODO
  }
  else
  {
    std::cerr << "Invalid source type provided - <" << source_type << ">" << std::endl;
    return 1;
  }

  return 0;
}