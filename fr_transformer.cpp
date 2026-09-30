#include "fr_transformer.hpp"

#include <cmath>
#include <iostream>

#include <gst/gstclock.h>
#include <gst/video/video-frame.h>
#include <gst/video/video-info.h>
#include <opencv2/core/types.hpp>
#include <opencv2/imgproc.hpp>

#include "yaml_util.hpp"

#define RECORD_START(el, b) (b = gst_element_get_current_running_time(el))
#define RECORD_END(el, b, msg) (g_print("%-20s: %11lu\n", msg, gst_element_get_current_running_time(el) - b))

typedef struct _GstFRTransformer {
    GstElement element;
    GstPad *sinkpad, *srcpad;

    cv::Ptr<cv::FaceDetectorYN> face_detector;
    cv::Ptr<cv::FaceRecognizerSF> face_recogniser;
    std::vector<FaceEmbeddings> face_database;
} GstFRTransformer;

G_DEFINE_TYPE (GstFRTransformer, gst_fr_transformer, GST_TYPE_ELEMENT);

static void detect_and_bind_box(GstFRTransformer * transformer, cv::Mat& frame, int scaled_width);

static void gst_fr_transformer_class_init(GstFRTransformerClass * klass)
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

static void gst_fr_transformer_init (GstFRTransformer *filter)
{
    GstElementClass * klass = GST_ELEMENT_GET_CLASS(filter);

    /* pad through which data comes in to the element */
    filter->sinkpad = gst_pad_new_from_template(gst_element_class_get_pad_template(klass, "sink"), "sink");

    /* configure chain function on the pad before adding the pad to the element */
    gst_pad_set_chain_function (filter->sinkpad, gst_fr_transformer_chain);
    gst_pad_set_event_function (filter->sinkpad, gst_fr_transformer_sink_event);

    gst_element_add_pad (GST_ELEMENT (filter), filter->sinkpad);

    /* pad through which data goes out of the element */
    filter->srcpad = gst_pad_new_from_template(gst_element_class_get_pad_template(klass, "src"), "src");
    gst_element_add_pad (GST_ELEMENT (filter), filter->srcpad);
}

bool gst_fr_transformer_set_data(GstFRTransformer * transformer,
                                 const std::string& face_dataset_file_path,
                                 const std::string& yunet_model_file_path,
                                 const std::string& sface_model_file_path)
{
    std::vector<FaceEmbeddings> face_database = parse_yaml_embeddings(face_dataset_file_path);

    if (face_database.empty())
    {
        std::cerr << "No embeddings were loaded." << std::endl;
        return 1;
    }

    transformer->face_database = face_database;

    transformer->face_detector = cv::FaceDetectorYN::create(yunet_model_file_path,
                                                            "",
                                                            cv::Size(320, 320));

    transformer->face_recogniser = cv::FaceRecognizerSF::create(sface_model_file_path, 
																"");

	std::cout << "Loaded facial recognition models." << std::endl;

	return 0;
}

