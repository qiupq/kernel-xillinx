/*
 * zturn_eds gpio driver for xps/axi_gpio IP.
 *
 * Copyright 2008 - 2013 zturn_eds, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */



#include <linux/of.h>
#include <linux/types.h>
#include <linux/sizes.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/sched.h>
#include <linux/init.h>
#include <linux/cdev.h>
#include <linux/stat.h>

#include <linux/device.h>

#include <linux/mutex.h>

#include <linux/uaccess.h>
#include <linux/dma-mapping.h>
#include <linux/ioport.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/string.h>
#include <linux/sysctl.h>

#include <linux/bitops.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/of_platform.h>
#include <linux/of_gpio.h>
#include <linux/io.h>
#include <linux/slab.h>


#ifndef PLM_ZTURN_H
#define PLM_ZTURN_H



//#define DEBUG_PLM_ZTURN

#define TEST_PLM_ZTURN
#define DT_PLM
#define PLM_ZTURN_NAME "zturnplm"
#define GLOBALMEM_SIZE 1024	/* size 128 ~512 */

static int char_major = 0;


/**
 * struct zynq_plm_char_device - Device Configuration driver structure
 *
 * @dev: Pointer to the device structure

 * @base_address: The virtual device base address of the device registers
 * @is_partial_bitstream: Status bit to indicate partial/full bitstream
 */struct zynq_plm_char_device {
 
	struct device *dev;
	struct cdev	cdev;
	dev_t	plmcdevt;
	struct class *plm_class;
	char  mem[GLOBALMEM_SIZE];
	void __iomem *base_address;
	resource_size_t res_start_addr;
	unsigned int phy_size;
	
	struct mutex mutex;
	spinlock_t lock;
	bool is_open;
};

struct zynq_plm_char_device *g_char_dev;
static unsigned int value[GLOBALMEM_SIZE];

