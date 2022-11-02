/***************************************************************************//**
*  \file       aab_nordicdriver.c
*
*  \details    NORDIC DRIVER experimentations
*
*       Without Firmware: 1915:0102
*       With bitcraze Firmware 0.53: 1915:0101 & 1915:7777
*       Driver name
*       Nordic semiconductors
*
*******************************************************************************/
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/kref.h>
#include <linux/usb.h>
#include <linux/device.h> /* Devices */
#include <linux/fs.h> /* For filesystem structures */
#include <uapi/asm-generic/errno-base.h>
#include <linux/kern_levels.h>
#include <linux/moduleparam.h>
#include <linux/uaccess.h>
        /* unsigned long copy_to_user(void __user *to, const void *from, unsigned long n);
         * unsigned long copy_from_user(void *to, const void __user *from, unsigned long n); */


MODULE_AUTHOR("T15username <T15username.T15name@mail.com") ;
MODULE_LICENSE("GPL v2") ;
MODULE_DESCRIPTION("Nordic USB Driver") ;
MODULE_VERSION("0.10");

/*
** This macro is used to tell the driver to use old method or new method.
** 
**  If it is 0, then driver will use old method. ie: __init and __exit
**  If it is non zero, then driver will use new method. ie: module_usb_driver
*/
#define IS_NEW_METHOD_USED 1


#define USB_VENDOR_ID 0x1915      //USB device's vendor ID
//#define USB_PRODUCT_ID 0x0102     //USB device's product ID
#define USB_PRODUCT_ID 0x7777     //USB device's product ID

#define USB_NORDIC_MINOR_BASE 0

#define BULK_EP1_OUT 0x01
#define BULK_EP1_IN 0x81
#define MAX_PACKET_SIZE 40

#define WRITES_IN_FLIGHT 32

/* usb_device_id provides a list of different types of USB devices that the driver supports
 * This is mandatory for the linux hotplug system */
static const struct usb_device_id nordic_table[] = {
        { USB_DEVICE( USB_VENDOR_ID, USB_PRODUCT_ID ) },    /* Put your USB device's Vendor and Product ID */
        { } /* Terminating entry */
};

/* This enables the linux hotplug system to load the driver automatically when the device is plugged in */
MODULE_DEVICE_TABLE(usb, nordic_table);

/* Structure to hold all of our device specific stuff */
struct usb_nordic_struct {
        struct usb_device       *udev;                  /* the usb device for this device */
        struct usb_interface    *interface;             /* the interface for this device */
        struct semaphore        limit_sem;              /* limiting the number of writes in progress */
        struct usb_anchor       submitted;              /* in case we need to retract our submissions */
        struct urb              *bulk_in_urb;           /* the urb to read data with */
        unsigned char           *bulk_in_buffer;        /* the buffer to receive data */
        size_t                  bulk_in_size;           /* the size of the receive buffer */
        size_t                  bulk_in_filled;         /* number of bytes in the buffer */
        size_t                  bulk_in_copied;         /* already copied to user space */
        __u8                    bulk_in_endpointAddr;   /* the address of the bulk in endpoint */
        __u8                    bulk_out_endpointAddr;  /* the address of the bulk out endpoint */
        int                     errors;                 /* the last request tanked */
        bool                    ongoing_read;           /* a read is going on */
        spinlock_t              err_lock;               /* lock for errors */
        struct kref             kref;			/* refcount */
        struct mutex            io_mutex;               /* synchronize I/O with disconnect */
        unsigned long           disconnected:1;		/* disconnected status */
        wait_queue_head_t       bulk_in_wait;           /* to wait for an ongoing read */
};

#define to_nordic_dev(d) container_of(d, struct usb_nordic_struct, kref)

static struct usb_driver nordic_usb_driver ;
static void nordic_draw_down(struct usb_nordic_struct *ndev) ;

static void nordic_delete(struct kref *kref) {
	struct usb_nordic_struct *ndev = to_nordic_dev(kref) ;
	printk("%s - Freeing bulk_in_urb via usb_free_urb", __func__) ;
	usb_free_urb(ndev->bulk_in_urb) ;
	usb_put_intf(ndev->interface) ;
	usb_put_dev(ndev->udev) ;
	kfree(ndev->bulk_in_buffer) ;
	kfree(ndev) ;
};


