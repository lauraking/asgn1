
/**
 * File: asgn1.c
 * Date: 13/03/2011
 * Author: Your Name 
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

/* TODO think through updating f_pos pointer */
/* TODO look through init setup from fb thread */
/* TODO print statements to test */
/* TODO test 	*/


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

#define MYDEV_NAME "asgn1"
#define MYIOC_TYPE 'k'

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Your Name");
MODULE_DESCRIPTION("COSC440 asgn1");


/**
 * The node structure for the memory page linked list.
 */ 
typedef struct page_node_rec {
  struct list_head list;
  struct page *page;
} page_node;

typedef struct asgn1_dev_t {
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
} asgn1_dev;

asgn1_dev asgn1_device;


int asgn1_major = 0;                      /* major number of module */  
int asgn1_minor = 0;                      /* minor number of module */
int asgn1_dev_count = 1;                  /* number of devices */
struct proc_dir_entry *proc_entry;	  /* initial proc entry */

/**
 * This function frees all memory pages held by the module.
 */
void free_memory_pages(void) {
  page_node *curr;

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
	/*
	if (asgn1_dev < 0 ) {

	} 
	while (asgn1_dev->num_pages > 0) {

	}
	*/
	

}


/**
 * This function opens the virtual disk, if it is opened in the write-only
 * mode, all memory pages will be freed.
 */
int asgn1_open(struct inode *inode, struct file *filp) {
  /* COMPLETE ME */
  /**
   * Increment process count, if exceeds max_nprocs, return -EBUSY
   *
   * if opened in write-only mode, free all memory pages
   *
   */

	/* TODO how to open virtual disk ? */
	/* TODO what about inode? not used in this function */

	atomic_inc(&asgn1_device.nprocs); 

	if (atomic_read(&asgn1_device.nprocs) > atomic_read(&asgn1_device.max_nprocs)) {

		return -EBUSY; 

	}	 

	if ((filp->f_flags) == O_WRONLY) {

		free_memory_pages();

	}
  return 0; /* success */
}


/**
 * This function releases the virtual disk, but nothing needs to be done
 * in this case. 
 */
int asgn1_release (struct inode *inode, struct file *filp) {
  /* COMPLETE ME */
  /**
   * decrement process count
   */

	atomic_dec(&asgn1_device.nprocs);

  return 0;
}


/**
 * This function reads contents of the virtual disk and writes to the user 
 */
ssize_t asgn1_read(struct file *filp, char __user *buf, size_t count,
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

	size_t size_not_read		/*size not read returned from copy to user */
  struct list_head *ptr = asgn1_device.mem_list.next;
  page_node *curr;
	
	int end_of_ram = 0;

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

	/* check f_pos if beyond data_size */
	if (f_pos > asgn1_device.data_size) {
		printk(KERN_INFO " f_pos beyond data size -> ret 0\n");
		return 0;
	} 

	list_for_each(ptr, &asgn1_device.mem_list) {
		
		curr = list_entry(ptr, page_node, list);
		if (curr_page_no == begin_page_no) {
			break;
		}
		curr_page_no++;
	}

	curr_size_read = 0;

	begin_offset = *f_pos % PAGE_SIZE;

	while (size_read < count) {
		size_to_be_read = min((PAGE_SIZE - begin_offset),(count-size_written));
	
		if (asgn1_device.data_size < (*f_pos + size_to_be_read)) {
			size_to_be_read = asgn1_device.data_size - *f_pos;
			end_of_ram = 1;
		} 
	
		size_not_read = copy_to_user(&buf[size_read], 
														page_address(curr->page) + begin_offset,
														size_to_be_read);

		if (size_not_read == size_to_be_read) {
			return -EFAULT;
		}
		
		if (size_not_read > 0) {
			size_read += (size_to_be_read - size_not_read);
			f_pos = *f_pos + size_read;
			return size_read;
		}
	
		if (end_of_ram) {
			size_read += size_to_be_read;
			break;
		}
	
		size_read += size_to_be_read;
		

	} 
	
	// after first through of loop set begin_offset to 0			
		begin_offset = 0;

		/* move pointer to next in mem_list*/
		ptr = ptr->next;

		/* retrieve the next page address and set to current page */
		curr = list_entry(ptr, page_node, list);

		/* TODO update page count */ 
		curr_page_no ++; 

	}
	
	/*TODO how to update f_pos and when to use *f_pos */
	*f_pos += size_written;
	
	
	filp->f_pos = *f_pos;

  return size_read;
}




static loff_t asgn1_lseek (struct file *file, loff_t offset, int cmd)
{
    loff_t testpos;

    size_t buffer_size = asgn1_device.num_pages * PAGE_SIZE;

    /* COMPLETE ME */
    /**
     * set testpos according to the command
     *
     * if testpos larger than buffer_size, set testpos to buffer_size
     * 
     * if testpos smaller than 0, set testpos to 0
     *
     * set file->f_pos to testpos
     */
    printk (KERN_INFO "Seeking to pos=%ld\n", (long)testpos);
    return testpos;
}


