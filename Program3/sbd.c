/**
 * Basic SBD outline: http://blog.superpat.com/2010/05/04/a-simple-block-driver-for-linux-kernel-2-6-31/
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>

#include <linux/sched.h>
#include <linux/kernel.h>  /* printk() */
#include <linux/slab.h>        /* kmalloc() */
#include <linux/fs.h>      /* everything... */
#include <linux/errno.h>   /* error codes */
#include <linux/timer.h>
#include <linux/types.h>   /* size_t */
#include <linux/fcntl.h>   /* O_ACCMODE */
#include <linux/hdreg.h>   /* HDIO_GETGEO */
#include <linux/kdev_t.h>
#include <linux/vmalloc.h>
#include <linux/genhd.h>
#include <linux/blkdev.h>
#include <linux/buffer_head.h> /* invalidate_bdev */
#include <linux/bio.h>
#include <linux/crypto.h>

MODULE_LICENSE("GPL");

static int sbd_major = 0;
module_param(sbd_major, int, 0);
static int hardsect_size = 512;
module_param(hardsect_size, int, 0);
static int nsectors = 1024;
module_param(nsectors, int, 0);
int ndevices = 4;
module_param(ndevices, int, 0);

//Enumerating the request modes
enum {
    RM_SIMPLE = 0, /* The extra-simple request function */
    RM_FULL = 1, /* The full-blown version */
    RM_NOQUEUE = 2, /* Use make_request */
};
static int request_mode = RM_SIMPLE;
module_param(request_mode, int, 0);

//Set minor numbers and partition management.
#define SBD_MINORS  16
#define MINOR_SHIFT 4
#define DEVNUM(kdevnum) (MINOR(kdev_t_to_nr(kdevnum)) >> MINOR_SHIFT

//Set sector number so kernel can talk to driver
#define KERNEL_SECTOR_SHIFT 9
#define KERNEL_SECTOR_SIZE  (1<<KERNEL_SECTOR_SHIFT)

//After some idle time, simulate media change
#define INVALIDATE_DELAY 30*HZ

static char *key = "group1012os444";
module_param(key, charp, S_IRUGO);

#define KEY_SIZE 32
static char crypto_key[KEY_SIZE];
static int key_size = 10;


struct crypto_cipher *cipher;

//Device structure.
struct sbd_dev {
    int size;                       // Device size in sectors.
    u8 *data;                       // The data array.
    short users;                    // How many users.
    short media_change;             // Flag a media change.
    spinlock_t lock;                // For mutual exclusion.
    struct request_queue *queue;    // The device request queue.
    struct gendisk *genDisk;             // The gendisk structure.
    struct timer_list timer;        // For simulated media changes.
};

static struct sbd_dev *Devices = NULL;

//Print the crypto key value.
ssize_t key_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    printk(KERN_DEBUG "crypt: Copying key\n");
    return scnprintf(buf, PAGE_SIZE, "%s\n", crypto_key);
}

//Store the size of the crypto key.
ssize_t key_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
    if (count != 16 && count != 24 && count != 32)
    {
        printk(KERN_WARNING "Crpyt: invalid key size %d\n", count);
        return -EINVAL;
    }
    
    printk(KERN_DEBUG "crpyt: storing key\n");
    snprintf(crypto_key, sizeof(crypto_key), "%.*s", (int)min(count, sizeof(crypto_key) - 1), buf);
    key_size = count;
    return count;
}

DEVICE_ATTR(key, 0600, key_show, key_store);