static int nordic_open(struct inode *inode, struct file *file){
	struct usb_nordic_struct * ndev ;
	struct usb_interface *interface ;
	int subminor ;
	int retval = 0 ;
	subminor = iminor(inode) ;
	interface = usb_find_interface(&nordic_usb_driver, subminor) ;
	if (!interface) {
		pr_err("%s - error, can't find device for minor %d\n", __func__, subminor);
		retval = -ENODEV ;
		goto exit;
	}

	ndev = usb_get_intfdata(interface) ;
	if (!ndev) {
		retval = -ENODEV ;
		goto exit;
	}

	retval = usb_autopm_get_interface(interface) ;
	if (retval)
		goto exit ;

	/* Increment our usage count for the device */
	kref_get(&ndev->kref);

	/* Save our object in the file's private structure */
	file->private_data = ndev ;
exit:
	return retval ;
}

static int nordic_release(struct inode * inode, struct file *file){
	struct usb_nordic_struct *ndev ;

	ndev = file->private_data ;
	if (ndev == NULL)
		return -ENODEV ;

	/* Allow the device to be autosuspended */
	usb_autopm_put_interface(ndev->interface) ;

	/* Decrement the count on our device */
	kref_put(&ndev->kref, nordic_delete) ;
	return 0 ;
}

static int nordic_flush(struct file *file, fl_owner_t id) {
	struct usb_nordic_struct *ndev ;
	int retval ;

	ndev = file->private_data ;
	if (ndev == NULL)
		return -ENODEV ;

	/* Wait for I/O to stop */
	mutex_lock(&ndev->io_mutex) ;
	nordic_draw_down(ndev) ;
	
	/* read out errors, leave subsequent opens a clean slate */
	spin_lock_irq(&ndev->err_lock) ;
	retval = ndev->errors ? ((ndev->errors == -ENOENT) ? 0 : -EIO) : -EIO;
	ndev->errors = 0 ;
	spin_unlock(&ndev->err_lock) ;

	mutex_unlock(&ndev->io_mutex) ;
	return retval ;
}


/* The read callback check if the URB transmission completed correctly or not */
static void nordic_read_bulk_callback(struct urb *urb) {
	struct usb_nordic_struct *ndev ;
	unsigned long flags ;

	ndev = urb->context ;

	spin_lock_irqsave(&ndev->err_lock, flags) ;
	if (urb->status) {
		if (!(urb->status == -ENOENT ||
		      urb->status == -ECONNRESET ||
		      urb->status == -ESHUTDOWN))
			dev_err(&ndev->interface->dev,"%s - nonzero read bulk received: %d\n", __func__, urb->status) ;
		ndev->errors = urb->status ;
	} else {
		ndev->bulk_in_filled = urb->actual_length ;
	}
	ndev->ongoing_read = 0 ;
	spin_unlock_irqrestore(&ndev->err_lock, flags) ;

	wake_up_interruptible(&ndev->bulk_in_wait) ;
}

