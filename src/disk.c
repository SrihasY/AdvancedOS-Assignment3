#include "disk.h"
#include <stdlib.h>
#include <string.h>

disk* create_disk(int nbytes) {
    disk* diskptr;
    if(nbytes < sizeof(disk)) {
        //disk too small
        return NULL;
    } else {
        diskptr = (disk*) malloc(sizeof(disk));
        if(diskptr==NULL) {
            //error in stat block
            return NULL;
        }
        diskptr->size = nbytes;
        diskptr->reads = 0;
        diskptr->writes = 0;
        diskptr->blocks = (nbytes-sizeof(disk))/BLOCKSIZE;
        diskptr->block_arr = (char **) malloc(diskptr->blocks * sizeof(char*));
        if(diskptr->block_arr==NULL) {
            //error in array of pointers
            free(diskptr);
            return NULL;
        }
        for(int i=0; i<diskptr->blocks; i++) {
            diskptr->block_arr[i] = (char *) malloc(BLOCKSIZE*sizeof(char));
            if(diskptr->block_arr[i]==NULL) {
                //error allocating disk memory
                for(int j=0; j<i; j++) {
                    free(diskptr->block_arr[j]);
                }
                free(diskptr);
                return NULL;
            }
        }
    }
    return diskptr;
}

int read_block(disk *diskptr, int blocknr, void *block_data) {
    if(blocknr < 0 || blocknr > (int) diskptr->blocks-1) {
        return -1;
    }
    memcpy(block_data, diskptr->block_arr[blocknr], BLOCKSIZE);
    diskptr->reads++;
    return 0;
}

int write_block(disk *diskptr, int blocknr, void *block_data) {
    if(blocknr < 0 || blocknr > (int) diskptr->blocks-1) {
        return -1;
    }
    memcpy(diskptr->block_arr[blocknr], block_data, BLOCKSIZE);
    diskptr->writes++;
    return 0;
}

int free_disk(disk *diskptr) {
    for(int i=0; i<(int) diskptr->blocks; i++) {
        free(diskptr->block_arr[i]);
    }
    free(diskptr->block_arr);
    free(diskptr);
    return 0;
}