#!/bin/bash

export KBUILD_OUTPUT=../Out
echo "----------------------boot------------------------------" | tee -a ${KBUILD_OUTPUT}/Buildbootimg.log

date | tee -a ${KBUILD_OUTPUT}/Buildbootimg.log
make ARCH=arm CROSS_COMPILE=arm-xilinx-linux-gnueabi- uImage | tee -a ${KBUILD_OUTPUT}/Buildbootimg.log
#cp /home/qiuqp/code/linux-xlnx/arch/arm/boot/uImage /home/qiuqp/Shared/zturn/uImage