/* Submit the data reading request */
static int nordic_do_read_io(struct usb_nordic_struct *ndev,
			     size_t count) {
	int retval ;

	/* Prepare a read 
	 * Initialize URB */
	printk("%s - usb_fill_bulk_urb of bulk_in_urb", __func__) ;
	usb_fill_bulk_urb(ndev->bulk_in_urb, /* the urb to be initialized */
			  ndev->udev, /* pointer to struct usb_device for this urb */
			  usb_rcvbulkpipe(ndev->udev,
				  	  ndev->bulk_in_endpointAddr), /* the endpoint pipe */
			  ndev->bulk_in_buffer, /* pointer to the transfer buffer*/
			  min(ndev->bulk_in_size, count), /* length of the transfer buffer */
			  nordic_read_bulk_callback, /* pointer to complete_t function.
			  			      * We'll get bulk_in_filled from this callback*/
			  ndev) ; /* context */

	/* Tells everybody to leave the URB alone */
	spin_lock_irq(&ndev->err_lock) ; /* We assume irq are not a problems here and
					    can be restored when we'll unlock */
	ndev->ongoing_read = 1 ; /* We declare we're under read operation */
	spin_unlock_irq(&ndev->err_lock) ;

	/* Prepare bulk submission in urb */
	ndev->bulk_in_filled = 0 ; /* 0 bytes in the buffer */
	ndev->bulk_in_copied = 0 ; /* 0 bytes already copied to user space */

	/* Issue an asynchronous transfer request for an endpoint */
	retval = usb_submit_urb(ndev->bulk_in_urb, GFP_KERNEL) ;
	if (retval < 0) {
		dev_err(&ndev->interface->dev, "%s - failed submitting read URB, error %d\n", __func__, retval) ;
		retval = (retval == -ENOMEM) ? retval : -EIO ;
		spin_lock_irq(&ndev->err_lock) ;
		ndev->ongoing_read = 0 ; /* The ongoing read is cancelled */
		spin_unlock_irq(&ndev->err_lock) ;
	}

	return retval ;
}

static ssize_t nordic_read(struct file *file, /* file pointer*/
			    char *buffer, /* buffer holding the data */
			    size_t count, /* size of the requested data transfer */
			    loff_t *ppos) /* long offset pointer to file position the user is accessing */
{
	struct usb_nordic_struct *ndev ;
	int retval ;  /* retval */
	bool ongoing_io ; /* To verify if a I/O operation is ongoing */

	ndev = file->private_data ; /* Returns pointer to data that's private to the device driver */

	if (!count) /* If no datas to be read */
		return 0 ;

	/* We prevent concurrent reads but it can be interrupted  */
	retval = mutex_lock_interruptible(&ndev->io_mutex) ; /* Returns 0 if lock is acquired */
	if (retval < 0)
		return retval ;

	if (ndev->disconnected) {
		retval = -ENODEV ;
		goto exit ;
	}
	
	/* If I/O is under way, we must not thouch anything */
retry:
	spin_lock_irq(&ndev->err_lock); /* No concurrent read for ongoing_read */
	ongoing_io = ndev->ongoing_read ;
	spin_unlock_irq(&ndev->err_lock) ;

	if (ongoing_io) {
		/* Nonblocking I/O shall not wait */
		if (file->f_flags & O_NONBLOCK) {
			retval = -EAGAIN ; /* Resource temporarily unavailable. 
					      This is a temporary condition and later calls to the
					      same routine may complete normally.*/
			goto exit ;
		}

		/*
                 * I/O may take forever
                 * hence wait in an interruptible state
                 */

		         /* Sleeps until a condition gets true */
		retval = wait_event_interruptible(ndev->bulk_in_wait, (!ndev->ongoing_read)) ;
		if (retval < 0)
			goto exit ;
	}

	/* Errors must be reported */
	retval = ndev->errors ;
	if (retval < 0) {
		ndev->errors = 0 ; /* Any error is reported once */
		retval =  (retval == -EPIPE) ? retval : -EIO ; /* Is fd connected to a pipe whose
								  reading end is closed */
		goto exit ;
	}

        /*
         * If the buffer is filled we may satisfy the read
         * else we need to start I/O
         */

	if (ndev->bulk_in_filled) { /* We had read data -- in the buffer -- */
		size_t available = ndev->bulk_in_filled - ndev->bulk_in_copied ; /* Buffer minus already copied to user space */
		size_t chunk = min(available, count) ; /* How many data is left to copy*/

		if (!available) { /* All data has been used. Actual I/O needs to be done */
			retval = nordic_do_read_io(ndev, count) ;
			if (retval < 0)
				goto exit ;
			else
				goto retry ;
		}
		
		/* Data available :
		 * chunk tells us how much shall be copied */
		if (copy_to_user(buffer,
				 ndev->bulk_in_buffer + ndev->bulk_in_copied, /* Data's address source calculation */
				 chunk)) /* Size of data to copy */
			retval = -EFAULT ;
		else
			retval = chunk ;
		
		ndev->bulk_in_copied += chunk ;

		/* If we are asked for more than we have, we start I/O but don't wait */
		if (available < count)
			nordic_do_read_io(ndev, count - chunk) ; 
	} else {
		/* No data in the buffer */
		retval = nordic_do_read_io(ndev, count) ;
		if (retval < 0)
			goto exit ;
		else
			goto retry ;
	}
exit:
	mutex_unlock(&ndev->io_mutex) ;
	return retval ;
}

