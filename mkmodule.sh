#!/bin/bash
echo "----------------------boot------------------------------" | tee -a Buildmodules.log

date | tee -a Buildmodules.log
make ARCH=arm CROSS_COMPILE=arm-xilinx-linux-gnueabi- modules | tee -a Buildmodules.log
cp /home/qiuqp/code/linux-xlnx/drivers/misc/plm-block.ko /home/qiuqp/Shared/zturn/plm-block.ko
