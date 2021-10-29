#include "disk.h"
#include "sfs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define SetBit(A,k)     ( A[(k/32)] |= (1 << (k%32)) )
#define ClearBit(A,k)   ( A[(k/32)] &= ~(1 << (k%32)) )
#define TestBit(A,k)    ( A[(k/32)] & (1 << (k%32)) )

#define MAXBLOCKS 1029
static disk* mountptr;

int format(disk *diskptr) {
    if(!diskptr)
        return -1;
    super_block sb;
    uint32_t M = diskptr->blocks-1;
    uint32_t I = 0.1*M;
    uint32_t IB = ceil(I/(double)256);
    uint32_t R = M-I-IB;
    uint32_t DBB = ceil(R/(double)32768);
    uint32_t DB = R-DBB;
    sb.magic_number = MAGIC;
    sb.blocks = M;
    sb.inode_blocks = I;
    sb.inodes = I*128;
    sb.inode_bitmap_block_idx = 1;
    sb.inode_block_idx = 1 + IB + DBB;
    sb.data_block_bitmap_idx = 1 + IB;
    sb.data_block_idx = 1 + IB + DBB + I;
    sb.data_blocks = DB;
    void* buffer = malloc(BLOCKSIZE);
    memcpy(buffer, &sb, sizeof(super_block));
    if(write_block(diskptr, 0, buffer) < 0) {
        free(buffer);
        return -1;
    }
    memset(buffer, 0, BLOCKSIZE);
    for(int i=sb.inode_bitmap_block_idx; i<sb.data_block_idx; i++) {
        if(write_block(diskptr, i, buffer) < 0) {
            free(buffer);
            return -1;
        }
    }
    free(buffer);
    return 0;
}

int mount(disk *diskptr) {
    if(!diskptr)
        return -1;
    super_block* sb = (super_block*) malloc(BLOCKSIZE);
    int retval;
    if(!sb || read_block(diskptr, 0, sb) < 0 || sb->magic_number != MAGIC) {
        retval = -1;
    } else {
        mountptr = diskptr;
        retval = 0;
    }
    free(sb);
    return retval;
}

int find_free_inode(super_block* sb) {
    int retval = 0;
    if(!mountptr) {
        printf("ERR: Disk not mounted.\n");
        return -1;
    }
    uint32_t* buffer = (uint32_t*) malloc(BLOCKSIZE);
    if(!buffer) {
        return -1;
    }
    for(int i= sb->inode_bitmap_block_idx; i<sb->data_block_bitmap_idx; i++) {
        if(read_block(mountptr, i, buffer) < 0) {
            retval = -1;
            break;
        }
        int found = 0;
        for(int j=0; j<BLOCKSIZE/4; j++) {
            if(buffer[j] < (UINT32_MAX)) {
                uint32_t* wordstart = buffer+j;
                for(int k=0; k<32; k++) {
                    if(!TestBit(wordstart, k)) {
                        if((i-sb->inode_bitmap_block_idx)*8*BLOCKSIZE+j*32+k >= sb->inode_blocks) {
                            retval = -1;
                            break;
                        }
                        SetBit(wordstart,k);
                        if(write_block(mountptr, i, buffer) < 0) {
                            retval = -1;
                            break;
                        }
                        retval = (i-sb->inode_bitmap_block_idx)*8*BLOCKSIZE+j*32+k;
                        break;
                    }
                }
                found = 1;
                break;
            }
        }
        if(found)
            break;
    }
    free(buffer);
    return retval;
}

