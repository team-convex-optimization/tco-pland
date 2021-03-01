#include <stdlib.h>
#include <stdio.h>

#include "tco_libd.h"
#include "camera.h"

int log_level = LOG_INFO | LOG_DEBUG | LOG_ERROR;

void pixel_callback(uint8_t *pixels, int length, void *args)
{
  log_debug("Received pixels. Length: %i", length);
}

int main(int argc, char *argv[])
{
  if (log_init("pland", "./log.txt") != 0)
  {
    printf("Failed to initialize the logger\n");
    return EXIT_FAILURE;
  }

  camera_user_data_t user_data = {&pixel_callback, NULL};
  if (camera_pipeline_run(argc, argv, &user_data) != 0)
  {
    log_error("Failed to run the camera pipeline");
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
