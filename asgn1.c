
/**
 * File: asgn1.c
 * Date: 13/03/2011
 * Author: Laura Kingsley
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
MODULE_AUTHOR("Laura Kingsley");
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
struct proc_dir_entry *proc_entry;	  		/* initial proc entry */
struct proc_dir_entry *maj_min_num_proc;		/* major number proc entry */

/**
 * This function frees all memory pages held by the module.
 */
void free_memory_pages(void) {
  page_node *curr;
	struct list_head *tmp;
	/*struct list_head *ptr = asgn1_device.mem_list.next; */
	struct list_head *ptr;
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
	
	printk(KERN_WARNING "WANT TO FREE %d PAGES FROM %s\n",
														asgn1_device.num_pages,
														MYDEV_NAME);
	list_for_each_safe(ptr, tmp,  &asgn1_device.mem_list) {
	
		curr = list_entry(ptr, page_node, list);
		if (curr->page) {
			__free_page(curr->page);
		}
		
		list_del(&curr->list);
		kfree(curr);		
	
	}	 
		
	asgn1_device.data_size = 0;
	asgn1_device.num_pages = 0; 
	printk(KERN_WARNING "FREE PAGES SUCCESS \n");


}


/**
 * This function opens the virtual disk, if it is opened in the write-only
 * mode, all memory pages will be freed.
 */
int asgn1_open(struct inode *inode, struct file *filp) {
  /**
   * Increment process count, if exceeds max_nprocs, return -EBUSY
   *
   * if opened in write-only mode, free all memory pages
   *
   */

	/* increment number of processes accessing device*/
	atomic_inc(&asgn1_device.nprocs); 

	/* check the number of processes against the max number of processes*/
	if (atomic_read(&asgn1_device.nprocs) > atomic_read(&asgn1_device.max_nprocs)) {

		return -EBUSY; 

	}	 

	/* check the APPEND flag and reset to file positin to EOF */
	if (filp->f_flags & O_APPEND) {
		filp->f_pos = asgn1_device.data_size;
	} 

	/* if the file is written is WRONLY and O_TRUNC then free memory pages */
	else if ((filp->f_flags & O_WRONLY) && (filp->f_flags & O_TRUNC)) {
		free_memory_pages();
	}
  return 0; /* success */
}


/**
 * This function releases the virtual disk, but nothing needs to be done
 * in this case. 
 */
