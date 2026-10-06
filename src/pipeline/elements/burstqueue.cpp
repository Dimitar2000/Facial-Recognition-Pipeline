/*
 *
 * gstburstqueue.c
 *
 * A bounded/prefilled, clock-paced frame queue.
 *
 * Behavior:
 *
 *   input:
 *       A B C D E F G H ...
 *
 *   prefill = 4
 *
 *   output:
 *             ........ABCDEG.......ABCDEG
 *                 
 *
 *
 * All buffers are preserved. Burst draining
 *
 * The initial buffering delay is compensated by shifting PTS/DTS by BUFFER.
 */

#include <glib-object.h>
#include <glib.h>
#include <glibconfig.h>
#include <gst/gst.h>
#include <gst/base/gstbasetransform.h>
#include <gst/gstbuffer.h>
#include <gst/gstpad.h>
#include <gst/gstpadtemplate.h>
#include <iostream>

#include "burstqueue.hpp"

#define GST_TYPE_BURST_QUEUE (gst_burst_queue_get_type())

G_DEFINE_TYPE(GstBurstQueue, gst_burst_queue, GST_TYPE_ELEMENT)


/* -------------------------------------------------------------------------
 * Properties
 * ------------------------------------------------------------------------- */

enum
{
    PROP_0,
    PROP_CONFIG
};

/*
 * Registers MyConfig as a GBoxed type.
 *
 * This creates:
 *
 *     my_config_get_type()
 *
 * which returns the GType.
 */
G_DEFINE_BOXED_TYPE(
    Config,
    my_config,
    my_config_copy,
    my_config_free
);

static void
gst_burst_queue_set_property(GObject      *object,
                             guint         prop_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
    GstBurstQueue *self = GST_BURST_QUEUE(object);

    GST_OBJECT_LOCK(self);

    g_mutex_lock(&self->lock);

    switch (prop_id) {
    case PROP_CONFIG:
        self->config = *(Config *) g_value_get_boxed(value);
        std::cout << "[burstqueue] setting buffer count " << self->config.buffer_count << std::endl;
        std::cout << "[burstqueue] setting fps " << self->config.fps << std::endl;
        gst_burst_queue_recalculate(self);
        break;

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }

    g_mutex_unlock(&self->lock);

    GST_OBJECT_UNLOCK(self);
}


static void
gst_burst_queue_get_property(GObject    *object,
                             guint       prop_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
    GstBurstQueue *self = GST_BURST_QUEUE(object);

    GST_OBJECT_LOCK(self);

    switch (prop_id) {
    case PROP_CONFIG:
        g_value_set_boxed(value, (gconstpointer *)&self->config);
        break;

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }

    GST_OBJECT_UNLOCK(self);
}


/* -------------------------------------------------------------------------
 * Helpers
 * ------------------------------------------------------------------------- */

static void
gst_burst_queue_recalculate(GstBurstQueue *self)
{
    if (self->config.fps == 0) {
        self->frame_duration = GST_CLOCK_TIME_NONE;
        return;
    }

    self->frame_duration = GST_SECOND / self->config.fps;

    /*
     * Delay introduced by prefill.
     *
     * If the first frame is released as soon as the Nth frame arrives,
     * the elapsed time between frame 0 and frame N-1 is:
     *
     *     (N - 1) * frame_duration
     *
     * If you deliberately wait one more frame period before releasing
     * the first frame, change this to buffer_count * frame_duration.
     */
    if (self->config.buffer_count > 0) {
        self->timestamp_offset = (self->config.buffer_count - 1) * self->frame_duration;
    } else {
        self->timestamp_offset = 0;
    }

    std::cout << "[burstqueue] Flushing queue ..." << std::endl;
    g_queue_clear(self->queue);
    self->restart = TRUE;
}


/* -------------------------------------------------------------------------
 * Buffer timestamp handling
 * ------------------------------------------------------------------------- */

static void
gst_burst_queue_adjust_timestamp(GstBurstQueue *self,
                                 GstBuffer    *buffer)
{
    GstClockTime pts = GST_BUFFER_PTS(buffer);

    GST_BUFFER_PTS(buffer) = MAX(pts, self->last_output_pts) + self->timestamp_offset;
    self->last_output_pts = GST_BUFFER_PTS(buffer);

}


/* -------------------------------------------------------------------------
 * Output task
 * ------------------------------------------------------------------------- */

