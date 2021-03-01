#include <stdlib.h>
#include <gst/gst.h>

#include "tco_libd.h"
#include "camera.h"

/* NOTE: "format=YUY2" works but gives a warning about subscribing to the V4L2_EVENT_SOURCE_CHANGE etc... */
static const gchar *pipeline_def =
    "v4l2src device=/dev/video0 io-mode=dmabuf !"
    "video/x-raw,width=640,height=480,framerate=30/1 !"
    "queue max-size-buffers=1 leaky=downstream !"
    "appsink name=appsink";

static void log_gst_version(void)
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

    GstElement *pipeline = gst_parse_launch(pipeline_def, NULL);
    if (!pipeline)
    {
        log_error("Failed to create a gstreamer pipeline");
        return -1;
    }

    GstBus *bus = gst_element_get_bus(pipeline);
    if (!bus)
    {
        log_error("Failed to create a gstreamer bus");
        return -1;
    }
    gst_bus_add_watch(bus, handle_bus_msg, loop);
    gst_object_unref(bus); /* Ensure it gets freed later on once the pipeline is freed */

    // Set up an appsink to pass frames to a user callback
    GstElement *appsink = gst_bin_get_by_name((GstBin *)pipeline, "appsink");
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
