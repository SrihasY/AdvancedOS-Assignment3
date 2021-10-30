#include "disk.h"
#include "sfs.h"
#include <libgen.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#define ROOT_INODE 0

static int dir_initialized = 0;

int init_dirsys(disk* diskptr) {
    int root_inode = create_file();
    if(root_inode < 0) {
        return -1;
    } else if(root_inode!=ROOT_INODE) {
        remove_file(root_inode);
        return -1;
    }
    dir_initialized = 1;
    return 0;
}

int find_dir(char* dirpath) {
    if(!dir_initialized) {
        printf("Directory system not initialized.\n");
        return -1;
    }
    char* dirc = strdup(dirpath);
    char* basec = strdup(dirpath);
    char* parent = dirname(dirc);
    char* base = basename(basec);
    dir_item* current = (dir_item*) malloc(sizeof(dir_item));
    if(!current) {
        free(dirc);
        free(basec);
        return -1;
    }
    int retval = -1;
    if(!strcmp(parent, "/") || !strcmp(parent, ".")) {
        //if searching for root inode itself
        if((!strcmp(parent, "/") && !strcmp(base, "/")) || (!strcmp(parent, ".") && !strcmp(base, "."))) {
            free(dirc);
            free(basec);
            free(current);
            return ROOT_INODE;
        }
        //search the root inode
        int offset = 0;
        while(offset < get_filesize(ROOT_INODE)) {
            if(read_i(ROOT_INODE, (char*) current, sizeof(dir_item), offset) < 0) {
                goto err;
            }
            //printf("%s %s", current->filename, base);
            if(strcmp(current->filename, base)==0) {
                if(!current->valid || !current->is_dir) {
                    goto err;
                } else {
                    retval = current->inumber;
                    break;
                }
            }
            offset += sizeof(dir_item);
        }
    } else {
        int parinode = find_dir(parent);
        if(parinode < 0)
            goto err;
        int offset = 0;
        while(offset < get_filesize(parinode)) {
            if(read_i(parinode, (char*) current, sizeof(dir_item), offset) < 0) {
                goto err;
            }
            if(strcmp(current->filename, base)==0) {
                if(!current->valid || !current->is_dir) {
                    goto err;
                } else {
                    retval = current->inumber;
                    break;
                }
            }
            offset += sizeof(dir_item);
        }
    }
    free(current);
    free(dirc);
    free(basec);
    return retval;
    err:
        free(current);
        free(dirc);
        free(basec);
        return -1;
}

int create_dir(char *dirpath) {
    if(!dir_initialized) {
        printf("Directory system not initialized.\n");
        return -1;
    }
    char* dirc = strdup(dirpath);
    char* basec = strdup(dirpath);
    char* parent = dirname(dirc);
    char* base = basename(basec);
    if(strlen(base) >= MAX_LEN) {
        printf("Directory name too long.\n");
        free(dirc);
        free(basec);
        return -1;
    }
    int par_inode = find_dir(parent);
    dir_item* current = (dir_item*) malloc(sizeof(dir_item));
    if(!current) {
        free(dirc);
        free(basec);
        return -1;
    }
    if(par_inode < 0) {
        printf("Parent directory not found.\n");
        goto err;
    }
    int offset = 0;
    int freespot = -1;
    while(offset < get_filesize(par_inode)) {
        if(read_i(par_inode, (char*) current, sizeof(dir_item), offset) < 0) {
            goto err;
        }
        if(strcmp(current->filename, base)==0) {
            //duplicate directory name
            printf("Duplicate directory name.\n");
            goto err;
        }
        if(!current->valid) {
            freespot = offset;
        }
        offset += sizeof(dir_item);
    }
    //initialize the dir_item for the new directory
    strcpy(current->filename, base);
    current->filename_len = strlen(base);
    current->is_dir = 1;
    current->valid = 1;
    current->inumber = create_file();
    if(current->inumber < 0) {
        printf("Unable to create directory file.\n");
        goto err;
    }
    //update the parent directory
    if(freespot>0) {
        if(write_i(par_inode, (char*) current, sizeof(dir_item), freespot) < 0) {
            printf("Parent directory update failed.\n");
            remove_file(current->inumber);
            goto err;
        }
    } else {
        if(write_i(par_inode, (char*) current, sizeof(dir_item), get_filesize(par_inode)) < 0) {
            printf("Parent directory update failed.\n");
            remove_file(current->inumber);
            goto err;
        }
    }
    free(dirc);
    free(basec);
    free(current);
    return 0;
    err:
        free(dirc);
        free(basec);
        free(current);
        return -1;
}

