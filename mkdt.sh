#!/bin/bash
export KBUILD_OUTPUT=../Out
echo "----------------------dts------------------------------" | tee -a ${KBUILD_OUTPUT}/Builddts.log

date | tee -a ${KBUILD_OUTPUT}/Builddts.log
make ARCH=arm CROSS_COMPILE=arm-xilinx-linux-gnueabi- dtbs | tee -a ${KBUILD_OUTPUT}/Builddts.log
#cp /home/qiuqp/code/linux-xlnx/arch/arm/boot/dts/zynq-zturn.dtb /home/qiuqp/Shared/zturn/devicetree.dtb

