#include <stdlib.h>
#include <gst/gst.h>

#include "tco_libd.h"
#include "camera.h"

void log_gst_version(void)
{
    const char *nano_str;
    guint major, minor, micro, nano;
    gst_version(&major, &minor, &micro, &nano);

    if (nano == 1)
    {
        nano_str = "(cvs)";
    }
    else if (nano == 2)
    {
        nano_str = "(pre-release)";
    }
    else
    {
        nano_str = "";
    }
    log_debug("GStreamer %d.%d.%d %s", major, minor, micro, nano_str);
}

static GstFlowReturn handle_new_sample(GstElement *sink, void *data)
{
    GstSample *sample;
    GstFlowReturn retval = GST_FLOW_OK;

    g_signal_emit_by_name(sink, "pull-sample", &sample);
    if (sample)
    {
        GstMapInfo info;
        GstBuffer *buf = gst_sample_get_buffer(sample);
        if (gst_buffer_map(buf, &info, GST_MAP_READ) == TRUE)
        {
            // Pass the frame to the user callback
            camera_user_data_t *user_data = (camera_user_data_t *)data;
            user_data->f(info.data, info.size, user_data->args);
        }
        else
        {
            log_error("Error: Couldn't get buffer info");
            retval = GST_FLOW_ERROR;
        }
        gst_buffer_unmap(buf, &info);
        gst_sample_unref(sample);
    }
    return retval;
}

static gboolean handle_bus_msg(GstBus *bus, GstMessage *msg, gpointer data)
{
    GMainLoop *loop = (GMainLoop *)data;
    switch (GST_MESSAGE_TYPE(msg))
    {
    case GST_MESSAGE_EOS:
        log_debug("BUSMSG: End of stream");
        g_main_loop_quit(loop);
        break;
    case GST_MESSAGE_ERROR:
    {
        GError *error;
        gst_message_parse_error(msg, &error, NULL);
        log_debug("BUSMSG: Error: %s", error->message);
        g_error_free(error);
        g_main_loop_quit(loop);
        break;
    }
    case GST_MESSAGE_WARNING:
    {
        GError *error;
        gst_message_parse_warning(msg, &error, NULL);
        log_debug("BUSMSG: Warning: %s", error->message);
        g_error_free(error);
        g_main_loop_quit(loop);
        break;
    }
    default:
        break;
    }
    return TRUE;
}

int camera_pipeline_run(int argc, char *argv[], camera_user_data_t *user_data)
{
    /* Initialisation */
    gst_init(&argc, &argv);
    log_gst_version();

    /* Create gstreamer elements */
    GMainLoop *loop = g_main_loop_new(NULL, FALSE);
    if (!loop)
    {
        log_error("Failed to create a gstreamer main loop");
        return -1;
    }

    GstElement *pipeline = gst_pipeline_new("camera-pipeline");
    GstElement *camera_source = gst_element_factory_make("v4l2src", "camera-source");
    GstElement *converter = gst_element_factory_make("videoconvert", "converter");
    GstElement *queue_leaking = gst_element_factory_make("queue", "queue_leaking");
    GstElement *appsink = gst_element_factory_make("appsink", "appsink");
    if (!pipeline || !camera_source || !converter || !queue_leaking || !appsink)
    {
        log_error("Failed to create one or more gstreamer elements");
        return -1;
    }

    /* Set fixed capabilities on the camera source. */
    GstCaps *camera_caps = gst_caps_new_simple("video/x-raw",
                                               "width", G_TYPE_INT, 1280,
                                               "height", G_TYPE_INT, 960,
                                               "format", G_TYPE_STRING, "I420",
                                               "framerate", G_TYPE_STRING, "30/1",
                                               NULL);
    if (!camera_caps)
    {
        log_error("Failed to create a gstreamer caps");
        return -1;
    }

    /* Set up the pipeline. */
    /* Configure the Video4Linux source to read from the camera. */
    g_object_set(G_OBJECT(camera_source), "device", "/dev/video0", NULL);

    /* Setup a leaky queue. */
    g_object_set(G_OBJECT(queue_leaking), "leaky", 2, NULL); /* 2 = downstream */
    g_object_set(G_OBJECT(queue_leaking), "max-size-buffers", 1, NULL);

    /* Add elements to pipeline. */
    gst_bin_add_many(GST_BIN(pipeline), camera_source, converter, queue_leaking, appsink, NULL);

    /* Link elements together to form the pipeline. */
    gst_element_link_filtered(camera_source, converter, camera_caps); /* Fixed capabilities. */
    gst_caps_unref(camera_caps);                                      /* Ensure it gets freed later on once the pipeline is freed */
    gst_element_link_many(converter, queue_leaking, appsink, NULL);

    GstBus *bus = gst_element_get_bus(pipeline);
    if (!bus)
    {
        log_error("Failed to create a gstreamer bus");
        return -1;
    }
    gst_bus_add_watch(bus, handle_bus_msg, loop);
    gst_object_unref(bus); /* Ensure it gets freed later on once the pipeline is freed */

    // Set up an appsink to pass frames to a user callback
    g_object_set(appsink, "emit-signals", TRUE, NULL);
    g_signal_connect(appsink, "new-sample", (GCallback)handle_new_sample, user_data);

    // Start the pipeline, runs until interrupted, EOS or error
    log_info("Starting pipeline");
    gst_element_set_state(pipeline, GST_STATE_PLAYING);
    g_main_loop_run(loop);

    // Cleanup
    log_info("Closing pipeline");
    gst_element_set_state(pipeline, GST_STATE_NULL); /* Resources get freed automatically after this call. */
    gst_object_unref(pipeline);
    return 0;
}