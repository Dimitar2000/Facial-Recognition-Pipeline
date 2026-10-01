#pragma once 

#include <gst/gstmeta.h>
#include <opencv2/core/types.hpp>
#include <optional>
#include <string>

typedef struct Identity {
    std::string name;
    int reference_matches;
    int reference_images;
    double min_similarity;
    double max_similarity;
} Identity;

typedef struct DetectedFace {
    cv::Size frame_size;
    cv::Rect face_rect;
    
    double min_similarity;
    double max_similarity;
    std::optional<Identity> identity;
} DetectedFace;

typedef struct FRDetectionMetadata {
    GstMeta meta;

    std::vector<DetectedFace> detected_faces;
} FRDetectionMetadata;

GType fr_detection_meta_api_get_type(void);
const GstMetaInfo *fr_detection_meta_get_info(void);

#define FR_DETECTION_META_API_TYPE (fr_detection_meta_api_get_type())
#define FR_DETECTION_META_INFO (fr_detection_meta_get_info())

gboolean fr_detection_meta_init(GstMeta *meta, gpointer params, GstBuffer *buffer);

void fr_detection_meta_free(GstMeta *meta, GstBuffer *buffer);

gboolean fr_detection_meta_transform(GstBuffer *dest,
                                     GstMeta   *meta,
                                     GstBuffer *src,
                                     GQuark     type,
                                     gpointer   data);