int remove_dir(char *dirpath) {
    if(!dir_initialized) {
        printf("Directory system not initialized.\n");
        return -1;
    }
    char* basec = strdup(dirpath);
    char* dirc = strdup(dirpath);
    char* parent = dirname(dirc);
    char* base = basename(basec);

    int parinode = find_dir(parent);
    int inumber = find_dir(dirpath);
    dir_item* current = (dir_item*) malloc(sizeof(dir_item));
    if(!current) {
        return -1;
    }
    if(parinode<0 || inumber <= 0) {
        printf("Unable to delete directory.\n");
        return -1;
    }
    int offset = 0;
    while(offset < get_filesize(inumber)) {
        if(read_i(inumber, (char*) current, sizeof(dir_item), offset) < 0) {
            goto err;
        }
        if(current->is_dir) {
            char* subdir = (char*) malloc((strlen(dirpath)+current->filename_len+2)*sizeof(char));
            strcpy(subdir, dirpath);
            strcat(subdir, "/");
            strcat(subdir, current->filename);
            if(remove_dir(subdir) < 0) {
                free(subdir);
                goto err;
            }
        } else {
            if(remove_file(current->inumber) < 0) {
                goto err;
            }
        }
        offset += sizeof(dir_item);
    }
    offset = 0;
    //find and update the dir_item in the parent directory
    while(offset < get_filesize(parinode)) {
        if(read_i(parinode, (char*) current, sizeof(dir_item), offset) < 0) {
            goto err;
        }
        if(strcmp(current->filename, base)==0) {
            if(remove_file(inumber) < 0) {
                goto err;
            }
            current->valid = 0;
            if(write_i(parinode, (char*) current, sizeof(dir_item), offset) < 0) {
                printf("Parent directory update failed.\n");
                remove_file(current->inumber);
                goto err;
            }
        }
        offset += sizeof(dir_item);
    }
    free(current);
    free(dirc);
    free(basec);
    return 0;
    err:
        free(current);
        free(dirc);
        free(basec);
        return -1;
}

int create_file_by_path(char* filepath) {
    int retval = -1;
    if(!dir_initialized) {
        printf("Directory system not initialized.\n");
        return -1;
    }
    char* dirc = strdup(filepath);
    char* basec = strdup(filepath);
    char* parent = dirname(dirc);
    char* base = basename(basec);
    if(strlen(base) >= MAX_LEN) {
        printf("Filename too long.\n");
        free(dirc);
        free(basec);
        return -1;
    }
    int par_inode = find_dir(parent);
    dir_item* current = (dir_item*) malloc(sizeof(dir_item));
    if(!current) {
        free(dirc);
        free(basec);
        return -1;
    }
    if(par_inode < 0) {
        printf("Parent directory not found.\n");
        goto err;
    }
    int offset = 0;
    int freespot = -1;
    while(offset < get_filesize(par_inode)) {
        if(read_i(par_inode, (char*) current, sizeof(dir_item), offset) < 0) {
            goto err;
        }
        if(strcmp(current->filename, base)==0) {
            //duplicate filename
            printf("Duplicate filename.\n");
            goto err;
        }
        if(!current->valid) {
            freespot = offset;
        }
        offset += sizeof(dir_item);
    }
    //initialize the dir_item for the new file
    strcpy(current->filename, base);
    current->filename_len = strlen(base);
    current->is_dir = 0;
    current->valid = 1;
    current->inumber = create_file();
    if(current->inumber < 0) {
        printf("Unable to create directory file.\n");
        goto err;
    }
    retval = current->inumber;
    //update the parent directory
    if(freespot>0) {
        if(write_i(par_inode, (char*) current, sizeof(dir_item), freespot) < 0) {
            printf("Parent directory update failed.\n");
            remove_file(current->inumber);
            goto err;
        }
    } else {
        if(write_i(par_inode, (char*) current, sizeof(dir_item), get_filesize(par_inode)) < 0) {
            printf("Parent directory update failed.\n");
            remove_file(current->inumber);
            goto err;
        }
    }
    free(dirc);
    free(basec);
    free(current);
    return retval;
    err:
        free(dirc);
        free(basec);
        free(current);
        return -1;
}

