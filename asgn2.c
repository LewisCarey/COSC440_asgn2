
/**
 * File: asgn2.c
 * Date: 13/03/2011
 * Author: Lewis Carey 
 * Version: 0.1
 *
 * This is a module which serves as a virtual ramdisk which disk size is
 * limited by the amount of memory available and serves as the requirement for
 * COSC440 assignment 1 in 2012.
 *
 * Note: multiple devices and concurrent modules are not supported in this
 *       version.
 */
 
/* This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/list.h>
#include <asm/uaccess.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/proc_fs.h>
#include <linux/device.h>
#include <linux/sched.h>
#include "gpio.c"
#include <linux/interrupt.h>

#define MYDEV_NAME "asgn2"
#define MYIOC_TYPE 'k'

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Lewis Carey");
MODULE_DESCRIPTION("COSC440 asgn2");


/**
 * The node structure for the memory page linked list.
 */ 
typedef struct page_node_rec {
  struct list_head list;
  struct page *page;
} page_node;


typedef struct asgn2_dev_t {
  dev_t dev;            /* the device */
  struct cdev *cdev;
  struct list_head mem_list; 
  int num_pages;        /* number of memory pages this module currently holds */
  size_t data_size;     /* total data size in this module */
  atomic_t nprocs;      /* number of processes accessing this device */ 
  atomic_t max_nprocs;  /* max number of processes accessing this device */
  struct kmem_cache *cache;      /* cache memory */
  struct class *class;     /* the udev class */
  struct device *device;   /* the udev device node */
} asgn2_dev;

// A circular buffer for holding the bytes
struct asgn2_circular_buffer {
	char *buffer;
	int readIndex; // Index you read from
	int writeIndex;// Index you write to
	int capacity;
} circ_buf;

// The write position
int w_pos = 0;
int r_pos = 0;

asgn2_dev asgn2_device;

int firstHalfByte = -1;	/* Saves the first half of the byte when it comes in */

int asgn2_major = 0;                      /* major number of module */  
int asgn2_minor = 0;                      /* minor number of module */
int asgn2_dev_count = 1;                  /* number of devices */


/**
 * This function frees all memory pages held by the module.
 */
void free_memory_pages(void) {
  page_node *curr;

  /* START SKELETON */
  /* COMPLETE ME */
  /**
   * Loop through the entire page list {
   *   if (node has a page) {
   *     free the page
   *   }
   *   remove the node from the page list
   *   free the node
   * }
   * reset device data size, and num_pages
   */  
  /* END SKELETON */
  /* START TRIM */
  while (!list_empty(&asgn2_device.mem_list)) {
    curr = list_entry(asgn2_device.mem_list.next, page_node, list);
    if (NULL != curr->page) __free_page(curr->page);
    list_del(asgn2_device.mem_list.next);
    if (NULL != curr) kmem_cache_free(asgn2_device.cache, curr);
  }
  asgn2_device.data_size = 0;
  asgn2_device.num_pages = 0;
  /* END TRIM */

}


/**
 * This function opens the virtual disk, if it is opened in the write-only
 * mode, all memory pages will be freed.
 */
int asgn2_open(struct inode *inode, struct file *filp) {
  /* START SKELETON */
  /* COMPLETE ME */
  /**
   * Increment process count, if exceeds max_nprocs, return -EBUSY
   *
   * if opened in write-only mode, free all memory pages
   *
   */
  /* END SKELETON */
  /* START TRIM */
  if (atomic_read(&asgn2_device.nprocs) >= atomic_read(&asgn2_device.max_nprocs)) {
    return -EBUSY;
  }

  atomic_inc(&asgn2_device.nprocs);

  if ((filp->f_mode & FMODE_WRITE) && !(filp->f_mode & FMODE_READ)) {
    free_memory_pages();
  }
  /* END TRIM */


  return 0; /* success */
}


/**
 * This function releases the virtual disk, but nothing needs to be done
 * in this case. 
 */
