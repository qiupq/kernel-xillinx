#!/bin/bash
mkdir -p ../Out
export KBUILD_OUTPUT=../Out
make ARCH=arm CROSS_COMPILE=arm-xilinx-linux-gnueabi- zynq_zturn_defconfig
