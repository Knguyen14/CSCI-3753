#include "stdio.h"
#include <string.h>
#include <malloc.h>
#include <fcntl.h>

#define DEVICE "dev_file"
#define BUFFER_SIZE 1024

int handle = 0;

int write_into_file(){
	int length = 0;
	char *data = (char *)malloc(BUFFER_SIZE*sizeof(char));

	scanf(" %[^\n]", data);
	length = strlen(data);
	ssize_t ret = write(handle, data, length);

	if(ret == -1) printf("Write failed\n");
	free(data);

	return 0;
}

int read_from_file(){
	int length = BUFFER_SIZE;
	char *data = (char *)malloc(BUFFER_SIZE * sizeof(char));
	memset(data,0,sizeof(data));
	ssize_t ret = read(handle, data, length);
	printf("%s\n", data);

	if(ret == -1) printf("FAILED READ\n");
	free(data);

	return 0;
}

int main(void){
	int acc = access(DEVICE,F_OK);

	if (acc == -1){ 
		printf("Module not loaded\n");
		return -1;
	}

	char in;

	while(1){
		printf("use r to read, w to write, e to exit\n");
		scanf(" %c", &in);

		switch(in){
			case 'r' :printf("Reading from device\n");
				read_from_file();
				break;
			case 'w' :printf("Writing to device\n");
				write_into_file();
				break;
			case 'e' :printf("Exiting\n");
				return 0;
				break;
			default:
				break;
			}
	}
	return 0;
}