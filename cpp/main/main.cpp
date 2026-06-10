#include <string.h>
#include <gst/gst.h>
#include <gst/pbutils/pbutils.h>

typedef struct _CustomData
{
    GstDiscoverer *discoverer;
    GMainLoop *loop;
} CustomData;

static void print_tag_foreach(const GstTagList *tags, const gchar *tag, gpointer user_data)
{
    GValue val = {
        0,
    };
    gchar *str;
    gint depth = GPOINTER_TO_INT(user_data);

    gst_tag_list_copy_value(&val, tags, tag);

    if (G_VALUE_HOLDS_STRING(&val))
        str = g_value_dup_string(&val);
    else
        str = gst_value_serialize(&val);

    g_print("%*s%s: %s\n", 2 * depth, " ", gst_tag_get_nick(tag), str);
    g_free(str);

    g_value_unset(&val);
}

static void print_stream_info(GstDiscovererStreamInfo *info, gint depth)
{
    gchar *desc = NULL;
    GstCaps *caps;
    const GstTagList *tags;

    caps = gst_discoverer_stream_info_get_caps(info);

    if (caps)
    {
        if (gst_caps_is_fixed(caps))
            desc = gst_pb_utils_get_codec_description(caps);
        else
            desc = gst_caps_to_string(caps);
        gst_caps_unref(caps);
    }

    g_print("%*s%s: %s\n", 2 * depth, " ", gst_discoverer_stream_info_get_stream_type_nick(info), (desc ? desc : ""));

    if (desc)
    {
        g_free(desc);
        desc = NULL;
    }

    tags = gst_discoverer_stream_info_get_tags(info);
    if (tags)
    {
        g_print("%*sTags:\n", 2 * (depth + 1), " ");
        gst_tag_list_foreach(tags, print_tag_foreach, GINT_TO_POINTER(depth + 2));
    }
}

static void print_topology(GstDiscovererStreamInfo *info, gint depth)
{
    GstDiscovererStreamInfo *next;

    if (!info)
        return;

    print_stream_info(info, depth);

    next = gst_discoverer_stream_info_get_next(info);
    if (next)
    {
        print_topology(next, depth + 1);
        gst_discoverer_stream_info_unref(next);
    }
    else if (GST_IS_DISCOVERER_CONTAINER_INFO(info))
    {
        GList *tmp, *streams;

        streams = gst_discoverer_container_info_get_streams(GST_DISCOVERER_CONTAINER_INFO(info));
        for (tmp = streams; tmp; tmp = tmp->next)
        {
            GstDiscovererStreamInfo *tmpinf = (GstDiscovererStreamInfo *)tmp->data;
            print_topology(tmpinf, depth + 1);
        }
        gst_discoverer_stream_info_list_free(streams);
    }
}

static void on_discovered_cb(GstDiscoverer *discoverer, GstDiscovererInfo *info, GError *err, CustomData *data)
{
    GstDiscovererResult result;
    const gchar *uri;
    const GstTagList *tags;
    GstDiscovererStreamInfo *sinfo;

    uri = gst_discoverer_info_get_uri(info);
    result = gst_discoverer_info_get_result(info);
    switch (result)
    {
    case GST_DISCOVERER_URI_INVALID:
        g_print("Invalid URI '%s'\n", uri);
        break;
    case GST_DISCOVERER_ERROR:
        g_print("Discoverer error: %s\n", err->message);
        break;
    case GST_DISCOVERER_TIMEOUT:
        g_print("Timeout\n");
        break;
    case GST_DISCOVERER_BUSY:
        g_print("Busy\n");
        break;
    case GST_DISCOVERER_MISSING_PLUGINS:
    {
        const GstStructure *s;
        gchar *str;

        s = gst_discoverer_info_get_misc(info);
        str = gst_structure_to_string(s);

        g_print("Missing plugins: %s\n", str);
        g_free(str);
        break;
    }
    case GST_DISCOVERER_OK:
        g_print("Discovered '%s'\n", uri);
        break;
    }

    if (result != GST_DISCOVERER_OK)
    {
        g_printerr("This URI cannot be played\n");
        return;
    }

    g_print("\nDuration: %" GST_TIME_FORMAT "\n", GST_TIME_ARGS(gst_discoverer_info_get_duration(info)));

    tags = gst_discoverer_info_get_tags(info);
    if (tags)
    {
        g_print("Tags:\n");
        gst_tag_list_foreach(tags, print_tag_foreach, GINT_TO_POINTER(1));
    }

    g_print("Seekable: %s\n", (gst_discoverer_info_get_seekable(info) ? "yes" : "no"));

    g_print("\n");

    sinfo = gst_discoverer_info_get_stream_info(info);
    if (!sinfo)
        return;

    g_print("Stream information:\n");

    print_topology(sinfo, 1);

    gst_discoverer_stream_info_unref(sinfo);

    g_print("\n");
}

static void on_finished_cb(GstDiscoverer *discoverer, CustomData *data)
{
    g_print("Finished discovering\n");

    g_main_loop_quit(data->loop);
}

int main(int argc, char **argv)
{
    CustomData data;
    GError *err = NULL;
    gchar *uri = (gchar*)"https://gstreamer.freedesktop.org/data/media/sintel_trailer-480p.webm";

    if (argc > 1)
    {
        uri = argv[1];
    }

    memset(&data, 0, sizeof(data));

    gst_init(&argc, &argv);

    g_print("Discovering '%s'\n", uri);

    data.discoverer = gst_discoverer_new(5 * GST_SECOND, &err);
    if (!data.discoverer)
    {
        g_print("Error creating discoverer instance: %s\n", err->message);
        g_clear_error(&err);
        return -1;
    }

    g_signal_connect(data.discoverer, "discovered", G_CALLBACK(on_discovered_cb), &data);
    g_signal_connect(data.discoverer, "finished", G_CALLBACK(on_finished_cb), &data);

    gst_discoverer_start(data.discoverer);

    if (!gst_discoverer_discover_uri_async(data.discoverer, uri))
    {
        g_print("Failed to start discovering URI '%s'\n", uri);
        g_object_unref(data.discoverer);
        return -1;
    }

    data.loop = g_main_loop_new(NULL, FALSE);
    g_main_loop_run(data.loop);

    gst_discoverer_stop(data.discoverer);

    g_object_unref(data.discoverer);
    g_main_loop_unref(data.loop);

    return 0;
}

