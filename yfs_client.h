#ifndef yfs_client_h
#define yfs_client_h

#include <string>
//#include "yfs_protocol.h"
#include "extent_client.h"
#include <vector>

using namespace std;

/*
 * TODO: part2A
 * implement the CREATE/MKNOD, LOOKUP and READDIR
 * pass test-lab1-part2-a, which test creating empty files, 
 * looking up names in a dictory, and listing dictory contents
 *
 * TODO: part2B
 * implement the SETATT, WRITE and READ FUSE operations in fuse.cc and yfs_client.cc
 * pass test-lab1-part2-b, do not modify RPC library
 * 
 * TODO: part2C
 * handle the MKDIR and UNLINK FUSE operations
 * pass test-lab1-part2-c
 * 
 * TODO: part2D
 * handle the SYMLINK and READLINK operations
 * pass test-lab1-part2-d
 */

class yfs_client
{
	extent_client *ec;

public:
	typedef unsigned long long inum;
	enum xxstatus
	{
		OK,
		RPCERR,
		NOENT,
		IOERR,
		EXIST
	};
	typedef int status;

	struct fileinfo
	{
		unsigned long long size;
		unsigned long atime;
		unsigned long mtime;
		unsigned long ctime;
	};

	struct dirinfo
	{
		unsigned long atime;
		unsigned long mtime;
		unsigned long ctime;
	};

	struct dirent
	{
		string name;
		yfs_client::inum inum;
	};

private:
	static string filename(const char *);
	static inum n2i(string);
	static string i2n(inum);

public:
	yfs_client();
	yfs_client(string, string);

	bool isfile(inum);
	bool isdir(inum);

	int getfile(inum, fileinfo &);
	int getdir(inum, dirinfo &);

	int setattr(inum, size_t);
	int lookup(inum, const char *, bool &, inum &);
	int create(inum, const char *, mode_t, inum &);
	int readdir(inum, list<dirent> &);
	int write(inum, size_t, off_t, const char *, size_t &);
	int read(inum, size_t, off_t, string &);
	int unlink(inum, const char *);
	int mkdir(inum, const char *, mode_t, inum &);

	/* you may need to add symbolic link related methods here */
};

#endif
