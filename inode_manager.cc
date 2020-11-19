#include "inode_manager.h"
#include <time.h>
#include <iostream>

using namespace std;

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define FIRST_BLOCK (3 + BLOCK_NUM / BPB + INODE_NUM / IPB)

// disk layer -----------------------------------------

disk::disk()
{
	/* set all bits in blocks 0 */
	memset(blocks, 0, sizeof(blocks));
}

void disk::read_block(blockid_t id, char *buf)
{
	if (id < 0 || id >= BLOCK_NUM || buf == NULL)
		return;
	else
		memcpy(buf, blocks[id], BLOCK_SIZE);
}

void disk::write_block(blockid_t id, const char *buf)
{
	if (id < 0 || id >= BLOCK_NUM || buf == NULL)
		return;
	else
		memcpy(blocks[id], buf, BLOCK_SIZE);
}

// block layer -----------------------------------------

// Allocate a free disk block.
blockid_t
block_manager::alloc_block()
{
	for (blockid_t i = FIRST_BLOCK; i < BLOCK_NUM; ++i)
	{
		if (!using_blocks[i])
		{
			using_blocks.flip(i);
			return i;
		}
	}

	return 0;
}

blockid_t *
block_manager::alloc_nblock(int size)
{
	int tmp = 0;
	blockid_t *ids;
	ids = new blockid_t[size];

	for (blockid_t i = FIRST_BLOCK; i < BLOCK_NUM; ++i)
	{
		if (!using_blocks[i])
		{
			using_blocks.flip();
			ids[tmp] = i;
			tmp++;
		}

		if (tmp == size)
			break;
	}

	return ids;
}

void 
block_manager::free_block(uint32_t id)
{
	using_blocks.set(id, 0);
}

void 
block_manager::free_nblock(uint32_t *ids, int size)
{
	for (int i = 0; i < size; ++i)
		using_blocks.set(i, 0);
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
}

void block_manager::read_block(uint32_t id, char *buf)
{
	d->read_block(id, buf);
}

void block_manager::write_block(uint32_t id, const char *buf)
{
	d->write_block(id, buf);
}

// inode layer -----------------------------------------

// usable inode block start from 2, 1 is root_dir
inode_manager::inode_manager()
{
	bm = new block_manager();
	
	using_inodes.flip(0);

	uint32_t root_dir = alloc_inode(extent_protocol::T_DIR); // T_DIR = 1
	if (root_dir != 1)
	{
		printf("\tim: error! alloc first inode %d, should be 1\n", root_dir);
		exit(0);
	}
}

bool
inode_manager::inumCheck(uint32_t inum)
{
	return (inum >= 0 && inum < INODE_NUM);
}

/* Create a new file and Return its inum. */
uint32_t
inode_manager::alloc_inode(uint32_t type)
{
	for (int i = 1; i < INODE_NUM; i++)
	{
		// usable inode i
		if (!using_inodes[i])
		{
			using_inodes.flip(i);
			struct inode ino;
			ino.type = type;
			ino.size = 0;

			uint32_t CURR_TIME = (uint32_t)time(NULL);
			ino.atime = CURR_TIME;
			ino.ctime = CURR_TIME;
			ino.mtime = CURR_TIME;

			put_inode(i, &ino);
			return i;
		}
	}

	return 0;
}

void 
inode_manager::free_inode(uint32_t inum)
{
	if (!inumCheck(inum))
		return;

	if (using_inodes[inum])
		using_inodes.flip(inum);
}

/* 
 * Return an inode structure by inum, NULL otherwise.
 * Caller should release the memory. 
 */
struct inode *
inode_manager::get_inode(uint32_t inum)
{
	if (!using_inodes[inum])
		return NULL;

	struct inode *ino, *ino_disk;
	char buf[BLOCK_SIZE];

	bm->read_block(IBLOCK(inum, bm->sb.nblocks), buf);
	ino_disk = (struct inode *)buf + inum % IPB;

	ino = (struct inode *)malloc(sizeof(struct inode));
	*ino = *ino_disk;
	return ino;
}

void inode_manager::put_inode(uint32_t inum, struct inode *ino)
{
	char buf[BLOCK_SIZE];
	struct inode *ino_disk;

	if (!ino)
		return;

	bm->read_block(IBLOCK(inum, bm->sb.nblocks), buf);
	ino_disk = (struct inode *)buf + inum % IPB;
	*ino_disk = *ino;
	bm->write_block(IBLOCK(inum, bm->sb.nblocks), buf);
}

/* 
 * Get all the data of a file by inum. 
 * Return alloced data, should be freed by caller.
 */
