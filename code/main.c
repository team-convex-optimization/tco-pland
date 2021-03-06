#include <stdio.h>
#include <stdlib.h>

#include <string.h>

#include "tco_libd.h"
#include "tco_shmem.h"
#include "compute.h"

int log_level = LOG_INFO | LOG_ERROR | LOG_DEBUG;

void proc_func(uint8_t *pixels, int length, void *args)
{
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
    return compute_run(0, &proc_func, NULL);
  }
  else
  {
    return compute_run(1, &proc_func, NULL);
  }
}
