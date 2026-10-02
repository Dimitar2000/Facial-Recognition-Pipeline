#pragma once

#include "gst/gstelement.h"
#include "gst/gstpad.h"

#define GST_TYPE_FR_METADATA_VISUALIZER (gst_fr_metadata_visualizer_get_type())

G_DECLARE_FINAL_TYPE(
    FRMetadataVisualizer,
    gst_fr_metadata_visualizer,
    GST,
    FR_METADATA_VISUALIZER,
    GstElement
)

gboolean gst_fr_metadata_visualizer_sink_event(GstPad *pad,
                                              GstObject *parent,
                                              GstEvent *event);

GstFlowReturn gst_fr_metadata_visualizer_chain(GstPad *pad,
                                              GstObject *parent,
                                              GstBuffer *buf);
