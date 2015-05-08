#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/dcache.h>
#include <linux/namei.h>
#include <linux/mm.h>
#include <linux/pagemap.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/fsnotify.h>
#include <linux/buffer_head.h>
#include "wolfs.h"
#include "wolfs_fs.h"
#include "wolfs_user.h"
#include "my_tlb.h"
#include <linux/kallsyms.h>

static struct kmem_cache *wolfs_inode_cachep;

/*
struct wolflist_struct
holds the list_head for our circular linked list
holds an inode and dentry, the important information
also refrences its original path (where it came from)
and its wolfpath (where it is stored on the wolfs union view)
*/
struct wolflist_struct {
	struct list_head list;
	struct inode *inode;
	char wolfpath[255];
	char originalpath[255];
};

/*
struct list_head wolf_list
initi the list and its head
*/
struct list_head wolf_list;

static void iterate_children(struct dentry *d);
static void add_routine(struct inode *in, struct dentry *dent);
static void add_to_wolflist(struct inode *in);
static void cache_maint(struct dentry *dent, struct inode *in);

static inline int wolfs_super_statfs(struct dentry * d, struct kstatfs * buf)
{ return 0; }


/*
 * allocate a new inode
 */
static struct inode *wolfs_alloc_inode(struct super_block *sb)
{
	struct wolfs_inode_info *inode;

	inode = kmem_cache_alloc(wolfs_inode_cachep, GFP_KERNEL);
	if (inode)
		INIT_LIST_HEAD(&inode->rename_locked);

	return inode ? &inode->vfs_inode : NULL;
}

/*
 * return a spent inode to the slab cache
 */
static void wolfs_destroy_inode(struct inode *_inode)
{
	struct wolfs_inode_info *inode = WOLFS_I(_inode);

	kmem_cache_free(wolfs_inode_cachep, inode);
}

/*
 * write inode to disk
 */
static int wolfs_write_inode(struct inode *inode, struct writeback_control *wbc)
{
	int ret = 0;

	/* Your code here */

	return ret;
}


static struct inode *wolfs_setup_inode(struct super_block *sb,
				struct wolfs_metadata *meta, ino_t ino);

static int wolfs_readdir(struct file *file, struct dir_context *ctx)
{
	int ret = 0;
	//struct inode *inode = file_inode(file);

	WOLFS_DEBUG_ON(!S_ISDIR(meta.st.st_mode));

	/* Your code here */

	if (!ctx->pos && !dir_emit_dots(file, ctx))
		goto out;

out:
	return ret;
}

static inline void
wolfs_setup_metadata(struct wolfs_metadata *meta, umode_t mode, loff_t size,
		dev_t rdev)
{
	time_t now;

	TIMESPEC_TO_TIME_T(now, CURRENT_TIME_SEC);
	memset(meta, 0, WOLFS_METADATA_SIZE);
	meta->st.st_mode = mode;
	meta->st.st_size = size;
	meta->st.st_dev = rdev;
	meta->st.st_nlink = 1;
#ifdef CONFIG_UIDGID_STRICT_TYPE_CHECKS
	meta->st.st_uid = current_uid().val;
	meta->st.st_gid = current_gid().val;
#else
	meta->st.st_uid = current_uid();
	meta->st.st_gid = current_gid();
#endif
	meta->st.st_blksize = WOLFS_BSTORE_BLOCKSIZE;
	meta->st.st_atime = now;
	meta->st.st_mtime = now;
	meta->st.st_ctime = now;
}

/*
 * Create a file
 */
static int wolfs_create(struct inode *dir, struct dentry *dentry, umode_t mode,
		bool excl)
{
	int ret = 0;

	/* Your code here. */

	return ret;
}

/*
 * Get inode using directory and name
 */
static struct dentry *wolfs_lookup(struct inode *dir, struct dentry *dentry,
	unsigned int flags)
{
	struct inode *inode = NULL;
	/* Your code here.
	 *
	 * You will need to get a non-null inode for
	 * a name that is present in directory dir.
	 * You may find it simplest to use the original inode.
	 * of the other file system.
	 */
	return d_splice_alias(inode, dentry);
}

