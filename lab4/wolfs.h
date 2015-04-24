#ifndef _WOLFS_H
#define _WOLFS_H

#  define WOLFS_DEBUG_ON(err) 
static inline void wolfs_error (const char * function, const char * fmt, ...) {}
static inline void wolfs_log (const char * function, const char * fmt, ...) {}

#endif
