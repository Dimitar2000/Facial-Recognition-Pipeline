#include "fr_element.hpp"

#include <string>

#include <gst/gstclock.h>
#include <gst/video/video-frame.h>
#include <gst/video/video-info.h>

#include "fr/cpu_fr_processor.hpp"
#include "pipeline/metadata/fr_metadata.hpp"
#include "util/record.hpp"

typedef struct _FRElement {
    GstElement element;
    GstPad *sinkpad, *srcpad;
    FRProcessor *processor;
} FRElement;

G_DEFINE_TYPE(FRElement, gst_fr_element, GST_TYPE_ELEMENT);

static void gst_fr_element_finalize(GObject *object)
{
    FRElement *element = GST_FR_ELEMENT(object);
    delete element->processor;
    G_OBJECT_CLASS(gst_fr_element_parent_class)->finalize(object);
}

static void gst_fr_element_class_init(FRElementClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    object_class->finalize = gst_fr_element_finalize;

    GstElementClass *element_class = GST_ELEMENT_CLASS(klass);

    gst_element_class_set_static_metadata(element_class,
        "Facial recognition element",
        "Filter/Effect/Video",
        "Detects and identifies faces in video frames",
        "dimitar-barantiev");

    GstCaps *caps = gst_caps_from_string(
        "video/x-raw,"
        "format=(string)RGB,"
        "width=(int)[1,MAX],"
        "height=(int)[1,MAX],"
        "framerate=(fraction)[0/1,MAX]");

    GstPadTemplate *src_factory = gst_pad_template_new(
        "src", GST_PAD_SRC, GST_PAD_ALWAYS, gst_caps_ref(caps));
    GstPadTemplate *sink_factory = gst_pad_template_new(
        "sink", GST_PAD_SINK, GST_PAD_ALWAYS, gst_caps_ref(caps));

    gst_element_class_add_pad_template(element_class, src_factory);
    gst_element_class_add_pad_template(element_class, sink_factory);
    gst_caps_unref(caps);
}

static void gst_fr_element_init(FRElement *element)
{
    GstElementClass *klass = GST_ELEMENT_GET_CLASS(element);

    element->sinkpad = gst_pad_new_from_template(
        gst_element_class_get_pad_template(klass, "sink"), "sink");
    gst_pad_set_chain_function(element->sinkpad, gst_fr_element_chain);
    gst_pad_set_event_function(element->sinkpad, gst_fr_element_sink_event);
    gst_element_add_pad(GST_ELEMENT(element), element->sinkpad);

    element->srcpad = gst_pad_new_from_template(
        gst_element_class_get_pad_template(klass, "src"), "src");
    gst_element_add_pad(GST_ELEMENT(element), element->srcpad);

    element->processor = nullptr;
}

void gst_fr_element_init_processor(FRElement *element,
                                   const std::string& face_dataset_file_path,
                                   const std::string& yunet_model_file_path,
                                   const std::string& sface_model_file_path,
                                   int scaled_dim)
{
    element->processor = new CPUFRProcessor(face_dataset_file_path,
                                            yunet_model_file_path,
                                            sface_model_file_path,
                                            scaled_dim);
}

gboolean gst_fr_element_sink_event(GstPad *pad, GstObject *parent, GstEvent *event)
{
    FRElement *element = GST_FR_ELEMENT(parent);

    switch (GST_EVENT_TYPE(event))
    {
    case GST_EVENT_CAPS:
        return gst_pad_push_event(element->srcpad, event);
    case GST_EVENT_EOS:
    default:
        return gst_pad_event_default(pad, parent, event);
    }
}

GstFlowReturn gst_fr_element_chain(GstPad *pad, GstObject *parent, GstBuffer *buf)
{
    FRElement *element = GST_FR_ELEMENT(parent);
    GstElement *element_gst = GST_ELEMENT(parent);
    TimeRecorder time_recorder;

    time_recorder.start("Total");

    GstCaps *caps = gst_pad_get_current_caps(pad);
    if (!caps)
    {
        return gst_pad_push(element->srcpad, buf);
    }

    GstVideoInfo info;
    if (!gst_video_info_from_caps(&info, caps))
    {
        return GST_FLOW_ERROR;
    }

    guint width = GST_VIDEO_INFO_WIDTH(&info);
    guint height = GST_VIDEO_INFO_HEIGHT(&info);
    guint stride = GST_VIDEO_INFO_PLANE_STRIDE(&info, 0);

    buf = gst_buffer_make_writable(buf);
    if (!buf)
    {
        gst_caps_unref(caps);
        return GST_FLOW_ERROR;
    }

    FRMetadata *metadata = reinterpret_cast<FRMetadata *>(
        gst_buffer_add_meta(buf, FR_METADATA_INFO, NULL));

    if (!metadata)
    {
        throw "MMM";
    }

    GstVideoFrame frame_view;

    if (gst_video_frame_map(&frame_view, &info, buf, GST_MAP_READWRITE))
    {
        guint8 *data = static_cast<guint8 *>(GST_VIDEO_FRAME_PLANE_DATA(&frame_view, 0));        
        
        cv::Mat frame(height, 
                      width, 
                      CV_8UC3, 
                      data,
                      stride);

        metadata->detected_faces = element->processor->process_frame(frame);
        metadata->frame_width = frame.cols;
        metadata->frame_heigth = frame.rows;

        gst_video_frame_unmap(&frame_view);
    }

    gst_caps_unref(caps);

    time_recorder.stop();

    return gst_pad_push(element->srcpad, buf);
}