/*
#include <gst/gst.h>
#include <gst/audio/audio.h>
#include <string.h>

#define CHUNK_SIZE 1024
#define SAMPLE_RATE 44100

typedef struct _CustomData
{
    GstElement *pipeline, *app_source, *tee, *audio_queue, *audio_convert1, *audio_resample, *audio_sink;
    GstElement *video_queue, *audio_convert2, *visual, *video_convert, *video_sink;
    GstElement *app_queue, *app_sink;

    guint64 num_samples;
    gfloat a, b, c, d;

    guint sourceid;

    GMainLoop *main_loop;
} CustomData;

static gboolean push_data(CustomData *data)
{
    GstBuffer *buffer;
    GstFlowReturn ret;
    int i;
    GstMapInfo map;
    gint16 *raw;
    gint num_samples = CHUNK_SIZE / 2;
    gfloat freq;

    buffer = gst_buffer_new_and_alloc(CHUNK_SIZE);

    GST_BUFFER_TIMESTAMP(buffer) = gst_util_uint64_scale(data->num_samples, GST_SECOND, SAMPLE_RATE);
    GST_BUFFER_DURATION(buffer) = gst_util_uint64_scale(num_samples, GST_SECOND, SAMPLE_RATE);

    gst_buffer_map(buffer, &map, GST_MAP_WRITE);
    raw = (gint16 *)map.data;
    data->c += data->d;
    data->d -= data->c / 1000;
    freq = 1100 + 1000 * data->d;
    for (i = 0; i < num_samples; i++)
    {
        data->a += data->b;
        data->b -= data->a / freq;
        raw[i] = (gint16)(500 * data->a);
    }
    gst_buffer_unmap(buffer, &map);
    data->num_samples += num_samples;

    g_signal_emit_by_name(data->app_source, "push-buffer", buffer, &ret);

    gst_buffer_unref(buffer);

    if (ret != GST_FLOW_OK)
    {
        return FALSE;
    }

    return TRUE;
}

static void start_feed(GstElement *source, guint size, CustomData *data)
{
    if (data->sourceid == 0)
    {
        g_print("Start feeding\n");
        data->sourceid = g_idle_add((GSourceFunc)push_data, data);
    }
}

static void stop_feed(GstElement *source, CustomData *data)
{
    if (data->sourceid != 0)
    {
        g_print("Stop feeding\n");
        g_source_remove(data->sourceid);
        data->sourceid = 0;
    }
}

static GstFlowReturn new_sample(GstElement *sink, CustomData *data)
{
    GstSample *sample;

    g_signal_emit_by_name(sink, "pull-sample", &sample);
    if (sample)
    {
        g_print("*");
        gst_sample_unref(sample);
        return GST_FLOW_OK;
    }

    return GST_FLOW_FLUSHING;
}

static void error_cb(GstBus *bus, GstMessage *msg, CustomData *data)
{
    GError *err;
    gchar *debug_info;

    gst_message_parse_error(msg, &err, &debug_info);
    g_printerr("Error received from element %s: %s\n", GST_OBJECT_NAME(msg->src), err->message);
    g_printerr("Debugging information: %s\n", debug_info ? debug_info : "none");
    g_clear_error(&err);
    g_free(debug_info);

    g_main_loop_quit(data->main_loop);
}

int main(int argc, char *argv[])
{
    CustomData data;
    GstPad *tee_audio_pad, *tee_video_pad, *tee_app_pad;
    GstPad *queue_audio_pad, *queue_video_pad, *queue_app_pad;
    GstAudioInfo info;
    GstCaps *audio_caps;
    GstBus *bus;

    memset(&data, 0, sizeof(data));
    data.b = 1;
    data.d = 1;

    gst_init(&argc, &argv);

    data.app_source = gst_element_factory_make("appsrc", "audio_source");
    data.tee = gst_element_factory_make("tee", "tee");
    data.audio_queue = gst_element_factory_make("queue", "audio_queue");
    data.audio_convert1 = gst_element_factory_make("audioconvert", "audio_convert1");
    data.audio_resample = gst_element_factory_make("audioresample", "audio_resample");
    data.audio_sink = gst_element_factory_make("autoaudiosink", "audio_sink");
    data.video_queue = gst_element_factory_make("queue", "video_queue");
    data.audio_convert2 = gst_element_factory_make("audioconvert", "audio_convert2");
    data.visual = gst_element_factory_make("wavescope", "visual");
    data.video_convert = gst_element_factory_make("videoconvert", "video_convert");
    data.video_sink = gst_element_factory_make("autovideosink", "video_sink");
    data.app_queue = gst_element_factory_make("queue", "app_queue");
    data.app_sink = gst_element_factory_make("appsink", "app_sink");

    data.pipeline = gst_pipeline_new("test-pipeline");

    if (!data.pipeline || !data.app_source || !data.tee || !data.audio_queue || !data.audio_convert1 ||
        !data.audio_resample || !data.audio_sink || !data.video_queue || !data.audio_convert2 || !data.visual ||
        !data.video_convert || !data.video_sink || !data.app_queue || !data.app_sink)
    {
        g_printerr("Not all elements could be created.\n");
        return -1;
    }

    g_object_set(data.visual, "shader", 0, "style", 0, NULL);

    gst_audio_info_set_format(&info, GST_AUDIO_FORMAT_S16, SAMPLE_RATE, 1, NULL);
    audio_caps = gst_audio_info_to_caps(&info);
    g_object_set(data.app_source, "caps", audio_caps, "format", GST_FORMAT_TIME, NULL);
    g_signal_connect(data.app_source, "need-data", G_CALLBACK(start_feed), &data);
    g_signal_connect(data.app_source, "enough-data", G_CALLBACK(stop_feed), &data);

    g_object_set(data.app_sink, "emit-signals", TRUE, "caps", audio_caps, NULL);
    g_signal_connect(data.app_sink, "new-sample", G_CALLBACK(new_sample), &data);
    gst_caps_unref(audio_caps);

    gst_bin_add_many(GST_BIN(data.pipeline), data.app_source, data.tee, data.audio_queue, data.audio_convert1, data.audio_resample,
                     data.audio_sink, data.video_queue, data.audio_convert2, data.visual, data.video_convert, data.video_sink, data.app_queue,
                     data.app_sink, NULL);

    if (gst_element_link_many(data.app_source, data.tee, NULL) != TRUE ||
        gst_element_link_many(data.audio_queue, data.audio_convert1, data.audio_resample, data.audio_sink, NULL) != TRUE ||
        gst_element_link_many(data.video_queue, data.audio_convert2, data.visual, data.video_convert, data.video_sink, NULL) != TRUE ||
        gst_element_link_many(data.app_queue, data.app_sink, NULL) != TRUE)
    {
        g_printerr("Elements could not be linked.\n");
        gst_object_unref(data.pipeline);
        return -1;
    }

    tee_audio_pad = gst_element_request_pad_simple(data.tee, "src_%u");
    g_print("Obtained request pad %s for audio branch.\n", gst_pad_get_name(tee_audio_pad));

    queue_audio_pad = gst_element_get_static_pad(data.audio_queue, "sink");

    tee_video_pad = gst_element_request_pad_simple(data.tee, "src_%u");
    g_print("Obtained request pad %s for video branch.\n", gst_pad_get_name(tee_video_pad));

    queue_video_pad = gst_element_get_static_pad(data.video_queue, "sink");

    tee_app_pad = gst_element_request_pad_simple(data.tee, "src_%u");
    g_print("Obtained request pad %s for app branch.\n", gst_pad_get_name(tee_app_pad));

    queue_app_pad = gst_element_get_static_pad(data.app_queue, "sink");

    if (gst_pad_link(tee_audio_pad, queue_audio_pad) != GST_PAD_LINK_OK ||
        gst_pad_link(tee_video_pad, queue_video_pad) != GST_PAD_LINK_OK ||
        gst_pad_link(tee_app_pad, queue_app_pad) != GST_PAD_LINK_OK)
    {
        g_printerr("Tee could not be linked\n");
        gst_object_unref(data.pipeline);
        return -1;
    }
    gst_object_unref(queue_audio_pad);
    gst_object_unref(queue_video_pad);
    gst_object_unref(queue_app_pad);

    bus = gst_element_get_bus(data.pipeline);
    gst_bus_add_signal_watch(bus);
    g_signal_connect(G_OBJECT(bus), "message::error", (GCallback)error_cb, &data);
    gst_object_unref(bus);

    gst_element_set_state(data.pipeline, GST_STATE_PLAYING);

    data.main_loop = g_main_loop_new(NULL, FALSE);
    g_main_loop_run(data.main_loop);

    gst_element_release_request_pad(data.tee, tee_audio_pad);
    gst_element_release_request_pad(data.tee, tee_video_pad);
    gst_element_release_request_pad(data.tee, tee_app_pad);
    gst_object_unref(tee_audio_pad);
    gst_object_unref(tee_video_pad);
    gst_object_unref(tee_app_pad);

    gst_element_set_state(data.pipeline, GST_STATE_NULL);
    gst_object_unref(data.pipeline);
    return 0;
}
*/

