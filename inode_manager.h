// inode layer interface.

#ifndef inode_h
#define inode_h

#include <stdint.h>
#include <map>
#include <time.h>
#include <iostream>
#include "extent_protocol.h" // TODO delete it

using namespace std;

#define DISK_SIZE 1024 * 1024 * 16 // 16MB
#define BLOCK_SIZE 512
#define BLOCK_NUM (DISK_SIZE / BLOCK_SIZE) // 32768

typedef uint32_t blockid_t;

// disk layer -----------------------------------------

class disk
{
private:
	unsigned char blocks[BLOCK_NUM][BLOCK_SIZE];

public:
	disk();
	~disk() {}

	void read_block(uint32_t id, char *buf);
	void write_block(uint32_t id, const char *buf);
};

// block layer -----------------------------------------

typedef struct superblock
{
	uint32_t size;
	uint32_t nblocks;
	uint32_t ninodes;
} superblock_t;

class block_manager
{
public:
	struct superblock sb;

private:
	disk *d;
	std::map<uint32_t, int> using_blocks; // bit map to manage block

public:
	block_manager();
	~block_manager() {}

	uint32_t alloc_block();
	void free_block(uint32_t id);
	void read_block(uint32_t id, char *buf);
	void write_block(uint32_t id, const char *buf);
};

// inode layer -----------------------------------------

#define INODE_NUM 1024

/*
 * Inodes per block. Here means a block only contain an inode, 
 * so that there are 1024(INODE_NUM) blocks for inodes
 */
#define IPB 1

/*
 * Find block which contains inode i, which start from 1
 * why 3: super block, inode free bitmap and block bitmap
 */
#define IBLOCK(i, nblocks) ((nblocks) / BPB + (i - 1) / IPB + 3)

/*
 * Bitmap bits per block
 * Every block's bitmap is (uint32_t, int), which is 8 bytes in total
 */
#define BPB (BLOCK_SIZE * 8)

/*
 * Block containing bit for block b
 * why 2: super block and inode free bitmap
 */
#define BBLOCK(b) ((b) / BPB + 2)

#define NDIRECT 100								  // number of direct blocks in one inode
#define NINDIRECT (BLOCK_SIZE / sizeof(uint32_t)) // number of direct blocks in one indirect block
#define MAXFILE (NDIRECT + NINDIRECT)			  // maxium number of direct blocks in one file

typedef struct inode
{
	short type;
	unsigned int size;
	unsigned int atime;
	unsigned int mtime;
	unsigned int ctime;
	blockid_t blocks[NDIRECT + 1]; // Data block addresses
} inode_t;

class inode_manager
{
private:
	block_manager *bm;

private:
	struct inode *get_inode(uint32_t inum);
	void put_inode(uint32_t inum, struct inode *ino);

public:
	inode_manager();
	~inode_manager() {}

	uint32_t alloc_inode(uint32_t type);
	void free_inode(uint32_t inum);
	void read_file(uint32_t inum, char **buf_out, int *size);
	void write_file(uint32_t inum, const char *buf_in, int size);
	void remove_file(uint32_t inum);
	void getattr(uint32_t inum, extent_protocol::attr &a);
};

// dir layer ----------------------------------------

// an entry in dir is (inode, filename)
#define DIR_INODE_LEN 4	 // uint32_t
#define DIR_FNAME_LEN 60 // maxium value
#define DIR_ENTRY_LEN (DIR_INODE_LEN + DIR_FNAME_LEN)

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define FIRST_BLOCK (3 + BLOCK_NUM / BPB + INODE_NUM / IPB)

#endif