int find_free_datablock(super_block* sb) {
    int retval = -1;
    if(!mountptr) {
        printf("ERR: Disk not mounted.\n");
        return -1;
    }
    uint32_t* buffer = (uint32_t*) malloc(BLOCKSIZE);
    if(!buffer) {
        return -1;
    }
    for(int i= sb->data_block_bitmap_idx; i<sb->data_block_idx; i++) {
        if(read_block(mountptr, i, buffer) < 0) {
            retval = -1;
            break;
        }
        int found = 0;
        for(int j=0; j<BLOCKSIZE/4; j++) {
            if(buffer[j] < (UINT32_MAX)) {
                uint32_t* wordstart = buffer+j;
                for(int k=0; k<32; k++) {
                    if(!TestBit(wordstart, k)) {
                        if((i-sb->data_block_bitmap_idx)*8*BLOCKSIZE+j*32+k >= sb->data_blocks) {
                            printf("limit reached. %d", sb->data_blocks);
                            retval = -1;
                            break;
                        }
                        SetBit(wordstart,k);
                        if(write_block(mountptr, i, buffer) < 0) {
                            retval = -1;
                            break;
                        }
                        retval = (i-sb->data_block_bitmap_idx)*8*BLOCKSIZE+j*32+k;
                        break;
                    }
                }
                found = 1;
                break;
            }
        }
        if(found)
            break;
    }
    free(buffer);
    return retval;
}

int free_inode_bitmap(int inumber, super_block* sb) {
    int k;
    if(!mountptr) {
        printf("ERR: Disk not mounted.\n");
        return -1;
    }
    if(inumber < 0 || inumber > sb->inodes) {
        return -1;
    }
    int* buffer = (int*) malloc(BLOCKSIZE);
    if(!buffer) {
        return -1;
    }
    if(read_block(mountptr, (int) sb->inode_bitmap_block_idx + inumber/(BLOCKSIZE*8), buffer) < 0) {
        free(buffer);
        return -1;
    }
    int block_inumber = inumber%(BLOCKSIZE*8);
    ClearBit(buffer, block_inumber);
    if(write_block(mountptr, (int) sb->inode_bitmap_block_idx + inumber/(BLOCKSIZE*8), buffer) < 0) {
        free(buffer);
        return -1;
    }
    free(buffer);
    return 0;
}

int create_file() {
    inode* new_inode;
    if(!mountptr) {
        printf("ERR: Disk not mounted.\n");
        return -1;
    }
    super_block* sb = (super_block*) malloc(BLOCKSIZE);
    if(!sb || read_block(mountptr, 0, sb) < 0) {
        return -1;
    }
    int inode_index = find_free_inode(sb);
    if(inode_index < 0) {
        free(sb);
        return -1;
    }
    inode* buffer = (inode*) malloc(BLOCKSIZE);
    if(!buffer) {
        free_inode_bitmap(inode_index, sb);
        free(sb);
        return -1;
    }
    if(read_block(mountptr, sb->inode_block_idx + inode_index/128, buffer) < 0) {
        free_inode_bitmap(inode_index, sb);
        goto err;
    }
    new_inode = buffer+(inode_index%128);
    if(new_inode->valid) {
        free_inode_bitmap(inode_index, sb);
        goto err;
    }
    new_inode->size=0;
    new_inode->valid=1;
    if(write_block(mountptr, sb->inode_block_idx + inode_index/128, buffer) < 0) {
        free_inode_bitmap(inode_index, sb);
        goto err;
    }
    free(sb);
    free(buffer);
    return inode_index;
    err:
        free(sb);
        free(buffer);
        return -1;
}

int free_data_bitmap(int dnumber, super_block* sb){
    if(!mountptr) {
        printf("ERR: Disk not mounted.\n");
        return -1;
    }
    if(dnumber < 0 || dnumber > sb->data_blocks) {
        return -1;
    }
    uint32_t* buffer = (uint32_t*) malloc(BLOCKSIZE);
    if(!buffer) {
        return -1;
    }
    if(read_block(mountptr, sb->data_block_bitmap_idx + dnumber/(BLOCKSIZE*8), buffer) < 0) {
        free(buffer);
        return -1;
    }
    int block_dnumber = dnumber%(BLOCKSIZE*8);
    ClearBit(buffer, block_dnumber);
    if(write_block(mountptr, sb->data_block_bitmap_idx + dnumber/(BLOCKSIZE*8), buffer) < 0) {
        free(buffer);
        return -1;
    }
    free(buffer);
    return 0;
}