static void nordic_write_bulk_callback(struct urb *urb) {
	struct usb_nordic_struct *ndev ;
	unsigned long flags ;
	ndev = urb->context ;

	/* sync/async unlink faults aren't errors */
	if (urb->status) {
		if (!(urb->status == -ENOENT ||
		      urb->status == -ECONNRESET ||
		      urb->status == -ESHUTDOWN))
			dev_err(&ndev->interface->dev, "%s - Nonzero write bulk status received: %d\n",
							__func__, urb->status) ;
		spin_lock_irqsave(&ndev->err_lock, flags) ;
		ndev->errors = urb->status ;
		spin_unlock_irqrestore(&ndev->err_lock, flags) ;
	}

	/* Free up our allocated buffer */
	usb_free_coherent(urb->dev,
			  urb->transfer_buffer_length,
			  urb->transfer_buffer,
			  urb->transfer_dma) ;
	up(&ndev->limit_sem) ;
}

static ssize_t nordic_write(struct file *file, /* file pointer  */
			    const char * user_buffer, /* buffer holding the data */
			    size_t count, /* size of the requested data transfer  */
			    loff_t *ppos) { /* long offset pointer to file position the user is accessing  */
	struct usb_nordic_struct *ndev ;
	int retval = 0 ;
	struct urb *urb = NULL ;
	char *buf = NULL ;
	size_t writesize = min_t(size_t, count, MAX_PACKET_SIZE) ;

	/* Remember kernel file has nothing to do with user space files*/
	ndev = file->private_data ;

	/* Verify if we actually have some data to write */
	if (count == 0)
		goto exit;

	/*
         * limit the number of URBs in flight to stop a user from using up all
         * RAM
         */
        if (!(file->f_flags & O_NONBLOCK)) { /* If file structure is blocking*/
                if (down_interruptible(&ndev->limit_sem)) {
                        retval = -ERESTARTSYS;
                        goto exit;
                }
        } else {
                if (down_trylock(&ndev->limit_sem)) {
                        retval = -EAGAIN;
                        goto exit;
                }
        }

	spin_lock_irq(&ndev->err_lock) ; /* atomic lock assuming nothing else might have disabled interrupts 
					  * (i.e. we can restore interrupts on unlock without disturbing 
					  * anything else) */
	retval = ndev->errors ; 
	if (retval < 0) {
                /* any error is reported once */
                ndev->errors = 0;
                /* to preserve notifications about reset */
                retval = (retval == -EPIPE) ? retval : -EIO;
        }
        spin_unlock_irq(&ndev->err_lock) ;
        if (retval < 0)
                goto error;

        /* create a urb, and a buffer for it, and copy the data to the urb */
        urb = usb_alloc_urb(0, GFP_KERNEL);
        if (!urb) {
                retval = -ENOMEM;
                goto error;
        }

	buf = usb_alloc_coherent(ndev->udev, writesize, GFP_KERNEL, &urb->transfer_dma) ;
	if (!buf) {
                retval = -ENOMEM;
                goto error;
        }

        if (copy_from_user(buf, user_buffer, writesize)) {
                retval = -EFAULT;
                goto error;
        }

        /* this lock makes sure we don't submit URBs to gone devices */
        mutex_lock(&ndev->io_mutex);
        if (ndev->disconnected) {                /* disconnect() was called */
                mutex_unlock(&ndev->io_mutex);
                retval = -ENODEV;
                goto error;
        }

	/* initialize the urb properly */
        usb_fill_bulk_urb(urb,
			  ndev->udev,
                          usb_sndbulkpipe(ndev->udev, ndev->bulk_out_endpointAddr),
                          buf,
			  writesize,
			  nordic_write_bulk_callback,
			  ndev); /* Context is saved to be used in the callback (complete_fn) function */
       
       	
	urb->transfer_flags |= URB_NO_TRANSFER_DMA_MAP; /* As we used usb_alloc_coherent, host controller driver will
  							 * attempt to use the dma address found in the transfer_dma field rather
							 * than determining a dma address themselves. */
        usb_anchor_urb(urb, &ndev->submitted);

	        /* send the data out the bulk port */
        retval = usb_submit_urb(urb, GFP_KERNEL);
        mutex_unlock(&ndev->io_mutex);
        if (retval) {
                dev_err(&ndev->interface->dev,
                        "%s - failed submitting write urb, error %d\n",
                        __func__, retval);
                goto error_unanchor;
        }

        /*
         * release our reference to this urb, the USB core will eventually free
         * it entirely
         */
        usb_free_urb(urb);

        return writesize;

error_unanchor:
        usb_unanchor_urb(urb);
error:
        if (urb) {
                usb_free_coherent(ndev->udev, writesize, buf, urb->transfer_dma);
                usb_free_urb(urb);
        }
        up(&ndev->limit_sem);

exit:
        return retval;
};


