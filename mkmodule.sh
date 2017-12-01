#!/bin/bash
export KBUILD_OUTPUT=../Out
echo "----------------------boot------------------------------" | tee -a ${KBUILD_OUTPUT}/Buildmodules.log

date | tee -a ${KBUILD_OUTPUT}/Buildmodules.log
make ARCH=arm CROSS_COMPILE=arm-xilinx-linux-gnueabi- modules | tee -a ${KBUILD_OUTPUT}/Buildmodules.log
#cp /home/qiuqp/code/linux-xlnx/drivers/misc/plm-block.ko /home/qiuqp/Shared/zturn/plm-block.ko