int free_indirect_data_bitmap(int index, super_block* sb, int indirect_blocks) {
    if(!mountptr) {
        printf("ERR: Disk not mounted.\n");
        return -1;
    }
    if(index < 0 || index > sb->data_blocks) {
        return -1;
    }
    uint32_t* buffer = (uint32_t*) malloc(BLOCKSIZE);
    if(!buffer) {
        return -1;
    }
    if(read_block(mountptr, sb->data_block_idx + index, buffer) < 0) {
        free(buffer);
        return -1;
    }
    for(int i=0; i<indirect_blocks; i++) {
        if(free_data_bitmap(buffer[i], sb) < 0) {
            free(buffer);
            return -1;
        }
    }
    if(free_data_bitmap(index, sb) < 0) {
        free(buffer);
        return -1;
    }
    free(buffer);
    return 0;
}

int remove_file(int inumber) {
    super_block* sb = (super_block*) malloc(BLOCKSIZE);
    if(!sb || read_block(mountptr, 0, sb) < 0) {
        return -1;
    }
    if(inumber < 0 || inumber > sb->inodes) {
        free(sb);
        return -1;
    }
    inode* buffer = (inode*) malloc(BLOCKSIZE);
    if(!buffer) {
        free(sb);
        return -1;
    }
    if(read_block(mountptr, sb->inode_block_idx+(inumber/128), buffer) < 0)
        goto err;
    inode* del_inode = buffer+(inumber%128);
    int n_blocks = ceil(del_inode->size/(double) BLOCKSIZE);
    for(int i=0; i<n_blocks; i++) {
        if(i<5) {
            if(free_data_bitmap(del_inode->direct[i], sb) < 0)
                goto err;
        } else {
            if(free_indirect_data_bitmap(del_inode->indirect, sb, n_blocks-5) < 0)
                goto err;
            break;
        }
    }
    del_inode->valid=0;
    if(write_block(mountptr, sb->inode_block_idx + inumber/128, buffer) < 0) {
        goto err;
    }
    if(free_inode_bitmap(inumber, sb) < 0) {
        goto err;
    }
    free(sb);
    free(buffer);
    return 0;
    err:
        free(sb);
        free(buffer);
        return -1;
}

int stat(int inumber) {
    if(!mountptr) {
        printf("ERR: Disk not mounted.\n");
        return -1;
    }
    super_block* sb = (super_block*) malloc(BLOCKSIZE);
    if(!sb || read_block(mountptr, 0, sb) < 0) {
        return -1;
    }
    if(inumber < 0 || inumber > sb->inodes) {
        free(sb);
        return -1;
    }
    inode* buffer = (inode*) malloc(BLOCKSIZE);
    if(!buffer) {
        free(sb);
        return -1;
    }
    if(read_block(mountptr, sb->inode_block_idx + inumber/128, buffer) < 0) {
        free(sb);
        free(buffer);
        return -1;
    }
    inode* node = buffer+(inumber%128);
    if(!node->valid) {
        free(sb);
        free(buffer);
        return -1;
    }
    int total_blocks =(int) ceil(node->size/(double)BLOCKSIZE);
    printf("Stats: \n");
    printf("\tInode Number: %d\n", inumber);
    printf("\tLogical size: %d\n", (int) node->size);
    printf("\tTotal data blocks: %d\n", total_blocks);
    printf("\tNumber of direct pointers used: %d\n", (total_blocks-5)>0 ? 5 : total_blocks);
    printf("\tNumber of indirect pointers used: %d\n", (total_blocks-5)>0 ? total_blocks-5 : 0);
    free(sb);
    free(buffer);
}

int get_block(super_block* sb, inode* node, int blocknum, void* buffer) {
    if(blocknum < 5) {
        if(read_block(mountptr, sb->data_block_idx + node->direct[blocknum], buffer) < 0) {
            return -1;
        }
        return 0;
    } else {
        uint32_t* indirect_block = (uint32_t*) malloc(BLOCKSIZE);
        if(!indirect_block) {
            return -1;
        }
        if(read_block(mountptr, sb->data_block_idx + node->indirect, indirect_block) < 0) {
            free(indirect_block);
            return -1;
        }
        if(read_block(mountptr, sb->data_block_idx + indirect_block[blocknum-5], buffer) < 0) {
            free(indirect_block);
            return -1;
        }
        free(indirect_block);
        return 0;
    }
}