/* ssize_t __kernel_read(struct file *file, void *buf, size_t count, loff_t *pos);
 * extern ssize_t __kernel_write(struct file *, const void *, size_t, loff_t *)
 */
static const struct file_operations nordic_fops = {
	.owner =        THIS_MODULE,
        .read =         nordic_read,
        .write =        nordic_write,
        .open =         nordic_open,
        .release =      nordic_release,
        .flush =        nordic_flush,
        .llseek =       noop_llseek,
};

static struct usb_class_driver nordic_class = {
	.name =		"Nordic%d",	/* Will set /dev/NordicX */
	.fops =		&nordic_fops,
	.minor_base =	USB_NORDIC_MINOR_BASE ,	
};



static int nordic_probe(struct usb_interface *interface,
                        const struct usb_device_id *id)
{
	struct usb_nordic_struct *ndev;
        struct usb_endpoint_descriptor *bulk_in, *bulk_out;
        int retval;

	printk("Waiting for Nordic USB device to be connected...") ;
	
	/* allocate memory for our device state and initialize it */
        ndev = kzalloc(sizeof(*ndev), GFP_KERNEL);
        if (!ndev)
		return -ENOMEM;

	/* Some initializations*/
	kref_init(&ndev->kref); /* refcount set */
	sema_init(&ndev->limit_sem, WRITES_IN_FLIGHT) ;
	mutex_init(&ndev->io_mutex) ; /* mutex initialized */
	spin_lock_init(&ndev->err_lock) ; /* spinlock initialized */
	init_usb_anchor(&ndev->submitted) ; /* Initialized USB anchor.
					       For callbacks to check all URB are ceased I/O */
	init_waitqueue_head(&ndev->bulk_in_wait) ;
	
	/*Increment refcount and get a pointer to a a struct usb_device */
	ndev->udev = usb_get_dev(interface_to_usbdev(interface)) ;
	/* Get a pointer to a struct sb_interface  */
	ndev->interface = usb_get_intf(interface) ;

	/* set up the endpoint information */
	/* use only the first bulk-in and bulk-out endpoints */
	retval = usb_find_common_endpoints(interface->cur_altsetting,
					   &bulk_in,  /* usb_endpoint_descriptor for bulk */
					   &bulk_out, /* usb_endpoint_descriptor for bulk */
					   NULL,      /* for interupts */
					   NULL) ;    /* for interrupts */
	if (retval) {
		dev_err(&interface->dev, "Bulk-in and Bulk-out endpoints not found\n") ;
		goto error ;
	}

	ndev->bulk_in_size = usb_endpoint_maxp(bulk_in) ;
	ndev->bulk_in_endpointAddr = bulk_in->bEndpointAddress ;
	ndev->bulk_in_buffer = kmalloc(ndev->bulk_in_size, GFP_KERNEL) ;
	if (!ndev->bulk_in_buffer) {
		retval = -ENOMEM ;
		goto error ;
	}
	printk("%s - Allocating bulk_in_urb via usb_alloc_urb...", __func__) ;
	ndev->bulk_in_urb = usb_alloc_urb(0, GFP_KERNEL) ;
	if (!ndev->bulk_in_urb) {
		retval = -ENOMEM ;
		goto error ;
	}

	ndev->bulk_out_endpointAddr = bulk_out->bEndpointAddress ;

	/* Save our data pointer in this interface device */ 
	usb_set_intfdata(interface, ndev) ;

	/*Now, we try to register our device */
	retval = usb_register_dev(interface, &nordic_class) ;
	if (retval) {
		dev_err(&interface->dev,
			"Failed to get a MINOR number for this device.\n") ;
		usb_set_intfdata(interface, NULL) ;
		goto error ;
	}

	/* Let the user know what node this device is now attached to */
	dev_info(&interface->dev,
		 "USB Nordic device is now attached to USB Nordic%d",
		 interface->minor) ;

	dev_info(&interface->dev,
		 "Nordic%d Endpoints> EP1_OUT: 0x%x  - EP1_IN: 0x%x",
		 interface->minor, 
		 ndev->bulk_out_endpointAddr,
		 ndev->bulk_in_endpointAddr) ; 

       return 0 ;
error:
	/* Frees allocated memory */
	kref_put(&ndev->kref, nordic_delete) ;
 	return retval ;	

};


