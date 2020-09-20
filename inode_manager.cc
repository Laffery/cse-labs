#include "inode_manager.h"
#include <time.h>

#define MIN(a,b) ((a)<(b) ? (a) : (b))
#define FIRST_BLOCK (3 + BLOCK_NUM/BPB + INODE_NUM/IPB)

// disk layer -----------------------------------------

disk::disk()
{
  /* set all bits in blocks 0 */
  // bzero(blocks, sizeof(blocks)); 
  memset(blocks, 0, sizeof(blocks));
}

void
disk::read_block(blockid_t id, char *buf)
{
  if (id < 0 || id >= BLOCK_NUM)
  {
    printf("\td: error! cannot read block with invalid id %d\n", id);
    return;
  }
  
  if (buf == NULL)
  {
    printf("\td: error! cannot write to a null ptr *buf\n");
    return;
  }

  memcpy(buf, blocks[id], BLOCK_SIZE);
}

void
disk::write_block(blockid_t id, const char *buf)
{
  if (id < 0 || id >= BLOCK_NUM)
  {
    printf("\td: error! cannot write to block with invalid id %d\n", id);
    return;
  }

  if (buf == NULL)
  {
    printf("\td: error! cannot read from a null ptr *buf\n");
    return;
  }

  memcpy(blocks[id], buf, MIN(sizeof(buf), BLOCK_SIZE));
}

// block layer -----------------------------------------

// Allocate a free disk block.
blockid_t
block_manager::alloc_block()
{
  /*
   * your code goes here.
   * note: you should mark the corresponding bit in block bitmap when alloc.
   * you need to think about which block you can start to be allocated.
   */
  for (blockid_t i = FIRST_BLOCK; i < BLOCK_NUM; ++i)
  {
    if (using_blocks[i] == 0)
    {
      using_blocks[i] = 1;
      printf("\tbm: allocate a block with id %i\n", i);     
      return i;
    }
  }

  printf("\tbm: error! no free block left to allocate\n");
  return 0;
}

void
block_manager::free_block(uint32_t id)
{
  /* 
   * your code goes here.
   * note: you should unmark the corresponding bit in the block bitmap when free.
   */
  if (id < FIRST_BLOCK || id >= BLOCK_NUM)
  {
    printf("\tbm: error! cannot free with an invalid block id %d\n", id);
  }
  else 
  {
    using_blocks[id] = 0;
    printf("\tbm: free block %d\n", id);
  }
}

// The layout of disk should be like this:
// |<-sb->|<-free block bitmap->|<-inode table->|<-data->|
block_manager::block_manager()
{
  d = new disk();

  // format the disk
  sb.size = BLOCK_SIZE * BLOCK_NUM;
  sb.nblocks = BLOCK_NUM;
  sb.ninodes = INODE_NUM;

  // init bit map with 0
  for (uint32_t i = 0; i < BLOCK_NUM; ++i)
  {
    using_blocks.insert(std::pair<uint32_t, int>(i, 0));
  }
}

void
block_manager::read_block(uint32_t id, char *buf)
{
  d->read_block(id, buf);
}

void
block_manager::write_block(uint32_t id, const char *buf)
{
  d->write_block(id, buf);
}

// inode layer -----------------------------------------

// usable inode block start from 2, 1 is root_dir
inode_manager::inode_manager()
{
  bm = new block_manager();
  uint32_t root_dir = alloc_inode(extent_protocol::T_DIR); // T_DIR = 1
  if (root_dir != 1) {
    printf("\tim: error! alloc first inode %d, should be 1\n", root_dir);
    exit(0);
  }
}

/* Create a new file.
 * Return its inum. */
uint32_t
inode_manager::alloc_inode(uint32_t type)
{
  /* 
   * your code goes here.
   * note: the normal inode block should begin from the 2nd inode block.
   * the 1st is used for root_dir, see inode_manager::inode_manager().
   */
  for (int i = 1; i <= INODE_NUM; i++)
  {
    if (!get_inode(i)) // usable
    {
      struct inode ino;
      ino.type = type;
      ino.size = 0;

      uint32_t CURR_TIME = (uint32_t)time(NULL);
      ino.atime = CURR_TIME;
      ino.ctime = CURR_TIME;
      ino.mtime = CURR_TIME;

      put_inode(i, &ino);

      printf("\tim: allocate inode %d\n", i);
      return i;
    }
  }

  printf("\tim: error! too full to allocate a new inode\n");
  return 0;
}

void
inode_manager::free_inode(uint32_t inum)
{
  /* 
   * your code goes here.
   * note: you need to check if the inode is already a freed one;
   * if not, clear it, and remember to write back to disk.
   */

  return;
}


/* Return an inode structure by inum, NULL otherwise.
 * Caller should release the memory. */
struct inode* 
inode_manager::get_inode(uint32_t inum)
{
  struct inode *ino, *ino_disk;
  char buf[BLOCK_SIZE];

  printf("\tim: get_inode %d\n", inum);

  if (inum < 0 || inum >= INODE_NUM) {
    printf("\tim: inum out of range\n");
    return NULL;
  }

  bm->read_block(IBLOCK(inum, bm->sb.nblocks), buf);
  // printf("%s:%d\n", __FILE__, __LINE__);

  ino_disk = (struct inode*)buf + inum%IPB;
  if (ino_disk->type == 0) {
    printf("\tim: inode not exist\n");
    return NULL;
  }

  ino = (struct inode*)malloc(sizeof(struct inode));
  *ino = *ino_disk;

  return ino;
}

void
inode_manager::put_inode(uint32_t inum, struct inode *ino)
{
  char buf[BLOCK_SIZE];
  struct inode *ino_disk;

  printf("\tim: put_inode %d\n", inum);
  if (ino == NULL)
    return;

  bm->read_block(IBLOCK(inum, bm->sb.nblocks), buf);
  ino_disk = (struct inode*)buf + inum%IPB;
  *ino_disk = *ino;
  bm->write_block(IBLOCK(inum, bm->sb.nblocks), buf);
}

/* Get all the data of a file by inum. 
 * Return alloced data, should be freed by caller. */
void
inode_manager::read_file(uint32_t inum, char **buf_out, int *size)
{
  /*
   * your code goes here.
   * note: read blocks related to inode number inum,
   * and copy them to buf_Out
   */
  
  return;
}

/* alloc/free blocks if needed */
void
inode_manager::write_file(uint32_t inum, const char *buf, int size)
{
  /*
   * your code goes here.
   * note: write buf to blocks of inode inum.
   * you need to consider the situation when the size of buf 
   * is larger or smaller than the size of original inode
   */
  
  return;
}

void
inode_manager::getattr(uint32_t inum, extent_protocol::attr &a)
{
  /*
   * your code goes here.
   * note: get the attributes of inode inum.
   * you can refer to "struct attr" in extent_protocol.h
   */
  struct inode* ino = get_inode(inum);

  if (!ino) 
  {
    printf("\tim: error! cannot get attr with an unfound inode id %d\n", inum);
    return;
  }
  
  a.atime = ino->atime;
  a.ctime = ino->ctime;
  a.mtime = ino->mtime;
  a.size  = ino->size;
  a.type  = ino->type;

  return;
}

void
inode_manager::remove_file(uint32_t inum)
{
  /*
   * your code goes here
   * note: you need to consider about both the data block and inode of the file
   */
  
  return;
}
