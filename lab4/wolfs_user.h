#ifndef WOLFS_USER_H
#define WOLFS_USER_H

/* Ioctl codes */
#define WOLFS_ADD  _IOW('k', 0, void*)
#define WOLFS_RM   _IOW('k', 1, void*)
#define WOLFS_LS   _IOR('k', 2, void*)

/* Argument Structure for add/remove */
typedef struct wolfs_ioctl_args {
	unsigned long len; 
	unsigned long *buf;
} wolfs_ioctl_args;

#endif