void inode_manager::read_file(uint32_t inum, char **buf_out, int *size)
{
	if (!inumCheck(inum))
		return;
	struct inode *ino = get_inode(inum);
	if (!ino)
		return;

	// how many block does this inode have
	int blockNumber = ino->size / BLOCK_SIZE + (ino->size % BLOCK_SIZE > 0);
	// allocate space for read buffer
	char *buf = (char *)malloc(blockNumber * BLOCK_SIZE);

	// clone direct blocks' data to buffer
	for (int i = 0; i < MIN(blockNumber, NDIRECT); ++i)
		bm->read_block(ino->blocks[i], buf + i * BLOCK_SIZE);

	// there is some indirect blocks in this inode
	if (blockNumber > NDIRECT) // all block of this inode is direct
	{
		// indirect blocks' id list
		uint32_t bidList[NINDIRECT];
		bm->read_block(ino->blocks[NDIRECT], (char *)bidList);

		for (int i = 0; i < blockNumber - NDIRECT; ++i)
			bm->read_block(bidList[i], buf + (NDIRECT + i) * BLOCK_SIZE);
	}

	*size = ino->size;
	*buf_out = buf;

	// update access time
	ino->atime = (uint32_t)time(NULL);
	put_inode(inum, ino);
}

/* alloc/free blocks if needed */
void inode_manager::write_file(uint32_t inum, const char *buf_in, int size)
{
	if (!inumCheck(inum))
		return;
	struct inode *ino = get_inode(inum);
	if (!ino)
		return;

	// how many blocks is needed
	int blockNumber = size / BLOCK_SIZE + (size % BLOCK_SIZE > 0);
	if ((uint32_t)blockNumber > MAXFILE)
		return;

	// how many blocks originally does this inode have
	int blockNumOrigin = ino->size / BLOCK_SIZE + (ino->size % BLOCK_SIZE > 0);

	// indirect blocks' id list
	uint32_t bidList[NINDIRECT];
	if (blockNumber > NDIRECT || blockNumOrigin > NDIRECT)
		bm->read_block(ino->blocks[NDIRECT], (char *)bidList);

	// need to allocate new block
	if (blockNumber > blockNumOrigin)
	{
		// block number to write is less than NDIRECT, just allocate direct blocks directly
		if (blockNumber <= NDIRECT)
		{
			for (int i = blockNumOrigin; i < blockNumber; ++i)
				ino->blocks[i] = bm->alloc_block();
		}

		else
		{
			// need to allocate both direct and indirect blocks
			if (blockNumOrigin <= NDIRECT)
			{
				for (int i = blockNumOrigin; i < NDIRECT; ++i)
					ino->blocks[i] = bm->alloc_block();

				// there is no indirect block in origin, so that we need to alloc
				for (int i = NDIRECT; i < blockNumber; ++i)
					bidList[i - NDIRECT] = bm->alloc_block();
			}

			// need to allocate indirect blocks only
			else
			{
				for (int i = blockNumOrigin; i < blockNumber; ++i)
					bidList[i - NDIRECT] = bm->alloc_block();
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
				bm->free_block(bidList[i - NDIRECT]);
		}

		else
		{
			// just need to free some direct blocks
			if (blockNumOrigin <= NDIRECT)
			{
				for (int i = blockNumber; i < blockNumOrigin; ++i)
					bm->free_block(ino->blocks[i]);
			}

			// need to free both direct and all indirect blocks
			else
			{
				// free some direct blocks
				for (int i = blockNumber; i < NDIRECT; ++i)
					bm->free_block(ino->blocks[i]);

				// free some indirect blocks
				for (int i = NDIRECT; i < blockNumOrigin; ++i)
					bm->free_block(bidList[i - NDIRECT]);
			}
		}
	}

	// write file data
	for (int i = 0; i < MIN(blockNumber, NDIRECT); ++i)
		bm->write_block(ino->blocks[i], buf_in + i * BLOCK_SIZE);

	// write data to indirect block
	if (blockNumber > NDIRECT)
	{
		for (int i = NDIRECT; i < blockNumber; ++i)
			bm->write_block(bidList[i - NDIRECT], buf_in + i * BLOCK_SIZE);
	}

	// update inode meta data
	ino->size = size;
	ino->mtime = (uint32_t)time(NULL);
	ino->ctime = ino->mtime;
	ino->atime = ino->mtime;
	put_inode(inum, ino);
}

void inode_manager::getattr(uint32_t inum, extent_protocol::attr &a)
{
	if (!inumCheck(inum))
		return;
	struct inode *ino = get_inode(inum);
	if (!ino)
		return;

	a.atime = ino->atime;
	a.ctime = ino->ctime;
	a.mtime = ino->mtime;
	a.size = ino->size;
	a.type = ino->type;
}

void inode_manager::remove_file(uint32_t inum)
{
	if (!inumCheck(inum))
		return;
	struct inode *ino = get_inode(inum);
	if (!ino)
		return;

	int blockNumber = ino->size / BLOCK_SIZE + (ino->size % BLOCK_SIZE > 0);

	/* free data block */

	// only need to free direct blocks
	if (blockNumber <= NDIRECT)
	{
		for (int i = 0; i < blockNumber; ++i)
		{
			bm->free_block(ino->blocks[i]);
		}
		// bm->free_nblock(ino->blocks, blockNumber);
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
		// bm->free_nblock(bidList, blockNumber - NDIRECT);

		// free indirect blocks table
		bm->free_block(ino->blocks[NDIRECT]);
	}

	free_inode(inum);
	bm->free_block(IBLOCK(inum, bm->sb.nblocks));
}