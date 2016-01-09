#include <stdio.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <stdlib.h>
#include <linux/kernel.h>

int main(){
	int number1, number2, simple_add_return;
	int *result = malloc(sizeof(int)); // needed mallco

	number1 = -11;
	number2 = 55;

	simple_add_return = syscall(324, number1, number2, result);
	printf("Result = %d/n", *result);
}
