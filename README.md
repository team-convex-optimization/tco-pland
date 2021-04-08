# Planning Daemon
Performs path planning by reading sensor data (as provided by sensord) and camera data which it
reads on its own in another instance (i.e. one instance for path planning by reading frames from
state shmem and another for reading camera frames and writing them to state shmem).

## Pipeline Overview
- ```camera pipeline```: Reads frames from the real camera and saves them in state shared memory.
- ```proc pipeline```: This is the processing pipeline which reads frames from state shared memory
  and performs processing to get useful information about where the car should go and how which is
  then written to plan shared memory.
- ```display pipeline```: Displays a window which shows the processed frames. This is only used for
  testing and should never be run on the target.

## Dependencies
- libglib2.0-dev (also contains libgobject-2.0-dev)
- libgstreamer1.0-dev
- gstreamer1.0-plugins-base (contains 'appsink' and 'appsrc')
- gstreamer1.0-plugins-good (contains 'v4l2src')
- pthread
