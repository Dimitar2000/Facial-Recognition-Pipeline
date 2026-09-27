#include <iostream>

#include <gst/gst.h>
#include <gst/audio/audio.h>

#include "debug.h"
#include "filter.hpp"

int main(int argc, char *argv[])
{
  GstElement *pipeline, *source, *video_convert, *processor, *video_convert2, *sink;
  GstBus *bus;
  GstMessage *msg;

  /* Initialize GStreamer */
  gst_init (&argc, &argv);

  if (!gst_element_register(nullptr, "myfilter", GST_RANK_NONE, GST_TYPE_MY_FILTER))
  {
    std::cerr << "Failed to register myprocess element" << std::endl;
    return 1;
  }

  /* Create the elements */
  source          = gst_element_factory_make ("v4l2src", "source");
  video_convert   = gst_element_factory_make ("videoconvert", "video_convert");
  processor       = gst_element_factory_make ("myfilter", "custom_frame_processor");
  video_convert2  = gst_element_factory_make ("videoconvert", "video_convert_2");
  sink            = gst_element_factory_make ("autovideosink", "sink");

  /* Create the empty pipeline */
  pipeline = gst_pipeline_new ("test-pipeline");

  if (!pipeline || !source || !video_convert || !processor || !video_convert2 || !sink)
  {
    g_printerr ("Not all elements could be created.\n");
    return -1;
  }

  /* Link all elements that can be automatically linked because they have "Always" pads */
  gst_bin_add_many (GST_BIN (pipeline), source, video_convert, processor, video_convert2, sink, NULL);
  
  if (gst_element_link_many (source, video_convert, processor, video_convert2, sink, NULL) != TRUE) {
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