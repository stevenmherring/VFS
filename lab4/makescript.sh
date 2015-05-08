#!/bin/bash

clear
make all
sudo insmod ./wolfs.ko
sudo mount -t wolfs wolfs foo
echo "Wolfs Mounted, you probably want to do ./wolf-add foo bar"

