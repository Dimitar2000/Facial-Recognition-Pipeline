#pragma once

#include "gst/gstelement.h"
#include "gst/gstpad.h"
#include <string>

#define GST_TYPE_FR_TRANSFORMER (gst_fr_transformer_get_type())

G_DECLARE_FINAL_TYPE(
    GstFRTransformer,
    gst_fr_transformer,
    GST,
    FR_TRANSFORMER,
    GstElement
)

void gst_fr_transformer_init_processor(GstFRTransformer *transformer,
                                       const std::string& face_dataset_file_path,
                                       const std::string& yunet_model_file_path,
                                       const std::string& sface_model_file_path);

void gst_fr_transformer_set_skips(GstFRTransformer *transformer,
                                  guint skips);

gboolean gst_fr_transformer_sink_event(GstPad *pad,
                                       GstObject *parent,
                                       GstEvent *event);

GstFlowReturn gst_fr_transformer_chain(GstPad *pad,
                                       GstObject *parent,
                                       GstBuffer *buf);