/**
 * This function writes from the user buffer to the virtual disk of this
 * module
 */
ssize_t asgn1_write(struct file *filp, const char __user *buf, size_t count,
		  loff_t *f_pos) {
  size_t orig_f_pos = *f_pos;  /* the original file position */
  size_t size_written = 0;  /* size written to virtual disk in this function */
  size_t begin_offset;      /* the offset from the beginning of a page to
			       start writing */
  int begin_page_no = *f_pos / PAGE_SIZE;  /* the first page this finction
					      should start writing to */

  int curr_page_no = 0;     /* the current page number */
  size_t curr_size_written; /* size written to virtual disk in this round */
  size_t size_to_be_written;  /* size to be read in the current round in 
				 while loop */
  
  struct list_head *ptr = asgn1_device.mem_list.next;
  page_node *curr;

	int end_page_no;				/* the highest page number to write this function call */

	page_node *ce;					/* to iteratively create page nodes */

	size_t cp_ret;						/* copy_from_user result */
  
	
	/* COMPLETE ME */
  /**
   * Traverse the list until the first page reached, and add nodes if necessary
   *
   * Then write the data page by page, remember to handle the situation
   *   when copy_from_user() writes less than the amount you requested.
   *   a while loop / do-while loop is recommended to handle this situation. 
   */

	/* check the max number of pages to be written against the device size */

	end_page_no = (*f_pos + count) / PAGE_SIZE;

	while(asgn1_device.num_pages < end_page_no) {
		ce = kmalloc(sizeof(page_node), GFP_KERNEL);
		//ce->page=virt_to(__get_free_page(GFP_KERNEL);
		ce->page = alloc_pages(GFP_KERNEL,0);
		list_add_tail(&(ce->list),&(asgn1_device.mem_list));
		asgn1_device.num_pages++;
	}

	/* TODO when to update the value of num_pages? after function */

	if (end_page_no > asgn1_device.num_pages) {
		asgn1_device.num_pages = end_page_no;
	}

	list_for_each(ptr, &asgn1_device.mem_list) {
		
		curr = list_entry(ptr, page_node, list);
		if (curr_page_no == begin_page_no) {
			break;
		}
		curr_page_no++;
	}

	/* TODO should i check to see if page correctly allocated? */
	//if (!curr->page) {
	//	curr->page = __get_free_page(GFP_KERNEL);
	//}

	begin_offset = *f_pos % PAGE_SIZE;

	while (size_written < count) {
		
		
		size_to_be_written = min((PAGE_SIZE - begin_offset),(count-size_written));

		/*size_to_be_written = PAGE_SIZE - begin_offset;
		
		if((count - size_written) > size_to_be_written) {
			size_to_be_written = count - size_written;
		} */

	/* TODO PAGE->INODE YAY LOOK UP FOR WHERE TO WRITE */
	/* TODO where exactly am i writing this to? */
		cp_ret = copy_from_user(page_address(curr->page) + begin_offset, 
														buf + size_written, 
														size_to_be_written);

		if (cp_ret == size_to_be_written) {
			return -EFAULT;
		}

		if (cp_ret > 0) {
			size_written += cp_ret;
			/* TODO where exactly is beginning of page ? or file */
			f_pos += size_written;
			if (asgn1_device.data_size < *f_pos) {
				asgn1_device.data_size = *f_pos;
			}
			return size_written;

		}

		/* update the current_size_written after write */
		size_written += size_to_be_written;
		// after first through of loop set begin_offset to 0			
		begin_offset = 0;

		/* move pointer to next in mem_list*/
		ptr = ptr->next;

		/* retrieve the next page address and set to current page */
		curr = list_entry(ptr, page_node, list);

		/* TODO update page count */ 
		curr_page_no ++; 

	}
	
	/*TODO how to update f_pos and when to use *f_pos */
	*f_pos += size_written;
	
	if (asgn1_device.data_size < *f_pos) {
		asgn1_device.data_size = *f_pos; 

	}

  asgn1_device.data_size = max(asgn1_device.data_size,
                               orig_f_pos + size_written);
  return size_written;
}

#define SET_NPROC_OP 1
#define TEM_SET_NPROC _IOW(MYIOC_TYPE, SET_NPROC_OP, int) 

/**
 * The ioctl function, which nothing needs to be done in this case.
 */
long asgn1_ioctl (struct file *filp, unsigned cmd, unsigned long arg) {
  int nr;
  int new_nprocs;
  int result;

  /* COMPLETE ME */
  /** 
   * check whether cmd is for our device, if not for us, return -EINVAL 
   *
   * get command, and if command is SET_NPROC_OP, then get the data, and
     set max_nprocs accordingly, don't forget to check validity of the 
     value before setting max_nprocs
   */

  return -ENOTTY;
}


/**
 * Displays information about current status of the module,
 * which helps debugging.
 */
int asgn1_read_procmem(char *buf, char **start, off_t offset, int count,
		     int *eof, void *data) {
  /* stub */
  int result;

  /* COMPLETE ME */
  /**
   * use snprintf to print some info to buf, up to size count
   * set eof
   */
  return result;
}


