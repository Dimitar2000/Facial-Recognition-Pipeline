#include "filter.hpp"

#include "glib.h"
#include "gst/gstclock.h"
#include "gst/gstelement.h"

#include "gst/video/video-frame.h"
#include "gst/video/video-info.h"
#include "opencv2/core/mat.hpp"
#include "opencv2/core/types.hpp"
#include "opencv2/imgproc.hpp"
#include "opencv2/objdetect.hpp"

#define RECORD_START(el, b) (b = gst_element_get_current_running_time(el))
#define RECORD_END(el, b, msg) (g_print("%-20s: %11lu\n", msg, gst_element_get_current_running_time(el) - b))

typedef struct _GstMyFilter {
    GstElement element;
    GstPad *sinkpad, *srcpad;

    cv::CascadeClassifier classifier; 
} GstMyFilter;

G_DEFINE_TYPE (GstMyFilter, gst_my_filter, GST_TYPE_ELEMENT);
GST_ELEMENT_REGISTER_DEFINE(my_filter, "my-filter", GST_RANK_NONE, GST_TYPE_MY_FILTER);

static void detect_and_bind_box(GstElement * el, cv::Mat& frame, cv::CascadeClassifier& cascade, double scale, double zoom_out_rec_scale);

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
            "src",
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
    filter->srcpad = gst_pad_new_from_template(gst_element_class_get_pad_template(klass, "src"), "src");
    gst_element_add_pad (GST_ELEMENT (filter), filter->srcpad);

    // Load classifiers from &quot;opencv/data/haarcascades&quot; directory 
    filter->classifier.load("../haarcascade_frontalcatface.xml") ; 
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
    GstElement  *filter_el = GST_ELEMENT(parent);

    GstClockTime clock_time_start;

    RECORD_START(filter_el, clock_time_start);

    // Create opencv Map view of the GstBuffer memory    
    GstCaps *caps = gst_pad_get_current_caps(pad);

    if (!caps)
    {
        return gst_pad_push (filter->srcpad, buf);
    }

    GstVideoInfo info;

    if (!gst_video_info_from_caps(&info, caps)) {
        // invalid caps
        return GST_FLOW_ERROR;
    }

    guint width  = GST_VIDEO_INFO_WIDTH(&info);
    guint height = GST_VIDEO_INFO_HEIGHT(&info);
    guint stride = GST_VIDEO_INFO_PLANE_STRIDE(&info, 0);

    GstVideoFrame frame;

    if (gst_video_frame_map(&frame, &info, buf, GST_MAP_READ)) {

        guint8 *data = (guint8 *)GST_VIDEO_FRAME_PLANE_DATA(&frame, 0);

        cv::Mat f(height,
                  width,
                  CV_8UC3,
                  data,
                  stride);

        detect_and_bind_box(filter_el, f, filter->classifier, 1, 3);

        gst_video_frame_unmap(&frame);
    }

    gst_caps_unref(caps);

    GstClockTime clock_time_end = gst_element_get_current_running_time(filter_el);

    RECORD_END(filter_el, clock_time_start, "Total");

    return gst_pad_push (filter->srcpad, buf);
}

static void detect_and_bind_box(GstElement * el,
 cv::Mat& frame, cv::CascadeClassifier& cascade, double scale, double zoom_out_rec_scale)
{
    std::vector<cv::Rect> faces;

    GstClockTime clock_time_base;
    
    RECORD_START(el, clock_time_base);
    cv::Mat gray = frame.clone();
    RECORD_END(el, clock_time_base, "Frame copy");
    
    RECORD_START(el, clock_time_base);
    cvtColor(gray, gray, cv::COLOR_RGB2GRAY); // Convert to Gray Scale
    RECORD_END(el, clock_time_base, "Greyscale");

    cv::Mat smallImg;

    // Resize the Grayscale Image 
    RECORD_START(el, clock_time_base);
    resize( gray, smallImg, cv::Size(), 1 / scale, 1 / scale, cv::INTER_LINEAR); 
    RECORD_END(el, clock_time_base, "Resize");

    RECORD_START(el, clock_time_base);
    equalizeHist( smallImg, smallImg );
    RECORD_END(el, clock_time_base, "Equalization");

    // Detect faces of different sizes using cascade classifier 
    RECORD_START(el, clock_time_base);
    cascade.detectMultiScale(smallImg, faces, 1.1, 
                            3, 0|cv::CASCADE_SCALE_IMAGE, cv::Size(30, 30) );
    RECORD_END(el, clock_time_base, "Detection");

    // Draw circles around the faces
    for ( size_t i = 0; i < faces.size(); i++ )
    {
        cv::Rect r = faces[i];
        cv::Scalar color = cv::Scalar(255, 0, 0); // Color for Drawing tool

        r.x = r.x - (r.width * (sqrt(zoom_out_rec_scale) - 1) / 2);
        r.y = r.y - (r.height * (sqrt(zoom_out_rec_scale) - 1) / 2);
        r.width = r.width * sqrt(zoom_out_rec_scale);
        r.height = r.height * sqrt(zoom_out_rec_scale);

        rectangle(frame, 
                  cv::Point(cvRound(r.x*scale), cvRound(r.y*scale)),
                  cv::Point(cvRound((r.x + r.width-1)*scale), cvRound((r.y + r.height-1)*scale)), 
                  color, 3, 8, 0);
    }
}