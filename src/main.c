#include "disk.h"
#include "sfs.h"
#include <stdlib.h>
#include <stdio.h>

int main(){
	disk* diskptr = create_disk(1000*BLOCKSIZE+24);
	if(diskptr == NULL) {
		printf("test\n");
	}
	printf("Format: %d\n", format(diskptr));
	printf("mount: %d\n", mount(diskptr));
	int inode;
	// for(int i=0; i<4; i++) {
	// 	inode = create_file();
	// 	printf("The inode of file is %d\n", inode); 
	// }
	// int* data = (int*) malloc(BLOCKSIZE*1000);
	// for(int i=0; i<BLOCKSIZE*1000/4; i++) {
	// 	data[i] = -i;
	// }
	// printf("Write:%d\n", write_i(inode, data, BLOCKSIZE*1000, 0));
	// stat(inode);
	// int* data2 = (int*) malloc(BLOCKSIZE*10);
	// printf("Read:%d\n", read_i(inode, data2, 10*BLOCKSIZE, 0));
	// for(int i=0; i<BLOCKSIZE*10/4; i++) {
	// 	//printf("%d %d\n", data[i], data2[i]);
	// }
	// printf("create: %d\n", create_file());
	// printf("write2: %d\n", write_i(4, data, 10*BLOCKSIZE, 0));
	// stat(4);
	// fit_to_size(inode, 6*BLOCKSIZE);
	// stat(inode);
	// printf("write3: %d\n", write_i(4, data, 10*BLOCKSIZE, 0));
	// stat(4);
	// remove_file(inode);
	// free_disk(diskptr);
	init_dirsys(diskptr);
	printf("%d\n", create_dir("usr"));
	printf("%d\n", create_dir("usr/srihas"));
	printf("%d\n", create_file_by_path("usr/srihas/file1"));
	int* data = (int*) malloc(BLOCKSIZE*50);
	for(int i=0; i<BLOCKSIZE*50/4; i++) {
		data[i] = -i;
	}
	printf("Write:%d\n", write_file("/usr/srihas/file1", data, BLOCKSIZE*50, 0));
	int* data2 = (int*) malloc(BLOCKSIZE*50);
	printf("Read:%d\n", read_file("/usr/srihas/file1", data2, 40*BLOCKSIZE, 0));
	for(int i=0; i<BLOCKSIZE*40/4; i++) {
		printf("%d %d\n", data[i], data2[i]);
	}
	printf("%d\n", remove_dir("/usr"));
	return 0;
}