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

void gst_fr_transformer_set_data(GstFRTransformer * transformer,
                                 cv::Ptr<cv::FaceDetectorYN>&& face_detector,
                                 cv::Ptr<cv::FaceRecognizerSF>&& face_recogniser,
                                 std::vector<FaceEmbeddings>&& face_database);

gboolean gst_fr_transformer_sink_event (GstPad *pad, 
                                        GstObject *parent, 
                                        GstEvent *event);

GstFlowReturn gst_fr_transformer_chain (GstPad *pad, 
                                        GstObject *parent, 
                                        GstBuffer *buf);