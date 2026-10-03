#pragma once

#include "gst/gstelement.h"
#include "gst/gstpad.h"
#include <string>

#define GST_TYPE_FR_ELEMENT (gst_fr_element_get_type())

G_DECLARE_FINAL_TYPE(
    FRElement,
    gst_fr_element,
    GST,
    FR_ELEMENT,
    GstElement
)

void gst_fr_element_init_processor(FRElement *element,
                                   const std::string& face_dataset_file_path,
                                   const std::string& yunet_model_file_path,
                                   const std::string& sface_model_file_path,
                                   int scaled_dim);

void gst_fr_element_set_skips(FRElement *element,
                              guint skips);

gboolean gst_fr_element_sink_event(GstPad *pad,
                                   GstObject *parent,
                                   GstEvent *event);

GstFlowReturn gst_fr_element_chain(GstPad *pad,
                                   GstObject *parent,
                                   GstBuffer *buf);