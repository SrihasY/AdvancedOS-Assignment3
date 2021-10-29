#include<stdint.h>

#define MAX_LEN 256

const static uint32_t MAGIC = 12345;


typedef struct inode {
	uint32_t valid; // 0 if invalid
	uint32_t size; // logical size of the file
	uint32_t direct[5]; // direct data block pointer
	uint32_t indirect; // indirect pointer
} inode;


typedef struct super_block {
	uint32_t magic_number;	// File system magic number
	uint32_t blocks;	// Number of blocks in file system (except super block)

	uint32_t inode_blocks;	// Number of blocks reserved for inodes == 10% of Blocks
	uint32_t inodes;	// Number of inodes in file system == length of inode bit map
	uint32_t inode_bitmap_block_idx;  // Block Number of the first inode bit map block
	uint32_t inode_block_idx;	// Block Number of the first inode block

	uint32_t data_block_bitmap_idx;	// Block number of the first data bitmap block
	uint32_t data_block_idx;	// Block number of the first data block
	uint32_t data_blocks;  // Number of blocks reserved as data blocks
} super_block;

typedef struct dir_item {
    uint8_t valid;
    uint8_t is_dir;
    char filename[MAX_LEN];
    uint8_t filename_len;
    uint32_t inumber;    
} dir_item;

int format(disk *diskptr);

int mount(disk *diskptr);

int create_file();

int remove_file(int inumber);

int stat(int inumber);

int read_i(int inumber, char *data, int length, int offset);

int write_i(int inumber, char *data, int length, int offset);

int fit_to_size(int inumber, int size);

int get_filesize(int inumber);

int read_file(char *filepath, char *data, int length, int offset);
int write_file(char *filepath, char *data, int length, int offset);
// create_dir and remove_dir are only for directories, similar functions are provided for files
int create_dir(char *dirpath);
int remove_dir(char *dirpath);

int init_dirsys(disk* diskptr);
int create_file_by_path(char* filepath);
int remove_file_by_path(char* filepath);