#pragma once 

#include <gst/gstmeta.h>
#include <vector>

#include "fr/fr_processor.hpp"

typedef struct FRMetadata {
    GstMeta meta;

    int frame_width;
    int frame_heigth;
    std::vector<FRProcessor::DetectedFace> detected_faces;
} FRMetadata;

GType fr_metadata_api_get_type(void);
const GstMetaInfo *fr_metadata_get_info(void);

#define FR_METADATA_API_TYPE (fr_metadata_api_get_type())
#define FR_METADATA_INFO (fr_metadata_get_info())

gboolean fr_metadata_init(GstMeta *meta, gpointer params, GstBuffer *buffer);

void fr_metadata_free(GstMeta *meta, GstBuffer *buffer);

gboolean fr_metadata_transform(GstBuffer *dest,
                               GstMeta   *meta,
                               GstBuffer *src,
                               GQuark     type,
                               gpointer   data);