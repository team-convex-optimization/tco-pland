#include <stdio.h>
#include <stdlib.h>

#include <string.h>

#include "tco_libd.h"
#include "tco_shmem.h"
#include "cam_mgr.h"
#include "segmentation.h"
#include "trajection.h"

int log_level = LOG_INFO | LOG_ERROR | LOG_DEBUG;

void proc_func(uint8_t *pixels, int length, void *args)
{
  segment((uint8_t(*)[TCO_SIM_HEIGHT][TCO_SIM_WIDTH])pixels);
  plot_vector_points((uint8_t(*)[TCO_SIM_HEIGHT][TCO_SIM_WIDTH])pixels);
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
