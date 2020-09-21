#include "inode_manager.h"
#include <time.h>
#include <iostream>

using namespace std;

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
  printf("\td: read from block %d\n", id);
  //cout << "\td: read '" << buf << "' from block " << id << endl;
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

  //cout << "\td: write data '" << buf << "' to block " << id << endl;
  printf("\td: write to block %d\n", id);
  memcpy(blocks[id], buf, BLOCK_SIZE);
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
  struct inode *ino = get_inode(inum);

  if (inum < 0 || inum >= INODE_NUM)
    return;

  else if (!ino)
  {
    printf("\tim: error! inode %d is already a freed one\n", inum);
    return;
  }

  else
  {
    ino->type = 0;
    ino->ctime = (uint32_t)time(NULL);
    put_inode(inum, ino);
  }
}


/* 
 * Return an inode structure by inum, NULL otherwise.
 * Caller should release the memory. 
 */
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
print_inode(struct inode *ino)
{
  cout << "type : " << ino->type << endl
       << "size : " << ino->size << endl
       << "atime: " << ino->atime << endl
       << "mtime: " << ino->mtime << endl
       << "ctime: " << ino->ctime << endl;

  for (int i = 0; i < NDIRECT; ++i)
  {
      cout << ino->blocks[i] << "  ";
  }

  cout << endl;
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
  // print_inode(ino);
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
  struct inode *ino = get_inode(inum);
  if (!ino)
  {
    printf("\tim: error! cannot to read inode %d\n", inum);
    return;
  }
  
  // how many block does this inode have
  int blockNumber = ino->size / BLOCK_SIZE + (ino->size % BLOCK_SIZE > 0);
  // allocate space for read buffer
  char *buf = (char *)malloc(blockNumber * BLOCK_SIZE);

  // clone direct blocks' data to buffer
  for (int i = 0; i < MIN(blockNumber, NDIRECT); ++i)
  {
// cout << "\t" << ino->blocks[i] << endl;
    bm->read_block(ino->blocks[i], buf + i * BLOCK_SIZE);
  }

  // there is some indirect blocks in this inode
  if (blockNumber > NDIRECT) // all block of this inode is direct
  {
    // indirect blocks' id list
    uint32_t bidList[NINDIRECT];
    bm->read_block(ino->blocks[NDIRECT], (char *)bidList);

    for (int i = 0; i < blockNumber - NDIRECT; ++i)
    {
      bm->read_block(bidList[i], buf + (NDIRECT + i) * BLOCK_SIZE);
    }
  }

  *size = ino->size;
  *buf_out = buf;
  
  // update access time
  ino->atime = (uint32_t)time(NULL);
  put_inode(inum, ino);

  printf("\tim: read inode %d\n", inum);
}