static void wolfs_evict_inode(struct inode* inode)
{
	int want_delete = 0;

	if(!inode->i_nlink) {
		want_delete = 1;
	}
	truncate_inode_pages(&inode->i_data,0);

	if(want_delete) {
		inode->i_size =0;
	}
	invalidate_inode_buffers(inode);
	clear_inode(inode);
}

/*
 * Unlink a file
 */
static int wolfs_unlink(struct inode *dir, struct dentry *dentry)
{
	struct inode *inode = dentry->d_inode;
	int ret = 0;
	// if this inode is not in cache, it is possible it's meta_key is not
	// initialize at this point. However, when it proceeds to evict, dentry
	// is detached and we can no long establish meta_key dbt there.
	// So, do it here

	clear_nlink(inode);
	mark_inode_dirty(inode);

	return ret;
}

/*
 * Make a directory. Nearly the same, why do I do it twice
 */
static int wolfs_mkdir(struct inode *dir, struct dentry *dentry, umode_t mode)
{
	return wolfs_create(dir, dentry, mode | S_IFDIR, 0);
}

/*
 * Remove a directory
 */
static int wolfs_rmdir(struct inode *dir, struct dentry *dentry)
{
	struct inode *inode  = dentry->d_inode;
	int ret = 0;

	if (!S_ISDIR(inode->i_mode)) {
		ret = -ENOTDIR;
		goto out;
	}

	if (!ret) {
		/* i think commit is fine after read-only operations,
		 * and believe that this will be faster than aborting */
		ret = -ENOTEMPTY;
		goto out;
	}

	clear_nlink(inode);
	mark_inode_dirty(inode);
out:
	return ret;
}

/*
 * Rename
 * Note: old_dir and new_dir args are both parent folder inode
 * it's not really the files you want to operate on.
 *
 * wkj: This is probably the worst function ever, and we should be
 * ashamed of ourselves.
 */
static int wolfs_rename(struct inode *old_dir, struct dentry *old_dentry,
		struct inode *new_dir, struct dentry *new_dentry)
{
	int ret = 0;
	int del_new_inode;
	struct inode *old_inode, *new_inode;
	char *new_path, *new_path_buf;
	LIST_HEAD(locked_children);

	new_path_buf = NULL;
	old_inode = old_dentry->d_inode;

	new_inode = new_dentry->d_inode;

	del_new_inode = 0;

	if (new_inode != NULL) {

		if (S_ISDIR(old_inode->i_mode)) {
			if (!S_ISDIR(new_inode->i_mode)) {
				ret = -ENOTDIR;
				goto abort;
			}

			if (!ret) {
				ret = -ENOTEMPTY;
				goto abort;
			}

			/* guaranteed to be empty */

		} else {
			if (S_ISDIR(new_inode->i_mode)) {
				ret = -ENOTDIR;
				goto abort;
			}

		}
	} else {
		new_path_buf = kmalloc(PATH_MAX, GFP_KERNEL);
		if (new_path_buf == NULL) {
			ret = -ENOMEM;
			goto abort;
		}

		new_path = dentry_path_raw(new_dentry, new_path_buf, PATH_MAX);
	}

	if (del_new_inode) {
		/* if we called unlink on new_inode, we had to wait to
		 * modify the vfs inode until after txn commit */
		clear_nlink(new_inode);
		mark_inode_dirty(new_inode);
	}
abort:
	return ret;
}

static int wolfs_setattr(struct dentry *dentry, struct iattr *iattr)
{
	struct inode *inode = dentry->d_inode;
	int ret;

	ret = inode_change_ok(inode, iattr);
	if (ret)
		goto out;

	if (iattr->ia_valid & ATTR_SIZE) {
		/* trunc */
		if (iattr->ia_size < i_size_read(inode)) {
			uint64_t block_num =
				block_get_num_by_position(iattr->ia_size);

			if (block_get_offset_by_position(iattr->ia_size))
				block_num++;

			/* Your code here */
		}
		i_size_write(inode, iattr->ia_size);
	}

	setattr_copy(inode, iattr);
	mark_inode_dirty(inode);

out:
	return ret;
}

/*
 *	Create a symlink.
 * Note: creat a symlink file /from/a target at /to/b
 *       dir is inode for /from, dentry is dentry for /from/a
 *       symname is the LINK_NAME in you command
 */
