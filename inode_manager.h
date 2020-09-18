// inode layer interface.

#ifndef inode_h
#define inode_h

#include <stdint.h>
#include <map>
#include "extent_protocol.h" // TODO: delete it

/*
 * TODO: part1A  
 * implement disk::read_block, disk::write_block, inode_manager::alloc_inode and inode_manager::getattr
 * to support CREATE and GETATTR APIs
 * should pass the test_create_and_getattr() in part1_tester, which tests creating empty files, getting their attributes like type.
 * 
 * TODO: part1B
 * implement inode_manager::write_file, inode_manager::read_file, block_manager::alloc_block, block_manager::free_block
 * to support PUT and GET APIs
 * should pass the test_put_and_get() in part1_tester, which, write and read files.
 * 
 * TODO: part1C
 * implement inode_manager::remove_file and inode_manager::free_inode
 * to support REMOVE API
 * should pass the test_remove() in part1_tester.
 */

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

// Find block which contains inode i
#define IBLOCK(i, nblocks) ((nblocks) / BPB + (i) / IPB + 3)

/*
 * Bitmap bits per block
 * Every block's bitmap is (uint32_t, int), which is 8 bytes in total
 */
#define BPB (BLOCK_SIZE * 8)

// Block containing bit for block b
#define BBLOCK(b) ((b) / BPB + 2)

#define NDIRECT 100
#define NINDIRECT (BLOCK_SIZE / sizeof(uint))
#define MAXFILE (NDIRECT + NINDIRECT)

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
  struct inode* get_inode(uint32_t inum);
  void put_inode(uint32_t inum, struct inode *ino);

public:
  inode_manager();
  uint32_t alloc_inode(uint32_t type);
  void free_inode(uint32_t inum);
  void read_file(uint32_t inum, char **buf, int *size);
  void write_file(uint32_t inum, const char *buf, int size);
  void remove_file(uint32_t inum);
  void getattr(uint32_t inum, extent_protocol::attr &a);
};

#endif