/*
#include <string.h>

#include <gtk/gtk.h>
#include <gst/gst.h>
#include <gdk/gdk.h>


int main(int argc, char *argv[])
{
    GstElement *pipeline, *audio_source, *tee, *audio_queue, *audio_convert, *audio_resample, *audio_sink;
    GstElement *video_queue, *visual, *video_convert, *video_sink;
    GstBus *bus;
    GstMessage *msg;
    GstPad *tee_audio_pad, *tee_video_pad;
    GstPad *queue_audio_pad, *queue_video_pad;

    gst_init(&argc, &argv);

    audio_source = gst_element_factory_make("audiotestsrc", "audio_source");
    tee = gst_element_factory_make("tee", "tee");
    audio_queue = gst_element_factory_make("queue", "audio_queue");
    audio_convert = gst_element_factory_make("audioconvert", "audio_convert");
    audio_resample = gst_element_factory_make("audioresample", "audio_resample");
    audio_sink = gst_element_factory_make("autoaudiosink", "audio_sink");

    video_queue = gst_element_factory_make("queue", "video_queue");
    visual = gst_element_factory_make("wavescope", "visual");
    video_convert = gst_element_factory_make("videoconvert", "csp");
    video_sink = gst_element_factory_make("autovideosink", "video_sink");

    pipeline = gst_pipeline_new("test-pipeline");

    if (!pipeline || !audio_source || !tee || !audio_queue || !audio_convert || !audio_resample || !audio_sink ||
        !video_queue || !visual || !video_convert || !video_sink)
    {
        g_printerr("Not all elements could be created.\n");
        return -1;
    }

    g_object_set(audio_source, "freq", 215.0f, NULL);
    g_object_set(visual, "shader", 0, "style", 1, NULL);

    gst_bin_add_many(GST_BIN(pipeline), audio_source, tee, audio_queue, audio_convert, audio_resample, audio_sink,
                     video_queue, visual, video_convert, video_sink, NULL);

    if (gst_element_link_many(audio_source, tee, NULL) != TRUE ||
        gst_element_link_many(audio_queue, audio_convert, audio_resample, audio_sink, NULL) != TRUE ||
        gst_element_link_many(video_queue, visual, video_convert, video_sink, NULL) != TRUE)
    {
        g_printerr("Elements could not be linked.\n");
        gst_object_unref(pipeline);
        return -1;
    }

    tee_audio_pad = gst_element_request_pad_simple(tee, "src_%u");
    g_print("Obtained request pad %s for audio branch.\n", gst_pad_get_name(tee_audio_pad));

    queue_audio_pad = gst_element_get_static_pad(audio_queue, "sink");

    tee_video_pad = gst_element_request_pad_simple(tee, "src_%u");
    g_print("Obtained request pad %s for video branch.\n", gst_pad_get_name(tee_video_pad));

    queue_video_pad = gst_element_get_static_pad(video_queue, "sink");

    if (gst_pad_link(tee_audio_pad, queue_audio_pad) != GST_PAD_LINK_OK ||
        gst_pad_link(tee_video_pad, queue_video_pad) != GST_PAD_LINK_OK)
    {
        g_printerr("Tee could not be linked.\n");
        gst_object_unref(pipeline);
        return -1;
    }
    gst_object_unref(queue_audio_pad);
    gst_object_unref(queue_video_pad);

    gst_element_set_state(pipeline, GST_STATE_PLAYING);

    bus = gst_element_get_bus(pipeline);
    msg = gst_bus_timed_pop_filtered(bus, GST_CLOCK_TIME_NONE, (GstMessageType)(GST_MESSAGE_ERROR | GST_MESSAGE_EOS));

    gst_element_release_request_pad(tee, tee_audio_pad);
    gst_element_release_request_pad(tee, tee_video_pad);

    gst_object_unref(tee_audio_pad);
    gst_object_unref(tee_video_pad);

    if (msg != NULL)
        gst_message_unref(msg);

    gst_object_unref(bus);
    gst_element_set_state(pipeline, GST_STATE_NULL);

    gst_object_unref(pipeline);
    return 0;
}
*/