gboolean gst_fr_transformer_sink_event (GstPad *pad, GstObject *parent, GstEvent  *event)
{
  gboolean ret;
    GstFRTransformer *filter = GST_FR_TRANSFORMER (parent);

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

GstFlowReturn gst_fr_transformer_chain (GstPad *pad, GstObject *parent, GstBuffer *buf)
{
    GstFRTransformer *transformer    = GST_FR_TRANSFORMER (parent);
    GstElement       *transformer_el = GST_ELEMENT(parent);

    GstClockTime clock_time_start;

    RECORD_START(transformer_el, clock_time_start);

    // Create opencv Map view of the GstBuffer memory    
    GstCaps *caps = gst_pad_get_current_caps(pad);

    if (!caps)
    {
        return gst_pad_push (transformer->srcpad, buf);
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

        detect_and_bind_box(transformer, f, 1024);

        gst_video_frame_unmap(&frame);
    }

    gst_caps_unref(caps);

    GstClockTime clock_time_end = gst_element_get_current_running_time(transformer_el);

    RECORD_END(transformer_el, clock_time_start, "Total");
    std::cout << std::endl;

    return gst_pad_push (transformer->srcpad, buf);
}

static void detect_and_bind_box(GstFRTransformer * transformer, cv::Mat& frame, int scaled_width)
{
    GstElement  *el = GST_ELEMENT(transformer);
    GstClockTime clock_time_base;

    cv::Size original_size = {frame.cols, frame.rows};
    cv::Size scaled_size   = {scaled_width, static_cast<int>(scaled_width / original_size.aspectRatio())};
    cv::Mat  scaled_frame;
    cv::Mat  faces;

    // If the frame does not have to be scaled or it is already smaller
    //  than the scaled version, just use it without compression
    if (!scaled_size.empty() && scaled_size.area() < original_size.area())
    {
        RECORD_START(el, clock_time_base);
        cv::resize(frame, scaled_frame, scaled_size);
        RECORD_END(el, clock_time_base, "Rescaling");
    }
    else
    {
        scaled_size = original_size;
        scaled_frame = frame;
    }

    RECORD_START(el, clock_time_base);
    transformer->face_detector->setInputSize(scaled_frame.size());
    RECORD_END(el, clock_time_base, "Setting input size");

    RECORD_START(el, clock_time_base);
    transformer->face_detector->detect(scaled_frame, faces);
    RECORD_END(el, clock_time_base, "Detection");

    if (faces.empty())
    {
        std::cout << "No faces detected\n";
        return;
    }

    for (int i = 0; i < faces.rows; i++)
    {
        cv::Mat face = faces.row(i);
        cv::Mat aligned_face;
        cv::Mat embedding;

        RECORD_START(el, clock_time_base);
        transformer->face_recogniser->alignCrop(scaled_frame, 
                                                face,
                                                aligned_face);
        RECORD_END(el, clock_time_base, "Alignment");

        RECORD_START(el, clock_time_base);
        transformer->face_recogniser->feature(aligned_face,
                                              embedding);
        RECORD_END(el, clock_time_base, "Embedding");

        int best_matches            =  0;
        std::string best_match_name = "Unknown";

        RECORD_START(el, clock_time_base);
        for (const auto& [name, ref_embeddings]: transformer->face_database)
        {
            std::cout << "Matching against reference embeddings of " << name << " ..." << std::endl;

            int matches = 0;

            for (const auto& ref_embedding: ref_embeddings)
            {
                double similarity = transformer->face_recogniser->match(embedding,
                                                                        ref_embedding,
                                                                        cv::FaceRecognizerSF::FR_COSINE);

                if (similarity >= 0.85)
                {
                    matches++;
                }
            }

            if (matches > best_matches)
            {
                best_matches = matches;
                best_match_name = name;
            }
        }
        RECORD_END(el, clock_time_base, "Matching");

        float x      = face.at<float>(0, 0);
        float y      = face.at<float>(0, 1);
        float width  = face.at<float>(0, 2);
        float height = face.at<float>(0, 3);

        double sx = static_cast<double>(original_size.width) / scaled_size.width;
        double sy = static_cast<double>(original_size.height) / scaled_size.height;

        cv::Rect face_scaled_rect(
            cvRound(x * sx),
            cvRound(y * sy),
            cvRound(width * sx),
            cvRound(height * sy)
        );

        cv::rectangle(frame,
                      face_scaled_rect,
                      cv::Scalar(0, 255, 0), 2);

        cv::putText(frame,
                    best_match_name,
                    cv::Point2d(static_cast<int>(face_scaled_rect.x), static_cast<int>(face_scaled_rect.y - 2)),
                    cv::FONT_HERSHEY_PLAIN,
                    4, {255, 0, 0}, 2);
    }

}