int put_block(super_block* sb, inode* node, int blocknum, void* buffer) {
    if(blocknum < 5) {
        if(write_block(mountptr, sb->data_block_idx + node->direct[blocknum], buffer) < 0) {
            return -1;
        }
        return 0;
    } else {
        uint32_t* indirect_block = (uint32_t*) malloc(BLOCKSIZE);
        if(!indirect_block) {
            return -1;
        }
        if(read_block(mountptr, sb->data_block_idx + node->indirect, indirect_block) < 0) {
            free(indirect_block);
            return -1;
        }
        if(write_block(mountptr, sb->data_block_idx + indirect_block[blocknum-5], buffer) < 0) {
            //printf("t %d %d %d", blocknum, indirect_block[0], indirect_block[1]);
            free(indirect_block);
            return -1;
        }
        free(indirect_block);
        return 0;
    }
}

int write_i(int inumber, char *data, int length, int offset) {
    if(!mountptr) {
        printf("ERR: Disk not mounted.\n");
        return -1;
    }
    if(length == 0)
        return 0;
    else if(length < 0)
        return -1;
    super_block* sb = (super_block*) malloc(BLOCKSIZE);
    if(!sb) {
        return -1;
    }
    if(read_block(mountptr, 0, sb) < 0) {
        free(sb);
        return -1;
    }
    if(inumber < 0 || inumber > sb->inodes) {
        free(sb);
        return -1;
    }
    inode* buffer = (inode*) malloc(BLOCKSIZE);
    if(!buffer) {
        free(sb);
        return -1;
    }
    if(read_block(mountptr, sb->inode_block_idx + inumber/128, buffer) < 0)
        goto err;

    inode* node = buffer+(inumber%128);
    if(!node->valid || offset > node->size)
        goto err;

    int byteswritten = 0;
    int blocknum = offset/BLOCKSIZE;
    int total_blocks =(int) ceil(node->size/(double)BLOCKSIZE);
    uint32_t* data_buffer = (uint32_t*) malloc(BLOCKSIZE);
    if(!data_buffer)
        goto err;
    while(byteswritten < length) {
        if(offset+byteswritten == total_blocks*BLOCKSIZE) {
            if(total_blocks==MAXBLOCKS) {
                break;
            } else {
                blocknum = total_blocks;
                if(total_blocks==5) {
                    int new_block;
                    if((new_block = find_free_datablock(sb)) < 0) {
                        //if disk is full, break
                        break;
                    }
                    node->indirect = new_block;
                    if((new_block = find_free_datablock(sb)) < 0) {
                        //if disk is full, break
                        break;
                    }
                    //update indirect block
                    memcpy(data_buffer, &new_block, sizeof(int));
                    if(write_block(mountptr, sb->data_block_idx + node->indirect, data_buffer) < 0) {
                        free(data_buffer);
                        goto err;
                    }
                } else if(total_blocks > 5){
                    if(read_block(mountptr, sb->data_block_idx + node->indirect, data_buffer) < 0) {
                        free(data_buffer);
                        goto err;
                    }
                    int new_block;
                    if((new_block = find_free_datablock(sb)) < 0) {
                        //if disk is full, break
                        break;
                    }
                    //update indirect block
                    memcpy(data_buffer+(blocknum-5), &new_block, sizeof(int));
                    if(write_block(mountptr, sb->data_block_idx + node->indirect, data_buffer) < 0) {
                        free(data_buffer);
                        goto err;
                    }
                } else {
                    int new_block;
                    if((new_block = find_free_datablock(sb)) < 0) {
                        printf("check");
                        //if disk is full, break
                        break;
                    }
                    //update direct pointer
                    node->direct[blocknum] = new_block;
                }
                total_blocks++;
                if(length-byteswritten > BLOCKSIZE) {
                    memcpy(data_buffer, data+byteswritten, BLOCKSIZE);
                    byteswritten+=BLOCKSIZE;
                    node->size = total_blocks*BLOCKSIZE;
                } else {
                    memcpy(data_buffer, data+byteswritten, length-byteswritten);
                    byteswritten=length;
                    node->size=offset+length;
                }
                if(put_block(sb, node, blocknum, data_buffer) < 0) {
                    free(data_buffer);
                    goto err;
                }
            }
        } else {
            blocknum = (offset+byteswritten)/BLOCKSIZE;
            if(get_block(sb, node, blocknum, data_buffer) < 0) {
                free(data_buffer);
                goto err;
            }
            if(length-byteswritten >= BLOCKSIZE-(offset+byteswritten)%BLOCKSIZE) {
                memcpy(data_buffer, data+byteswritten, BLOCKSIZE-(offset+byteswritten)%BLOCKSIZE);
                byteswritten += BLOCKSIZE-(offset+byteswritten)%BLOCKSIZE;
                node->size = total_blocks*BLOCKSIZE;
            } else {
                memcpy(data_buffer, data+byteswritten, length-byteswritten);
                byteswritten = length;
                node->size = offset+length;
            }
            if(put_block(sb, node, blocknum, data_buffer) < 0) {
                free(data_buffer);
                goto err;
            }
        }
    }
    // write inode back to disk
    if(write_block(mountptr, sb->inode_block_idx + inumber/128, buffer) < 0) {
        free(data_buffer);
        goto err;
    }
    free(sb);
    free(buffer);
    free(data_buffer);
    return byteswritten;
    err:
        free(sb);
        free(buffer);
        return -1;
}

