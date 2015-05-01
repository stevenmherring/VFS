#include "wolfs_user.h"
#include <stdio.h>
#include <sys/ioctl.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

int main(int argc, char **argv) {
  
  int fd, ret = 0;
  struct wolfs_ioctl_args arg;
  char *src = ".";

  if (argc < 2) {
    printf("Need an argument\n");
    // target is the file or directory to add
    // source is the wolfs mount point the target is removed from
    printf("Usage: wolf-rm target [source] \n");
    return -1;
  }

  if (argc > 2) 
    src = argv[2];
  
  fd = open(src, O_DIRECTORY);
  if(fd < 0) {
    printf("Failed to open source %s\n", src);
    return fd;
  }

  arg.len = strlen(argv[1]);
  arg.buf = argv[1];

  ret = ioctl(fd, WOLFS_RM, &arg);

  if(ret != 0) 
    printf("Something went wrong with the ioctl %d\n", ret);

  return ret;
}
