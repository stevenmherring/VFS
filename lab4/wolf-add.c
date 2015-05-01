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
  char *dest = ".";

  if (argc < 2) {
    printf("Need an argument\n");
    // target is the file or directory to add
    // destination is the wolfs mount point the target is added to
    printf("Usage: wolf-add target [destination] \n");
    return -1;
  }
  
  if (argc > 2) 
    dest = argv[2];

  fd = open(dest, O_DIRECTORY);

  if(fd < 0) {
    printf("Failed to open target dir %s\n", dest);
    return fd;
  }

  arg.len = strlen(argv[1]);
  arg.buf = argv[1];

  ret = ioctl(fd, WOLFS_ADD, &arg);

  if(ret != 0) 
    printf("Something went wrong with the ioctl %d\n", ret);

  return ret;
}
