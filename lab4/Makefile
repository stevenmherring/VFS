ifdef M
	include $(M)/.mkinclude
else
	include $(PWD)/.mkinclude
endif

obj-m += wolfs.o

wolfs-y := wolfs_super.o 

all:	wolf-add wolf-rm wolf-ls
	make -C /lib/modules/$(KDIR)/build M=$(PWD) modules
clean:
	rm wolf-add wolf-rm wolf-ls
	make -C /lib/modules/$(KDIR)/build M=$(PWD) clean

wolf-add: wolf-add.c
	gcc -o wolf-add wolf-add.c

wolf-rm: wolf-rm.c
	gcc -o wolf-rm wolf-rm.c

wolf-ls: wolf-ls.c
	gcc -o wolf-ls wolf-ls.c
