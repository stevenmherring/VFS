
/*
This function will iterate wolf_list to find a corresponding inode
Pass in the following flags for different functionality
Flags values;
LS == 0
RM == 1
CHMOD == 2

static int iterate_through_inodelist (struct inode *in, int flags, void *information){

	struct inode *inodePtr;
	struct dentry *lsDentry;
	struct file *lsFile;
	lsFile = (struct file *) information;

	switch(flags){

		case 0:
			list_for_each_entry(lsDentry,&lsFile->f_path.dentry->d_subdirs, d_child) {

					printk(KERN_ERR "Entry: %pd\n", lsDentry);

			}

		return 0;
		case 1:

		return 0;

		case 2:

		return 0;
	}
}*/

static struct qstr fill_qstr_struct (struct dentry *dentToParse){
	struct qstr qname;
	qname.name = dentToParse->d_name.name;
	qname.len = strlen(dentToParse->d_name.name);
	qname.hash = full_name_hash(dentToParse->d_name.name, qname.len);
	return qname;
}
