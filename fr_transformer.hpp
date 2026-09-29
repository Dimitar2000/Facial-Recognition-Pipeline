#include "gst/gstelement.h"
#include "gst/gstpad.h"

#define GST_TYPE_FR_TRANSFORMER (gst_fr_transformer_get_type())

G_DECLARE_FINAL_TYPE(
    GstFRTransformer,
    gst_fr_transformer,
    GST,
    FR_TRANSFORMER,
    GstElement
)

static gboolean gst_fr_transformer_sink_event (GstPad *pad, GstObject *parent, GstEvent *event);

GstFlowReturn gst_fr_transformer_chain (GstPad    *pad,
                                        GstObject *parent,
                                        GstBuffer *buf);