/* alloc/free blocks if needed */
void
inode_manager::write_file(uint32_t inum, const char *buf_in, int size)
{
  /*
   * your code goes here.
   * note: write buf to blocks of inode inum.
   * you need to consider the situation when the size of buf 
   * is larger or smaller than the size of original inode
   */
  struct inode *ino = get_inode(inum);
  if (!ino)
  {
    printf("\tim: error! cannot write to inode %d\n", inum);
    return;
  }

  // how many blocks is needed
  int blockNumber = size / BLOCK_SIZE + (size % BLOCK_SIZE > 0);
  if ((uint32_t)blockNumber > MAXFILE)
  {
    printf("\tim: error! write to many data into inode %d\n", inum);
    return;
  }
  
  // how many blocks originally does this inode have
  int blockNumOrigin = ino->size / BLOCK_SIZE + (ino->size % BLOCK_SIZE > 0);
  printf("\tim: blockNumber is %d and blockNumOrigin is %d\n", blockNumber, blockNumOrigin);

  // indirect blocks' id list
  uint32_t bidList[NINDIRECT];
  bm->read_block(ino->blocks[NDIRECT], (char *)bidList);

  // need to allocate new block
  if (blockNumber > blockNumOrigin)
  {
    // block number to write is less than NDIRECT, just allocate direct blocks directly
    if (blockNumber <= NDIRECT)
    {
      for (int i = blockNumOrigin; i < blockNumber; ++i)
      {
        ino->blocks[i] = bm->alloc_block();
      }
    }

    else
    {
      // need to allocate both direct and indirect blocks
      if (blockNumOrigin <= NDIRECT)
      {
        for (int i = blockNumOrigin; i < NDIRECT; ++i)
        {
          ino->blocks[i] = bm->alloc_block();
        }

        // there is no indirect block in origin, so that we need to alloc 
        for (int i = NDIRECT; i < blockNumber; ++i)
        {
          bidList[i - NDIRECT] = bm->alloc_block();
        }
      }

      // need to allocate indirect blocks only
      else
      {
        for (int i = blockNumOrigin; i < blockNumber; ++i)
        {
          bidList[i - NDIRECT] = bm->alloc_block();
        }
      }      

      // write new indirect blocks
      bm->write_block(ino->blocks[NDIRECT], (char *)bidList);
    }
  }

  // need to free blocks
  else
  {
    // just need to free some indirect blocks
    if (blockNumber > NDIRECT)
    {
      for (int i = blockNumber; i < blockNumOrigin; ++i)
      {
        bm->free_block(bidList[i - NDIRECT]);
      }
    }

    else
    {
      // just need to free some direct blocks
      if (blockNumOrigin <= NDIRECT)
      {
        for (int i = blockNumber; i < blockNumOrigin; ++i)
        {
          bm->free_block(ino->blocks[i]);
        }
      }

      // need to free both direct and all indirect blocks
      else
      {
        // free some direct blocks
        for (int i = blockNumber; i < NDIRECT; ++i)
        {
          bm->free_block(ino->blocks[i]);
        }

        // free some indirect blocks
        for (int i = NDIRECT; i < blockNumOrigin; ++i)
        {
          bm->free_block(bidList[i - NDIRECT]);
        }
      }
    }
  }

  // write file data
  for (int i = 0; i < MIN(blockNumber, NDIRECT); ++i)
  {
    bm->write_block(ino->blocks[i], buf_in + i * BLOCK_SIZE);
    
    // char *data = (char *)malloc(16);
    // bm->read_block(ino->blocks[i] , data);

    printf("\tim: inode %d's direct blocks[%d] write file data\n", inum, i);
  }

  // write data to indirect block
  if (blockNumber > NDIRECT)
  {
    for (int i = NDIRECT; i < blockNumber; ++i)
    {
      bm->write_block(bidList[i - NDIRECT], buf_in + i * BLOCK_SIZE);
      printf("\tim: inode %d's indirect blocks[%d] write file data\n", inum, i);
    }
  }

  // update inode meta data
  ino->size = size;
  ino->mtime = (uint32_t)time(NULL);
  put_inode(inum, ino);

  //struct inode *pno = get_inode(inum);
  //print_inode(pno);  

  // for (int i = 0; i < blockNumber; ++i)
  //   cout << ino->blocks[i] << ' ' << pno->blocks[i] << ' ';
  // cout << "\n";
  
  printf("\tim: write %d blocks data to inode %d\n", blockNumber, inum);
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

  printf("\tim: get attr of inode %d\n", inum);
}

void
inode_manager::remove_file(uint32_t inum)
{
  /*
   * your code goes here
   * note: you need to consider about both the data block and inode of the file
   */
  struct inode* ino = get_inode(inum);
  if (!ino)
  {
    printf("\tim: errpr! no inode %d to remove\n", inum);
    return;
  }

  int blockNumber = ino->size / BLOCK_SIZE + (ino->size % BLOCK_SIZE > 0);

  /* free data block */

  // only need to free direct blocks
  if (blockNumber <= NDIRECT)
  {
    for (int i = 0; i < blockNumber; ++i)
    {
      bm->free_block(ino->blocks[i]);
    }
  }
  
  // need to free both direct and indirect blocks
  else
  {
    // free direct blocks
    for (int i = 0; i < NDIRECT; ++i)
    {
      bm->free_block(ino->blocks[i]);
    }

    // get indirect blocks' id list
    uint32_t bidList[NINDIRECT];
    bm->read_block(ino->blocks[NDIRECT], (char *)bidList);

    // free indirect blocks
    for (int i = 0; i < blockNumber - NDIRECT; ++i)
    {
      bm->free_block(bidList[i]);
    }

    // free indirect blocks table
    bm->free_block(ino->blocks[NDIRECT]);
  }
  
  free_inode(inum);
  bm->free_block(IBLOCK(inum, bm->sb.nblocks));
}