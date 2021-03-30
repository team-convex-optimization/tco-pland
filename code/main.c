#include <stdio.h>
#include <stdlib.h>

#include <string.h>

#include "tco_libd.h"
#include "tco_shmem.h"

#include "cam_mgr.h"
#include "segmentation.h"
#include "trajection.h"
#include "draw.h"

const int log_level = LOG_INFO | LOG_ERROR | LOG_DEBUG;
const int draw_enabled = 1;

void proc_func(uint8_t (*pixels)[TCO_SIM_HEIGHT][TCO_SIM_WIDTH], int length, void *args)
{
  for (size_t y = 211; y < TCO_SIM_HEIGHT; y++)
  {
    for (size_t x = 0; x < TCO_SIM_WIDTH; x++)
    {
      (*pixels)[y][x] = 0;
    }
  }
  segment(pixels);
  track_distances(pixels, 210);
  // track_center(pixels, 210);
}

int main(int argc, char *argv[])
{
  if (log_init("pland", "./log.txt") != 0)
  {
    printf("Failed to initialize the logger\n");
    return EXIT_FAILURE;
  }

  if (argc == 2 && (strcmp(argv[1], "--sandbox") == 0 || strcmp(argv[1], "-s") == 0))
  {
    return cam_mgr_run(0, &proc_func, NULL);
  }
  else
  {
    return cam_mgr_run(1, &proc_func, NULL);
  }
}