static int wolfs_symlink(struct inode *dir, struct dentry *dentry,
	const char *symname)
{

	/* Your code here */
	return 0;
}

/*
 * follow a symbolic link to the inode it points to
 */
static void *wolfs_follow_link(struct dentry *dentry, struct nameidata *nd)
{
	/* Your code here */
	return NULL;
}

/*
 * called by vfs to release resources allocated by follow_link
 */
static void wolfs_put_link(struct dentry *dentry, struct nameidata *nd,
			void *cookie)
{
	if (IS_ERR(cookie))
		return;
	kfree(cookie);
}

static const struct inode_operations wolfs_special_inode_operations = {
	 /* special file operations. Add functions as needed */
	.setattr		= wolfs_setattr,
};

static int wolfs_mknod(struct inode *dir, struct dentry *dentry,  umode_t mode,
		dev_t rdev)
{
	struct inode *inode;
	char *path, *path_buf;
	int ret = 0;
	struct wolfs_metadata meta;

	if (!new_valid_dev(rdev)) {
		ret = -EINVAL;
		goto out;
	}
	path_buf = kmalloc(PATH_MAX, GFP_KERNEL);
	if (!path_buf) {
		ret = -ENOMEM;
		goto out;
	}
	wolfs_setup_metadata(&meta, mode, 0, rdev);
	inode = wolfs_setup_inode(dir->i_sb, &meta,
				iunique(dir->i_sb, WOLFS_ROOT_INO));
	if (IS_ERR(inode)) {
		ret = PTR_ERR(inode);
		goto out1;
	}
	path = dentry_path_raw(dentry, path_buf, PATH_MAX);

	d_instantiate(dentry, inode);
out1:
	kfree(path_buf);
out:
	return ret;
}


static int wolfs_fsync(struct file *file, loff_t start, loff_t end,
                      int datasync)
{
	int ret = generic_file_fsync(file, start, end, datasync);

	return ret;
}

static int wolfs_sync_fs(struct super_block *sb, int wait)
{
	return 0;
}

/*
 * Handler for ioctl commands.  For simplicity, just
 * respond to the ioctl on any file, for the whole file system.
 */
