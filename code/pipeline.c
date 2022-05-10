#include <stdlib.h>
#include <string.h>

#include <gst/gst.h>

#include "tco_libd.h"
#include "tco_shmem.h"
#include "pipeline.h"

typedef struct gst_pipeline_t
{
    GMainLoop *loop;
    GstElement *pipeline, *app_source;
    guint source_id; /* To control the GSource. */
    pl_user_data_t *user_data;
} gst_pipeline_t;

static gst_pipeline_t pipeline_main = {NULL, NULL, NULL};
static gst_pipeline_t pipeline_display = {NULL, NULL, NULL};

static const gchar *pipeline_camera_def =
    "v4l2src device=/dev/video0 !"
    "video/x-raw,format=YUY2,width=640,height=480,framerate=30/1 !"
    "videocrop top=0 left=0 right=0 bottom=240 !"
    "queue max-size-buffers=1 leaky=downstream !"
    "videoconvert n-threads=4 !"
    "appsink name=appsink caps=video/x-raw,format=GRAY8,width=640,height=240";

static const gchar *pipeline_camera_sim_def =
    "appsrc name=appsrc caps=video/x-raw,format=GRAY8,width=640,height=240 !"
    "videoconvert !"
    "appsink name=appsink caps=video/x-raw,format=GRAY8,width=640,height=240";

static const gchar *pipeline_display_def =
    "appsrc name=appsrc caps=video/x-raw,format=GRAY8,width=640,height=240 !"
    "videoconvert !"
    "ximagesink";

/**
 * @brief Print out the version of the GStreamer linked with the app.
 */
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

/**
 * @brief This method is called by the idle GSource in the mainloop, to feed CHUNK_SIZE bytes into
 * appsrc. The idle handler is added to the mainloop when appsrc requests to start sending data (via
 * the 'need-data' signal) and is removed when appsrc has enough data ('enough-data' signal).
 * @param pipeline_info The definition of the initialized pipeline.
 * @return TRUE on success and FALSE on failure.
 */
static gboolean push_data(gst_pipeline_t *pipeline_info)
{
    if (!pipeline_info->user_data)
    {
        return FALSE;
    }
    GstBuffer *buffer; /* Buffer for storing the frame data. */
    GstFlowReturn ret;
    GstMapInfo map; /* For storing information about a memory map. */
    guint8 *frame_raw;
    uint32_t frame_size = TCO_FRAME_HEIGHT * TCO_FRAME_WIDTH * sizeof(uint8_t);

    /* Create a new empty buffer */
    buffer = gst_buffer_new_and_alloc(frame_size);

    gst_buffer_map(buffer, &map, GST_MAP_WRITE);
    frame_raw = map.data;
    pipeline_info->user_data->frame_injector_data.func((uint8_t(*)[TCO_FRAME_HEIGHT][TCO_FRAME_WIDTH])frame_raw, frame_size, pipeline_info->user_data->frame_injector_data.args);
    gst_buffer_unmap(buffer, &map);

    /* Push the buffer into the appsrc */
    g_signal_emit_by_name(pipeline_info->app_source, "push-buffer", buffer, &ret);

    /* Free the buffer now that we are done with it */
    gst_buffer_unref(buffer);

    if (ret != GST_FLOW_OK)
    {
        /* We got some error, stop sending data */
        return FALSE;
    }

    return TRUE;
}

/**
 * @brief Callback triggered when appsrc needs data. It adds an idle handler to the mainloop to
 * start pushing data into the appsrc.
 * @param source The appsrc element which triggered the signal.
 * @param size The size of the buffer to feed the source.
 * @param pipeline_info The definition of the initialized pipeline.
 */
static void start_feed(GstElement *source, guint size, gst_pipeline_t *pipeline_info)
{
    if (pipeline_info->source_id == 0)
    {
        pipeline_info->source_id = g_idle_add((GSourceFunc)push_data, pipeline_info);
    }
}

/**
 * @brief Callback triggered when appsrc has enough data hence the app should stop injecting more.
 * It also removes the idle handler from the mainloop.
 * @param source The appsrc element which triggered the signal.
 * @param pipeline_info The definition of the initialized pipeline.
 */
