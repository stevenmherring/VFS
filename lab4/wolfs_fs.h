#ifndef WOLFS_FS_H
#define WOLFS_FS_H

#include <linux/list.h>
#include <asm/stat.h>
#include <linux/fs.h>
#include <linux/page-flags.h>
#include <linux/mm_types.h>
#include <linux/fscache.h>
#include "wolfs.h"

int init_wolfs_fs(void);
void exit_wolfs_fs(void);

/* if you want to change block size in wolfs,
 * modify WOLFS_BSTORE_BLOCKSIZE_BITS */

#define WOLFS_BSTORE_BLOCKSIZE_BITS		12
#define WOLFS_BSTORE_BLOCKSIZE			\
		(1 << (WOLFS_BSTORE_BLOCKSIZE_BITS))
#define block_get_num_by_position(pos)		\
		((pos) >> (WOLFS_BSTORE_BLOCKSIZE_BITS))
#define block_get_offset_by_position(pos)	\
		((pos) & ((WOLFS_BSTORE_BLOCKSIZE) - 1))

#define WOLFS_SUPER_MAGIC 0xF3560
#define WOLFS_ROOT_INO 1

#define TIME_T_TO_TIMESPEC(ts, t) do \
		{ \
			ts.tv_sec = t; \
			ts.tv_nsec = 0; \
		} while (0)

#define TIMESPEC_TO_TIME_T(t, ts) t = ts.tv_sec;

struct wolfs_inode_info {
        /* Put other bookkeeping here */
	struct list_head rename_locked;
	struct inode vfs_inode;
};

static inline struct wolfs_inode_info *WOLFS_I(struct inode *inode)
{
	return container_of(inode, struct wolfs_inode_info, vfs_inode);
}

#define MAX_BLOCK_NUM 0xFFFFFFFFFFFFFFFF
#define WOLFS_BSTORE_NOTFOUND	-1
#define WOLFS_BSTORE_NOMEM	-2

struct wolfs_metadata {
	struct stat st;
};
#define WOLFS_METADATA_SIZE (sizeof(struct wolfs_metadata))

typedef int (*filler_fn)(uint64_t block_num, void *block, void *extra);
typedef int (*bstore_meta_scan_callback_fn)(const char *name,
	void *meta, void *extra);

#define BSTORE_SCAN_CONTINUE	1
#define BSTORE_SCAN_NEXT_TIME	2

#define FTIO_MAX_INLINE 128
/* we reuse struct bio_vec because it is convenient */
struct ftio {
	//unsigned long ft_flags;
	//unsigned long ft_rw;
	unsigned short ft_max_vecs; /* maximum number of vecs */
	unsigned short ft_vcnt; /* how many fi_vecs populated */
	unsigned short ft_bvidx; /* current index into fi_vec */
	struct bio_vec *ft_bio_vec;
	struct bio_vec ft_inline_vecs[0];
};
static inline struct bio_vec *current_ftio_bvec(struct ftio *ftio)
{
	BUG_ON(ftio->ft_bvidx >= ftio->ft_vcnt);
	return ftio->ft_bio_vec + ftio->ft_bvidx;
}
static inline struct page *current_ftio_page(struct ftio *ftio)
{
	BUG_ON(ftio->ft_bvidx >= ftio->ft_vcnt);
	return (ftio->ft_bio_vec + ftio->ft_bvidx)->bv_page;
}

static inline void advance_ftio_page(struct ftio *ftio)
{
	BUG_ON(ftio->ft_bvidx >= ftio->ft_vcnt);
	ftio->ft_bvidx++;
}

static inline void unlock_ftio_pages(struct ftio *ftio)
{
	unsigned i;
	for (i = 0; i < ftio->ft_vcnt; i++)
		unlock_page((ftio->ft_bio_vec + i)->bv_page);
}

static inline void set_ftio_pages_uptodate(struct ftio *ftio)
{
	unsigned i;
	for (i = 0; i < ftio->ft_vcnt; i++)
		SetPageUptodate((ftio->ft_bio_vec + i)->bv_page);
}


static inline void set_ftio_pages_error(struct ftio *ftio)
{
	unsigned i;
	for (i = 0; i < ftio->ft_vcnt; i++) {
		struct page *page = (ftio->ft_bio_vec + i)->bv_page;
		ClearPageUptodate(page);
		SetPageError(page);
	}
}


static inline void advance_ftio_bytes(struct ftio *ftio, size_t bytes)
{
	struct bio_vec *bvec = current_ftio_bvec(ftio);
	bvec->bv_offset += bytes;
	BUG_ON(bvec->bv_offset > bvec->bv_len);
	if (bvec->bv_offset == bvec->bv_len)
		advance_ftio_page(ftio);
}

struct scan_pages_info {
	filler_fn filler;
	uint64_t prefetch_block_num; /* prefetch up to this block */
	struct ftio *ftio;
	int do_continue;
};


int bstore_fill_block(uint64_t current_block_num, void *block_buf, void *extra);

struct directory_is_empty_info {
	const char *dirname;
	int saw_child;
};

#endif /* WOLFS_FS_H */