int asgn1_release (struct inode *inode, struct file *filp) {
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

	size_t size_not_read;		/*size not read returned from copy to user */
  struct list_head *ptr = asgn1_device.mem_list.next;
  page_node *curr;
	
	int end_of_ram = 0;

	size_t adjust_data_size;

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
	if (*f_pos >= asgn1_device.data_size) {
		printk(KERN_WARNING "BEGINING OF READ CHECK -> ret 0\n");
		return 0;
	} 
	
	printk(KERN_WARNING "INITAL READ CONDITIONS\n");
	printk(KERN_WARNING "read data_size= %d\n", asgn1_device.data_size);
	printk(KERN_WARNING "read f_pos = %u\n", *f_pos);
	printk(KERN_WARNING "SEEK BEGIN PAGE NO: %d\n",begin_page_no);
	
	/* traverse through page list to access first read page*/
	list_for_each(ptr, &asgn1_device.mem_list) {
		
		curr = list_entry(ptr, page_node, list);
		
		if (curr_page_no == begin_page_no) {
			/* found first page -> break to start reading*/
			printk(KERN_WARNING "FOUND PAGE NO: %d\n",curr_page_no);
			break;
		}
		curr_page_no++;
	}

	/* calculate beginning offset of first page*/
	begin_offset = *f_pos % PAGE_SIZE;
	printk(KERN_WARNING "BEGIN OFFSET: %d\n",begin_offset);
	
	/* adjust the data size applicable*/
	adjust_data_size = asgn1_device.data_size - begin_offset;

	printk(KERN_WARNING "ADJUSTED D SIZE: %d\n",adjust_data_size);

	adjust_data_size = min((int)(count - begin_offset),(int)adjust_data_size);
	printk(KERN_WARNING "A D_SIZE after count-offset compare:%d\n",adjust_data_size);
	
	/* check that count is not beyond the data size */
	if (*f_pos + count > asgn1_device.data_size) {
		/* *f_pos + count is greater than data size */

		printk(KERN_WARNING "*F_POS + COUNT GREATER THAN DATA SIZE\n");
		printk(KERN_WARNING "COUNT SHRUNK FROM %d TO ",count);
		/* adjust count to available left to read*/
		count = asgn1_device.data_size - *f_pos;
		printk(KERN_WARNING "%d\n",count);
		
	}

	printk(KERN_WARNING "\nENTER READ WHILE LOOP!!\n");
	
	/* begin read while loop*/
	while (size_read < adjust_data_size) {	
		printk(KERN_WARNING "READ ADJUST DATA SIZE= %d\n", adjust_data_size);
		printk(KERN_WARNING "SIZE READ = %d\n", size_read);
		printk(KERN_WARNING "FPOS = %u\n", *f_pos);
		
		/* calculate size to be read in this run of loop*/
		size_to_be_read = min((int)(PAGE_SIZE - begin_offset),(int)(count-size_read));
		printk(KERN_WARNING "WANT TO READ = %d\n", size_to_be_read);
	
		printk(KERN_WARNING "ON PAGE %d\n", curr_page_no);

		/* copy size to user buffer*/
		size_not_read = copy_to_user(buf + size_read, 
														page_address(curr->page) + begin_offset,
														size_to_be_read);

		
		printk(KERN_WARNING "SIZE NOT READ = %d\n", size_not_read);
		/* calculate the size read during copy function*/
		curr_size_read = size_to_be_read - size_not_read;
	
		printk(KERN_WARNING "USER READ %d bytes\n",curr_size_read);

		/* update file position as result of read*/
		*f_pos += curr_size_read;
		printk(KERN_WARNING "UPDATED F_POS = %u\n", *f_pos);
 
		/* update size read*/
		size_read += curr_size_read;
		printk(KERN_WARNING "TOTAL SIZE READ = %d \n",size_read);

		/* if the total size read equals amount supposed to read*/
		if (size_read == adjust_data_size){
			printk(KERN_WARNING "SIZE_READ == ADJUST_DATA_SIZE\n");
			printk(KERN_WARNING "RETURN size_read TO USER\n");
			return size_read;

		} 

		/* check if copied nothing */
		if (size_not_read == size_to_be_read) {
			printk(KERN_WARNING "COPY TO USER COPIED NOTHING -> RET -EFAULT\n");
			return -EFAULT;
		}
		
		/* check if copy did not complete fully*/
		if (size_not_read > 0) {
			/* break loop and return size read up to now to user*/
			printk(KERN_WARNING "SIZE_NOT READ = %d\n",size_not_read);
			printk(KERN_WARNING "SIZE_NOT_READ > 0 -> break while loop\n");
			break;
		}
	
		/* after first through of loop set begin_offset to 0*/			
		begin_offset = 0;

		/* move pointer to next in mem_list*/
		ptr = ptr->next;

		/* retrieve the next page address and set to current page */
		curr = list_entry(ptr, page_node, list);

		/* update page count */ 
		curr_page_no ++; 

	}

	printk(KERN_WARNING "OUT OF READ WHILE LOOP\n");
	printk(KERN_WARNING "RETURN SIZE_READ: %d TO USER\n",size_read);
  return size_read;
}