/*
static gboolean print_field(GQuark field, const GValue *value, gpointer pfx)
{
    gchar *str = gst_value_serialize(value);

    g_print("%s  %15s: %s\n", (gchar *)pfx, g_quark_to_string(field), str);
    g_free(str);
    return TRUE;
}

static void print_caps(const GstCaps *caps, const gchar *pfx)
{
    guint i;

    g_return_if_fail(caps != NULL);

    if (gst_caps_is_any(caps))
    {
        g_print("%sANY\n", pfx);
        return;
    }
    if (gst_caps_is_empty(caps))
    {
        g_print("%sEMPTY\n", pfx);
        return;
    }

    for (i = 0; i < gst_caps_get_size(caps); i++)
    {
        GstStructure *structure = gst_caps_get_structure(caps, i);

        g_print("%s%s\n", pfx, gst_structure_get_name(structure));
        gst_structure_foreach(structure, print_field, (gpointer)pfx);
    }
}


static void print_pad_templates_information(GstElementFactory *factory)
{
    const GList *pads;
    GstStaticPadTemplate *padtemplate;

    g_print("Pad Templates for %s:\n", gst_element_factory_get_longname(factory));
    if (!gst_element_factory_get_num_pad_templates(factory))
    {
        g_print("  none\n");
        return;
    }

    pads = gst_element_factory_get_static_pad_templates(factory);
    while (pads)
    {
        padtemplate = (GstStaticPadTemplate *)pads->data;
        pads = g_list_next(pads);

        if (padtemplate->direction == GST_PAD_SRC)
            g_print("  SRC template: '%s'\n", padtemplate->name_template);
        else if (padtemplate->direction == GST_PAD_SINK)
            g_print("  SINK template: '%s'\n", padtemplate->name_template);
        else
            g_print("  UNKNOWN!!! template: '%s'\n", padtemplate->name_template);

        if (padtemplate->presence == GST_PAD_ALWAYS)
            g_print("    Availability: Always\n");
        else if (padtemplate->presence == GST_PAD_SOMETIMES)
            g_print("    Availability: Sometimes\n");
        else if (padtemplate->presence == GST_PAD_REQUEST)
            g_print("    Availability: On request\n");
        else
            g_print("    Availability: UNKNOWN!!!\n");

        if (padtemplate->static_caps.string)
        {
            GstCaps *caps;
            g_print("    Capabilities:\n");
            caps = gst_static_caps_get(&padtemplate->static_caps);
            print_caps(caps, "      ");
            gst_caps_unref(caps);
        }

        g_print("\n");
    }
}

static void print_pad_capabilities(GstElement *element, const gchar *pad_name)
{
    GstPad *pad = NULL;
    GstCaps *caps = NULL;

    pad = gst_element_get_static_pad(element, pad_name);
    if (!pad)
    {
        g_printerr("Could not retrieve pad '%s'\n", pad_name);
        return;
    }

    caps = gst_pad_get_current_caps(pad);
    if (!caps)
        caps = gst_pad_query_caps(pad, NULL);

    g_print("Caps for the %s pad:\n", pad_name);
    print_caps(caps, "      ");
    gst_caps_unref(caps);
    gst_object_unref(pad);
}

int main(int argc, char *argv[])
{
    GstElement *pipeline, *source, *sink;
    GstElementFactory *source_factory, *sink_factory;
    GstBus *bus;
    GstMessage *msg;
    GstStateChangeReturn ret;
    gboolean terminate = FALSE;

    gst_init(&argc, &argv);

    source_factory = gst_element_factory_find("audiotestsrc");
    sink_factory = gst_element_factory_find("autoaudiosink");
    if (!source_factory || !sink_factory)
    {
        g_printerr("Not all element factories could be created.\n");
        return -1;
    }

    print_pad_templates_information(source_factory);
    print_pad_templates_information(sink_factory);

    source = gst_element_factory_create(source_factory, "source");
    sink = gst_element_factory_create(sink_factory, "sink");

    pipeline = gst_pipeline_new("test-pipeline");

    if (!pipeline || !source || !sink)
    {
        g_printerr("Not all elements could be created.\n");
        return -1;
    }

    gst_bin_add_many(GST_BIN(pipeline), source, sink, NULL);
    if (gst_element_link(source, sink) != TRUE)
    {
        g_printerr("Elements could not be linked.\n");
        gst_object_unref(pipeline);
        return -1;
    }

    g_print("In NULL state:\n");
    print_pad_capabilities(sink, "sink");

    ret = gst_element_set_state(pipeline, GST_STATE_PLAYING);
    if (ret == GST_STATE_CHANGE_FAILURE)
    {
        g_printerr("Unable to set the pipeline to the playing state (check the bus for error messages).\n");
    }

    bus = gst_element_get_bus(pipeline);
    do
    {
        msg = gst_bus_timed_pop_filtered(bus, GST_CLOCK_TIME_NONE, (GstMessageType)(GST_MESSAGE_ERROR | GST_MESSAGE_EOS | GST_MESSAGE_STATE_CHANGED));

        if (msg != NULL)
        {
            GError *err;
            gchar *debug_info;

            switch (GST_MESSAGE_TYPE(msg))
            {
            case GST_MESSAGE_ERROR:
                gst_message_parse_error(msg, &err, &debug_info);
                g_printerr("Error received from element %s: %s\n", GST_OBJECT_NAME(msg->src), err->message);
                g_printerr("Debugging information: %s\n", debug_info ? debug_info : "none");
                g_clear_error(&err);
                g_free(debug_info);
                terminate = TRUE;
                break;
            case GST_MESSAGE_EOS:
                g_print("End-Of-Stream reached.\n");
                terminate = TRUE;
                break;
            case GST_MESSAGE_STATE_CHANGED:
                if (GST_MESSAGE_SRC(msg) == GST_OBJECT(pipeline))
                {
                    GstState old_state, new_state, pending_state;
                    gst_message_parse_state_changed(msg, &old_state, &new_state, &pending_state);
                    g_print("\nPipeline state changed from %s to %s:\n",
                            gst_element_state_get_name(old_state), gst_element_state_get_name(new_state));
                    print_pad_capabilities(sink, "sink");
                }
                break;
            default:
                g_printerr("Unexpected message received.\n");
                break;
            }
            gst_message_unref(msg);
        }
    } while (!terminate);

    gst_object_unref(bus);
    gst_element_set_state(pipeline, GST_STATE_NULL);
    gst_object_unref(pipeline);
    gst_object_unref(source_factory);
    gst_object_unref(sink_factory);
    return 0;
}
*/