int asgn2_release (struct inode *inode, struct file *filp) {
  /* START SKELETON */
  /* COMPLETE ME */
  /**
   * decrement process count
   */
  /* END SKELETON */
  /* START TRIM */
  atomic_dec(&asgn2_device.nprocs);
  /* END TRIM */
  return 0;
}

/**
 * This function writes from the user buffer to the virtual disk of this
 * module
 */
size_t asgn2_write(const char *buf, size_t count) {
  //size_t orig_f_pos = w_pos;  /* the original file position */
  size_t size_written = 0;  /* size written to virtual disk in this function */
  size_t begin_offset;      /* the offset from the beginning of a page to
  			       start writing */
  int begin_page_no = w_pos / PAGE_SIZE;  /* the first page this finction
  					      should start writing to */

  int curr_page_no = 0;     /* the current page number */
  size_t curr_size_written; /* size written to virtual disk in this round */
  size_t size_to_be_written;  /* size to be read in the current round in
  				 while loop */
  
  struct list_head *ptr = asgn2_device.mem_list.next;
  page_node *curr;

  /* START SKELETON */
  /* COMPLETE ME */
  /**
   * Traverse the list until the first page reached, and add nodes if necessary
   *
   * Then write the data page by page, remember to handle the situation
   *   when copy_from_user() writes less than the amount you requested.
   *   a while loop / do-while loop is recommended to handle this situation.
   */
  /* END SKELETON */
  /* START TRIM */
	//printk(KERN_WARNING "Writing from letter %c another %d letters.\n", buf, count);

  while (size_written < count) {
    curr = list_entry(ptr, page_node, list);
    if (ptr == &asgn2_device.mem_list) {
      /* not enough page, so add page */
      curr = kmem_cache_alloc(asgn2_device.cache, GFP_KERNEL);
      if (NULL == curr) {
  	printk(KERN_WARNING "Not enough memory left\n");
  	break;
      }
      curr->page = alloc_page(GFP_KERNEL);
      if (NULL == curr->page) {
  	printk(KERN_WARNING "Not enough memory left\n");
        kmem_cache_free(asgn2_device.cache, curr);
  	break;
      }
      //INIT_LIST_HEAD(&curr->list);
      list_add_tail(&(curr->list), &asgn2_device.mem_list);
      asgn2_device.num_pages++;
      ptr = asgn2_device.mem_list.prev;
    } else if (curr_page_no < begin_page_no) {
      /* move on to the next page */
      ptr = ptr->next;
      curr_page_no++;
    } else {
      /* this is the page to write to */
      begin_offset = w_pos % PAGE_SIZE;
      size_to_be_written = (size_t)min((size_t)(count - size_written),
  				       (size_t)(PAGE_SIZE - begin_offset));
      do {
        curr_size_written = size_to_be_written;
  	  memmove(page_address(curr->page) + begin_offset,
  	  	         buf + size_written, size_to_be_written);
        size_written += curr_size_written;
        begin_offset += curr_size_written;
        w_pos += curr_size_written;
        size_to_be_written -= curr_size_written;
      } while (size_to_be_written > 0);
      curr_page_no++;
      ptr = ptr->next;
    }
  }

  /* END TRIM */


  asgn2_device.data_size = w_pos - r_pos;
  
return size_written;
}
/**
 * The tasklet that reads the data from the circular buffer
 * and chooses what to write into memory.
 */
