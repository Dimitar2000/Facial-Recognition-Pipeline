#pragma once

#include <cmath>
#include <gst/gstclock.h>
#include <stdexcept>

// Calculator of static configuration for pipeline elements
// Currently it computes:
//  - Number of post-FR queue slots 
class ConfigCalculator
{
public:
    // Compute required post-FR queue slots <S> for a stable desired FPS.
    // Assumption is:
    //      FR will skip frames (1, S-1) and process onlt frame S.
    // The amortized latency of FR should be <= maximum needed to guarantee desired pipeline FPS
    guint compute_queue_slots(double       desired_fps,
                              GstClockTime base_latency_pl_ns,
                              GstClockTime queue_latency_ns,
                              GstClockTime max_latency_fr_ns)
    {
        // Frame period in nanoseconds.
        const double frame_period_ns = GST_SECOND / desired_fps;

        // Latency available for amortizing FR processing.
        const double budget_fr = frame_period_ns - base_latency_pl_ns - queue_latency_ns;

        if (budget_fr <= 0.0)
        {
            throw std::runtime_error("Cannot achieve desired FPS. Budget is not enough.");
        }

        const double slots = max_latency_fr_ns / (budget_fr);

        return static_cast<guint>(std::ceil(slots));
    }
};