int remove_file_by_path(char* filepath) {
    if(!dir_initialized) {
        printf("Directory system not initialized.\n");
        return -1;
    }
    char* dirc = strdup(filepath);
    char* basec = strdup(filepath);
    char* parent = dirname(dirc);
    char* base = basename(basec);
    if(strlen(base) >= MAX_LEN) {
        printf("Filename too long.\n");
        free(dirc);
        free(basec);
        return -1;
    }
    int par_inode = find_dir(parent);
    dir_item* current = (dir_item*) malloc(sizeof(dir_item));
    if(!current) {
        free(dirc);
        free(basec);
        return -1;
    }
    if(par_inode < 0) {
        printf("Parent directory not found.\n");
        goto err;
    }
    int offset = 0;
    while(offset < get_filesize(par_inode)) {
        if(read_i(par_inode, (char*) current, sizeof(dir_item), offset) < 0) {
            goto err;
        }
        if(strcmp(current->filename, base)==0) {
            if(current->is_dir) {
                printf("Delete failed, the path is a directory.\n");
                goto err;
            } else if(!current->valid) {
                printf("File already deleted.\n");
                goto err;
            }
            remove_file(current->inumber);
            current->valid = 0;
            if(write_i(par_inode, (char*) current, sizeof(dir_item), offset) < 0) {
                printf("Parent directory update failed.\n");
                remove_file(current->inumber);
                goto err;
            }
        }
        offset += sizeof(dir_item);
    }
    //update the parent directory
    free(dirc);
    free(basec);
    free(current);
    return 0;
    err:
        free(dirc);
        free(basec);
        free(current);
        return -1;
}

int read_file(char *filepath, char *data, int length, int offset) {
    int retval = -1;
    if(!dir_initialized) {
        printf("Directory system not initialized.\n");
        return -1;
    }
    char* dirc = strdup(filepath);
    char* basec = strdup(filepath);
    char* parent = dirname(dirc);
    char* base = basename(basec);
    if(strlen(base) >= MAX_LEN) {
        printf("Filename too long.\n");
        free(dirc);
        free(basec);
        return -1;
    }
    int par_inode = find_dir(parent);
    dir_item* current = (dir_item*) malloc(sizeof(dir_item));
    if(!current) {
        free(dirc);
        free(basec);
        return -1;
    }
    if(par_inode < 0) {
        printf("Parent directory not found.\n");
        goto err;
    }
    int diroffset = 0;
    while(diroffset < get_filesize(par_inode)) {
        if(read_i(par_inode, (char*) current, sizeof(dir_item), diroffset) < 0) {
            goto err;
        }
        if(strcmp(current->filename, base)==0) {
            if(!current->valid) {
                goto err;
            }
            if((retval = read_i(current->inumber, data, length, offset)) < 0) {
                goto err;
            }
            break;
        }
        diroffset += sizeof(dir_item);
    }
    free(dirc);
    free(basec);
    free(current);
    return retval;
    err:
        free(dirc);
        free(basec);
        free(current);
        return -1;
}

int write_file(char *filepath, char *data, int length, int offset) {
    int retval = -1;
    if(!dir_initialized) {
        printf("Directory system not initialized.\n");
        return -1;
    }
    char* dirc = strdup(filepath);
    char* basec = strdup(filepath);
    char* parent = dirname(dirc);
    char* base = basename(basec);
    if(strlen(base) >= MAX_LEN) {
        printf("Filename too long.\n");
        free(dirc);
        free(basec);
        return -1;
    }
    int par_inode = find_dir(parent);
    dir_item* current = (dir_item*) malloc(sizeof(dir_item));
    if(!current) {
        free(dirc);
        free(basec);
        return -1;
    }
    if(par_inode < 0) {
        printf("Parent directory not found.\n");
        goto err;
    }
    int diroffset = 0;
    while(diroffset < get_filesize(par_inode)) {
        if(read_i(par_inode, (char*) current, sizeof(dir_item), diroffset) < 0) {
            goto err;
        }
        if(strcmp(current->filename, base)==0) {
            if(!current->valid) {
                goto err;
            }
            if((retval = write_i(current->inumber, data, length, offset)) < 0) {
                goto err;
            }
            break;
        }
        diroffset += sizeof(dir_item);
    }
    free(dirc);
    free(basec);
    free(current);
    return retval;
    err:
        free(dirc);
        free(basec);
        free(current);
        return -1;
}