int page_queue_write (void) {
	//printk(KERN_WARNING "I am a tasklet and I am running");	
	
	// Check that there is memory to write in the circular buffer
	if (circ_buf.readIndex == circ_buf.writeIndex) {
		printk(KERN_WARNING "No more data to read from the circular buffer");
		return 0;
	} 
	// Read from the circular buffer and write into memory
	
	// If write has looped around (is below read)
	if (circ_buf.writeIndex < circ_buf.readIndex) {
		int bytesNotWritten = (circ_buf.writeIndex + circ_buf.capacity) - circ_buf.readIndex - 
						asgn2_write(&circ_buf.buffer[circ_buf.readIndex], 
						(circ_buf.writeIndex + circ_buf.capacity) - circ_buf.readIndex);
		circ_buf.readIndex += ((circ_buf.writeIndex + circ_buf.capacity) - circ_buf.readIndex) - bytesNotWritten;
		// Mod the position to ensure wrap around
		circ_buf.readIndex = circ_buf.readIndex % circ_buf.capacity;
	}
	// Otherwise...
	else {
		int bytesNotWritten = (circ_buf.writeIndex - circ_buf.readIndex) - 
						asgn2_write(&circ_buf.buffer[circ_buf.readIndex], 
						circ_buf.writeIndex - circ_buf.readIndex);
		// Set the read and write indices by just adding on to read
		circ_buf.readIndex += (circ_buf.writeIndex - circ_buf.readIndex) - bytesNotWritten;
	} 		
	
	return 0;
}

DECLARE_TASKLET(t_name, page_queue_write, (unsigned long) &circ_buf);

/**
 * This function writes the byte into the circular buffer.
 * Keeps track of the circular buffer positions and capacity.
 *
 * If buffer is full, just do not write to the buffer and just drop the bytes.
 */
int cb_write (char byte) {
	//printk(KERN_WARNING "Writing byte to buffer: %c readIndex: %d writeIndex: %d\n", byte, circ_buf.readIndex, circ_buf.writeIndex);

	// If write is one less than read, we know the buffer is full. Drop bytes
	if ((circ_buf.writeIndex + 1) % circ_buf.capacity == circ_buf.readIndex) {
		// We are full, drop byte
		printk(KERN_WARNING "Circular buffer is full, dropping byte.\n");
		return 0;
	}
	// Otherwise we can write to the circular buffer
	else {
		// Write to circular buffer
		circ_buf.buffer[circ_buf.writeIndex] = byte;		
	
		// Increment the write location
		circ_buf.writeIndex = (circ_buf.writeIndex + 1) % circ_buf.capacity;	

		// Schedule the tasklet
		tasklet_schedule(&t_name);
	}

	return 0;
}

/**
 * This function will call gpio.c read_half_byte() to read the half byte which generated the interrupt.
 * If we have the other half, we add them together and pass them to the write which schedules the circular buffer.
 */
int get_half_byte (void) {
	
	// If we are saving the firstHalfByte, process the full byte
	if (firstHalfByte != -1) {
		int secondHalfByte = read_half_byte();
		char fullByte = (char) firstHalfByte << 4 | secondHalfByte;
		firstHalfByte = -1;
		// Send the fullbyte to circular buffer
		return cb_write(fullByte);
	} 
	// Get first half of the byte
	else { 
		firstHalfByte = read_half_byte();	
	}
	
	return 0;
}


// This is the interrupt handler for the assignment
irqreturn_t dummyport_interrupt(int irq, void *dev_id) {
	get_half_byte();

	return 0;
}

/**
 * This function reads contents of the virtual disk and writes to the user 
 *
 * Anything that is read needs to be removed from the page queue.
 */
