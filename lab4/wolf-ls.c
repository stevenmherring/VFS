#include "wolfs_user.h"
#include <stdio.h>
#include <sys/ioctl.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

int main(int argc, char **argv) {
  
  int ret = 0;
 
  int fd = open(".", O_DIRECTORY);
  if(fd < 0) {
    printf("Failed to open the current dir\n");
    return fd;
  }

  ret = ioctl(fd, WOLFS_LS, NULL);

  if(ret != 0) 
    printf("Something went wrong with the ioctl %d\n", ret);

  return ret;
}