static void
gst_burst_queue_output_task(gpointer user_data)
{
    GstBurstQueue *self = GST_BURST_QUEUE(user_data);

    bool error = false;

    while (!error) {
        GstBuffer *buffer;

        g_mutex_lock(&self->lock);

        /*
         * Wait for signal that new burst can be done
         */
        while(!self->restart 
              &&  ((self->eos && !g_queue_is_empty(self->queue)) 
                    || g_queue_get_length(self->queue) < self->config.buffer_count))
        {
            g_cond_wait(&self->cond, &self->lock);
        }

        int buffer_count = self->config.buffer_count;
        int pushed = 0;

        g_mutex_unlock(&self->lock);

        /*
         * Once we've reached N, we're in burst mode until N frames are pushed.
         */

        while(!self->restart
                && (self->eos && !g_queue_is_empty(self->queue)) 
                    || pushed < buffer_count)
        {
            g_mutex_lock(&self->lock);

            /*
            * Take exactly one buffer.
            *
            * There is intentionally+ NO clock wait here.
            */
            buffer = (GstBuffer *)g_queue_pop_head(self->queue);

            g_mutex_unlock(&self->lock);

            /*
            * This can block while FR processes the frame.
            *
            * That's intentional.
            *
            * The sink chain continues independently and can keep
            * filling self->queue while we're blocked here.
            */
            if (buffer != NULL)
            {
                g_mutex_lock(&self->lock);
                gst_burst_queue_adjust_timestamp(self, buffer);
                g_mutex_unlock(&self->lock);
                
                // Skip all but last buffer
                bool skip = pushed < buffer_count - 1;

                GstFlowReturn ret;
                
                if (skip)
                {
                    std::cout << "[burstqueue] Buffer " << pushed << " no FR : " << GST_BUFFER_PTS(buffer) << std::endl;
                    ret = gst_pad_push(self->srcpad_fr_bypass, buffer);
                }
                else
                {
                    std::cout << "[burstqueue] Buffer " << pushed << " -> FR : " << GST_BUFFER_PTS(buffer) << std::endl;
                    ret = gst_pad_push(self->srcpad_fr, buffer);
                }

                if (ret != GST_FLOW_OK) {
                    GST_ERROR_OBJECT(
                        self,
                        "Downstream returned %s",
                        gst_flow_get_name(ret));

                    error = true;
                    break;
                }
            }
            else
            {
                std::cout << "[burstqueue] NULL buffer " << pushed << std::endl;
                break;
            }

            /*
            * EOS + empty queue.
            */
            if (self->eos && g_queue_is_empty(self->queue)) {
                gst_pad_push_event(
                    self->srcpad_fr_bypass,
                    gst_event_new_eos());
                gst_pad_push_event(
                    self->srcpad_fr,
                    gst_event_new_eos());

                break;
            }

            /*
             * This burst was not initiated because of EOS.
             */
            pushed++;
        }

        g_mutex_lock(&self->lock);
        
        if (self->restart)
        {
            self->restart = FALSE;
            std::cout << "Restart completed" << std::endl;
        }

        g_mutex_unlock(&self->lock);
    }
}


/* -------------------------------------------------------------------------
 * Sink chain
 * ------------------------------------------------------------------------- */

static GstFlowReturn
gst_burst_queue_chain(GstPad    *pad,
                      GstObject *parent,
                      GstBuffer *buffer)
{
    GstBurstQueue *self = GST_BURST_QUEUE(parent);

    g_mutex_lock(&self->lock);
    
    while (g_queue_get_length(self->queue) >= 2 * self->config.buffer_count) {
        g_mutex_unlock(&self->lock);  
        // ...    
        g_mutex_lock(&self->lock);
    }

    std::cout << "[burstqueue] Buffer queued : " << GST_BUFFER_PTS(buffer) << std::endl;
    
    /*
     * Important:
     *
     * We NEVER drop here.
     *
     * Upstream can continue producing while the output task is
     * waiting for its next pacing point.
     */
    g_queue_push_tail(self->queue, buffer);

    g_cond_signal(&self->cond);

    g_mutex_unlock(&self->lock);

    return GST_FLOW_OK;
}


/* -------------------------------------------------------------------------
 * Events
 * ------------------------------------------------------------------------- */

static gboolean
gst_burst_queue_sink_event(GstPad    *pad,
                           GstObject *parent,
                           GstEvent  *event)
{
    GstBurstQueue *self = GST_BURST_QUEUE(parent);

    switch (GST_EVENT_TYPE(event)) {

    case GST_EVENT_EOS:

        g_mutex_lock(&self->lock);

        self->eos = TRUE;
        g_cond_broadcast(&self->cond);

        g_mutex_unlock(&self->lock);

        return TRUE;

    default:
    {
        GstEvent *bypass_event = gst_event_ref(event);
        gboolean fr_result = gst_pad_push_event(self->srcpad_fr, event);
        gboolean bypass_result = gst_pad_push_event(self->srcpad_fr_bypass,
                                                     bypass_event);
        return fr_result && bypass_result;
    }
    }
}


/* -------------------------------------------------------------------------
 * State handling
 * ------------------------------------------------------------------------- */

static GstStateChangeReturn
gst_burst_queue_change_state(GstElement    *element,
                             GstStateChange transition)
{
    GstBurstQueue *self = GST_BURST_QUEUE(element);

    switch (transition) {

    case GST_STATE_CHANGE_PAUSED_TO_READY:

        g_mutex_lock(&self->lock);

        g_cond_broadcast(&self->cond);

        while (!g_queue_is_empty(self->queue)) {
            GstBuffer *buffer = (GstBuffer *)g_queue_pop_head(self->queue);

            gst_buffer_unref(buffer);
        }

        self->started = FALSE;
        self->eos = FALSE;

        g_mutex_unlock(&self->lock);

        gst_task_stop(self->task);

        break;

    default:
        break;
    }

    return GST_ELEMENT_CLASS(
        gst_burst_queue_parent_class)->change_state(element,
                                                      transition);
}