//Handle IO request at the lowest level. Writes the data of the request after being encrypted.
static void sbd_transfer(struct sbd_dev *dev, unsigned long sector,
                         unsigned long nsect, char *buffer, int write) {
    
    int i;
    u8 *src;
    u8 *src_temp;
    unsigned long nbytes = nsect*KERNEL_SECTOR_SIZE;
    unsigned long len = nbytes;
    u8 *dst;
    unsigned long offset = sector*KERNEL_SECTOR_SIZE;
    
    
    //Forcing the key to be set.
    if (key_size == 0)
    {
        crypto_cipher_clear_flags(cipher, ~0);
        //Set the crypto cipher key.
        crypto_cipher_setkey(cipher, key, strlen(key));
        //Store the length of the key.
        key_size = strlen(key);
        printk("Key size %d\n", key_size);
    }
    else
    {
        crypto_cipher_clear_flags(cipher, ~0);
        //Set the crypto cipher key.
        crypto_cipher_setkey(cipher, crypto_key, key_size);
        //Store the length of the key.
        key_size = strlen(key);
    }
    
    //Check if the curr sector start address + the number of sectors in the device * sector size is greater than the device size.
    if ((offset + nbytes) > dev->size)
    {
        printk(KERN_NOTICE "Beyond end write (%ld %ld)\n", offset, nbytes);
        return;
    }
    
    //Check the write flag to determine type of request
    //Write request -- encrypt data.
    if (write)
    {
        
        src = buffer;
        src_temp = buffer;
        dst = dev->data+offset;
        unsigned long length = nbytes;
        len = nbytes;
        
        printk("Writing\n");
        if (key_size != 0)
        {
            
            printk("Encrypting\n");
            //i increments in crypto block-sized chunks to iterate over the data.
            for (i = 0; i < nbytes; i += crypto_cipher_blocksize(cipher))
            {
                //Takes the cipher block, and encypts each byte then writes it in the buffer.
                crypto_cipher_encrypt_one(cipher, dst + i, src + i);
            }
        }
        else
        {
            memcpy(dev->data + offset, buffer, nbytes);
        }
    }
    //Read request -- decrypt the data.
    else
    {
        src_temp = buffer;
        src = buffer;
        unsigned long length = nbytes;
        len = nbytes;
        
        dst = dev->data+offset;
        printk("Reading\n");
        printk("KEY: %s",key);
        if (key_size != 0)
        {
            
            printk("decrypting\n");
            //i increments in crypto block-sized chunks to iterate over the data.
            for (i = 0; i < nbytes; i += crypto_cipher_blocksize(cipher))
            {
                //Takes the cipher block, and decrypts each byte then writes it in the buffer.
                crypto_cipher_decrypt_one(cipher,buffer + i,dev->data + offset + i);
            }
        }
        else
        {
            memcpy(buffer, dev->data + offset, nbytes);
        }
    }
    
    
    printk("%s:", "ENCRYPTED");
    while (len--)
        printk("%u", (unsigned) *src++);
    
    len = nbytes;
    printk("\n%s:", "UNENCRYPTED");
    while (len--)
        printk("%u", (unsigned) *dst++);
    printk("\n");
    
    
    
}

//Handle unfinished requests from the device request queue.
static void sbd_request(struct request_queue *q) {
    
    struct request *req;
    int ret;
    
    //Fetch incomplete request in the queue.
    req = blk_fetch_request(q);
    while (req) {
        struct sbd_dev *dev = req->rq_disk->private_data;
        //Check if it is a filesystem request.
        if (req->cmd_type != REQ_TYPE_FS) {
            printk(KERN_NOTICE "Skip non-fs request\n");
            ret = -EIO;
            goto done;
        }
        //Pass the request off the transfer.
        sbd_transfer(dev, blk_rq_pos(req), blk_rq_cur_sectors(req), bio_data(req->bio), rq_data_dir(req));
        ret = 0;
    done:
        //Finish the current request and if finished properly, fetch another.
        if (!__blk_end_request_cur(req, ret)) {
            req = blk_fetch_request(q);
        }
    }
}

// Info on bio strcuture: http://www.makelinux.net/books/lkd2/ch13lev1sec3
//Also will include powerpoint used to study this in the repo.
// Bio structure is a vector containing references (?) to the different segmented buffer chunks of a device.
//Tranfer bio structure.
static int sbd_transfer_bio(struct sbd_dev *dev, struct bio *bio) {
    
    struct bio_vec bvec;
    struct bvec_iter iter;
    sector_t sector = bio->bi_iter.bi_sector;
    
    //Iterate throught each section and transfer
    bio_for_each_segment(bvec, bio, iter)
    {
        //Creates a kernal virtual address for a segment of the bio.
        char *buffer = __bio_kmap_atomic(bio, iter);
        //Transfer data.
        sbd_transfer(dev, sector, bio_cur_bytes(bio),
                     buffer, bio_data_dir(bio) == WRITE);
        //Increment the sector.
        sector += bio_cur_bytes(bio);
        //Removes the kernal address created by __bio_kmap_atomic.
        __bio_kunmap_atomic(bio);
    }
    return 0;
}

//Transfer of full request.
static int sbd_transfer_request(struct sbd_dev *dev, struct request *req) {
    
    struct bio *bio;
    int nsect = 0;
    
    //steps through each request and sends it to sbd_transfer_bio to set up the bio structure.
    __rq_for_each_bio(bio, req)
    {
        sbd_transfer_bio(dev, bio);
        nsect += bio->bi_iter.bi_size / KERNEL_SECTOR_SIZE;
    }
    
    return nsect;
}


//Request function that handles more complex things, like clustering.
static void sbd_full_request(struct request_queue *q) {
    
    struct request *req;
    struct sbd_dev *dev = q->queuedata;
    int ret;
    
    //Fetches a request and sends it to sbd_transfer_request if it is a filesystem request.
    while ((req = blk_fetch_request(q)) != NULL)
    {
        //If not a filesystem request.
        if (req->cmd_type != REQ_TYPE_FS)
        {
            printk(KERN_NOTICE "Skips non-fs request\n");
            __blk_end_request(req, -EIO, blk_rq_cur_bytes(req));
            ret = -EIO;
            goto done;
        }
        //Transfer the request to the device bio struct.
        sbd_transfer_request(dev, req);
        ret = 0;
        //Completes a request but we don't handle them because our device handles filesystem requests.
    done:
        __blk_end_request_all(req, ret);
    }
    
}