static loff_t asgn1_lseek (struct file *file, loff_t offset, int cmd)
{
    loff_t testpos;
		
    size_t buffer_size = asgn1_device.num_pages * PAGE_SIZE;
		
		printk(KERN_WARNING "ENTER LSEEK\n");

    /**
     * set testpos according to the command
     *
     * if testpos larger than buffer_size, set testpos to buffer_size
     * 
     * if testpos smaller than 0, set testpos to 0
     *
     * set file->f_pos to testpos
     */

		/* check cmd against the valid lseek instructions */
		if (cmd == SEEK_SET) {
				/* setting position relative to the beginning of the file */
				testpos = offset;
		
		} else if (cmd == SEEK_CUR) {
				/* setting position relative to the current position */
				testpos = file->f_pos + offset;
		
		} else if (cmd == SEEK_END) {
				/* setting position relative to the end of the file */
				testpos = asgn1_device.data_size + offset;
		
		} else {
				/* INVALID CMD -> EXIT */
				/* CMD != SEEK_SET, SEEK_CUR, SEEK_END */
				return -EINVAL; 

		}

		if (testpos > asgn1_device.data_size || testpos < 0) {
			/*invalid proposition */
			/* return error */
			printk(KERN_WARNING "invalid lseek: testpos = %ld\n",(long) testpos);
			printk(KERN_WARNING "return -EINVAL\n");
			return -EINVAL;

		}
		
		/* new value passed so set new file position */
		file->f_pos = testpos;
		
    printk (KERN_WARNING "Seeking to pos=%ld\n", (long)testpos);
		printk (KERN_WARNING "EXIT LSEEK\n");
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

	size_t size_not_written;						/* copy_from_user result */
  
	
  /**
   * Traverse the list until the first page reached, and add nodes if necessary
   *
   * Then write the data page by page, remember to handle the situation
   *   when copy_from_user() writes less than the amount you requested.
   *   a while loop / do-while loop is recommended to handle this situation. 
   */


	printk(KERN_WARNING "IN WRITE\n");
	
	/* Calculate the end ending page of write and compare to current number of pages*/
	end_page_no = (*f_pos + count -1) / PAGE_SIZE;
	printk(KERN_WARNING "END PAGE NUMBER = %d\n",end_page_no);
	printk(KERN_WARNING "DEVICE PAGES = %d\n",asgn1_device.num_pages);
	
	/* malloc space for pages and add to list as needed*/
	while(asgn1_device.num_pages <= end_page_no) {
		ce = kmalloc(sizeof(page_node), GFP_KERNEL);
		/* check for null pointer indicating no memory available */
		if (!(ce)) {
			printk(KERN_WARNING "kmalloc fail: ce -> NULL pointer-> exit\n");
			return -ENOMEM;/*failure*/
		}
		
		/* allocate page and point to the page from current node*/
		ce->page = alloc_pages(GFP_KERNEL,0);
	
		/* add this node to the tail of the page memory list*/
		list_add_tail(&(ce->list),&(asgn1_device.mem_list));
		
		printk(KERN_WARNING "CREATED PAGE NO: %d\n",asgn1_device.num_pages);
		
		/* increase device number of pages by one*/
		asgn1_device.num_pages++;
	
	}

	printk(KERN_WARNING "UPDATED NUM PAGES: %d\n",asgn1_device.num_pages);

	/* traverse memory list to find starting node */
	printk(KERN_WARNING "LOOKING FOR BEG PAGE: %d",begin_page_no);
	list_for_each(ptr, &asgn1_device.mem_list) {
		
		curr = list_entry(ptr, page_node, list);
		if (curr_page_no == begin_page_no) {
			/* found beginning page -> break to start writing*/
			printk(KERN_WARNING "FOUND PAGE %d \n",curr_page_no);
			break;
		} 
		printk(KERN_WARNING "C_PG: %d != B_PG: %d\n",curr_page_no,begin_page_no);
		
		/* increase page number to continue searching for beginning node*/
		curr_page_no++;
	}

	/* calculate write page offset */
	begin_offset = *f_pos % PAGE_SIZE;

	printk(KERN_WARNING "BEGIN F_POS : %u\n",*f_pos);
	printk(KERN_WARNING "BEGIN OFFSET: %d\n",begin_offset);
	printk(KERN_WARNING "SIZE_WRITTEN: %d\nCOUNT: %d\n",size_written,count);
	
	/* begin write while loop */
	while (size_written < count) {
		printk(KERN_WARNING "\nIN WRITE WHILE LOOP!!\n");	
		printk(KERN_WARNING "LOOP F_POS : %u\n",*f_pos);
		printk(KERN_WARNING "LOOP OFFSET: %d\n",begin_offset);
		printk(KERN_WARNING "LOOP SIZE_WRITTEN: %d\nCOUNT: %d\n\n",
																									size_written,count);
		
		/* calcualte size to be written in turn of while loop*/
		/* limited by size of page and offset or the last page indicated by count*/
		size_to_be_written = min((int)(PAGE_SIZE - begin_offset),(int)(count-size_written));

		printk(KERN_WARNING "SIZE TO BE WRITTEN: %d bytes\n",
																									size_to_be_written);
		printk(KERN_WARNING "ON PAGE: %d\n", curr_page_no);
		
		/* get data from user to write to file in size_to_be_written increment*/
		size_not_written = copy_from_user(
														page_address(curr->page) + begin_offset, 
														buf + size_written, 
														size_to_be_written);

		printk(KERN_WARNING "SIZE NOT WRITTEN: %d\n",size_not_written);

		/* calculate current size written from the copy funciton */
		curr_size_written = size_to_be_written - size_not_written;
		printk(KERN_WARNING "WROTE: %d TO PAGE: %d\n",
														curr_size_written,curr_page_no);

		if (size_to_be_written == size_not_written) {
			/* error in copy from user*/
			/* zero bytes were written*/
			/* exit loop before updating any variables*/
			printk(KERN_WARNING "curr_size_written is 0 return \n");
			return -EFAULT; 
		}

		/* update the current_size_written after write */
		size_written += curr_size_written;
		printk(KERN_WARNING "UPDATE SIZE_WRITTEN: %d\n",size_written);
		
		/* update file position */
		printk(KERN_WARNING "MOVE F_POS FROM %u to\n",*f_pos);
		*f_pos += curr_size_written;			
		printk(KERN_WARNING "UPDATE F POS: %u\n",*f_pos);
		
		if (size_not_written > 0) {
			/* size not written in copy to user indicates error*/
			/* exit loop and return the amount written up to this point*/
			printk(KERN_WARNING "SIZE NOT WRITTEN > 0 -> EXIT LOOP");
			break;
		}

		/* after first through of loop set begin_offset to 0 */			
		begin_offset = 0;
		printk(KERN_WARNING "RESET BEGIN OFFSET TO %d\n",begin_offset);
		
		/* move pointer to next in mem_list*/
		ptr = ptr->next;

		/* retrieve the next page address and set to current page */
		curr = list_entry(ptr, page_node, list);

		/* update page count */ 
		curr_page_no ++; 
			printk(KERN_WARNING "NEXT PAGE NO IS : %d\n\n",curr_page_no);
	}
	/* out of write while loop*/
	
	/* updating device data size after write completion*/
	printk(KERN_WARNING "UPDATE DATASIZE FROM %d ",asgn1_device.data_size);
  asgn1_device.data_size = max(asgn1_device.data_size,
                               orig_f_pos + size_written);
	printk(KERN_WARNING "TO %d\n",asgn1_device.data_size);
	printk(KERN_WARNING "TOTAL WRITTEN = %d bytes\n\nEXIT WRITE\n",
																										size_written);
  return size_written;
}

