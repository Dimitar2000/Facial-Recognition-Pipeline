#include "gst/gstelement.h"
#include "gst/gstpad.h"
#include "yaml_util.hpp"
#include <opencv2/objdetect/face.hpp>

#define GST_TYPE_FR_TRANSFORMER (gst_fr_transformer_get_type())

G_DECLARE_FINAL_TYPE(
    GstFRTransformer,
    gst_fr_transformer,
    GST,
    FR_TRANSFORMER,
    GstElement
)

bool gst_fr_transformer_set_data(GstFRTransformer * transformer,
                                 const std::string& face_dataset_file_path,
                                 const std::string& yunet_model_file_path,
                                 const std::string& sface_model_file_path);

gboolean gst_fr_transformer_sink_event (GstPad *pad, 
                                        GstObject *parent, 
                                        GstEvent *event);

GstFlowReturn gst_fr_transformer_chain (GstPad *pad, 
                                        GstObject *parent, 
                                        GstBuffer *buf);