//Directly make a request.
static void sbd_make_request(struct request_queue *q, struct bio *bio) {
    
    struct sbd_dev *dev = q->queuedata;
    int status;
    
    //Goes through the bio request without a request queue.
    status = sbd_transfer_bio(dev, bio);
    //Complete the I/O to the bio structure. This handles decrementing of bi_size.
    bio_endio(bio, status);
}


//Opens the device and increases the number of users.
static int sbd_open(struct block_device *bdev, fmode_t mode) {
    
    struct sbd_dev *dev = bdev->bd_disk->private_data;
    
    //Remove the media timer
    del_timer_sync(&dev->timer);
    
    //Get the lock on the drive.
    spin_lock(&dev->lock);
    if (!dev->users)
        //Check if media has changed.
        check_disk_change(bdev);
    //Increase users.
    dev->users++;
    //Release lock.
    spin_unlock(&dev->lock);
    return 0;
}

//Remove the device and decrements the number of users.
static void sbd_release(struct gendisk *disk, fmode_t mode) {
    
    struct sbd_dev *dev = disk->private_data;
    
    //Hang and wait for the lock to decrement user count.
    spin_lock(&dev->lock);
    dev->users--;
    
    //Start the timer to invaldidate the drive if no one is using it
    if (!dev->users) {
        dev->timer.expires = jiffies + INVALIDATE_DELAY;
        add_timer(&dev->timer);
    }
    
    //Give up the lock.
    spin_unlock(&dev->lock);
}

//Make media change.
int sbd_media_changed(struct gendisk *genDisk) {
    
    struct sbd_dev *dev = genDisk->private_data;
    
    return dev->media_change;
}

//Switch media_change flag and clear the device data if the media_change flag is set.
int sbd_revalidate(struct gendisk *genDisk) {
    
    struct sbd_dev *dev = genDisk->private_data;
    
    if (dev->media_change)
    {
        dev->media_change = 0;
        memset(dev->data, 0, dev->size);
    }
    return 0;
}

//Runs with device times, simulates media removal.
void sbd_invalidate(unsigned long ldev) {
    
    struct sbd_dev *dev = (struct sbd_dev *) ldev;
    
    //Get the lock to check if there was a media change.
    spin_lock(&dev->lock);
    if (dev->users || !dev->data)
        printk(KERN_WARNING "sdb: timer sanity check failed\n");
    else
        dev->media_change = 1;
    
    //Give up the lock.
    spin_unlock(&dev->lock);
}

//Implementation of ioctl()
//http://man7.org/linux/man-pages/man2/ioctl.2.html
//http://opensourceforu.com/2011/08/io-control-in-linux/
//Handles request for drives geometry
//Even though this is virtul, things like fdisk need this for stuff like partitioning.
int sbd_ioctl(struct block_device *bdev, fmode_t mode,
              unsigned int cmd, unsigned long arg) {
    
    long size;
    struct hd_geometry geo;
    struct sbd_dev *dev = bdev->bd_disk->private_data;
    
    switch (cmd)
    {
            //Get device geometry.
        case HDIO_GETGEO:
            size = dev->size*(hardsect_size / KERNEL_SECTOR_SIZE);
            geo.cylinders = (size & ~0x3f) >> 6;
            geo.heads = 4;
            geo.sectors = 16;
            geo.start = 4;
            if (copy_to_user((void __user *) arg, &geo, sizeof(geo)))
                return -EFAULT;
            
            return 0;
    }
    
    //Request does no apply to specified object the cmd references.
    return -ENOTTY;
}

//Driver device operations.
static struct block_device_operations sbd_ops = {
    .owner = THIS_MODULE,
    .open = sbd_open,
    .release = sbd_release,
    .media_changed = sbd_media_changed,
    .revalidate_disk = sbd_revalidate,
    .ioctl = sbd_ioctl
};