int read_i(int inumber, char *data, int length, int offset) {
    if(!mountptr) {
        printf("ERR: Disk not mounted.\n");
        return -1;
    }
    if(length == 0)
        return 0;
    else if(length < 0)
        return -1;
    super_block* sb = (super_block*) malloc(BLOCKSIZE);
    if(!sb) {
        return -1;
    }
    if(read_block(mountptr, 0, sb) < 0) {
        free(sb);
        return -1;
    }
    if(inumber < 0 || inumber > sb->inodes) {
        free(sb);
        return -1;
    }
    inode* buffer = (inode*) malloc(BLOCKSIZE);
    if(!buffer) {
        free(sb);
        return -1;
    }
    if(read_block(mountptr, sb->inode_block_idx + inumber/128, buffer) < 0)
        goto err;

    inode* node = buffer+(inumber%128);
    if(!node->valid || offset >= node->size)
        goto err;

    int bytesread = 0, number_read_blocks;
    if(offset+length > node->size) {
        number_read_blocks = (node->size-1)/BLOCKSIZE - offset/BLOCKSIZE + 1;
    } else {
        number_read_blocks = (offset+length-1)/BLOCKSIZE - offset/BLOCKSIZE + 1;
    }
    char* data_buffer = (char*) malloc(BLOCKSIZE);
    if(!data_buffer)
        goto err;
    for(int i=0; i<number_read_blocks; i++) {
        if(i==0) {
            if(get_block(sb, node, offset/BLOCKSIZE, data_buffer) < 0) {
                free(data_buffer);
                goto err;
            }
            if(number_read_blocks == 1 && (offset+length > node->size)) {
                memcpy(data, data_buffer + offset%BLOCKSIZE, node->size - offset);
                bytesread += node->size - offset;
            }
            else if(number_read_blocks == 1 && (offset+length <= node->size)) {
                memcpy(data, data_buffer + offset%BLOCKSIZE, length);
                bytesread += length;
            }
            else {
                memcpy(data, data_buffer + offset%BLOCKSIZE, BLOCKSIZE - offset%BLOCKSIZE);
                bytesread += BLOCKSIZE - offset%BLOCKSIZE;
            }
        } else if(i==number_read_blocks-1) {
            if(get_block(sb, node, offset/BLOCKSIZE + i, data_buffer) < 0) {
                free(data_buffer);
                goto err;
            }
            if(offset+length > node->size) {
                memcpy(data+bytesread, data_buffer, (node->size-1)%BLOCKSIZE + 1);
                bytesread += (node->size-1)%BLOCKSIZE + 1;
            } else {
                memcpy(data+bytesread, data_buffer, (offset+length-1)%BLOCKSIZE + 1);
                bytesread += (offset+length-1)%BLOCKSIZE + 1;
            }
        } else {
            if(get_block(sb, node, offset/BLOCKSIZE + i, data_buffer) < 0) {
                free(data_buffer);
                goto err;
            }
            memcpy(data+bytesread, data_buffer, BLOCKSIZE);
            bytesread += BLOCKSIZE;
        }
    }
    free(sb);
    free(buffer);
    free(data_buffer);
    return bytesread;
    err:
        free(sb);
        free(buffer);
        return -1;
}

