#include <gst/gst.h>
#include <iostream>


int main(int argc, char *argv[]) {
    GstElement *pipeline, *source, *sink;
    GstElement *filter;
    GstBus *bus;
    GstMessage *msg;
    GstStateChangeReturn ret;

    gst_init(&argc, &argv);

    source = gst_element_factory_make("videotestsrc", "source");
    sink = gst_element_factory_make("autovideosink", "sink");
    filter = gst_element_factory_make("vertigotv", "filter");

    pipeline = gst_pipeline_new("test-pipeline");

    if (!pipeline || !source || !sink || !filter) {
        g_printerr("Not all elements could be created.\n");
        return -1;
    }

    gst_bin_add_many(GST_BIN(pipeline), source, sink, filter, NULL);

    /*
    if (gst_element_link(source, sink) != true) {
        g_printerr("Elements could not be linked.\n");
        gst_object_unref(pipeline);
        return -1;
    }
    */

    if (gst_element_link(source, filter) != true) {
        g_printerr("Elements(source, filter) could not be linked.\n");
        gst_object_unref(pipeline);
        return -1;
    }

    if (gst_element_link(filter, sink) != true) {
        g_printerr("Elements(filter, sink) could not be linked.\n");
        gst_object_unref(pipeline);
        return -1;
    }

    g_object_set(source, "pattern", 0, NULL);

    ret = gst_element_set_state(pipeline, GST_STATE_PLAYING);
    if (ret == GST_STATE_CHANGE_FAILURE) {
        g_printerr("Unable to set the pipeline to the playing state.\n");
        gst_object_unref(pipeline);
        return -1;
    }

    bus = gst_element_get_bus(pipeline);
    msg = gst_bus_timed_pop_filtered(
        bus, GST_CLOCK_TIME_NONE,
        (GstMessageType)(GST_MESSAGE_ERROR | GST_MESSAGE_EOS)
    );

    if (msg != NULL) {
        GError *err;
        gchar *debug_info;

        switch (GST_MESSAGE_TYPE (msg)) {
            case GST_MESSAGE_ERROR:
                gst_message_parse_error(msg, &err, &debug_info);
                g_printerr("Error received from element %s: %s\n",
                    GST_OBJECT_NAME (msg->src), err->message);
                g_printerr("Debugging information: %s\n",
                    debug_info ? debug_info : "none");
                g_clear_error(&err);
                g_free(debug_info);
                break;

            case GST_MESSAGE_EOS:
                g_print("End-Of-Stream reached.\n");
                break;

            default:
                g_printerr("Unexpected message received.\n");
                break;
        }
        gst_message_unref(msg);
    }

    gst_object_unref(bus);
    gst_element_set_state(pipeline, GST_STATE_NULL);
    gst_object_unref(pipeline);
    return 0;
}


/*
int main(int argc, char *argv[]) {
    GstElement *pipeline;
    GstBus *bus;
    GstMessage *msg;

    gst_init(&argc, &argv);

    pipeline = gst_parse_launch(
        "playbin uri=https://gstreamer.freedesktop.org/data/media/sintel_trailer-480p.webm",
        NULL
    );

    gst_element_set_state(pipeline, GST_STATE_PLAYING);

    bus = gst_element_get_bus(pipeline);
    msg = gst_bus_timed_pop_filtered(
        bus, GST_CLOCK_TIME_NONE, (GstMessageType)(GST_MESSAGE_ERROR | GST_MESSAGE_EOS)
    );

    if (GST_MESSAGE_TYPE (msg) == GST_MESSAGE_ERROR) {
        g_printerr(
            "An error occurred! Re-run with the GST_DEBUG=*:WARN "
            "environment variable set for more details.\n"
        );
    }

    gst_message_unref(msg);
    gst_object_unref(bus);
    gst_element_set_state(pipeline, GST_STATE_NULL);
    gst_object_unref(pipeline);

    return 0;
}
*/

/*
int main(int argc, char *argv[]) {
    gst_init(nullptr, nullptr);

    guint major, minor, micro, nano;
    gst_version(&major, &minor, &micro, &nano);

    std::cout << "Gstreamer version: " << major << "." << minor << "." << micro << "." << nano << std::endl;

    return 0;
}
*/