static void stop_feed(GstElement *source, gst_pipeline_t *pipeline_info)
{
    if (pipeline_info->source_id != 0)
    {
        g_source_remove(pipeline_info->source_id);
        pipeline_info->source_id = 0;
    }
}

/**
 * @brief Callback triggered when appsink receives a new sample. This is where the user-defined
 * processing function is called to process the frame and do whatever else it wants/needs to do.
 * @param sink The appsink element which triggered the signal.
 * @param pipeline_info The definition of the initialized pipeline.
 * @return GST_FLOW_OK on success and GST_FLOW_ERROR on failure.
 */
static GstFlowReturn handle_new_sample(GstElement *sink, gst_pipeline_t *pipeline_info)
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
            /* Pass the frame to the user callback. */
            pl_user_data_t *user_data = (pl_user_data_t *)pipeline_info->user_data;
            user_data->frame_processor_data.func((uint8_t(*)[TCO_FRAME_HEIGHT][TCO_FRAME_WIDTH])info.data, info.size, user_data->frame_processor_data.args);
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

/**
 * @brief Called when a message is posted on the bus. This handles errors and some warnings by
 * stopping the pipeline.
 * @param bus The bus element of the pipeline.
 * @param msg The error message received.
 * @param pipeline_info The definition of the initialized pipeline.
 */
static int handle_bus_msg(GstBus *const bus, GstMessage *const message, gpointer const user_data)
{
    gst_pipeline_t *const pipeline_info = user_data;
    /* log_debug("Got %s message", GST_MESSAGE_TYPE_NAME(message)); */
    switch (GST_MESSAGE_TYPE(message))
    {
    case GST_MESSAGE_ERROR:
    {
        GError *err;
        gchar *debug_info;

        /* Print error details on the screen */
        gst_message_parse_error(message, &err, &debug_info);
        log_error("BUS: Error received from element %s: %s", GST_OBJECT_NAME(message->src), err->message);
        log_error("BUS: Debugging information: %s", debug_info ? debug_info : "none");
        g_clear_error(&err);
        g_free(debug_info);

        g_main_loop_quit(pipeline_info->loop);
        break;
    }
    case GST_MESSAGE_EOS:
        /* 'end-of-stream' */
        g_main_loop_quit(pipeline_info->loop);
        break;
    default:
        /* Unhandled message */
        break;
    }

    /* Continue receiving messages */
    return 1;
}

/**
 * @brief A utility function which initializes a pipeline given its string definition. The outcome
 * is that the struct at pipeline info is populated with initialized values.
 * @param pipeline_info Location of the pipeline that will be initialized.
 * @param pipeline_definition A string definition of the pipeline.
 * @return 0 on success, -1 on failure.
 */
static int common_pipeline_init(gst_pipeline_t *pipeline_info, const gchar *pipeline_definition)
{
    /* Make sure the struct is zerod out (apart from the user data). */
    void *const user_data = pipeline_info->user_data;
    memset(pipeline_info, 0, sizeof(gst_pipeline_t));
    pipeline_info->user_data = user_data;

    /* Initialisation */
    gst_init(NULL, NULL);
    log_gst_version();

    /* Create gstreamer elements */
    pipeline_info->pipeline = gst_parse_launch(pipeline_definition, NULL);
    if (!pipeline_info->pipeline)
    {
        log_error("Failed to create a gstreamer pipeline");
        return -1;
    }

    GstBus *bus = gst_element_get_bus(pipeline_info->pipeline);
    if (!bus)
    {
        log_error("Failed to create a gstreamer bus");
        return -1;
    }

    /* Instruct the bus to emit signals for each received message, and connect to the interesting
    signals */
    gst_bus_add_watch(bus, &handle_bus_msg, pipeline_info); /* XXX: This returns a source that should be removed with 'g_source_remove' */
    gst_object_unref(bus);                                  /* Ensure it gets freed later on once the pipeline is freed. */
    /* TODO: Catch 'window closed' warnings to stop the pipeline. */

    return 0;
}

/**
 * @brief A utility function which runs a pipeline then once it stops, it makes sure to perform all
 * the required cleanup steps before returning.
 * @param pipeline_info The definition of the initialized pipeline which should be started then
 * cleaned up after it stops.
 * @return 0 on success, -1 on failure.
 */
