#include "filter.hpp"

typedef struct _GstMyFilter {
  GstElement element;
  GstPad *sinkpad, *srcpad;

  gboolean silent;
} GstMyFilter;

G_DEFINE_TYPE (GstMyFilter, gst_my_filter, GST_TYPE_ELEMENT);
GST_ELEMENT_REGISTER_DEFINE(my_filter, "my-filter", GST_RANK_NONE, GST_TYPE_MY_FILTER);

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
    gst_pad_set_event_function (filter->sinkpad, gst_my_filter_sink_event);

    gst_element_add_pad (GST_ELEMENT (filter), filter->sinkpad);

    /* pad through which data goes out of the element */
    filter->srcpad = gst_pad_new_from_template(gst_element_class_get_pad_template(klass, "source"), "source");
    gst_element_add_pad (GST_ELEMENT (filter), filter->srcpad);

    /* properties initial value */
    filter->silent = FALSE;
}

gboolean gst_my_filter_sink_event (GstPad *pad, GstObject *parent, GstEvent  *event)
{
  gboolean ret;
  GstMyFilter *filter = GST_MY_FILTER (parent);

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_CAPS:
      /* we should handle the format here */

      /* push the event downstream */
      ret = gst_pad_push_event (filter->srcpad, event);
      break;
    case GST_EVENT_EOS:
      /* end-of-stream, we should close down all stream leftovers here */
      ret = gst_pad_event_default (pad, parent, event);
      break;
    default:
      /* just call the default handler */
      ret = gst_pad_event_default (pad, parent, event);
      break;
  }
  return ret;
}

GstFlowReturn gst_my_filter_chain (GstPad *pad, GstObject *parent, GstBuffer *buf)
{
    GstMyFilter *filter = GST_MY_FILTER (parent);

    if (!filter->silent)
        g_print ("Have data of size %" G_GSIZE_FORMAT" bytes!\n", gst_buffer_get_size (buf));

    return gst_pad_push (filter->srcpad, buf);
}