/*
typedef struct _CustomData
{
    GstElement *playbin;

    GtkWidget *sink_widget;
    GtkWidget *slider;
    GtkWidget *streams_list;
    gulong slider_update_signal_id;

    GstState state;
    gint64 duration;
} CustomData;

static void play_cb(GtkButton *button, CustomData *data)
{
    gst_element_set_state(data->playbin, GST_STATE_PLAYING);
}

static void pause_cb(GtkButton *button, CustomData *data)
{
    gst_element_set_state(data->playbin, GST_STATE_PAUSED);
}

static void stop_cb(GtkButton *button, CustomData *data)
{
    gst_element_set_state(data->playbin, GST_STATE_READY);
}

static void delete_event_cb(GtkWidget *widget, GdkEvent *event, CustomData *data)
{
    stop_cb(NULL, data);
    gtk_main_quit();
}

static void slider_cb(GtkRange *range, CustomData *data)
{
    gdouble value = gtk_range_get_value(GTK_RANGE(data->slider));
    gst_element_seek_simple(data->playbin, GST_FORMAT_TIME, (GstSeekFlags)(GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_KEY_UNIT),
                            (gint64)(value * GST_SECOND));
}

static void create_ui(CustomData *data)
{
    GtkWidget *main_window;
    GtkWidget *main_box;
    GtkWidget *main_hbox;
    GtkWidget *controls;
    GtkWidget *play_button, *pause_button, *stop_button;

    main_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    g_signal_connect(G_OBJECT(main_window), "delete-event", G_CALLBACK(delete_event_cb), data);

    play_button = gtk_button_new_from_icon_name("media-playback-start", GTK_ICON_SIZE_SMALL_TOOLBAR);
    g_signal_connect(G_OBJECT(play_button), "clicked", G_CALLBACK(play_cb), data);

    pause_button = gtk_button_new_from_icon_name("media-playback-pause", GTK_ICON_SIZE_SMALL_TOOLBAR);
    g_signal_connect(G_OBJECT(pause_button), "clicked", G_CALLBACK(pause_cb), data);

    stop_button = gtk_button_new_from_icon_name("media-playback-stop", GTK_ICON_SIZE_SMALL_TOOLBAR);
    g_signal_connect(G_OBJECT(stop_button), "clicked", G_CALLBACK(stop_cb), data);

    data->slider = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0, 100, 1);
    gtk_scale_set_draw_value(GTK_SCALE(data->slider), 0);
    data->slider_update_signal_id = g_signal_connect(G_OBJECT(data->slider), "value-changed", G_CALLBACK(slider_cb), data);

    data->streams_list = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(data->streams_list), FALSE);

    controls = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_box_pack_start(GTK_BOX(controls), play_button, FALSE, FALSE, 2);
    gtk_box_pack_start(GTK_BOX(controls), pause_button, FALSE, FALSE, 2);
    gtk_box_pack_start(GTK_BOX(controls), stop_button, FALSE, FALSE, 2);
    gtk_box_pack_start(GTK_BOX(controls), data->slider, TRUE, TRUE, 2);

    main_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_box_pack_start(GTK_BOX(main_hbox), data->sink_widget, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(main_hbox), data->streams_list, FALSE, FALSE, 2);

    main_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_box_pack_start(GTK_BOX(main_box), main_hbox, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(main_box), controls, FALSE, FALSE, 0);
    gtk_container_add(GTK_CONTAINER(main_window), main_box);
    gtk_window_set_default_size(GTK_WINDOW(main_window), 640, 480);

    gtk_widget_show_all(main_window);
}

static gboolean refresh_ui(CustomData *data)
{
    gint64 current = -1;

    if (data->state < GST_STATE_PAUSED)
        return TRUE;

    if (!GST_CLOCK_TIME_IS_VALID(data->duration))
    {
        if (!gst_element_query_duration(data->playbin, GST_FORMAT_TIME, &data->duration))
        {
            g_printerr("Could not query current duration.\n");
        }
        else
        {
            gtk_range_set_range(GTK_RANGE(data->slider), 0, (gdouble)data->duration / GST_SECOND);
        }
    }

    if (gst_element_query_position(data->playbin, GST_FORMAT_TIME, &current))
    {
        g_signal_handler_block(data->slider, data->slider_update_signal_id);
        gtk_range_set_value(GTK_RANGE(data->slider), (gdouble)current / GST_SECOND);
        g_signal_handler_unblock(data->slider, data->slider_update_signal_id);
    }
    return TRUE;
}

static void tags_cb(GstElement *playbin, gint stream, CustomData *data)
{
    gst_element_post_message(playbin,
                             gst_message_new_application(GST_OBJECT(playbin),
                                                         gst_structure_new_empty("tags-changed")));
}

static void error_cb(GstBus *bus, GstMessage *msg, CustomData *data)
{
    GError *err;
    gchar *debug_info;

    gst_message_parse_error(msg, &err, &debug_info);
    g_printerr("Error received from element %s: %s\n", GST_OBJECT_NAME(msg->src), err->message);
    g_printerr("Debugging information: %s\n", debug_info ? debug_info : "none");
    g_clear_error(&err);
    g_free(debug_info);

    gst_element_set_state(data->playbin, GST_STATE_READY);
}

static void eos_cb(GstBus *bus, GstMessage *msg, CustomData *data)
{
    g_print("End-Of-Stream reached.\n");
    gst_element_set_state(data->playbin, GST_STATE_READY);
}

static void state_changed_cb(GstBus *bus, GstMessage *msg, CustomData *data)
{
    GstState old_state, new_state, pending_state;
    gst_message_parse_state_changed(msg, &old_state, &new_state, &pending_state);
    if (GST_MESSAGE_SRC(msg) == GST_OBJECT(data->playbin))
    {
        data->state = new_state;
        g_print("State set to %s\n", gst_element_state_get_name(new_state));
        if (old_state == GST_STATE_READY && new_state == GST_STATE_PAUSED)
        {
            refresh_ui(data);
        }
    }
}

static void analyze_streams(CustomData *data)
{
    gint i;
    GstTagList *tags;
    gchar *str, *total_str;
    guint rate;
    gint n_video, n_audio, n_text;
    GtkTextBuffer *text;

    text = gtk_text_view_get_buffer(GTK_TEXT_VIEW(data->streams_list));
    gtk_text_buffer_set_text(text, "", -1);

    g_object_get(data->playbin, "n-video", &n_video, NULL);
    g_object_get(data->playbin, "n-audio", &n_audio, NULL);
    g_object_get(data->playbin, "n-text", &n_text, NULL);

    for (i = 0; i < n_video; i++)
    {
        tags = NULL;

        g_signal_emit_by_name(data->playbin, "get-video-tags", i, &tags);
        if (tags)
        {
            total_str = g_strdup_printf("video stream %d:\n", i);
            gtk_text_buffer_insert_at_cursor(text, total_str, -1);
            g_free(total_str);
            gst_tag_list_get_string(tags, GST_TAG_VIDEO_CODEC, &str);
            total_str = g_strdup_printf("  codec: %s\n", str ? str : "unknown");
            gtk_text_buffer_insert_at_cursor(text, total_str, -1);
            g_free(total_str);
            g_free(str);
            gst_tag_list_free(tags);
        }
    }

    for (i = 0; i < n_audio; i++)
    {
        tags = NULL;

        g_signal_emit_by_name(data->playbin, "get-audio-tags", i, &tags);
        if (tags)
        {
            total_str = g_strdup_printf("\naudio stream %d:\n", i);
            gtk_text_buffer_insert_at_cursor(text, total_str, -1);
            g_free(total_str);
            if (gst_tag_list_get_string(tags, GST_TAG_AUDIO_CODEC, &str))
            {
                total_str = g_strdup_printf("  codec: %s\n", str);
                gtk_text_buffer_insert_at_cursor(text, total_str, -1);
                g_free(total_str);
                g_free(str);
            }
            if (gst_tag_list_get_string(tags, GST_TAG_LANGUAGE_CODE, &str))
            {
                total_str = g_strdup_printf("  language: %s\n", str);
                gtk_text_buffer_insert_at_cursor(text, total_str, -1);
                g_free(total_str);
                g_free(str);
            }
            if (gst_tag_list_get_uint(tags, GST_TAG_BITRATE, &rate))
            {
                total_str = g_strdup_printf("  bitrate: %d\n", rate);
                gtk_text_buffer_insert_at_cursor(text, total_str, -1);
                g_free(total_str);
            }
            gst_tag_list_free(tags);
        }
    }

    for (i = 0; i < n_text; i++)
    {
        tags = NULL;

        g_signal_emit_by_name(data->playbin, "get-text-tags", i, &tags);
        if (tags)
        {
            total_str = g_strdup_printf("\nsubtitle stream %d:\n", i);
            gtk_text_buffer_insert_at_cursor(text, total_str, -1);
            g_free(total_str);
            if (gst_tag_list_get_string(tags, GST_TAG_LANGUAGE_CODE, &str))
            {
                total_str = g_strdup_printf("  language: %s\n", str);
                gtk_text_buffer_insert_at_cursor(text, total_str, -1);
                g_free(total_str);
                g_free(str);
            }
            gst_tag_list_free(tags);
        }
    }
}

static void application_cb(GstBus *bus, GstMessage *msg, CustomData *data)
{
    if (g_strcmp0(gst_structure_get_name(gst_message_get_structure(msg)), "tags-changed") == 0)
    {
        analyze_streams(data);
    }
}

int main(int argc, char *argv[])
{
    CustomData data;
    GstStateChangeReturn ret;
    GstBus *bus;
    GstElement *gtkglsink, *videosink;

    gtk_init(&argc, &argv);
    gst_init(&argc, &argv);

    memset(&data, 0, sizeof(data));
    data.duration = GST_CLOCK_TIME_NONE;

    data.playbin = gst_element_factory_make("playbin", "playbin");
    videosink = gst_element_factory_make("glsinkbin", "glsinkbin");
    gtkglsink = gst_element_factory_make("gtkglsink", "gtkglsink");

    if (gtkglsink != NULL && videosink != NULL)
    {
        g_printerr("Successfully created GTK GL Sink");

        g_object_set(videosink, "sink", gtkglsink, NULL);

        g_object_get(gtkglsink, "widget", &data.sink_widget, NULL);
    }
    else
    {
        g_printerr("Could not create gtkglsink, falling back to gtksink.\n");

        videosink = gst_element_factory_make("gtksink", "gtksink");
        g_object_get(videosink, "widget", &data.sink_widget, NULL);
    }

    if (!data.playbin || !videosink)
    {
        g_printerr("Not all elements could be created.\n");
        return -1;
    }

    g_object_set(data.playbin, "uri", "https://gstreamer.freedesktop.org/data/media/sintel_trailer-480p.webm", NULL);
    g_object_set(data.playbin, "video-sink", videosink, NULL);

    g_signal_connect(G_OBJECT(data.playbin), "video-tags-changed", (GCallback)tags_cb, &data);
    g_signal_connect(G_OBJECT(data.playbin), "audio-tags-changed", (GCallback)tags_cb, &data);
    g_signal_connect(G_OBJECT(data.playbin), "text-tags-changed", (GCallback)tags_cb, &data);

    create_ui(&data);

    bus = gst_element_get_bus(data.playbin);
    gst_bus_add_signal_watch(bus);
    g_signal_connect(G_OBJECT(bus), "message::error", (GCallback)error_cb, &data);
    g_signal_connect(G_OBJECT(bus), "message::eos", (GCallback)eos_cb, &data);
    g_signal_connect(G_OBJECT(bus), "message::state-changed", (GCallback)state_changed_cb, &data);
    g_signal_connect(G_OBJECT(bus), "message::application", (GCallback)application_cb, &data);
    gst_object_unref(bus);

    ret = gst_element_set_state(data.playbin, GST_STATE_PLAYING);
    if (ret == GST_STATE_CHANGE_FAILURE)
    {
        g_printerr("Unable to set the pipeline to the playing state.\n");
        gst_object_unref(data.playbin);
        gst_object_unref(videosink);
        return -1;
    }

    g_timeout_add_seconds(1, (GSourceFunc)refresh_ui, &data);
    gtk_main();

    gst_element_set_state(data.playbin, GST_STATE_NULL);
    gst_object_unref(data.playbin);
    // gst_object_unref(videosink);

    return 0;
}
*/

