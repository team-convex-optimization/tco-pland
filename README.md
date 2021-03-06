# Planning Daemon
Performs path planning by reading sensor data (as provided by sensord) and camera data which it
reads on its own.

## Dependencies
- libglib2.0-dev (also contains libgobject-2.0-dev)
- libgstreamer1.0-dev
- gstreamer1.0-plugins-base (contains 'appsink' and 'appsrc')
- pthread