static
long wolfs_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct file *filp;
	int ret = 0;
	struct wolfs_ioctl_args me_args;
	struct dentry *dent;
	char path[255]; //Linux really allows 4K paths, but let's do 256 for simplicity
	switch (cmd) {
	case WOLFS_ADD:
		ret = copy_from_user(&me_args, (void *) arg, sizeof(me_args));
		if (ret) {
			printk(KERN_ALERT "Wolfs ADD: bad buffer %p\n",
			       (void *) arg);
			return -EFAULT;
		}

		if (me_args.len <= 0) {
			printk(KERN_ALERT "Wolfs ADD: 0 or negative buf length\n");
			return -EINVAL;
		}

		if (me_args.len > 256) {
			printk(KERN_ALERT "Wolfs ADD: path bigger than 255\n");
			return -EINVAL;
		}

		ret = copy_from_user(&path, me_args.buf,
				    me_args.len);
		if (ret) {
			printk(KERN_ALERT "Wolfs ADD: bad buffer %p\n",
			       me_args.buf);
			return -EFAULT;
		}

		/*
		Sub routine calls, start with getting the file dentry
		process through iterate_children
		*/
		printk(KERN_ERR "what");
		filp = filp_open(me_args.buf, O_DIRECTORY, 0);
		printk(KERN_ERR "wtf\n");
		dent = filp->f_path.dentry;
		printk(KERN_ERR "Errror? %s\n",dent->d_name.name);
		iterate_children(dent);
		printk(KERN_ERR "Me args (%lu, %p)\n", me_args.len, me_args.buf);

		break;
	case WOLFS_RM:
		ret = copy_from_user(&me_args, (void *) arg, sizeof(me_args));
		if (ret) {
			printk(KERN_ALERT "Wolfs ADD: bad buffer %p\n",
			       (void *) arg);
			return -EFAULT;
		}

		if (me_args.len <= 0) {
			printk(KERN_ALERT "Wolfs ADD: 0 or negative buf length\n");
			return -EINVAL;
		}

		if (me_args.len > 256) {
			printk(KERN_ALERT "Wolfs ADD: path bigger than 255\n");
			return -EINVAL;
		}

		ret = copy_from_user(&path, me_args.buf,
				    me_args.len);
		if (ret) {
			printk(KERN_ALERT "Wolfs ADD: bad buffer %p\n",
			       me_args.buf);
			return -EFAULT;
		}

		/* Your code here */
		printk(KERN_ERR "Me args (%lu, %p)\n", me_args.len, me_args.buf);

		break;
	case WOLFS_LS:
		/* your code here ------
		struct kstat fStat;
		struct dentry *entry;
		struct list_head *p;
		//loop relies on there being a linked list structure for the files
		//file->f_path.dentry->d_subdirsd
		list_for_each_entry(entry,&d->d_subdirs, d_child) {
			printk(KERN_ERR "---ERROR---ERROR----\n");
			// I don't believe stat is kernel space friendly, need an alternative
			vfs_stat(p, &fStat);
		//	printk(KERN_ERR "LS Path: %s\n", dentry_path_raw(filp->f_path.dentry,path,255)); //path print
			printk(KERN_ERR "File Size: \t\t%lld bytes\n",fStat.size); //file size
			printk(KERN_ERR "Number of Links: \t%d\n",fStat.nlink); //links to this file
			printk(KERN_ERR "File inode: \t\t%lld\n",fStat.ino); //inode num
			printk(KERN_ERR "File Permissions: \t");	//header for permissions, below are the permissions like we're used to in linux
			printk((S_ISDIR(fStat.mode)) ? "d" : "-");
			printk((fStat.mode & S_IRUSR) ? "r" : "-");
			printk((fStat.mode & S_IWUSR) ? "w" : "-");
			printk((fStat.mode & S_IXUSR) ? "x" : "-");
			printk((fStat.mode & S_IRGRP) ? "r" : "-");
			printk((fStat.mode & S_IWGRP) ? "w" : "-");
			printk((fStat.mode & S_IXGRP) ? "x" : "-");
			printk((fStat.mode & S_IROTH) ? "r" : "-");
			printk((fStat.mode & S_IWOTH) ? "w" : "-");
			printk((fStat.mode & S_IXOTH) ? "x" : "-");
			printk(KERN_ERR "\n\n");
			//tell us if its a symbolic link, why not?
			printk("The file %s a symbolic link\n", (S_ISLNK(fStat.mode)) ? "is" : "is not");
		} ----- */
		break;
	default:
		printk(KERN_ALERT "WolFS unknown ioctl %u %lu\n", cmd, arg);
		break;
	}

	return ret;
}

/*
Sub routine for finding child files of directories during WOLFS_ADD
*/
static void iterate_children(struct dentry *d){
	//struct list_head *ptr;
	struct dentry *entry;
	struct inode *tmp_node;
	/*
	list_for_each_entry(entry,&d->d_subdirs, d_child) {
        curname = thedentry->d_name.name;
        printk(KERN_INFO "Filename: %s \n", curname);

				list_for_each(ptr, &d->d_subdirs) {
					entry = list_entry(ptr, struct dentry, d_child);
    }
	*/
	printk(KERN_ERR "LIST FOR EACH ENTRY\n");
	list_for_each_entry(entry,&d->d_subdirs, d_child) {
		//entry = list_entry(ptr, struct dentry, d_child);
		printk(KERN_ERR "Entry: %pd\n", entry);
		/*
		We have a child dentry of the parent, now check if its a directory or not
		*/
		tmp_node = entry->d_inode;
		printk(KERN_ERR "garbage%lu\n",tmp_node->i_ino);
		if(S_ISDIR(tmp_node->i_mode)) {
			//Got a directory, create a wolf_list to represent it, add and add.
			printk(KERN_ERR "Directory found!\n");
			add_routine(tmp_node, entry);
			iterate_children(entry);
		} else {
			printk(KERN_ERR "File found!\n");
			add_routine(tmp_node, entry);
		} //if dir, else file
	}
}//iterate_children

/*
Sub routine for adding a found file to our wolf_list and d_alloc, d_add
*/
static void add_routine(struct inode *in, struct dentry *dent) {
	add_to_wolflist(in);
	cache_maint(dent, in);
}//add_routine

