#include "gst/gstelement.h"
#include "gst/gstpad.h"

G_DECLARE_FINAL_TYPE(
    GstMyFilter,
    gst_my_filter,
    GST,
    MY_FILTER,
    GstElement
)

/* Standard macros for defining types for this element.  */
#define GST_TYPE_MY_FILTER              (gst_my_filter_get_type())
#define GST_MY_FILTER(obj)              (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_MY_FILTER,GstMyFilter))
#define GST_MY_FILTER_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_MY_FILTER,GstMyFilterClass))
#define GST_IS_MY_FILTER(obj)           (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_MY_FILTER))
#define GST_IS_MY_FILTER_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_MY_FILTER))

gboolean gst_my_filter_sink_event (GstPad *pad, GstObject *parent, GstEvent *event);

GstFlowReturn gst_my_filter_chain (GstPad    *pad,
                                   GstObject *parent,
                                   GstBuffer *buf);