/*
typedef struct _CustomData
{
  GstElement *playbin;
  gboolean playing;
  gboolean terminate;
  gboolean seek_enabled;
  gboolean seek_done;
  gint64 duration;
} CustomData;

static void handle_message (CustomData * data, GstMessage * msg);

int main(int argc, char *argv[])
{
    CustomData data;
    GstBus *bus;
    GstMessage *msg;
    GstStateChangeReturn ret;

    data.playing = FALSE;
    data.terminate = FALSE;
    data.seek_enabled = FALSE;
    data.seek_done = FALSE;
    data.duration = GST_CLOCK_TIME_NONE;

    gst_init (&argc, &argv);

    data.playbin = gst_element_factory_make ("playbin", "playbin");

    if (!data.playbin) {
        g_printerr ("Not all elements could be created.\n");
        return -1;
    }

    g_object_set (data.playbin, "uri",
        "https://gstreamer.freedesktop.org/data/media/sintel_trailer-480p.webm",
        NULL);

    ret = gst_element_set_state (data.playbin, GST_STATE_PLAYING);
    if (ret == GST_STATE_CHANGE_FAILURE) {
        g_printerr ("Unable to set the pipeline to the playing state.\n");
        gst_object_unref (data.playbin);
        return -1;
    }

    bus = gst_element_get_bus (data.playbin);
    do {
        msg = gst_bus_timed_pop_filtered (bus, 100 * GST_MSECOND,
            (GstMessageType)(GST_MESSAGE_STATE_CHANGED | GST_MESSAGE_ERROR | GST_MESSAGE_EOS | GST_MESSAGE_DURATION));

        if (msg != NULL) {
            handle_message (&data, msg);
        }
        else {
            if (data.playing) {
                gint64 current = -1;

                if (!gst_element_query_position (data.playbin, GST_FORMAT_TIME, &current)) {
                    g_printerr ("Could not query current position.\n");
                }

                if (!GST_CLOCK_TIME_IS_VALID (data.duration)) {
                    if (!gst_element_query_duration (data.playbin, GST_FORMAT_TIME, &data.duration)) {
                        g_printerr ("Could not query current duration.\n");
                    }
                }

                g_print ("Position %" GST_TIME_FORMAT " / %" GST_TIME_FORMAT "\r",
                    GST_TIME_ARGS (current), GST_TIME_ARGS (data.duration));

                if (data.seek_enabled && !data.seek_done && current > 10 * GST_SECOND) {
                    g_print ("\nReached 10s, performing seek...\n");
                    gst_element_seek_simple (data.playbin, GST_FORMAT_TIME,
                        (GstSeekFlags)(GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_KEY_UNIT), 30 * GST_SECOND);
                    data.seek_done = TRUE;
                }
            }
        }
    } while (!data.terminate);

    gst_object_unref (bus);
    gst_element_set_state (data.playbin, GST_STATE_NULL);
    gst_object_unref (data.playbin);
    return 0;
}

static void handle_message (CustomData * data, GstMessage * msg)
{
    GError *err;
    gchar *debug_info;

    switch (GST_MESSAGE_TYPE (msg)) {
        case GST_MESSAGE_ERROR:
            gst_message_parse_error (msg, &err, &debug_info);
            g_printerr ("Error received from element %s: %s\n",
                GST_OBJECT_NAME (msg->src), err->message);
            g_printerr ("Debugging information: %s\n",
                debug_info ? debug_info : "none");
            g_clear_error (&err);
            g_free (debug_info);
            data->terminate = TRUE;
            break;
        case GST_MESSAGE_EOS:
            g_print ("\nEnd-Of-Stream reached.\n");
            data->terminate = TRUE;
            break;
        case GST_MESSAGE_DURATION:
            data->duration = GST_CLOCK_TIME_NONE;
            break;
        case GST_MESSAGE_STATE_CHANGED:{
            GstState old_state, new_state, pending_state;
            gst_message_parse_state_changed (msg, &old_state, &new_state,
                &pending_state);
            if (GST_MESSAGE_SRC (msg) == GST_OBJECT (data->playbin)) {
                g_print ("Pipeline state changed from %s to %s:\n",
                    gst_element_state_get_name (old_state), gst_element_state_get_name (new_state));

                data->playing = (new_state == GST_STATE_PLAYING);

                if (data->playing) {
                    GstQuery *query;
                    gint64 start, end;
                    query = gst_query_new_seeking (GST_FORMAT_TIME);
                    if (gst_element_query (data->playbin, query)) {
                        gst_query_parse_seeking (query, NULL, &data->seek_enabled, &start,
                            &end);
                        if (data->seek_enabled) {
                        g_print ("Seeking is ENABLED from %" GST_TIME_FORMAT " to %"
                            GST_TIME_FORMAT "\n", GST_TIME_ARGS (start),
                            GST_TIME_ARGS (end));
                        } else {
                        g_print ("Seeking is DISABLED for this stream.\n");
                        }
                    }
                    else {
                        g_printerr ("Seeking query failed.");
                    }
                    gst_query_unref (query);
                }
            }
            }
            break;
        default:
            g_printerr ("Unexpected message received.\n");
            break;
    }
    gst_message_unref (msg);
}
*/