static int common_pipeline_start_and_cleanup(gst_pipeline_t *pipeline_info)
{
    /* Start playing the pipeline */
    log_info("Starting pipeline");
    gst_element_set_state(pipeline_info->pipeline, GST_STATE_PLAYING);

    /* Create a GLib Main Loop and set it to run */
    pipeline_info->loop = g_main_loop_new(NULL, FALSE);
    if (!pipeline_info->loop)
    {
        log_error("Failed to create a gstreamer main loop");
        return -1;
    }
    g_main_loop_run(pipeline_info->loop);

    /* Cleanup */
    log_info("Closing pipeline");
    gst_element_set_state(pipeline_info->pipeline, GST_STATE_NULL); /* Resources get freed automatically after this call. */
    gst_object_unref(pipeline_info->pipeline);

    memset(pipeline_info, 0, sizeof(gst_pipeline_t));
    return 0;
}

int pl_camera_pipeline_run(pl_user_data_t *const user_data)
{
    pipeline_main.user_data = user_data;
    if (common_pipeline_init(&pipeline_main, pipeline_camera_def) != 0)
    {
        log_error("Failed to perform common pipeline initialization for main pipeline");
        return -1;
    }

    /* Set up an appsink to pass frames to a user callback */
    GstElement *appsink = gst_bin_get_by_name((GstBin *)pipeline_main.pipeline, "appsink");
    g_object_set(appsink, "emit-signals", TRUE, NULL);
    g_signal_connect(appsink, "new-sample", (GCallback)handle_new_sample, &pipeline_main);

    if (common_pipeline_start_and_cleanup(&pipeline_main) != 0)
    {
        log_error("Failed to start and/or cleanup main pipeline");
        return -1;
    }
    return 0;
}

int pl_proc_pipeline_run(pl_user_data_t *const user_data)
{
    pipeline_main.user_data = user_data;
    if (common_pipeline_init(&pipeline_main, pipeline_camera_sim_def) != 0)
    {
        log_error("Failed to perform common pipeline initialization for main pipeline");
        return -1;
    }

    /* Set up an appsink to pass frames to a user callback */
    GstElement *appsink = gst_bin_get_by_name((GstBin *)pipeline_main.pipeline, "appsink");
    g_object_set(appsink, "emit-signals", TRUE, NULL);
    g_signal_connect(appsink, "new-sample", (GCallback)handle_new_sample, &pipeline_main);

    /* Set up an appsrc to inject frame into pipeline from simulator-written shmem. */
    pipeline_main.app_source = gst_bin_get_by_name((GstBin *)pipeline_main.pipeline, "appsrc");
    g_object_set(pipeline_main.app_source, "emit-signals", TRUE, NULL);
    g_signal_connect(pipeline_main.app_source, "need-data", G_CALLBACK(start_feed), &pipeline_main);
    g_signal_connect(pipeline_main.app_source, "enough-data", G_CALLBACK(stop_feed), &pipeline_main);

    if (common_pipeline_start_and_cleanup(&pipeline_main) != 0)
    {
        log_error("Failed to start and/or cleanup main pipeline");
        return -1;
    }
    return 0;
}

int pl_display_pipeline_run(pl_user_data_t *const user_data)
{
    pipeline_display.user_data = user_data;
    if (common_pipeline_init(&pipeline_display, pipeline_display_def) != 0)
    {
        log_error("Failed to perform common pipeline initialization for display pipeline");
        return -1;
    }

    /* Set up an appsrc to inject frame into pipeline from output of simulator camera pipeline. */
    pipeline_display.app_source = gst_bin_get_by_name((GstBin *)pipeline_display.pipeline, "appsrc");
    g_object_set(pipeline_display.app_source, "emit-signals", TRUE, NULL);
    g_signal_connect(pipeline_display.app_source, "need-data", G_CALLBACK(start_feed), &pipeline_display);
    g_signal_connect(pipeline_display.app_source, "enough-data", G_CALLBACK(stop_feed), &pipeline_display);

    if (common_pipeline_start_and_cleanup(&pipeline_display) != 0)
    {
        log_error("Failed to start and/or cleanup display pipeline");
        return -1;
    }
    return 0;
}
