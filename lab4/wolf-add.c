#include "wolfs_user.h"
#include <stdio.h>
#include <sys/ioctl.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

int main(int argc, char **argv) {
  
  int ret = 0;
  struct wolfs_ioctl_args arg;

  if (argc != 2) {
    printf("Need an argument\n");
    return -1;
  }

  int fd = open(".", O_DIRECTORY);
  if(fd < 0) {
    printf("Failed to open the current dir\n");
    return fd;
  }

  arg.len = strlen(argv[1]);
  arg.buf = argv[1];

  ret = ioctl(fd, WOLFS_ADD, &arg);

  if(ret != 0) 
    printf("Something went wrong with the ioctl %d\n", ret);

  return ret;
}