int fit_to_size(int inumber, int size) {
    if(!mountptr) {
        return -1;
    }
    if(size < 0) {
        return -1;
    }
    super_block* sb = (super_block*) malloc(BLOCKSIZE);
    if(!sb) {
        return -1;
    }
    if(read_block(mountptr, 0, sb) < 0) {
        free(sb);
        return -1;
    }
    if(inumber < 0 || inumber > sb->inodes) {
        free(sb);
        return -1;
    }
    inode* buffer = (inode*) malloc(BLOCKSIZE);
    if(!buffer) {
        free(sb);
        return -1;
    }
    if(read_block(mountptr, sb->inode_block_idx + inumber/128, buffer) < 0)
        goto err;
    inode* node = buffer+(inumber%128);
    if(!node->valid) {
        goto err;
    }
    int del_blocks = 0;
    int total_blocks = ceil(node->size/(double)BLOCKSIZE);
    if(size < node->size) {
        del_blocks = total_blocks - (int)ceil(size/(double)BLOCKSIZE);
        if(del_blocks>0)  {
            uint32_t* indirect_block;
            if(total_blocks > 5) {
                indirect_block = (uint32_t*) malloc(BLOCKSIZE);
                if(!indirect_block) {
                    goto err;
                }
                if(read_block(mountptr, sb->data_block_idx + node->indirect, indirect_block) < 0) {
                    free(indirect_block);
                    goto err;
                }
            }
            for(int i=total_blocks-1; i>=total_blocks-del_blocks; i--) {
                if(i==5) {
                    if(free_data_bitmap(indirect_block[0], sb) < 0) {
                        free(indirect_block);
                        goto err;
                    }
                    if(free_data_bitmap(node->indirect, sb) < 0) {
                        free(indirect_block);
                        goto err;
                    }
                } else if(i>5) {
                    if(free_data_bitmap(indirect_block[i-5], sb) < 0) {
                        free(indirect_block);
                        goto err;
                    }
                } else {
                    if(free_data_bitmap(node->direct[i], sb) < 0) {
                        free(indirect_block);
                        goto err;
                    }
                }
            }
            if(total_blocks>5)
                free(indirect_block);
        }
        node->size = size;
        // write inode back to disk
        if(write_block(mountptr, sb->inode_block_idx + inumber/128, buffer) < 0) {
            goto err;
        }
    }
    free(sb);
    free(buffer);
    return 0;
    err:
        free(sb);
        free(buffer);
        return -1;
}

int get_filesize(int inumber) {
    if(!mountptr) {
        printf("ERR: Disk not mounted.\n");
        return -1;
    }
    super_block* sb = (super_block*) malloc(BLOCKSIZE);
    if(!sb || read_block(mountptr, 0, sb) < 0) {
        return -1;
    }
    if(inumber < 0 || inumber > sb->inodes) {
        free(sb);
        return -1;
    }
    inode* buffer = (inode*) malloc(BLOCKSIZE);
    if(!buffer) {
        free(sb);
        return -1;
    }
    if(read_block(mountptr, sb->inode_block_idx + inumber/128, buffer) < 0) {
        free(sb);
        free(buffer);
        return -1;
    }
    inode* node = buffer+(inumber%128);
    if(!node->valid) {
        free(sb);
        free(buffer);
        return -1;
    }
    int filesize = node->size;
    free(sb);
    free(buffer);
    return filesize;
}