#ifdef DEBUG_PLM_ZTURN
#define plm_dbg(fmt, args...) \
	pr_err("%s:%d: " fmt "\n", __func__, __LINE__, ##args)

#define plm_pr_info(fmt, args...) printk(KERN_INFO "%s: " fmt "\n", __func__ , ## args)

#define plm_pr_err(fmt, args...) \
	pr_err("%s:%d: " fmt "\n", __func__, __LINE__, ##args)


#else
#define plm_dbg(fmt, args...)	
#define plm_pr_info(fmt, args...) printk(KERN_INFO "%s: " fmt "\n", __func__ , ## args)

#define plm_pr_err(fmt, args...) \
	pr_err("%s:%d: " fmt "\n", __func__, __LINE__, ##args)

#endif

/* Register Offset Definitions */

/* Read/Write access to the PLM registers */

#define plm_ioread(offset)		    __raw_readl(offset)
#define plm_iowrite(offset, val)	__raw_writel(val, offset)

#endif /* end if PLM_ZTURN_H*/

static int zynq_plm_char_open(struct inode *inode, struct file *file)
{
	int status = 0;
	
	if(NULL == g_char_dev)
	{
		status = -EFAULT;
		plm_dbg("ERROR=%d",status);
		goto err;
	}
	status = mutex_lock_interruptible(&g_char_dev->mutex);
	if (status)
	{
		plm_dbg("ERROR=%d",status);
		goto err_out;
	}

	if (g_char_dev->is_open) {
		status = -EBUSY;
		plm_dbg("ERROR=%d",status);
		goto err_out;
	}

	g_char_dev->is_open = 1;
	file->private_data = g_char_dev;

	plm_dbg();
err_out:
	mutex_unlock(&g_char_dev->mutex);
err:
	return status;
}

static int zynq_plm_char_release(struct inode *inode, struct file *file)
{
	if(NULL == g_char_dev)
	{
		plm_dbg("release ERROR");
		return -EFAULT;
	}
	else
	{
		g_char_dev->is_open = 0;
	}
	
	plm_dbg();

	return 0;
}
/**
 *	
 *   zynq_plm_llseek() -seek() method for sequential files.
 *	@file: the file in question
 *	@offset: new position
 *	@whence: 0 for absolute, 1 for relative position
 *
 *	Ready-made ->f_op->llseek()
 */

loff_t zynq_plm_llseek(struct file *file, loff_t offset, int whence)
{
	//struct seq_file *m = file->private_data;
	loff_t retval = 0;
	plm_dbg("start,offset=%d ,whence =%d",(int)offset,whence);
	if(NULL == g_char_dev)
	{
		retval = -EINVAL;
		goto err_out;
	}
	mutex_init(&g_char_dev->mutex);
	if((file->f_pos + offset ) > g_char_dev->phy_size ||
	   (file->f_pos + offset ) < 0 ||
	   offset < 0)
	{
		retval = -EINVAL;
		goto out;
	}

	switch (whence) {
	case SEEK_CUR:/* if 1 */
		file->f_pos += offset;
		retval = file->f_pos;
		break;
		
	case SEEK_SET:	/* if 0 */
		file->f_pos = offset;
		retval = file->f_pos;
		break;

	default:
		retval = -EINVAL;
	}

out:
	mutex_unlock(&g_char_dev->mutex);
err_out:	
	plm_dbg("retval =%d\n",(int)retval);
	return retval;

}

/**
 * zynq_plm_char_read() - The is the driver read function.
 * @file:	Pointer to the file structure.
 * @buf:	Pointer to the bitstream location.
 * @size:	The number of bytes read.
 * @ppos:	Pointer to the offsetvalue
 * returns:	Success or error status.
 */
static ssize_t zynq_plm_char_read(struct file *filp, char __user *buf, size_t size, loff_t *ppos)
{
	int ret = 0;
	unsigned long off = *ppos;
	unsigned int count = size;
	unsigned int  data;
	
	char temp[8];
	mutex_init(&g_char_dev->mutex);
	plm_dbg("read count = %u ppos=%ld",count,off);

	if(off >= GLOBALMEM_SIZE)
	{
		ret = -EFAULT;
		return ret;

	}
	if( count > GLOBALMEM_SIZE - off )
	{
		count = GLOBALMEM_SIZE - off;
	}
	/*  read one address */


	data = plm_ioread(g_char_dev->base_address + off*4);
	sprintf(temp, "%08X", data);
	plm_dbg("baseaddr off =%u  value[i] =%#x", (unsigned int)(off*4), data);

#if 0	
	static void tsunami_flash_copy_from(struct map_info *map,
	void 			*addr, 
	unsigned long 	offset, 
	ssize_t 		len)
	{
		unsigned char *dest;
		dest = addr;
		while(len && (offset < MAX_TIG_FLASH_SIZE)) {
			*dest = tsunami_tig_readb(offset);
			offset++;
			dest++;
			len--;
		}
	}


	/*  read mass address */
	
	memset(value, 0, sizeof(value[GLOBALMEM_SIZE/2]));
	for(i=0; i < count  ;i++  )
	{
		value[i] = plm_ioread(g_char_dev->base_address + (off+i)*4);
		
		plm_pr_err("baseaddr off =%u  value[i] =%#x,=%u\n", (unsigned int)(off+i+1), value[i],value[i]);
	}
	
#endif
	if(copy_to_user(buf, (void *)(temp), sizeof(temp)))
	{
		ret = -EFAULT;
	}
	else
	{
		*ppos += count;
		ret = count;
		
		plm_dbg("success read %u bytes",count);
	
	}

	mutex_unlock(&g_char_dev->mutex);
	return ret;	

}

/**
 * zynq_plm_char_write() - This function is  driver write function.
 *
 * @file:	Pointer to the file structure.
 * @buf:	Pointer to the bitstream location.
 * @count:	The number of bytes to be written.
 * @ppos:	Pointer to the offset value
 * returns:	Success or error status.
 **/

static ssize_t zynq_plm_char_write(struct file *filp, const char __user *buf, size_t size, loff_t *ppos)
{
	unsigned int count = size;
	int ret = 0 ,i = 0, k = 0;
	unsigned long off = *ppos;
	char *d, *t;

	mutex_init(&g_char_dev->mutex);

	if(off >= g_char_dev->phy_size)
	{
		ret = -EFAULT;
	}
	
	if(!ret && (count > g_char_dev->phy_size) )
	{
		count = g_char_dev->phy_size - off;
	}
	
	memset(g_char_dev->mem, 0, sizeof(g_char_dev->mem));
	memset(value, 0, sizeof(value[GLOBALMEM_SIZE]));
	
	plm_dbg("write count = %u ,ppos=%ld,mem-size=%u ",count,off,sizeof(g_char_dev->mem));
	/* copy data from user space */
	if(!ret && !(copy_from_user(g_char_dev->mem, buf, count)))
	{
		t = g_char_dev->mem;
		
		for( i =0 ; i < count  ; i++ )
		{
			d = strsep(&t,";");
			if(unlikely(!d || !*d))
			{
				plm_dbg("for_break");
				break;
			}
			else
			{
				plm_dbg("%d is %s\n",i+1,d);
				
				if (unlikely(value[i] < 0 || *d == '\012') )
				{
					plm_dbg("for_break");
					break;
				}
				
				value[i] = simple_strtoul(d, NULL, 0);
				plm_dbg("value[%d]=%d\n",i+1,value[i]);
				
			}
		}
		ret = count;


	}
	else
	{
		ret = -EFAULT;
	}
	
	plm_dbg("write_count-i=%d\n",i);

	for(k = 0; k < i  ; k++  )
	{
		plm_iowrite(g_char_dev->base_address + (off+k) * 4 , value[k]);
	}
	
	plm_dbg("ret =%d\n",ret);
	mutex_unlock(&g_char_dev->mutex);

	return ret;
}
static int zynq_plm_data_mmap(struct file *filp, struct vm_area_struct *vma)
{
	unsigned long offset, vsize, psize, addr ;
	int ret;
	/* VMA properties */
	offset = vma->vm_pgoff << PAGE_SHIFT;
	vsize = vma->vm_end - vma->vm_start;
	psize = g_char_dev->phy_size - offset;
	addr = ((unsigned long)g_char_dev->res_start_addr + offset) >> PAGE_SHIFT;

	/* Check against the FPGA region's physical memory size */
	if (vsize > psize) {
		plm_pr_err( "requested mmap mapping too large\n");
		return -EINVAL;
	}

	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
	ret=io_remap_pfn_range(vma, vma->vm_start, addr, vsize,
					   vma->vm_page_prot);
	plm_dbg("ret =%d\n",ret);

	return ret;
}

static const struct file_operations zynq_plm_char_fops={
	.owner = THIS_MODULE,
	.open = zynq_plm_char_open,
	.release = zynq_plm_char_release,
	.read = zynq_plm_char_read,
	.write = zynq_plm_char_write,
	.llseek = zynq_plm_llseek,
	.mmap = zynq_plm_data_mmap,

	
};

static int plm_eds_probe(struct platform_device *pdev)
{
	int result = 0;
	dev_t devno;
	struct resource *res;
	
#ifdef DT_PLM	
	struct device_node *np = pdev->dev.of_node;
	const void *prop;
	int size;
#endif
	plm_pr_info("start");
#ifdef DT_PLM	
		prop = of_get_property(np, "compatible", &size);
	
		if (prop != NULL) {
			//plm_dbg("prop =%s\n",(char *)prop);
			if ((strcmp((const char *)prop, "zturn,plm-eds")) == 0)
					plm_dbg("find zturn,plm-eds!!!\n");
			
		}
		else{
			plm_pr_err("fail to of_get_property!");
			goto out4;
		}
		
		//prop = of_get_property(np, "device_type", &size);
		
		//plm_dbg("prop =%s\n",(char *)prop);
#endif

	g_char_dev = devm_kzalloc(&pdev->dev, sizeof(*g_char_dev), GFP_KERNEL);
	if (!g_char_dev)
	{
		plm_pr_err("fail to devm_kzalloc!");
		goto out4;		
	}
	memset(g_char_dev, 0, sizeof(struct zynq_plm_char_device));

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	g_char_dev->base_address = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(g_char_dev->base_address))
	{
		plm_pr_err("fail to set base_address!");
		goto out3;		
	}
	g_char_dev->res_start_addr = res->start;
	g_char_dev->phy_size =	(unsigned int)(res->end - res->start + 1);
	platform_set_drvdata(pdev, g_char_dev);

	spin_lock_init(&g_char_dev->lock);
	mutex_init(&g_char_dev->mutex);

	plm_dbg("ioremap start=%pa end=%pa size=%d to %p\n", 
	&res->start, &res->end, g_char_dev->phy_size, g_char_dev->base_address);


	/* malloc a char device start */
	if(char_major)
	{
		devno =MKDEV(char_major, 0);
		result = register_chrdev_region(devno, 1, PLM_ZTURN_NAME);
	}
	else
	{
		result = alloc_chrdev_region(&devno, 0, 1, PLM_ZTURN_NAME);
		char_major = MAJOR(devno);
	}
	if(result < 0)
	{
		plm_pr_err("ERROR major =%d, result=%d \n",char_major, result);
		goto out3;
	}
	
	g_char_dev->plmcdevt = devno;

	cdev_init(&g_char_dev->cdev, &zynq_plm_char_fops);
	g_char_dev->cdev.owner = THIS_MODULE;
	
	result = cdev_add(&g_char_dev->cdev, devno, 1);
	if(result)
	{
		plm_pr_err("plm: ERROR zynq_plm_char_add_fail,%d\n", result);
		goto out3;
	}

	g_char_dev->plm_class = class_create(THIS_MODULE, PLM_ZTURN_NAME);
	if (IS_ERR(g_char_dev->plm_class)) {
		plm_pr_err("failed to create class\n");
		goto out2;
	}

	g_char_dev->dev = device_create(g_char_dev->plm_class,
				 &pdev->dev,
			     devno,
			     g_char_dev,
			     PLM_ZTURN_NAME);
	if (IS_ERR(g_char_dev->dev)) {
		plm_pr_err("failed to create device\n");
		goto out1;
	}

	
	plm_pr_err("success\n");
	
	return result;
	
out1:
	class_destroy(g_char_dev->plm_class);
out2:
	cdev_del(&g_char_dev->cdev);
	unregister_chrdev_region(g_char_dev->plmcdevt, 1);

out3:	
	kfree(g_char_dev);
out4:

	return result;
}

static struct of_device_id plm_eds_of_match[] = {
	{ .compatible = "zturn,plm-eds", },
	{ /* end of list */ },
};
MODULE_DEVICE_TABLE(of, plm_eds_of_match);

static struct platform_driver plm_eds_driver = {
	.probe = plm_eds_probe,
	.driver = {
		.owner = THIS_MODULE,
		.name = PLM_ZTURN_NAME,
		.of_match_table = plm_eds_of_match,
	},
};



static int __init plm_eds_init(void)
{
	plm_dbg("success\n");
	return platform_driver_register(&plm_eds_driver);
}

static void __exit plm_eds_exit(void)
{
	
	if(NULL != g_char_dev)
	{
	
		plm_dbg("release char device\n");
		if(NULL != g_char_dev->plm_class)
		{
			device_destroy(g_char_dev->plm_class,  MKDEV(char_major, 0));
			class_destroy(g_char_dev->plm_class);
		}
		if(NULL != &g_char_dev->cdev)
			cdev_del(&g_char_dev->cdev);

		unregister_chrdev_region(g_char_dev->plmcdevt, 1);
		kfree(g_char_dev);
	}

	platform_driver_unregister(&plm_eds_driver);
	plm_pr_err("success\n");

}

module_init(plm_eds_init);
module_exit(plm_eds_exit);


MODULE_AUTHOR("zturn_eds, Inc.");
MODULE_DESCRIPTION("zturn_eds PLM driver");
MODULE_LICENSE("GPL");