/*
typedef struct _CustomData
{
    GstElement *pipeline;

    // source
    GstElement *source;

    // audio
    GstElement *convert;
    GstElement *resample;
    GstElement *sink;

    // video
    GstElement *video_convert;
    GstElement *video_sink;
} CustomData;

static void pad_added_handler (GstElement *src, GstPad *pad, CustomData *data);
static void pad_added_video_handler (GstElement *src, GstPad *pad, CustomData *data);

int main(int argc, char *argv[]) {
    CustomData data;
    GstBus *bus;
    GstMessage *msg;
    GstStateChangeReturn ret;
    gboolean terminate = FALSE;

    gst_init(&argc, &argv);

    data.source = gst_element_factory_make("uridecodebin", "source");
    data.convert = gst_element_factory_make("audioconvert", "convert");
    data.resample = gst_element_factory_make("audioresample", "resample");
    data.sink = gst_element_factory_make("autoaudiosink", "sink");

    data.video_convert = gst_element_factory_make("videoconvert", "video_convert");
    data.video_sink = gst_element_factory_make("autovideosink", "video_sink");

    data.pipeline = gst_pipeline_new("test-pipeline");

    if (!data.pipeline || !data.source || !data.convert || !data.resample || !data.sink || !data.video_convert || !data.video_sink)
    {
        g_printerr("Not all elements could be created.\n");
        return -1;
    }

    gst_bin_add_many(GST_BIN(data.pipeline), data.source, data.convert, data.resample, data.sink, data.video_convert, data.video_sink, NULL);

    if (!gst_element_link_many(data.convert, data.resample, data.sink, NULL)) {
        g_printerr("Elements(audio) could not be linked.\n");
        gst_object_unref(data.pipeline);
        return -1;
    }

    if (!gst_element_link_many(data.video_convert, data.video_sink, NULL)) {
        g_printerr("Elements(video) could not be linked.\n");
        gst_object_unref(data.pipeline);
        return -1;
    }

    g_object_set(
        data.source, "uri",
        "https://gstreamer.freedesktop.org/data/media/sintel_trailer-480p.webm",
        NULL
    );

    g_signal_connect(
        data.source, "pad-added", G_CALLBACK(pad_added_handler), &data
    );

    g_signal_connect(
        data.source, "pad-added", G_CALLBACK(pad_added_video_handler), &data
    );

    ret = gst_element_set_state(data.pipeline, GST_STATE_PLAYING);
    if (ret == GST_STATE_CHANGE_FAILURE) {
        g_printerr("Unable to set the pipeline to the playing state.\n");
        gst_object_unref(data.pipeline);
        return -1;
    }

    bus = gst_element_get_bus(data.pipeline);
    do {
        msg = gst_bus_timed_pop_filtered(
            bus, GST_CLOCK_TIME_NONE,
            (GstMessageType)(GST_MESSAGE_STATE_CHANGED | GST_MESSAGE_ERROR | GST_MESSAGE_EOS)
        );

        if (msg != NULL) {
            GError *err;
            gchar *debug_info;

            switch (GST_MESSAGE_TYPE(msg)) {
                case GST_MESSAGE_ERROR:
                    gst_message_parse_error(msg, &err, &debug_info);
                    g_printerr("Error recevied from element %s: %s\n",
                        GST_OBJECT_NAME(msg->src), err->message);
                    g_printerr("Debugging information: %s\n",
                        debug_info ? debug_info : "none");
                    g_clear_error(&err);
                    g_free(debug_info);
                    terminate = TRUE;
                    break;

                case GST_MESSAGE_EOS:
                    g_printerr("End-Of-Stream reached.\n");
                    terminate = TRUE;
                    break;

                case GST_MESSAGE_STATE_CHANGED:
                    if (GST_MESSAGE_SRC(msg) == GST_OBJECT(data.pipeline)) {
                        GstState old_state, new_state, pending_state;
                        gst_message_parse_state_changed(msg, &old_state, &new_state, &pending_state);
                        g_print("Pipeline state changed from %s to %s:\n",
                            gst_element_state_get_name(old_state), gst_element_state_get_name(new_state));
                    }
                    break;

                default:
                    g_printerr("Unexpected message received.\n");
                    break;
            }
            gst_message_unref(msg);
        }
    } while (!terminate);

    gst_object_unref(bus);
    gst_element_set_state(data.pipeline, GST_STATE_NULL);
    gst_object_unref(data.pipeline);

    return 0;
}

static void pad_added_handler(GstElement* src, GstPad* new_pad, CustomData* data)
{
    GstPad* sink_pad = gst_element_get_static_pad(data->convert, "sink");
    GstPadLinkReturn ret;
    GstCaps* new_pad_caps = NULL;
    GstStructure* new_pad_struct = NULL;
    const gchar* new_pad_type = NULL;

    g_print("Received new pad '%s' from '%s'\n", GST_PAD_NAME(new_pad), GST_ELEMENT_NAME(src));

    if (gst_pad_is_linked(sink_pad)) {
        g_print("We are already linked. Ignoring.\n");
        goto exit;
    }

    new_pad_caps = gst_pad_get_current_caps(new_pad);
    new_pad_struct = gst_caps_get_structure(new_pad_caps, 0);
    new_pad_type = gst_structure_get_name(new_pad_struct);

    if (!g_str_has_prefix(new_pad_type, "audio/x-raw")) {
        g_print("It has type '%s' which is not raw audio. Ignoring.\n", new_pad_type);
        goto exit;
    }

    ret = gst_pad_link(new_pad, sink_pad);
    if (GST_PAD_LINK_FAILED(ret)) {
        g_print("Type is '%s' but link failed.\n", new_pad_type);
    }
    else {
        g_print("Link succeeded (type '%s').\n", new_pad_type);
    }

exit:
    if (new_pad_caps != NULL){
        gst_caps_unref(new_pad_caps);
    }

    gst_object_unref(sink_pad);
}

static void pad_added_video_handler (GstElement *src, GstPad *new_pad, CustomData *data)
{
    GstPad* sink_pad = gst_element_get_static_pad(data->video_convert, "sink");
    GstPadLinkReturn ret;
    GstCaps* new_pad_caps = NULL;
    GstStructure* new_pad_struct = NULL;
    const gchar* new_pad_type = NULL;

    g_print("Received new pad '%s' from '%s'\n", GST_PAD_NAME(new_pad), GST_ELEMENT_NAME(src));

    if (gst_pad_is_linked(sink_pad)) {
        g_print("We are already linked. Ignoring.\n");
        goto exit;
    }

    new_pad_caps = gst_pad_get_current_caps(new_pad);
    new_pad_struct = gst_caps_get_structure(new_pad_caps, 0);
    new_pad_type = gst_structure_get_name(new_pad_struct);

    if (!g_str_has_prefix(new_pad_type, "video/x-raw")) {
        g_print("It has type '%s' which is not raw video. Ignoring.\n", new_pad_type);
        goto exit;
    }

    ret = gst_pad_link(new_pad, sink_pad);
    if (GST_PAD_LINK_FAILED(ret)) {
        g_print("Type is '%s' but link failed.\n", new_pad_type);
    }
    else {
        g_print("Link succeeded (type '%s').\n", new_pad_type);
    }

exit:
    if (new_pad_caps != NULL){
        gst_caps_unref(new_pad_caps);
    }

    gst_object_unref(sink_pad);
}
*/

/*
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
*/

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