ssize_t asgn2_read(struct file *filp, char __user *buf, size_t count,
		 loff_t *f_pos) {
  size_t size_read = 0;     /* size read from virtual disk in this function */
  size_t begin_offset;      /* the offset from the beginning of a page to
			       start reading */
  int begin_page_no = *f_pos / PAGE_SIZE; /* the first page which contains
					     the requested data */
  int curr_page_no = 0;     /* the current page number */
  size_t curr_size_read;    /* size read from the virtual disk in this round */
  size_t size_to_be_read;   /* size to be read in the current round in 
			       while loop */

  struct list_head *ptr = asgn2_device.mem_list.next;
  page_node *curr;

  /* START SKELETON */
  /* COMPLETE ME */
  /**
   * check f_pos, if beyond data_size, return 0
   * 
   * Traverse the list, once the first requested page is reached,
   *   - use copy_to_user to copy the data to the user-space buf page by page
   *   - you also need to work out the start / end offset within a page
   *   - Also needs to handle the situation where copy_to_user copy less
   *       data than requested, and
   *       copy_to_user should be called again to copy the rest of the
   *       unprocessed data, and the second and subsequent calls still
   *       need to check whether copy_to_user copies all data requested.
   *       This is best done by a while / do-while loop.
   *
   * if end of data area of ramdisk reached before copying the requested
   *   return the size copied to the user space so far
   */
  /* END SKELETON */
  /* START TRIM */
  if (*f_pos >= asgn2_device.data_size) return 0;
  count = min(asgn2_device.data_size - (size_t)*f_pos, count);

  while (size_read < count) {
    curr = list_entry(ptr, page_node, list);
    if (ptr == &asgn2_device.mem_list) {
      /* We have already passed the end of the data area of the
         ramdisk, so we quit and return the size we have read
         so far */
      printk(KERN_WARNING "invalid virtual memory access\n");
      return size_read;
    } else if (curr_page_no < begin_page_no) {
      /* haven't reached the page occupued by *f_pos yet, 
         so move on to the next page */
      ptr = ptr->next;
      curr_page_no++;
    } else {
      /* this is the page to read from */
      begin_offset = *f_pos % PAGE_SIZE;
      size_to_be_read = (size_t)min((size_t)(count - size_read), 
				    (size_t)(PAGE_SIZE - begin_offset));

      do {
        curr_size_read = size_to_be_read - 
	  copy_to_user(buf + size_read, 
	  	       page_address(curr->page) + begin_offset,
		       size_to_be_read);
        size_read += curr_size_read;
        *f_pos += curr_size_read;
	r_pos += curr_size_read; // Update our record of the read position
        begin_offset += curr_size_read;
        size_to_be_read -= curr_size_read;
      } while (curr_size_read > 0);

      curr_page_no++;
      ptr = ptr->next;

	// Remove the page if needed
	if ((asgn2_device.data_size > PAGE_SIZE)) {
		if (NULL != curr->page) __free_page(curr->page);
		list_del(asgn2_device.mem_list.next);
		if (NULL != curr) kmem_cache_free(asgn2_device.cache, curr);
		w_pos -= PAGE_SIZE;
		r_pos -= PAGE_SIZE;
		f_pos -= PAGE_SIZE;
		asgn2_device.data_size = asgn2_device.data_size - PAGE_SIZE;
		asgn2_device.num_pages--;	
	} else {
		printk(KERN_WARNING "End of the file has been reached");
		asgn2_device.data_size = 0;
		w_pos = 0;
		r_pos = 0;
		f_pos = 0;
	}
    }
  }
  /* END TRIM */

  return size_read;
}


#define SET_NPROC_OP 1
#define TEM_SET_NPROC _IOW(MYIOC_TYPE, SET_NPROC_OP, int) 

/**
 * The ioctl function, which nothing needs to be done in this case.
 */
long asgn2_ioctl (struct file *filp, unsigned cmd, unsigned long arg) {
  int nr;
  int new_nprocs;
  int result;

  /* START SKELETON */
  /* COMPLETE ME */
  /** 
   * check whether cmd is for our device, if not for us, return -EINVAL 
   *
   * get command, and if command is SET_NPROC_OP, then get the data, and
     set max_nprocs accordingly, don't forget to check validity of the 
     value before setting max_nprocs
   */
  /* END SKELETON */
  /* START TRIM */
  if (_IOC_TYPE(cmd) != MYIOC_TYPE) {

    printk(KERN_WARNING "%s: magic number does not match\n", MYDEV_NAME);
    return -EINVAL;
  }

  nr = _IOC_NR(cmd);

  switch (nr) {
  case SET_NPROC_OP:
    result = get_user(new_nprocs, (int *)arg);

    if (result) {
      printk(KERN_WARNING "%s: failed to get new max nprocs\n", MYDEV_NAME);
      return -EINVAL;
    }

    if (new_nprocs < 1) {
      printk(KERN_WARNING "%s: invalid new max nprocs %d\n", MYDEV_NAME, new_nprocs);
      return -EINVAL;
    }

    atomic_set(&asgn2_device.max_nprocs, new_nprocs);

    printk(KERN_WARNING "%s: max_nprocs set to %d\n",
            __stringify (KBUILD_BASENAME), atomic_read(&asgn2_device.max_nprocs));
    return 0;
  } 
  /* END TRIM */                       

  return -ENOTTY;
}


