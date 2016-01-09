#include<linux/init.h>
#include<linux/module.h>

#include<linux/fs.h>
#include<asm/uaccess.h>
#define BUFFER_SIZE 1024

static char device_buffer[BUFFER_SIZE];
int opened = 0, closed = 0, end_of_data = 0;
static char const *device_name = "simple_character_device";

ssize_t simple_char_driver_read (struct file *pfile, char __user *buffer, size_t length, loff_t *offset)
{
	/* *buffer is the userspace buffer to where you are writing the data you want to be read from the device file*/
	/*  length is the length of the userspace buffer*/
	/*  current position of the opened file*/
	/* copy_to_user function. source is device_buffer (the buffer defined at the start of the code) and destination is the userspace 		buffer *buffer */
	int buffer_left, reading, read;

	buffer_left = (BUFFER_SIZE - *offset);
	if(buffer_left > length){
		reading = length;
	}
	else{
		reading = buffer_left;
	}
	read = reading - copy_to_user(buffer, device_buffer + *offset, reading);
	*offset += read;

	return read;
}

ssize_t simple_char_driver_write (struct file *pfile, const char __user *buffer, size_t length, loff_t *offset)
{
	/* *buffer is the userspace buffer where you are writing the data you want to be written in the device file*/
	/*  length is the length of the userspace buffer*/
	/*  current position of the opened file*/
	/* copy_from_user function. destination is device_buffer (the buffer defined at the start of the code) and source is the userspace 		buffer *buffer */
	int buffer_left, writing, written;

	if(*offset == 0){ //make sure we are at the end of the data
		*offset = end_of_data;
	}

	buffer_left = BUFFER_SIZE - *offset;

	if(buffer_left > length){
		writing = length;
	}
	else {
		writing = buffer_left;
	}

	written = writing - copy_from_user(device_buffer + *offset, buffer, writing);
	*offset += written;
	end_of_data += written;

	return written;
}

int simple_char_driver_open (struct inode *pinode, struct file *pfile)
{
	opened++;
	/* print to the log file that the device is opened and also print the number of times this device has been opened until now*/
	printk(KERN_EMERG "Opened %d times\n", opened);
	return 0;
}


int simple_char_driver_close (struct inode *pinode, struct file *pfile)
{
	closed++;
	/* print to the log file that the device is closed and also print the number of times this device has been closed until now*/
	printk(KERN_EMERG "closed %d times\n",closed);
	return 0;
}

struct file_operations simple_char_driver_file_operations = {

	.owner 	= THIS_MODULE,
	/* add the function pointers to point to the corresponding file operations. look at the file fs.h in the linux souce code*/
	.open 		= (*simple_char_driver_open),
	.release 	= (*simple_char_driver_close), // no close in fs.h
	.read 		= (*simple_char_driver_read),
	.write 		= (*simple_char_driver_write),
	
};

static int simple_char_driver_init(void)
{
	/* print to the log file that the init function is called.*/
	/* register the device */
	printk(KERN_EMERG "Initalizing simple_char_driver");
	register_chrdev(240 , device_name, &simple_char_driver_file_operations);

	return 0;
}

static int simple_char_driver_exit(void)
{
	/* print to the log file that the exit function is called.*/
	/* unregister  the device using the register_chrdev() function. */
	printk(KERN_EMERG "Exiting simple_char_driver");
	unregister_chrdev(240, device_name);

	return 0;
}

/* add module_init and module_exit to point to the corresponding init and exit function*/
module_init(simple_char_driver_init);
module_exit(simple_char_driver_exit);