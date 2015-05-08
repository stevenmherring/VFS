#!/bin/bash
clear
sudo umount wolfs
sudo rmmod wolfs
make clean
sudo dmesg --clear
echo "Wolfs unloaded and unmounted, run makescript.sh to remake"

