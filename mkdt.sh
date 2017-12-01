#!/bin/bash
echo "----------------------dts------------------------------" | tee -a Builddts.log

date | tee -a Builddts.log
make ARCH=arm CROSS_COMPILE=arm-xilinx-linux-gnueabi- dtbs | tee -a Builddts.log
cp /home/qiuqp/code/linux-xlnx/arch/arm/boot/dts/zynq-zturn.dtb /home/qiuqp/Shared/zturn/devicetree.dtb