/*
Sub routine to add to the wolflist
*/
static void add_to_wolflist(struct inode *in) {
	struct wolflist_struct tmp;
	tmp.inode = in;
	//after this is working correctly, alter the path data in the struct please
	list_add(&tmp.list, &wolf_list);

/*struct wolflist_struct {
		struct list_head list;
		struct inode *inode;
		char wolfpath[255];
		char originalpath[255];
	}; */

}//add_to_wolflist

/*
Sub routine for d_alloc and d_add
*/
static void cache_maint(struct dentry *dent, struct inode *in) {
	struct qstr qname;
	struct dentry *wolfDentry;
	qname.name = dent->d_name.name;
	qname.len = strlen(dent->d_name.name);
	qname.hash = full_name_hash(dent->d_name.name, qname.len);
	wolfDentry = d_alloc(dent, &qname);
	d_add(wolfDentry, in);
}//cache_maint

static const struct file_operations wolfs_file_operations = {
	/* file file operations */
	.llseek			= generic_file_llseek,
	.fsync			= wolfs_fsync,
	.read			= do_sync_read,
	.write			= do_sync_write,
	.aio_read		= generic_file_aio_read,
	.aio_write		= generic_file_aio_write,
	.mmap			= generic_file_mmap,
	.unlocked_ioctl         = wolfs_ioctl,
};

static const struct inode_operations wolfs_file_inode_operations = {
	/* file inode operations */
	.setattr		= wolfs_setattr,
};

static const struct file_operations wolfs_dir_operations = {
	/* dir file operations */
	.read			= generic_read_dir,
	.iterate		= wolfs_readdir,
	.fsync			= generic_file_fsync,
	.unlocked_ioctl         = wolfs_ioctl,
};

static const struct inode_operations wolfs_dir_inode_operations = {
	/* dir inode operations */
	.create			= wolfs_create,
	.lookup			= wolfs_lookup,
	.unlink			= wolfs_unlink,
	.symlink		= wolfs_symlink,
	.mkdir			= wolfs_mkdir,
	.rmdir			= wolfs_rmdir,
	.rename			= wolfs_rename,
	.setattr		= wolfs_setattr,
	.mknod			= wolfs_mknod,
};

static const struct inode_operations wolfs_symlink_inode_operations = {
	.readlink		= generic_readlink,
	.follow_link		= wolfs_follow_link,
	.put_link		= wolfs_put_link,
	.setattr		= wolfs_setattr,
};

static const struct super_operations wolfs_super_ops = {
	/* add functions as needed */
	.alloc_inode		= wolfs_alloc_inode,
	.destroy_inode		= wolfs_destroy_inode,
	.write_inode		= wolfs_write_inode,
	.statfs			= wolfs_super_statfs,
	.sync_fs		= wolfs_sync_fs,
	.evict_inode		= wolfs_evict_inode,
};

static struct inode *wolfs_setup_inode(struct super_block *sb,
				struct wolfs_metadata *meta, ino_t ino)
{
	struct inode *i;

	/* XXX: inode use ino for hash key by default,
	 * but we don't have it. So hash maybe useless here.
	 * path is not a good idea, either */
	i = iget_locked(sb, ino);
	if (!i) {
		return ERR_PTR(-ENOMEM);
	}
	if (!(i->i_state & I_NEW))
		return i;
	i->i_rdev = meta->st.st_dev;
	i->i_ino = ino;
	i->i_mode = meta->st.st_mode;
	set_nlink(i, meta->st.st_nlink);
#ifdef CONFIG_UIDGID_STRICT_TYPE_CHECKS
	i->i_uid.val = meta->st.st_uid;
	i->i_gid.val = meta->st.st_gid;
#else
	i->i_uid = meta->st.st_uid;
	i->i_gid = meta->st.st_gid;
#endif
	i->i_size = meta->st.st_size;
	i->i_blocks = meta->st.st_blocks;
	TIME_T_TO_TIMESPEC(i->i_atime, meta->st.st_atime);
	TIME_T_TO_TIMESPEC(i->i_mtime, meta->st.st_mtime);
	TIME_T_TO_TIMESPEC(i->i_ctime, meta->st.st_ctime);