#define SET_NPROC_OP 1
#define TEM_SET_NPROC _IOW(MYIOC_TYPE, SET_NPROC_OP, int) 
#define GET_DATA_SIZE 2
#define TEM_GET_DSIZE _IOR(MYIOC_TYPE, GET_DATA_SIZE, size_t)
#define GET_MAJOR 3
#define TEM_AVAIL_DATA _IOR(MYIOC_TYPE, GET_MAJOR, int)

/**
 * The ioctl function, which nothing needs to be done in this case.
*/
long asgn1_ioctl (struct file *filp, unsigned cmd, unsigned long arg) {
  int nr;
  int new_nprocs;
  int result;
	size_t avail;
  /** 
   * check whether cmd is for our device, if not for us, return -EINVAL 
   *
   * get command, and if command is SET_NPROC_OP, then get the data, and
     set max_nprocs accordingly, don't forget to check validity of the 
     value before setting max_nprocs
   */
	/*Check if command is for our device */
	if (_IOC_TYPE(cmd) != MYIOC_TYPE) {
		printk(KERN_WARNING "CMD IS NOT FOR OUR DEVICE -> RETURN ERROR\n");
		return -EINVAL;
	}	

	
	printk(KERN_WARNING "IN IOCTL \n");
	/* get sequential number of the command with the device */
	nr = _IOC_NR(cmd);
	printk(KERN_WARNING "NR = %d\n",nr);
	printk(KERN_WARNING "SET_NPROC_OP value = %d\n",SET_NPROC_OP);
	printk(KERN_WARNING "CMD = %u\n",cmd); 
	
	/* switch the ioctl command versus the set of valid commands for device*/
	switch (nr) {
		/* set number of processes command*/
		case SET_NPROC_OP:
			printk(KERN_WARNING "CMD = SET_NPROC_OP\n"); 
			/* get value for n_procs from user*/
			result = copy_from_user((int*) &new_nprocs, arg, sizeof(int));
			/* result check */			
			if (result < 0 ) {
				printk(KERN_WARNING "MMAP SET_N_PROCS copy from user failure");
				return -EINVAL;
			}
			
			printk(KERN_WARNING "new_nprocs = %d\n",new_nprocs);

			/* check for valid new max number of processes*/
			if (new_nprocs < atomic_read(&asgn1_device.nprocs)) {
					printk(KERN_WARNING "%d new_nprocs INVALID -> return error\n",new_nprocs);
					return -EINVAL;
			}
			
			/* set the new max to the given value from user*/			
  		atomic_set(&asgn1_device.max_nprocs,new_nprocs);	
			printk(KERN_WARNING "NEW MAX_NPROCS: %d\n",atomic_read(&asgn1_device.max_nprocs));

			return 0;

		case GET_DATA_SIZE:
			/* command to push the value of the device data size to user*/
			printk(KERN_WARNING "CMD = GET_DATA_SIZE");
			result = put_user(asgn1_device.data_size, (size_t*)arg);
			/* check the put_user result */
			if (result < 0 ) {
				printk(KERN_WARNING "MMAP GET_DATA_SIZE put_user failure");
				return -EINVAL;
			}
			return 0;
		
		case GET_MAJOR:
			/* command to send the major number of device to user*/
			printk(KERN_WARNING "CMD = GET_MAJOR");
			printk(KERN_WARNING "MAJOR NUM= %d\n",asgn1_major);
			result = put_user(asgn1_major, (int *)arg);
			/* check put_user result for failure*/
			if (result < 0 ) {
				printk(KERN_WARNING "MMAP GET_MAJOR put_user failure");
				return -EINVAL;
			}

			return 0;
		
	
	
	default:
			printk(KERN_WARNING "cmd did not match any of cases -> return error\n");
			return -EINVAL;
	}

  return -ENOTTY;/* error in switch case*/
}

