#pragma once

#include <glib-object.h>
#include <glib.h>
#include <gst/gst.h>

G_BEGIN_DECLS

#define GST_TYPE_BURST_QUEUE (gst_burst_queue_get_type())

G_DECLARE_FINAL_TYPE(
    GstBurstQueue,
    gst_burst_queue,
    GST,
    BURST_QUEUE,
    GstElement
);

G_END_DECLS

typedef struct _Config {
    /**
     * Number of buffers required before the first burst starts.
     */
    guint buffer_count;

    /**
    * FPS numerator.
    *
    * Example: 30/1 for 30 FPS.
    */
    gdouble fps;
} Config;


/* Copy function */
static Config *
my_config_copy(const Config *config)
{
    Config *copy = g_new(Config, 1);
    *copy = *config;
    return copy;
}


/* Free function */
static void
my_config_free(Config *config)
{
    g_free(config);
}


/* Convenient macro */
#define MY_TYPE_CONFIG (my_config_get_type())



/**
 * GstBurstQueue
 *
 * A prefill-and-burst queue for frame-rate matching around an expensive
 * processing element such as a frame-rate conversion / interpolation
 * component.
 *
 * Behavior:
 *
 *   1. Buffers incoming GstBuffers until `buffer-count` is reached.
 *
 *   2. Once the prefill threshold is reached, starts draining the internal
 *      queue immediately, with NO pacing between buffers.
 *
 *   3. gst_pad_push() may block while downstream (e.g. FR) is processing.
 *      The sink-side chain continues independently and can keep accumulating
 *      buffers in the internal queue.
 *
 *   4. When the internal queue becomes empty, the output task waits for the
 *      next input buffer.
 *
 *   5. Buffers are never dropped because the queue is full. The caller is
 *      responsible for selecting a sufficiently large buffer-count.
 *
 *   6. Timestamps are shifted by timestamp-offset to compensate for the
 *      initial prefill latency.
 *
 *
 * Typical pipeline:
 *
 *   source ! burstqueue ! frame-rate-converter ! queue ! sink
 *
 *
 * Example:
 *
 *   source FPS = 30
 *   buffer-count = 7
 *
 *   source:
 *
 *       A B C D E F G H I J ...
 *
 *   initial queue:
 *
 *       [A B C D E F G]
 *
 *   burst:
 *
 *       A -> FR
 *       B -> FR
 *       C -> FR
 *       D -> FR
 *       E -> FR
 *       F -> FR
 *       G -> FR
 *
 *   While FR is processing these buffers, the input side continues:
 *
 *       [H I J K ...]
 *
 *   No frame pacing occurs inside this element.
 */
struct _GstBurstQueue
{
    GstElement parent;

    /* ---------------------------------------------------------------------
     * Pads
     * ------------------------------------------------------------------ */

    GstPad *sinkpad;
    GstPad *srcpad_fr;
    GstPad *srcpad_fr_bypass;

    /* ---------------------------------------------------------------------
     * Configuration
     * ------------------------------------------------------------------ */

    Config config;

    /* ---------------------------------------------------------------------
     * Timestamp configuration
     * ------------------------------------------------------------------ */

    /**
     * Duration of one source frame.
     *
     * Calculated as:
     *
     *   GST_SECOND * fps_d / fps_n
     */
    GstClockTime frame_duration;

    /**
     * Timestamp offset introduced by the initial prefill.
     *
     * Typically:
     *
     *   (buffer_count - 1) * frame_duration
     *
     * if the first buffer is pushed immediately when the Nth buffer arrives.
     */
    GstClockTime timestamp_offset;

    /**
     * Last PTS assigned to an output buffer.
     */
    GstClockTime last_output_pts;

    /* ---------------------------------------------------------------------
     * Internal buffer queue
     * ------------------------------------------------------------------ */

    /**
     * Buffers waiting to be pushed downstream.
     *
     * Ownership of GstBuffer references is held by this queue until the
     * buffer is popped and pushed downstream.
     */
    GQueue *wait_queue;
    GQueue *push_queue;


    /* ---------------------------------------------------------------------
     * Synchronization
     * ------------------------------------------------------------------ */

    /**
     * Protects:
     *
     *   queue
     *   flushing
     *   eos
     *   started
     *   timestamp_offset
     *   last_output_pts
     */
    GMutex lock;

    /**
     * Signals:
     *
     *   - a buffer has arrived
     *   - prefill threshold has been reached
     *   - flush state has changed
     *   - EOS has arrived
     */
    GCond cond_buf_pushed;

    /**
     * Signals:
     * 
     *  - buffer batch transfered: stage 1 ==> stage 2 queue
     */
    GCond cond_buf_transfered;


    /* ---------------------------------------------------------------------
     * Output task
     * ------------------------------------------------------------------ */

    /**
     * Dedicated task responsible for draining the queue.
     *
     * There is deliberately NO timer/clock pacing in this task.
     *
     * It repeatedly:
     *
     *   pop buffer
     *   adjust timestamp
     *   gst_pad_push()
     *
     * as quickly as downstream allows.
     */
    GstTask *task;

    /**
     * Lock used by GstTask.
     */
    GRecMutex task_lock;


    /* ---------------------------------------------------------------------
     * Runtime state
     * ------------------------------------------------------------------ */
    /**
     * TRUE after receiving EOS.
     *
     * EOS is forwarded only after all queued buffers have been drained.
     */
    gboolean eos;

    /**
     * TRUE once the initial buffer_count prefill has completed.
     *
     * Before this becomes TRUE, the output task waits until:
     *
     *   queue length >= buffer_count
     *
     * After this becomes TRUE, it drains whenever at least one buffer
     * is available.
     */
    gboolean started;


    /* ---------------------------------------------------------------------
     * Segment / timestamp state
     * ------------------------------------------------------------------ */

    /**
     * Current upstream segment.
     *
     * Kept so that timestamp handling can eventually be made fully
     * segment-aware.
     */
    GstSegment segment;

    /**
     * Whether `segment` contains a valid received segment.
     */
    gboolean have_segment;
};


/* -------------------------------------------------------------------------
 * Element API
 * ------------------------------------------------------------------------- */

/**
 * Recalculate frame_duration and timestamp_offset from the current
 * configuration.
 *
 * Normally called after changing:
 *
 *   buffer_count
 *   fps_n
 *   fps_d
 */
static void gst_burst_queue_recalculate(GstBurstQueue *self);

/**
 * Adjust the PTS/DTS of a buffer by timestamp_offset.
 *
 * The buffer remains owned by the caller.
 */
static void gst_burst_queue_adjust_timestamp(
    GstBurstQueue *self,
    GstBuffer    *buffer);


/**
 * Return the number of currently buffered frames.
 *
 * This is primarily useful for debugging / instrumentation.
 */
static guint gst_burst_queue_get_level(
    GstBurstQueue *self);


/**
 * Clear all currently buffered frames.
 *
 * Takes ownership of no additional references.
 */
static void gst_burst_queue_clear(
    GstBurstQueue *self);