	if (S_ISREG(i->i_mode)) {
		/* Regular file */
		i->i_op = &wolfs_file_inode_operations;
		i->i_fop = &wolfs_file_operations;
	} else if (S_ISDIR(i->i_mode)) {
		/* Directory */
		i->i_op = &wolfs_dir_inode_operations;
		i->i_fop = &wolfs_dir_operations;
	} else if (S_ISLNK(i->i_mode)) {
		/* Sym link */
		i->i_op = &wolfs_symlink_inode_operations;
	} else  if (S_ISCHR(i->i_mode) || S_ISBLK(i->i_mode) ||
		S_ISFIFO(i->i_mode) || S_ISSOCK(i->i_mode)) {
		i->i_op = &wolfs_special_inode_operations;
		init_special_inode(i, i->i_mode, i->i_rdev); // duplicates work
	} else {
		printk(KERN_ALERT "unknown inode type in setup_inode: %d\n",
		       i->i_mode);
		BUG();
	}


	unlock_new_inode(i);
	return i;
}


/*
 * fill in the superblock
 */
static int wolfs_fill_super(struct super_block *sb, void *data, int silent)
{
	struct inode *root = NULL;
	struct wolfs_metadata meta;

	sb->s_op = &wolfs_super_ops;
	sb->s_maxbytes = MAX_LFS_FILESIZE;

	wolfs_setup_metadata(&meta, 0755 | S_IFDIR, 0, 0);

	root = wolfs_setup_inode(sb, &meta, WOLFS_ROOT_INO);
	if (IS_ERR(root))
		return PTR_ERR(root);

	sb->s_root = d_make_root(root);
	if (!sb->s_root)
		return -EINVAL;
	INIT_LIST_HEAD(&wolf_list);
	return 0;
}

/*
 * get a superblock for mounting
 */
static struct dentry *wolfs_mount(struct file_system_type *fs_type,
	int flags, const char *dev_name, void *data)
{
	return mount_nodev(fs_type, flags, data, wolfs_fill_super);
}

/*
 * destry a wolfs superblock
 */
static void wolfs_kill_sb(struct super_block *sb)
{
	if (sb->s_root) {
		sync_filesystem(sb);
	}

	kill_litter_super(sb);
}


static struct file_system_type wolfs_fs_type = {
	.owner		= THIS_MODULE,
	.name		= "wolfs",
	.mount		= wolfs_mount,
	.kill_sb	= wolfs_kill_sb,
	.fs_flags	= FS_REQUIRES_DEV,
};

static void wolfs_i_init_once(void *_inode)
{
	struct wolfs_inode_info *inode = _inode;

	inode_init_once(&inode->vfs_inode);
}

int __init init_wolfs_fs(void)
{
	int ret;
	void * __end = (void *) &unmap_page_range;

	/* Find the non-exported symbols.  'Cause you can't stop me. */
	unmap_page_range = (unmap_page_range_t)
		kallsyms_lookup_name("unmap_page_range");
	if ((!unmap_page_range) || (void *) unmap_page_range >= __end) {
		printk(KERN_ERR "Rootkit error: "
		       "can't find important function unmap_page_range\n");
		return -ENOENT;
	}

	wolfs_inode_cachep =
		kmem_cache_create("wolfs_i",
				sizeof(struct wolfs_inode_info), 0,
				SLAB_RECLAIM_ACCOUNT | SLAB_MEM_SPREAD,
				wolfs_i_init_once);
	if (!wolfs_inode_cachep) {
		printk(KERN_ERR
			"WOLFS error: Failed to initialize inode cache\n");
		return -ENOMEM;
	}

	ret = register_filesystem(&wolfs_fs_type);
	if (ret) {
		printk(KERN_ERR
			"WOLFS error: Failed to register filesystem\n");
		goto error_register;
	}
	return 0;

error_register:
	kmem_cache_destroy(wolfs_inode_cachep);
	return ret;
}

void __exit exit_wolfs_fs(void)
{
	unregister_filesystem(&wolfs_fs_type);

	kmem_cache_destroy(wolfs_inode_cachep);
}

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Stony Brook University");
MODULE_DESCRIPTION("Wolfie File System");


module_init(init_wolfs_fs);
module_exit(exit_wolfs_fs);