//Setup the driver deivce.
static void setup_device(struct sbd_dev *currDevice, int which) {
    
    //Clear the memory space.
    memset(currDevice, 0, sizeof(struct sbd_dev));
    //Our standard size of memory for the device = size of block * size of drive.
    currDevice->size = nsectors*hardsect_size;
    //Allocate the memory.
    currDevice->data = vmalloc(currDevice->size);
    if (currDevice->data == NULL) {
        printk(KERN_NOTICE "vmalloc failure.\n");
        return;
    }
    spin_lock_init(&currDevice->lock);
    
    // The timer which "invalidates" the device.
    init_timer(&currDevice->timer);
    currDevice->timer.data = (unsigned long)currDevice;
    currDevice->timer.function = sbd_invalidate;
    
    // The I/O queue, depending on whether we are using our own
    // make_request function or not.
    switch (request_mode) {
        case RM_NOQUEUE:
            currDevice->queue = blk_alloc_queue(GFP_KERNEL);
            if (currDevice->queue == NULL)
                //Failed. Skip to deallocate memory.
                goto out_vfree;
            //Setup the make request function.
            blk_queue_make_request(currDevice->queue, sbd_make_request);
            break;
            
        case RM_FULL:
            currDevice->queue = blk_init_queue(sbd_full_request, &currDevice->lock);
            if (currDevice->queue == NULL)
                //Failed. Skip to deallocate memory.
                goto out_vfree;
            break;
            
        default:
            printk(KERN_NOTICE "Bad request mode %d, using simple\n", request_mode);
            /* fall into.. */
            
        case RM_SIMPLE:
            currDevice->queue = blk_init_queue(sbd_request, &currDevice->lock);
            if (currDevice->queue == NULL)
                //Failed. Skip to deallocate memory.
                goto out_vfree;
            break;
    }
    
    //I/O request was received properly.
    blk_queue_logical_block_size(currDevice->queue, hardsect_size);
    currDevice->queue->queuedata = currDevice;
    
    //
    //Setup the gendisk structure for the currDevice.
    //
    
    //Alloc the disk.
    currDevice->genDisk = alloc_disk(SBD_MINORS);
    if (!currDevice->genDisk) {
        printk(KERN_NOTICE "alloc_disk failure\n");
        goto out_vfree;
    }
    
    //Reference: https://yannik520.github.io/blkdevarch.html
    currDevice->genDisk->major = sbd_major;
    currDevice->genDisk->first_minor = which*SBD_MINORS;
    currDevice->genDisk->fops = &sbd_ops;
    currDevice->genDisk->queue = currDevice->queue;
    currDevice->genDisk->private_data = currDevice;
    snprintf(currDevice->genDisk->disk_name, 32, "bdes%c", which + 'a');
    set_capacity(currDevice->genDisk, nsectors*(hardsect_size / KERNEL_SECTOR_SIZE));
    add_disk(currDevice->genDisk);
    return;
    
out_vfree:
    if (currDevice->data)
        vfree(currDevice->data);
}

//Setup the devices for the driver.
static int __init sbd_init(void) {
    
    int i;
    
    //Initialize the cipher as an aes cipher with default type and mask
    cipher = crypto_alloc_cipher("aes", 0, 0);
    key_size = strlen(crypto_key);
    
    
    sbd_major = register_blkdev(sbd_major, "sbd");
    if (sbd_major <= 0)
    {
        printk(KERN_WARNING "sbd: Can't get major number\n");
        return -EBUSY;
    }
    
    //Allocate memory for the number of devices based on ndevices.
    Devices = kmalloc(ndevices * sizeof(struct sbd_dev), GFP_KERNEL);
    
    //If no memory allocated, deallocate crytpo structure, unregister the block device and exit.
    if (Devices == NULL)
        goto out_unregister;
    for (i = 0; i < ndevices; i++)
        setup_device(Devices + i, i);
    
    return 0;
    
out_unregister:
    crypto_free_cipher(cipher);
    unregister_blkdev(sbd_major, "sbd");
    return -ENOMEM;
}

static void sbd_exit(void) {
    
    int i;
    
    //Free the cipher.
    crypto_free_cipher(cipher);
    for (i = 0; i < ndevices; i++)
    {
        struct sbd_dev *currDevice = Devices + i;
        
        //Waits for the timer to finish on other CPU's and then deletes it.
        del_timer_sync(&currDevice->timer);
        
        //If the device has a genDisk struct.
        if (currDevice->genDisk)
        {
            //Deletes the genDisk structure.
            del_gendisk(currDevice->genDisk);
            //Ensures the reference point is changed for the device genDisk member.
            put_disk(currDevice->genDisk);
        }
        
        //If the device request queue is not NULL.
        if (currDevice->queue)
        {
            
            if (request_mode == RM_NOQUEUE)
                //Decerement the reference count by calling kobject_put.
                blk_put_queue(currDevice->queue);
            else
                //Releases the reference to the request queue.
                blk_cleanup_queue(currDevice->queue);
        }
        
        //If the current device has data.
        if (currDevice->data)
            //Free the data at that address.
            vfree(currDevice->data);
        
    }
    
    //Unregister the devices and free the memory.
    unregister_blkdev(sbd_major, "sbd");
    kfree(Devices);
}

module_init(sbd_init);
module_exit(sbd_exit);