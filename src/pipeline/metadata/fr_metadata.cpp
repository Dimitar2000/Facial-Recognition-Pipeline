#include "fr_metadata.hpp"

#include <new>

GType fr_metadata_api_get_type(void)
{
    static GType type = 0;
    static const gchar *tags[] = {"fr_metadata", NULL};

    if (g_once_init_enter(&type)) 
    {
        GType _type = gst_meta_api_type_register("FRMetadataAPI", tags);

        g_once_init_leave(&type, _type);
    }

    return type;
}

const GstMetaInfo *fr_metadata_get_info(void)
{
    static const GstMetaInfo *meta_info = NULL;

    if (g_once_init_enter(&meta_info)) 
    {
        const GstMetaInfo *mi =
            gst_meta_register(FR_METADATA_API_TYPE,
                              "FRMetadata",
                              sizeof(FRMetadata),
                              fr_metadata_init,
                              fr_metadata_free,
                              fr_metadata_transform);

        g_once_init_leave(&meta_info, mi);
    }

    return meta_info;
}

gboolean fr_metadata_init(GstMeta *meta, gpointer params, GstBuffer *buffer)
{
    FRMetadata *m = reinterpret_cast<FRMetadata *>(meta);
    new (&m->detected_faces) std::vector<FRProcessor::DetectedFace>();

    return TRUE;
}

void fr_metadata_free(GstMeta *meta, GstBuffer *buffer)
{
    FRMetadata *m = reinterpret_cast<FRMetadata *>(meta);

    m->detected_faces.~vector<FRProcessor::DetectedFace>();
}

gboolean fr_metadata_transform(GstBuffer *dest,
                              GstMeta   *meta,
                              GstBuffer *src,
                              GQuark     type,
                              gpointer   data)
{
    FRMetadata *src_meta = reinterpret_cast<FRMetadata *>(meta);

    FRMetadata *dst_meta = reinterpret_cast<FRMetadata *>(
        gst_buffer_add_meta(dest, FR_METADATA_INFO, NULL));

    dst_meta->detected_faces = src_meta->detected_faces;
    dst_meta->frame_width = src_meta->frame_width;
    dst_meta->frame_heigth = src_meta->frame_heigth;

    return TRUE;
}