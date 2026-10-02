#pragma once 

#include <gst/gstmeta.h>
#include <vector>

#include "fr/fr_processor.hpp"

typedef struct FRDetectionMetadata {
    GstMeta meta;

    std::vector<FRProcessor::DetectedFace> detected_faces;
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