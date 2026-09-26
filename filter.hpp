#include "gst/gstelement.h"
#include "gst/gstpad.h"
#include "gst/gstplugin.h"
#include "gst/gstversion.h"

typedef struct _GstMyFilter {
  GstElement element;
  GstPad *sinkpad, *srcpad;

  gboolean silent;
} GstMyFilter;

/* Standard definition defining a class for this element. */
typedef struct _GstMyFilterClass {
  GstElementClass parent_class;
} GstMyFilterClass;

/* Standard macros for defining types for this element.  */
#define GST_TYPE_MY_FILTER              (gst_my_filter_get_type())
#define GST_MY_FILTER(obj)              (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_MY_FILTER,GstMyFilter))
#define GST_MY_FILTER_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_MY_FILTER,GstMyFilterClass))
#define GST_IS_MY_FILTER(obj)           (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_MY_FILTER))
#define GST_IS_MY_FILTER_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_MY_FILTER))

/* Standard function returning type information. */
GType gst_my_filter_get_type (void);

GST_ELEMENT_REGISTER_DECLARE(my_filter)
GST_ELEMENT_REGISTER_DEFINE(my_filter, "my-filter", GST_RANK_NONE, GST_TYPE_MY_FILTER);

G_DEFINE_TYPE (GstMyFilter, gst_my_filter, GST_TYPE_ELEMENT);

static GstFlowReturn gst_my_filter_chain (GstPad    *pad,
                                          GstObject *parent,
                                          GstBuffer *buf);

static void gst_my_filter_class_init(GstMyFilterClass * klass)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);

    gst_element_class_set_static_metadata (element_class,
        "An example plugin",
        "Example/FirstExample",
        "Shows the basic structure of a plugin",
        "your name <your.name@your.isp>");

    GstCaps *caps = gst_caps_from_string(
        "video/x-raw,"
        "format=(string)RGB,"
        "width=(int)[1,MAX],"
        "height=(int)[1,MAX],"
        "framerate=(fraction)[0/1,MAX]"
    );

    GstPadTemplate * src_factory =
        gst_pad_template_new (
            "source",
            GST_PAD_SRC,
            GST_PAD_ALWAYS,
            gst_caps_ref(caps)
        );

    GstPadTemplate * sink_factory =
        gst_pad_template_new(
            "sink",
            GST_PAD_SINK,
            GST_PAD_ALWAYS,
            gst_caps_ref(caps)
        );

    gst_element_class_add_pad_template(element_class, src_factory);
    gst_element_class_add_pad_template(element_class, sink_factory);

    gst_caps_unref(caps);
}

static void gst_my_filter_init (GstMyFilter *filter)
{
    GstElementClass * klass = GST_ELEMENT_GET_CLASS(filter);

    /* pad through which data comes in to the element */
    filter->sinkpad = gst_pad_new_from_template(gst_element_class_get_pad_template(klass, "sink"), "sink");

    /* configure chain function on the pad before adding the pad to the element */
    gst_pad_set_chain_function (filter->sinkpad, gst_my_filter_chain);

    gst_element_add_pad (GST_ELEMENT (filter), filter->sinkpad);

    /* pad through which data goes out of the element */
    filter->srcpad = gst_pad_new_from_template(gst_element_class_get_pad_template(klass, "source"), "source");
    gst_element_add_pad (GST_ELEMENT (filter), filter->srcpad);

    /* properties initial value */
    filter->silent = FALSE;
}

static GstFlowReturn gst_my_filter_chain (GstPad *pad, GstObject *parent, GstBuffer *buf)
{
    GstMyFilter *filter = GST_MY_FILTER (parent);

    if (!filter->silent)
        g_print ("Have data of size %" G_GSIZE_FORMAT" bytes!\n",
            gst_buffer_get_size (buf));

    return gst_pad_push (filter->srcpad, buf);
}