/**
 * Displays information about current status of the module,
 * which helps debugging.
 */
int asgn1_read_procmem(char *buf, char **start, off_t offset, int count,
		     int *eof, void *data) {
  /* stub */
  int result;

  /**
   * use snprintf to print some info to buf, up to size count
   * set eof
   */
	
	/* check the available buffer size against the minimum number of bytes needed */
	if (count < 61)  {
		printk(KERN_WARNING "NOT ENOUGH BUFFER SPACE FOR PROCMEM OUTPUT\n");
		return -EINVAL;
	}

	/* print results to user*/
	result = sprintf(buf,"DEVICE DATA SIZE: %d bytes\nNUM PAGES: %d\nMAX PROCS: %d\n",
										asgn1_device.data_size,
										asgn1_device.num_pages,
										atomic_read(&asgn1_device.max_nprocs));
	/* set eof to 1 */
	*eof = 1;											
  return result;
}

/**
 * Displays information about current status of the module,
 * which helps debugging.
 */

/* read function for the major and minor number proc entry*/
/* prints out device major and minor number to user*/
int asgn1_read_nums(char *buf, char **start, off_t offset, int count,
		     int *eof, void *data) {
 
  int result;

  /**
   * use snprintf to print some info to buf, up to size count
   * set eof
   */
	
	/* check the available buffer size against the minimum number of bytes needed */
	if (count < 61)  {
		printk(KERN_WARNING "NOT ENOUGH BUFFER SPACE FOR PROCMEM OUTPUT\n");
		return -EINVAL;
	}

	result = sprintf(buf,"Device major number %d \nDevice minor number: %d\n",
										asgn1_major, asgn1_minor);
	/* set eof to 1 */
	*eof = 1;											
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
		unsigned long last_req = (len/PAGE_SIZE) + offset;

    /**
     * check offset and len
     *
     * loop through the entire page list, once the first requested page
     *   reached, add each page with remap_pfn_range one by one
     *   up to the last requested page
     */

		printk(KERN_WARNING "ENTERED MMAP FUNCTION\n");

		/* checks before mmap execution */
		
		printk(KERN_WARNING "last req page: %u\n",last_req);
		/* mapping loop */
		list_for_each_entry(curr, &asgn1_device.mem_list, list) {

			/* check the starting and ending condition of mapping pages*/
			if (index >= offset && last_req>index) {
				/* retrieve page frame number of current page*/
				pfn = page_to_pfn(curr->page);
				printk(KERN_WARNING "PFN = %ld\n",pfn);
				
				/* remap function for mapping current page*/
				if (remap_pfn_range (vma,
														vma->vm_start + PAGE_SIZE * (index - offset),
														pfn,
														PAGE_SIZE,
														vma->vm_page_prot)) {
					return -EAGAIN;
	
				}	 
				printk(KERN_WARNING "remap success for index: %ld \n", index);
				
			}
			/* increment the page index*/
			index++;
		} 
		
		printk(KERN_WARNING "EXITING MMAP FUNCTION SUCCESS\n");
	  
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
  
  /**
   * set nprocs and max_nprocs of the device
   *
   * allocate major number
   * allocate cdev, and set ops and owner field 
   * add cdev
   * initialize the page list
   * create proc entries
   */

	printk(KERN_WARNING " IN INIT MODULE");	
  
	/* initialize the number of processes*/
	atomic_set(&asgn1_device.nprocs, 0);
  atomic_set(&asgn1_device.max_nprocs,16);	  
	
	/* allocate character device region*/
  result = alloc_chrdev_region (
						&asgn1_device.dev,
						asgn1_minor, 
						asgn1_dev_count,
						MYDEV_NAME);
  
	/* check result of allocation*/
	if (result < 0) {
		printk(KERN_WARNING "error in register chrdev");
		goto fail_device;
  } 

	/*set up major number */
	asgn1_major = MAJOR(asgn1_device.dev);

	/*allocate cdev region */
	asgn1_device.cdev = cdev_alloc();
	asgn1_device.cdev->ops = &asgn1_fops;
	asgn1_device.cdev->owner = THIS_MODULE;
  
  result = cdev_add(asgn1_device.cdev, asgn1_device.dev, asgn1_dev_count);
 
	/*check that the cdev added successfully */
	if (result<0) {
		printk(KERN_WARNING "Unable to add cdev");
		goto fail_device;
	} 

  INIT_LIST_HEAD(&asgn1_device.mem_list);	 
	
	/* create and initialize first proc entry for device*/
  proc_entry = create_proc_entry("driver/procmem", S_IRUGO | S_IWUSR, NULL);
  
  if (!proc_entry) {	
		/* create proc entry failed*/
		printk(KERN_WARNING "I failed to make driver/procmem\n");
		goto fail_device;

  }  

	/* set read function for proc*/
	proc_entry->read_proc = asgn1_read_procmem;
	printk(KERN_WARNING "I created driver/procmem\n");
  
	/* create major minor number proc */
  maj_min_num_proc = create_proc_entry("driver/numbers", S_IRUGO | S_IWUSR, NULL);
  
  if (!maj_min_num_proc) {	
		/* creating major minor number proc failed*/
		printk(KERN_WARNING "I failed to make driver/numbers\n");
		goto fail_device;
  }  

	/* set read function for major minor number proc*/
	maj_min_num_proc->read_proc = asgn1_read_nums;

	printk(KERN_WARNING "I created driver/numbers\n");

	/* create device class*/
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

	/* initialize data size to 0*/
	asgn1_device.data_size = 0;
  
  printk(KERN_WARNING "set up udev entry\n");
  printk(KERN_WARNING "Hello world from %s\n", MYDEV_NAME);
  return 0;

  /* cleanup code called when any of the initialization steps fail */
fail_device:
   class_destroy(asgn1_device.class);

  /* CLEANUP CODE */
	
	/* if the proc entries exist, remove them*/
	if (proc_entry) {
		remove_proc_entry("driver/procmem",NULL);
	} 

	if (maj_min_num_proc) {
		remove_proc_entry("driver/numbers",NULL);
	}

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
  
  /**
   * free all pages in the page list 
   * cleanup in reverse order
   */
  
	/* free memory pages*/
	free_memory_pages();

	/* if the proc entries exist, remove them*/	
	if (proc_entry) {
		remove_proc_entry("driver/procmem",NULL);
	} 

	if (maj_min_num_proc) {
		remove_proc_entry("driver/numbers",NULL);
	}

	/*delete the cdev */
	cdev_del(asgn1_device.cdev);

	/*unregister the device */
	unregister_chrdev_region(asgn1_device.dev, asgn1_dev_count);

	printk(KERN_WARNING "Good bye from %s\n", MYDEV_NAME);
}

module_init(asgn1_init_module);
module_exit(asgn1_exit_module); 