static void nordic_disconnect(struct usb_interface *interface) {
	struct usb_nordic_struct *ndev;
	int minor = interface->minor;

	ndev = usb_get_intfdata(interface) ;
	usb_set_intfdata(interface, NULL) ;

	/* Release our minor number */
	usb_deregister_dev(interface, &nordic_class) ;

	/* Prevent more I/O from starting */
	mutex_lock(&ndev->io_mutex) ;
	ndev->disconnected = 1 ;
	mutex_unlock(&ndev->io_mutex) ;

	printk("%s - Killing bulk_in_urb via usb_kill_urb...", __func__) ;
	usb_kill_urb(ndev->bulk_in_urb) ;
	usb_kill_anchored_urbs(&ndev->submitted) ;

	/* Decrement our usage count */
	kref_put(&ndev->kref, nordic_delete) ;

	dev_info(&interface->dev, "Nordic USB:%d is now disconnected", minor) ;

};


static void nordic_draw_down(struct usb_nordic_struct *ndev) {
	int time ;

	time = usb_wait_anchor_empty_timeout(&ndev->submitted, 1000);
	if (!time)
		usb_kill_anchored_urbs(&ndev->submitted) ;
	usb_kill_urb(ndev->bulk_in_urb) ;
}

static int nordic_suspend(struct usb_interface *intf, pm_message_t message) {
	struct usb_nordic_struct *ndev = usb_get_intfdata(intf) ;

	if (!ndev)
		return 0 ;
	nordic_draw_down(ndev);
	return 0 ;
}

static int nordic_resume(struct usb_interface *intf){
	return 0 ;
}


static int nordic_pre_reset(struct usb_interface *intf) {
	struct usb_nordic_struct *ndev = usb_get_intfdata(intf) ;

	mutex_lock(&ndev->io_mutex) ;
	nordic_draw_down(ndev) ;
	return 0 ;
}

static int nordic_post_reset(struct usb_interface *intf) {
	struct usb_nordic_struct *ndev = usb_get_intfdata(intf) ;

	ndev->errors = -EPIPE ;
	mutex_unlock(&ndev->io_mutex) ;
	return 0 ;
}

static struct usb_driver nordic_usb_driver = { 
        .name =         "Nordic USB Driver",
        .probe =        nordic_probe,
        .disconnect =   nordic_disconnect,
        .suspend =      nordic_suspend,
        .resume =       nordic_resume,
        .pre_reset =    nordic_pre_reset,
        .post_reset =   nordic_post_reset,
        .id_table =     nordic_table,
        .supports_autosuspend = 1,
};

module_usb_driver(nordic_usb_driver);
