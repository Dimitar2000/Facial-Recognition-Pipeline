#include "fr_metadata_visualizer.hpp"

#include <gst/gstbuffer.h>
#include <gst/video/video-frame.h>
#include <gst/video/video-info.h>

#include <opencv2/imgproc.hpp>
#include <string>

#include "pipeline/metadata/fr_metadata.hpp"

typedef struct _FRMetadataVisualizer {
    GstElement element;
    GstPad *sinkpad;
    GstPad *srcpad;

    int skip_counter;
} FRMetadataVisualizer;

G_DEFINE_TYPE(FRMetadataVisualizer, gst_fr_metadata_visualizer, GST_TYPE_ELEMENT);

static void gst_fr_metadata_visualizer_class_init(FRMetadataVisualizerClass *klass)
{
    GstElementClass *element_class = GST_ELEMENT_CLASS(klass);

    gst_element_class_set_static_metadata(element_class,
        "FR metadata visualizer",
        "Filter/Effect/Video",
        "Reads metadata from a buffer and prepares it for visualization",
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

static void gst_fr_metadata_visualizer_init(FRMetadataVisualizer *element)
{
    GstElementClass *klass = GST_ELEMENT_GET_CLASS(element);

    element->sinkpad = gst_pad_new_from_template(
        gst_element_class_get_pad_template(klass, "sink"), "sink");
    gst_pad_set_chain_function(element->sinkpad, gst_fr_metadata_visualizer_chain);
    gst_pad_set_event_function(element->sinkpad, gst_fr_metadata_visualizer_sink_event);
    gst_element_add_pad(GST_ELEMENT(element), element->sinkpad);

    element->srcpad = gst_pad_new_from_template(
        gst_element_class_get_pad_template(klass, "src"), "src");
    gst_element_add_pad(GST_ELEMENT(element), element->srcpad);

    element->skip_counter = 0;
}

static void gst_fr_metadata_visualizer_handle_metadata(FRMetadataVisualizer& meta_visualizer, cv::Mat& frame, FRMetadata *metadata)
{
    if (!metadata)
    {
        meta_visualizer.skip_counter++;
        
        cv::putText(frame,
                    "Skipped " + std::to_string(meta_visualizer.skip_counter),
                    cv::Point2d(0, frame.rows - 1),
                    cv::FONT_HERSHEY_PLAIN,
                    3,
                    {255, 255, 0},
                    2);

        return;
    }

    meta_visualizer.skip_counter = 0;

    if (metadata->detected_faces.empty())
    {

        cv::putText(frame,
                    "No faces detected",
                    cv::Point2d(0, frame.rows - 1),
                    cv::FONT_HERSHEY_PLAIN,
                    3,
                    {255, 255, 0},
                    2);
        
        return;
    }

    for (const auto& face: metadata->detected_faces) 
    {
        std::vector<gchar *> metadata_lines;
        
        if (face.identity.has_value())
        {
            auto id = face.identity;
            
            metadata_lines = {
                g_strdup_printf("Visualize"),
                g_strdup_printf("%s", id->name.c_str()),
                g_strdup_printf("%i/%i", id->reference_matches, id->reference_images),
                g_strdup_printf("(%.3g, %.3g)", id->min_similarity, id->max_similarity)
            };
        }
        else
        {
            metadata_lines = {
                g_strdup_printf("Unknown"),
                g_strdup_printf("(%.3g, %.3g)", face.min_similarity, face.max_similarity),
            };
        }

        auto rect = face.face_rect;

        cv::rectangle(frame, rect, cv::Scalar(0, 255, 0), 2);

        for (int i = 0; i < metadata_lines.size(); i++)
        {
            cv::putText(frame,
                        metadata_lines[i],
                        cv::Point2d(static_cast<int>(rect.x),
                                        static_cast<int>(rect.y - i * 40)),
                        cv::FONT_HERSHEY_PLAIN,
                        3,
                        {255, 0, 0},
                        2);
            g_free(metadata_lines[i]);
        }
    }
}

gboolean gst_fr_metadata_visualizer_sink_event(GstPad *pad,
                                              GstObject *parent,
                                              GstEvent *event)
{
    FRMetadataVisualizer *element = GST_FR_METADATA_VISUALIZER(parent);

    switch (GST_EVENT_TYPE(event))
    {
    case GST_EVENT_CAPS:
        return gst_pad_push_event(element->srcpad, event);
    case GST_EVENT_EOS:
    default:
        return gst_pad_event_default(pad, parent, event);
    }
}

GstFlowReturn gst_fr_metadata_visualizer_chain(GstPad *pad,
                                              GstObject *parent,
                                              GstBuffer *buf)
{
    FRMetadataVisualizer *element = GST_FR_METADATA_VISUALIZER(parent);

    GstCaps *caps = gst_pad_get_current_caps(pad);
    if (!caps)
    {
        return gst_pad_push(element->srcpad, buf);
    }

    GstVideoInfo info;
    if (!gst_video_info_from_caps(&info, caps))
    {
        gst_caps_unref(caps);
        return GST_FLOW_ERROR;
    }

    gst_caps_unref(caps);

    guint width = GST_VIDEO_INFO_WIDTH(&info);
    guint height = GST_VIDEO_INFO_HEIGHT(&info);
    guint stride = GST_VIDEO_INFO_PLANE_STRIDE(&info, 0);

    FRMetadata *metadata = reinterpret_cast<FRMetadata *>(
        gst_buffer_get_meta(buf, FR_METADATA_API_TYPE)
    );

    GstVideoFrame frame_view;

    if (gst_video_frame_map(&frame_view, &info, buf, GST_MAP_READ))
    {
        guint8 *data = static_cast<guint8 *>(GST_VIDEO_FRAME_PLANE_DATA(&frame_view, 0));
        
        cv::Mat frame(height, 
                      width, 
                      CV_8UC3, 
                      data, 
                      stride);

        gst_fr_metadata_visualizer_handle_metadata(*element, frame, metadata);

        gst_video_frame_unmap(&frame_view);
    }


    // The visualizer is a pass-through element for now. It keeps the original frame
    // intact and lets the caller decide how to render or annotate it later.
    return gst_pad_push(element->srcpad, buf);
}