/* -------------------------------------------------------------------------
 * Finalize
 * ------------------------------------------------------------------------- */

static void
gst_burst_queue_finalize(GObject *object)
{
    GstBurstQueue *self = GST_BURST_QUEUE(object);

    gst_task_stop(self->task);

    g_mutex_lock(&self->lock);

    while (!g_queue_is_empty(self->queue)) {
        GstBuffer *buffer = (GstBuffer *)
            g_queue_pop_head(self->queue);

        gst_buffer_unref(buffer);
    }

    g_mutex_unlock(&self->lock);

    gst_object_unref(self->task);

    g_queue_free(self->queue);

    g_cond_clear(&self->cond);
    g_mutex_clear(&self->lock);
    g_rec_mutex_clear(&self->task_lock);

    G_OBJECT_CLASS(gst_burst_queue_parent_class)->finalize(object);
}


/* -------------------------------------------------------------------------
 * Class init
 * ------------------------------------------------------------------------- */

static void
gst_burst_queue_class_init(GstBurstQueueClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    GstElementClass *element_class = GST_ELEMENT_CLASS(klass);

    object_class->set_property =
        gst_burst_queue_set_property;

    object_class->get_property =
        gst_burst_queue_get_property;

    object_class->finalize =
        gst_burst_queue_finalize;

    element_class->change_state =
        gst_burst_queue_change_state;

    g_object_class_install_property(
        object_class,
        PROP_CONFIG,
        g_param_spec_boxed(
            "config",
            "Config",
            "Element configuration",
            MY_TYPE_CONFIG,
            static_cast<GParamFlags>(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

    gst_element_class_set_static_metadata(
        element_class,
        "Burst Queue",
        "Generic",
        "Prefill and clock-paced buffer queue with dual source output",
        "Example");

    GstCaps *caps = gst_caps_from_string(
        "video/x-raw,"
        "format=(string)RGB,"
        "width=(int)[1,MAX],"
        "height=(int)[1,MAX],"
        "framerate=(fraction)[0/1,MAX]");

    // Standard sink pad template
    gst_element_class_add_pad_template(
        element_class,
        gst_pad_template_new(
            "sink",
            GST_PAD_SINK,
            GST_PAD_ALWAYS,
            caps));

    gst_element_class_add_pad_template(
        element_class,
        gst_pad_template_new(
            "src_fr",
            GST_PAD_SRC,
            GST_PAD_ALWAYS,
            caps));

    gst_element_class_add_pad_template(
        element_class,
        gst_pad_template_new(
            "src_fr_bypass",
            GST_PAD_SRC,
            GST_PAD_ALWAYS,
            caps));

    gst_caps_unref(caps);
}


/* -------------------------------------------------------------------------
 * Instance init
 * ------------------------------------------------------------------------- */

static void
gst_burst_queue_init(GstBurstQueue *self)
{
    self->config.buffer_count = 1;
    self->config.fps = 20.0f;

    self->queue = g_queue_new();

    g_mutex_init(&self->lock);
    g_cond_init(&self->cond);
    g_rec_mutex_init(&self->task_lock);

    gst_segment_init(&self->segment, GST_FORMAT_TIME);

    self->restart = FALSE;
    self->eos = FALSE;
    self->started = FALSE;
    self->have_segment = FALSE;
    self->last_output_pts = 0;

    gst_burst_queue_recalculate(self);

    /*
     * Sink Pad setup.
     */
    self->sinkpad =
        gst_pad_new_from_template(
            gst_element_class_get_pad_template(
                GST_ELEMENT_GET_CLASS(self),
                "sink"),
            "sink");

    gst_pad_set_chain_function(
        self->sinkpad,
        GST_DEBUG_FUNCPTR(gst_burst_queue_chain));

    gst_pad_set_event_function(
        self->sinkpad,
        GST_DEBUG_FUNCPTR(gst_burst_queue_sink_event));

    gst_element_add_pad(GST_ELEMENT(self), self->sinkpad);

    /*
     * Source Pads setup (static dual pads).
     */
    self->srcpad_fr =
        gst_pad_new_from_template(
            gst_element_class_get_pad_template(
                GST_ELEMENT_GET_CLASS(self),
                "src_fr"),
            "src_fr");

    self->srcpad_fr_bypass =
        gst_pad_new_from_template(
            gst_element_class_get_pad_template(
                GST_ELEMENT_GET_CLASS(self),
                "src_fr_bypass"),
            "src_fr_bypass");

    gst_element_add_pad(GST_ELEMENT(self), self->srcpad_fr);
    gst_element_add_pad(GST_ELEMENT(self), self->srcpad_fr_bypass);

    /*
     * Output task initialization.
     */
    self->task =
        gst_task_new(gst_burst_queue_output_task,
                     self,
                     NULL);

    gst_task_set_lock(self->task,
                      &self->task_lock);

    gst_task_start(self->task);
}