/**
 * Displays information about current status of the module,
 * which helps debugging.
 */
int asgn2_read_procmem(char *buf, char **start, off_t offset, int count,
		     int *eof, void *data) {
  /* stub */
  int result;

  /* START SKELETON */
  /* COMPLETE ME */
  /**
   * use snprintf to print some info to buf, up to size count
   * set eof
   */
  /* END SKELETON */
  /* START TRIM */
  result = snprintf(buf, count,
	            "major = %d\nnumber of pages = %d\ndata size = %u\n"
                    "disk size = %d\nnprocs = %d\nmax_nprocs = %d\n",
	            asgn2_major, asgn2_device.num_pages, 
                    asgn2_device.data_size, 
                    (int)(asgn2_device.num_pages * PAGE_SIZE),
                    atomic_read(&asgn2_device.nprocs), 
                    atomic_read(&asgn2_device.max_nprocs)); 
  *eof = 1; /* end of file */
  /* END TRIM */
  return result;
}

struct file_operations asgn2_fops = {
  .owner = THIS_MODULE,
  .read = asgn2_read,
  //.write = asgn2_write,
  .unlocked_ioctl = asgn2_ioctl,
  .open = asgn2_open,
  //.mmap = asgn2_mmap,
  .release = asgn2_release,
  //.llseek = asgn2_lseek
};


/**
 * Initialise the module and create the master device
 */
