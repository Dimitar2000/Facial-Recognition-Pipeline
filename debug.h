#include "glib-object.h"
#include "glib.h"
#include "gst/gstcaps.h"
#include "gst/gstelementfactory.h"

/* Functions below print the Capabilities in a human-friendly format */
gboolean print_field (GQuark field, const GValue * value, gpointer pfx);

void print_caps (const GstCaps * caps, const gchar * pfx);

/* Prints information about a Pad Template, including its Capabilities */
void print_pad_templates_information (GstElementFactory * factory);

/* Shows the CURRENT capabilities of the requested pad in the given element */
void print_pad_capabilities (GstElement * element, gchar * pad_name);