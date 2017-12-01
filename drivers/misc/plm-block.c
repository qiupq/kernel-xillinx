/*
 * zturn_eds block driver .
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

/*

 * Sample disk driver, from the beginning.

 */



#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/kernel.h>       
#include <linux/slab.h>         /* kmalloc() */
#include <linux/fs.h>           /* everything... */
#include <linux/errno.h>        /* error codes */
#include <linux/timer.h>
#include <linux/types.h>        /* size_t */
#include <linux/fcntl.h>        /* O_ACCMODE */
#include <linux/hdreg.h>        /* HDIO_GETGEO */
#include <linux/kdev_t.h>
#include <linux/vmalloc.h>
#include <linux/genhd.h>
#include <linux/blkdev.h>
#include <linux/buffer_head.h>  /* invalidate_bdev */
#include <linux/bio.h>


#define DEBUG_EDS
//#define TEST1


#ifdef DEBUG_EDS
#define eds_dbg(fmt, args...) \
	pr_err("%s:%d: " fmt "\n", __func__, __LINE__, ##args)

#define eds_pr_info(fmt, args...) printk(KERN_INFO "%s: " fmt "\n", __func__ , ## args)

#define eds_pr_err(fmt, args...) \
	pr_err("%s:%d: " fmt "\n", __func__, __LINE__, ##args)


#else
#define eds_dbg(fmt, args...)	
#define eds_pr_info(fmt, args...) printk(KERN_INFO "%s: " fmt "\n", __func__ , ## args)

#define eds_pr_err(fmt, args...) \
	pr_err("%s:%d: " fmt "\n", __func__, __LINE__, ##args)

#endif


static int vmem_disk_major = 0;

module_param(vmem_disk_major, int, 0);

static int hardsect_size = 512;

module_param(hardsect_size, int, 0);

//static int nsectors = 1024;     /* How big the drive is */

//module_param(nsectors, int, 0);

static int ndevices = 1;

module_param(ndevices, int, 0);

#define RAMDISK_SECTOR_SIZE 512
#define RAMDISK_SECTOR_COUNT 16
#define RAMDISK_HEADS 4
#define RAMDISK_CYLINDERS 256

#define RAMDISK_SECTOR_TOTAL (RAMDISK_SECTOR_COUNT * RAMDISK_HEADS * RAMDISK_CYLINDERS) //16384 sectors
#define RAMDISK_SIZE (hardsect_size * RAMDISK_SECTOR_TOTAL) //8MB = 16384 x 512=8388608=8*1024*1024



/*

 * The different "request modes" we can use.

 */

enum {

	RM_SIMPLE  = 0, /* The extra-simple request function */

	RM_FULL    = 1, /* The full-blown version */

	RM_NOQUEUE = 2, /* Use make_request */

};

static int request_mode = RM_NOQUEUE;

module_param(request_mode, int, 0);



/*

 * Minor number and partition management.

 */

#define vmem_disk_MINORS    16

#define MINOR_SHIFT     4

#define DEVNUM(kdevnum) (MINOR(kdev_t_to_nr(kdevnum)) >> MINOR_SHIFT



	/*

	 * We can tweak our hardware sector size, but the kernel talks to us

	 * in terms of small sectors, always.

	 */

#define KERNEL_SECTOR_SIZE      512



	/*

	 * After this much idle time, the driver will simulate a media change.

	 */

#define INVALIDATE_DELAY        30*HZ



	/*

	 * The internal representation of our device.

	 */

	struct vmem_disk_dev {

	int size;                       /* Device size in sectors */

	u8 *data;                       /* The data array */

	short users;                    /* How many users */

	short media_change;             /* Flag a media change? */

	spinlock_t lock;                /* For mutual exclusion */

	struct request_queue *queue;    /* The device request queue */

	struct gendisk *gd;             /* The gendisk structure */

	struct timer_list timer;        /* For simulated media changes */

	};



static struct vmem_disk_dev *Devices = NULL;


#ifdef TEST1

/*

 * Handle an I/O request.

 */

static void vmem_disk_transfer(struct vmem_disk_dev *dev, unsigned long sector,

	unsigned long nsect, char *buffer, int write)

{

	unsigned long offset = sector*KERNEL_SECTOR_SIZE;

	unsigned long nbytes = nsect*KERNEL_SECTOR_SIZE;

	eds_dbg ( "");


	if ((offset + nbytes) > dev->size) {

		eds_dbg ( "Beyond-end write (%ld %ld)\n", offset, nbytes);

		return;

	}

	if (write)

		memcpy(dev->data + offset, buffer, nbytes);

	else

		memcpy(buffer, dev->data + offset, nbytes);

}
#endif
#if 0 //qiupq


/*

 * The simple form of the request function.

 */

static void vmem_disk_request(struct request_queue *q)

{

	struct request *req;



	while ((req = elv_next_request(q)) != NULL) {

		struct vmem_disk_dev *dev = req->rq_disk->private_data;

		if (! blk_fs_request(req)) {

			eds_dbg ( "Skip non-fs request\n");

			end_request(req, 0);

			continue;

		}



		vmem_disk_transfer(dev, req->sector, req->current_nr_sectors,

			req->buffer, rq_data_dir(req));



		end_request(req, 1);

	}

}

#endif



/*

 * Transfer a single BIO.

 */
#ifdef TEST1

static int vmem_disk_xfer_bio(struct vmem_disk_dev *dev, struct bio *bio)
{
	int i;
	struct bio_vec *bvec;
	sector_t sector = bio->bi_sector;
	/* Do each segment independently. */
	eds_dbg ( "");

	bio_for_each_segment(bvec, bio, i) {
		char *buffer = __bio_kmap_atomic(bio, i, KM_USER0);
		vmem_disk_transfer(dev, sector, bio_cur_sectors(bio),
			buffer, bio_data_dir(bio) == WRITE);
		sector += bio_cur_sectors(bio);
		__bio_kunmap_atomic(bio, KM_USER0);
	}
	return 0; /* Always "succeed" */
}
#endif


/*
 * The direct make request version.
 */
static void vmem_disk_make_request(struct request_queue *q, struct bio *bio)
{

#if 1
		char* pRHdata;
		char* pBuffer;
		struct bio_vec bvec;
		struct bvec_iter i;
		int err = 0;
		
		struct block_device* bdev = bio->bi_bdev;
		struct vmem_disk_dev* pdev = bdev->bd_disk->private_data;
		

		if(((bio->bi_iter.bi_sector * hardsect_size) + bio->bi_iter.bi_size) > RAMDISK_SIZE)
		{
			err = -EIO;
			
			eds_dbg ( "%d",err);
			goto out;
		}
		pRHdata = pdev->data + (bio->bi_iter.bi_sector * hardsect_size);
		bio_for_each_segment(bvec, bio, i){
			pBuffer = kmap(bvec.bv_page) + bvec.bv_offset;
			switch(bio_data_dir(bio)){
				case READ:
					
					
					eds_dbg ( "read");
					memcpy(pBuffer, pRHdata, bvec.bv_len);
					flush_dcache_page(bvec.bv_page);
					break;
					
				case WRITE:
					
					eds_dbg ( "write");
					flush_dcache_page(bvec.bv_page);
					memcpy(pRHdata, pBuffer, bvec.bv_len);
					break;
					
				default:
					
					eds_dbg ( "default");
					kunmap(bvec.bv_page);
					goto out;
			}
			
			kunmap(bvec.bv_page);
			pRHdata += bvec.bv_len;
		}
		
	out:
		bio_endio(bio, err);

#endif
#ifdef TEST1
	//struct vmem_disk_dev *dev = q->queuedata;
	int status = 0;
	status = vmem_disk_xfer_bio(dev, bio);
	bio_endio(bio, status);
#endif

#if 0
	struct block_device *bdev = bio->bi_bdev;
	struct brd_device *brd = bdev->bd_disk->private_data;
	int rw;
	struct bio_vec bvec;
	sector_t sector;
	struct bvec_iter iter;
	int err = -EIO;

	sector = bio->bi_iter.bi_sector;
	if (bio_end_sector(bio) > get_capacity(bdev->bd_disk))
		goto out;

	if (unlikely(bio->bi_rw & REQ_DISCARD)) {
		err = 0;
		discard_from_brd(brd, sector, bio->bi_iter.bi_size);
		goto out;
	}

	rw = bio_rw(bio);
	if (rw == READA)
		rw = READ;

	bio_for_each_segment(bvec, bio, iter) {
		unsigned int len = bvec.bv_len;
		err = vmem_do_bvec(brd, bvec.bv_page, len,
					bvec.bv_offset, rw, sector);
		if (err)
			break;
		sector += len >> SECTOR_SHIFT;
	}

out:
	bio_endio(bio, err);

#endif


}

#if 0 //qiupq

/*

 * Transfer a full request.

 */

static int vmem_disk_xfer_request(struct vmem_disk_dev *dev, struct request *req)

{



	struct req_iterator iter;

	int nsect = 0;

	struct bio_vec *bvec;



	/* Macro rq_for_each_bio is gone.

	 * In most cases one should use rq_for_each_segment.

	 */

	rq_for_each_segment(bvec, req, iter) {

		char *buffer = __bio_kmap_atomic(iter.bio, iter.i, KM_USER0);

		sector_t sector = iter.bio->bi_sector;

		vmem_disk_transfer(dev, sector, bio_cur_sectors(iter.bio),

			buffer, bio_data_dir(iter.bio) == WRITE);

		sector += bio_cur_sectors(iter.bio);

		__bio_kunmap_atomic(iter.bio, KM_USER0);

		nsect += iter.bio->bi_size/KERNEL_SECTOR_SIZE;

	}

	return nsect;

}

#endif

#if 0 //qiupq


/*

 * Smarter request function that "handles clustering".

 */

static void vmem_disk_full_request(struct request_queue *q)

{

	struct request *req;

	int sectors_xferred;

	struct vmem_disk_dev *dev = q->queuedata;



	while ((req = elv_next_request(q)) != NULL) {

		if (! blk_fs_request(req)) {

			eds_dbg ( "Skip non-fs request\n");

			end_request(req, 0);

			continue;

		}

		sectors_xferred = vmem_disk_xfer_request(dev, req);

		end_request (req, 1);

	}

}

#endif






/*

 * Open and close.

 */



static int vmem_disk_open(struct block_device *bdev, fmode_t mode)

{

	struct vmem_disk_dev *dev = bdev->bd_disk->private_data;



	del_timer_sync(&dev->timer);

	spin_lock(&dev->lock);

	dev->users++;

	spin_unlock(&dev->lock);

	eds_dbg ( "users=%d",dev->users);


	return 0;

}



static void vmem_disk_release(struct gendisk *disk, fmode_t mode)
{

	struct vmem_disk_dev *dev = disk->private_data;



	spin_lock(&dev->lock);

	dev->users--;



	if (!dev->users) {
		eds_dbg ( "add-timer");
		dev->timer.expires = jiffies + INVALIDATE_DELAY;
		add_timer(&dev->timer);
	}

	spin_unlock(&dev->lock);

	eds_dbg ( "dev->users=%d",dev->users);

}



/*

 * Look for a (simulated) media change.

 */

int vmem_disk_media_changed(struct gendisk *gd)

{

	struct vmem_disk_dev *dev = gd->private_data;

	eds_dbg ( "");


	return dev->media_change;

}



/*

 * Revalidate.  WE DO NOT TAKE THE LOCK HERE, for fear of deadlocking

 * with open.  That needs to be reevaluated.

 */

int vmem_disk_revalidate(struct gendisk *gd)

{

	struct vmem_disk_dev *dev = gd->private_data;

	eds_dbg ( "dev->media_change=%d",dev->media_change);


	if (dev->media_change) {

		dev->media_change = 0;

		memset (dev->data, 0, dev->size);

	}

	return 0;

}



/*

 * The "invalidate" function runs out of the device timer; it sets

 * a flag to simulate the removal of the media.

 */

void vmem_disk_invalidate(unsigned long ldev)

{

	struct vmem_disk_dev *dev = (struct vmem_disk_dev *) ldev;



	spin_lock(&dev->lock);

	if (dev->users || !dev->data)

		eds_dbg ( "vmem_disk: timer sanity check failed\n");

	else

		dev->media_change = 1;

	spin_unlock(&dev->lock);
	eds_dbg ( "media_change=%d",dev->media_change);

}



/*

 * The ioctl() implementation

 */



static int vmem_disk_getgeo(struct block_device *bdev, struct hd_geometry *geo)

{



	eds_dbg ( "");

	geo->cylinders = RAMDISK_CYLINDERS;

	geo->heads = RAMDISK_HEADS;

	geo->sectors = RAMDISK_SECTOR_COUNT;

	geo->start = get_start_sect(bdev);



	return 0;

}



/*

 * The device operations structure.

 */

static struct block_device_operations vmem_disk_ops = {

	.owner           = THIS_MODULE,

	.open            = vmem_disk_open,

	.release         = vmem_disk_release,

	.media_changed   = vmem_disk_media_changed,

	.revalidate_disk = vmem_disk_revalidate,

	.getgeo          = vmem_disk_getgeo,

};





/*

 * Set up our internal device.

 */

static void vmem_setup_device(struct vmem_disk_dev *dev, int which)

{

	/*

	 * Get some memory.

	 */

	memset (dev, 0, sizeof (struct vmem_disk_dev));

	
	dev->size = RAMDISK_SIZE;

	//dev->size = nsectors*hardsect_size;//512*1024=512kB
	dev->data = vmalloc(dev->size);
	if (dev->data == NULL) {
		eds_dbg ( "vmalloc failure.\n");
		return;
	}
	eds_dbg ( "dev->size = %d ,hardsect_size=%d",dev->size,hardsect_size);
	spin_lock_init(&dev->lock);

	/* The timer which "invalidates" the device. */

	init_timer(&dev->timer);
	dev->timer.data = (unsigned long) dev;
	dev->timer.function = vmem_disk_invalidate;

	switch (request_mode) {
	case RM_NOQUEUE:
		dev->queue = blk_alloc_queue(GFP_KERNEL);
		if (dev->queue == NULL)
			goto out_vfree;
		blk_queue_make_request(dev->queue, vmem_disk_make_request);
		break;

	case RM_FULL:
#if 0 //qiupq
		dev->queue = blk_init_queue(vmem_disk_full_request, &dev->lock);
		if (dev->queue == NULL)
#endif
			goto out_vfree;
		break;

	case RM_SIMPLE:
#if 0 //qiupq
		dev->queue = blk_init_queue(vmem_disk_request, &dev->lock);
		if (dev->queue == NULL)
#endif
			goto out_vfree;
		break;
		
	default:
			eds_dbg( "Bad request mode %d, using simple\n", request_mode);
			/* fall into.. */

	}
	//blk_queue_hardsect_size(dev->queue, hardsect_size);//change to below
	
	blk_queue_logical_block_size(dev->queue, hardsect_size);
	blk_queue_physical_block_size(dev->queue, hardsect_size);
	
	dev->queue->queuedata = dev;

	dev->gd = alloc_disk(vmem_disk_MINORS);
	if (! dev->gd) {
		eds_dbg ( "alloc_disk failure\n");
		goto out_vfree;
	}
	dev->gd->major = vmem_disk_major;
	dev->gd->first_minor = which*vmem_disk_MINORS;
	dev->gd->fops = &vmem_disk_ops;
	dev->gd->queue = dev->queue;
	dev->gd->private_data = dev;
	snprintf (dev->gd->disk_name, 32, "vmem_disk%c", which + 'a');
	dev->gd->flags |= GENHD_FL_SUPPRESS_PARTITION_INFO;///
	//set_capacity(dev->gd, nsectors*(hardsect_size/KERNEL_SECTOR_SIZE));///
	set_capacity(dev->gd, RAMDISK_SECTOR_TOTAL);
	add_disk(dev->gd);

	return;

out_vfree:

	if (dev->data)

		vfree(dev->data);

}





static int __init vmem_disk_init(void)

{

	int i;

	eds_dbg("enter-init");

	vmem_disk_major = register_blkdev(vmem_disk_major, "vmem_disk");

	if (vmem_disk_major <= 0) {

		eds_dbg("vmem_disk: unable to get major number\n");

		return -EBUSY;

	}


	Devices = kmalloc(ndevices*sizeof (struct vmem_disk_dev), GFP_KERNEL);

	if (Devices == NULL)
		goto out_unregister;

	for (i = 0; i < ndevices; i++)
		vmem_setup_device(Devices + i, i);



	return 0;



out_unregister:

	unregister_blkdev(vmem_disk_major, "sbd");

	return -ENOMEM;

}



static void vmem_disk_exit(void)

{

	int i;

	for (i = 0; i < ndevices; i++) 
	{
		struct vmem_disk_dev *dev = Devices + i;
		del_timer_sync(&dev->timer);

		if (dev->gd) 
		{
			del_gendisk(dev->gd);
			put_disk(dev->gd);
		}

		if (dev->queue) 
		{
			blk_cleanup_queue(dev->queue);
		}

		if (dev->data)
			vfree(dev->data);

	}
	
	kfree(Devices);

	unregister_blkdev(vmem_disk_major, "vmem_disk");
	eds_dbg("exit");


}



module_init(vmem_disk_init);

module_exit(vmem_disk_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("zturn_eds, Inc.");
MODULE_DESCRIPTION("zturn_eds block driver");