int __init asgn2_init_module(void){
  int result; 

  /* START SKELETON */
  /* COMPLETE ME */
  /**
   * set nprocs and max_nprocs of the device
   *
   * allocate major number
   * allocate cdev, and set ops and owner field 
   * add cdev
   * initialize the page list
   * create proc entries
   */
  /* END SKELETON */

  /* START TRIM */
	// Initialise the handler
	int initHandler = gpio_dummy_init();
	if (initHandler != 0) {
    		printk(KERN_WARNING "asgn2: Failed to initialise interrupt handler\n");
		return -EINVAL;
	}

	// Initialise the circular buffer	
	//circ_buf.capacity = PAGE_SIZE;
	circ_buf.capacity = PAGE_SIZE;
	circ_buf.buffer = kmalloc(sizeof (char) * circ_buf.capacity, GFP_KERNEL);
	circ_buf.readIndex = 0;
	circ_buf.writeIndex = 0;	

  atomic_set(&asgn2_device.nprocs, 0);
  atomic_set(&asgn2_device.max_nprocs, 1);

  result = alloc_chrdev_region(&asgn2_device.dev, asgn2_minor, 
                               asgn2_dev_count, MYDEV_NAME);

  if (result < 0) {
    printk(KERN_WARNING "asgn2: can't get major number\n");
    return -EBUSY;
  }

  asgn2_major = MAJOR(asgn2_device.dev);

  if (NULL == (asgn2_device.cdev = cdev_alloc())) {
    printk(KERN_WARNING "%s: can't allocate cdev\n", MYDEV_NAME);
    result = -ENOMEM;
    goto fail_cdev;
  }

  asgn2_device.cdev->ops = &asgn2_fops;
  asgn2_device.cdev->owner = THIS_MODULE;
  
  result = cdev_add(asgn2_device.cdev, asgn2_device.dev, asgn2_dev_count);
  if (result < 0) {
    printk(KERN_WARNING "%s: can't register chrdev_region to the system\n",
           MYDEV_NAME);
    goto fail_cdev;
  }
  
  /* allocate pages */
  INIT_LIST_HEAD(&asgn2_device.mem_list);
  asgn2_device.num_pages = 0;
  asgn2_device.data_size = 0;

  if (NULL == create_proc_read_entry(MYDEV_NAME, 
				     0, /* default mode */ 
				     NULL, /* parent dir */
				     asgn2_read_procmem,
				     NULL /* client data */)) {
    printk(KERN_WARNING "%s: can't create procfs entry\n", MYDEV_NAME);
    result = -ENOMEM;
    goto fail_proc_entry;
  }

  asgn2_device.cache = kmem_cache_create(MYDEV_NAME, sizeof(page_node), 
                                         0, 0, NULL); 
  
  if (NULL == asgn2_device.cache) {
    printk(KERN_WARNING "%s: can't create cache\n", MYDEV_NAME);
    result = -ENOMEM;
    goto fail_kmem_cache_create;
  }
  /* END TRIM */
 
  asgn2_device.class = class_create(THIS_MODULE, MYDEV_NAME);
  if (IS_ERR(asgn2_device.class)) {
  /* START TRIM */
    printk(KERN_WARNING "%s: can't create udev class\n", MYDEV_NAME);
    result = -ENOMEM;
    goto fail_class;
  /* END TRIM */
  }

  asgn2_device.device = device_create(asgn2_device.class, NULL, 
                                      asgn2_device.dev, "%s", MYDEV_NAME);
  if (IS_ERR(asgn2_device.device)) {
    printk(KERN_WARNING "%s: can't create udev device\n", MYDEV_NAME);
    result = -ENOMEM;
    goto fail_device;
  }
  
  printk(KERN_WARNING "set up udev entry\n");
  printk(KERN_WARNING "Hello world from %s\n", MYDEV_NAME);
  return 0;

  /* cleanup code called when any of the initialization steps fail */
fail_device:
   class_destroy(asgn2_device.class);

  /* START SKELETON */
  /* COMPLETE ME */
	
	kfree(circ_buf.buffer);

  /* PLEASE PUT YOUR CLEANUP CODE HERE, IN REVERSE ORDER OF ALLOCATION */

  /* END SKELETON */
  /* START TRIM */ 

fail_class:
   kmem_cache_destroy(asgn2_device.cache);  
fail_kmem_cache_create:
  remove_proc_entry(MYDEV_NAME, NULL /* parent dir */);
fail_proc_entry:
  cdev_del(asgn2_device.cdev);
fail_cdev:
  unregister_chrdev_region(asgn2_device.dev, asgn2_dev_count);
  /* END TRIM */
  return result;
}


/**
 * Finalise the module
 */
void __exit asgn2_exit_module(void){
	
	// Frees the circular buffer
	kfree(circ_buf.buffer);

  device_destroy(asgn2_device.class, asgn2_device.dev);
  class_destroy(asgn2_device.class);
  printk(KERN_WARNING "cleaned up udev entry\n");
  
  /* START SKELETON */
  /* COMPLETE ME */
  /**
   * free all pages in the page list 
   * cleanup in reverse order
   */
  /* END SKELETON */
	
	gpio_dummy_exit();

  /* START TRIM */
  free_memory_pages();
  kmem_cache_destroy(asgn2_device.cache);
  remove_proc_entry(MYDEV_NAME, NULL /* parent dir */);
  cdev_del(asgn2_device.cdev);
  unregister_chrdev_region(asgn2_device.dev, asgn2_dev_count);

  /* END TRIM */
  printk(KERN_WARNING "Good bye from %s\n", MYDEV_NAME);
}


module_init(asgn2_init_module);
module_exit(asgn2_exit_module);


