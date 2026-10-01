#include "fr_detection_metadata.hpp"

GType fr_detection_meta_api_get_type(void)
{
    static GType type = 0;
    static const gchar *tags[] = {"fr_detection_metadata", NULL};

    if (g_once_init_enter(&type)) 
    {
        GType _type = gst_meta_api_type_register("FRDetectionMetadata", tags);

        g_once_init_leave(&type, _type);
    }

    return type;
}

const GstMetaInfo * fr_detection_meta_get_info(void)
{
    static const GstMetaInfo *meta_info = NULL;

    if (g_once_init_enter(&meta_info)) 
    {
        const GstMetaInfo *mi =
        gst_meta_register(FR_DETECTION_META_API_TYPE, 
                          "FRDetectionMeta",
                          sizeof(FRDetectionMetadata),
                          fr_detection_meta_init,
                          fr_detection_meta_free,
                          fr_detection_meta_transform);

        g_once_init_leave(&meta_info, mi);
    }

    return meta_info;
}


gboolean fr_detection_meta_init(GstMeta *meta, gpointer params, GstBuffer *buffer)
{
    FRDetectionMetadata *m = (FRDetectionMetadata *)meta;

    m->detected_faces.clear();

    return TRUE;
}

void fr_detection_meta_free(GstMeta *meta, GstBuffer *buffer)
{
    FRDetectionMetadata *m = (FRDetectionMetadata *)meta;

    /* free dynamically allocated members here */
    m->detected_faces.clear();
}

gboolean fr_detection_meta_transform(GstBuffer *dest,
                                     GstMeta   *meta,
                                     GstBuffer *src,
                                     GQuark     type,
                                     gpointer   data)
{
    FRDetectionMetadata *src_meta = (FRDetectionMetadata *)meta;

    FRDetectionMetadata *dst_meta = (FRDetectionMetadata *)gst_buffer_add_meta(
        dest,
        FR_DETECTION_META_INFO,
        NULL
    );

    dst_meta->detected_faces = src_meta->detected_faces;

    return TRUE;
}