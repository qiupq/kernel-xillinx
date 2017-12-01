#!/bin/bash
echo "----------------------boot------------------------------" | tee -a Buildbootimg.log

date | tee -a Buildbootimg.log
make ARCH=arm CROSS_COMPILE=arm-xilinx-linux-gnueabi- uImage | tee -a Buildbootimg.log
cp /home/qiuqp/code/linux-xlnx/arch/arm/boot/uImage /home/qiuqp/Shared/zturn/uImage
