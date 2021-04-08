#include <stdio.h>
#include <stdlib.h>

#include <string.h>

#include "tco_libd.h"
#include "tco_shmem.h"

#include "pipeline_mgr.h"
#include "pre_proc.h"
#include "planner.h"
#include "draw.h"

const int log_level = LOG_INFO | LOG_ERROR | LOG_DEBUG;
const int draw_enabled = 1;

void usage()
{
  printf("Usage: ./tco_pland.bin <[--proc-test | -pr] | [--proc-real | -pr] | [--camera | -c] | [--help | -h]>\n"
         "'-pt': Runs the processing pipeline and shows the debug window with procesessed frames\n"
         "'-pr': Runs the processing pipeline without the debug window. This is the one that should be running on the target board.\n"
         "'-c': Runs the camera reading pipeline.");
}

void user_proc_func(uint8_t (*pixels)[TCO_FRAME_HEIGHT][TCO_FRAME_WIDTH], int length, void *args)
{
  pre_proc(pixels);
  plnr_step(pixels);
  draw_run(pixels);
}

int user_deinit()
{
  return plnr_deinit();
}

int main(int argc, char *argv[])
{
  if (log_init("pland", "./log.txt") != 0)
  {
    printf("Failed to initialize the logger\n");
    return EXIT_FAILURE;
  }

  if (plnr_init() != 0)
  {
    log_error("Failed to init planner");
    return EXIT_FAILURE;
  }

  if (argc == 2 && (strcmp(argv[1], "--proc-test") == 0 || strcmp(argv[1], "-pt") == 0))
  {
    return pl_mgr_run(1, 0, &user_proc_func, NULL, &user_deinit);
  }
  else if (argc == 2 && (strcmp(argv[1], "--proc-real") == 0 || strcmp(argv[1], "-pr") == 0))
  {
    return pl_mgr_run(0, 0, &user_proc_func, NULL, &user_deinit);
  }
  else if (argc == 2 && (strcmp(argv[1], "--camera") == 0 || strcmp(argv[1], "-c") == 0))
  {
    return pl_mgr_run(0, 1, NULL, NULL, NULL);
  }
  else
  {
    user_deinit();
    usage();
  }
}
