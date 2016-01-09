#include <linux/kernel.h>
#include <linux/linkage.h>

asmlinkage long sys_helloworld(void){
	printk(KERN_EMERG "hello world\n");
	return 0;
}