static int asgn1_mmap (struct file *filp, struct vm_area_struct *vma)
{
    unsigned long pfn;
    unsigned long offset = vma->vm_pgoff << PAGE_SHIFT;
    unsigned long len = vma->vm_end - vma->vm_start;
    unsigned long ramdisk_size = asgn1_device.num_pages * PAGE_SIZE;
    page_node *curr;
    unsigned long index = 0;

    /* COMPLETE ME */
    /**
     * check offset and len
     *
     * loop through the entire page list, once the first requested page
     *   reached, add each page with remap_pfn_range one by one
     *   up to the last requested page
     */
    return 0;
}


struct file_operations asgn1_fops = {
  .owner = THIS_MODULE,
  .read = asgn1_read,
  .write = asgn1_write,
  .unlocked_ioctl = asgn1_ioctl,
  .open = asgn1_open,
  .mmap = asgn1_mmap,
  .release = asgn1_release,
  .llseek = asgn1_lseek
};


/**
 * Initialise the module and create the master device
 */
int __init asgn1_init_module(void){
  int result; 
  int reg_result;

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

	
	/* TODO should we set result to something?? */
	/* TODO should set reg_result */
  atomic_set(&asgn1_device.nprocs, 0);
  atomic_set(&asgn1_device.max_nprocs,255);	/* TODO figure out max_nprocs */
  
  reg_result = register_chrdev_region (asgn1_device.dev, asgn1_dev_count,MYDEV_NAME);
  if (reg_result != 0) {
	printk(KERN_WARNING "error in register chrdev");
	/* reg_result = TODO set result */
	goto fail_device;
  } 

  asgn1_device.cdev = cdev_alloc();
  cdev_init(asgn1_device.cdev, &asgn1_fops);	 
  
  cdev_add(asgn1_device.cdev, asgn1_device.dev, asgn1_dev_count);
 
  INIT_LIST_HEAD(&asgn1_device.mem_list);	 
 
  /*TODO FIGURE OUT IF NECESSARY */
  proc_entry = create_proc_entry("driver/proc_entry", S_IRUGO | S_IWUSR, NULL);
  
 // proc_entry = proc_create("driver/proc_entry", S_IRUGO | S_IWUSR, NULL, asgn1_fops);
  
  
  if (!proc_entry) {	

	printk(KERN_INFO "I failed to make driver/proc__entry\n");
	goto fail_device;

  }  

  /* TODO what to do with f_ops proc_entry-> */
  printk(KERN_INFO "I created driver/proc_entry\n");

  asgn1_device.class = class_create(THIS_MODULE, MYDEV_NAME);
  if (IS_ERR(asgn1_device.class)) {
  }

  asgn1_device.device = device_create(asgn1_device.class, NULL, 
                                      asgn1_device.dev, "%s", MYDEV_NAME);
  if (IS_ERR(asgn1_device.device)) {
    printk(KERN_WARNING "%s: can't create udev device\n", MYDEV_NAME);
    result = -ENOMEM;
    goto fail_device;
  }
  
  printk(KERN_WARNING "set up udev entry\n");
  printk(KERN_WARNING "Hello world from %s\n", MYDEV_NAME);
  return 0;

  /* cleanup code called when any of the initialization steps fail */
fail_device:
   class_destroy(asgn1_device.class);

  /* COMPLETE ME */
  /* PLEASE PUT YOUR CLEANUP CODE HERE, IN REVERSE ORDER OF ALLOCATION */

  if (proc_entry) {
	remove_proc_entry("driver/proc_entry/",NULL);
  }

  /*TODO how to eliminate list_head */

	/*TODO check order of steps below */

	/* free cdev */
	kfree(asgn1_device.cdev);

	/* delete the cdev */
	cdev_del(asgn1_device.cdev);
	
	/* unregister device */
	unregister_chrdev_region(asgn1_device.dev, asgn1_dev_count);

 
  
  return result;
}


/**
 * Finalise the module
 */
void __exit asgn1_exit_module(void){
  device_destroy(asgn1_device.class, asgn1_device.dev);
  class_destroy(asgn1_device.class);
  printk(KERN_WARNING "cleaned up udev entry\n");
  
  /* COMPLETE ME */
  /**
   * free all pages in the page list 
   * cleanup in reverse order
   */
  
	/* TODO what is different from cleanup above ? */
	free_memory_pages();

	/*TODO could be multiple processes */	
	if (proc_entry) {
		remove_proc_entry("driver/proc_entry/",NULL);
	} 

	/* TODO go through list_head and deallocate!! */

  /* free cdev */
	kfree(asgn1_device.cdev);
	
	/*delete the cdev */
	cdev_del(asgn1_device.cdev);

	/*unregister the device */
	unregister_chrdev_region(asgn1_device.dev, asgn1_dev_count);

	printk(KERN_WARNING "Good bye from %s\n", MYDEV_NAME);
}


module_init(asgn1_init_module);
module_exit(asgn1_exit_module);


