#ifndef yfs_client_h
#define yfs_client_h

#include <string>
#include "lock_protocol.h"
#include "lock_client.h"
#include "lock_client_cache.h"
//#include "yfs_protocol.h"
#include "extent_client.h"
#include <vector>

using namespace std;

#define MIN(a, b) ((a) < (b) ? (a) : (b))

#define _LOCK_

class yfs_client
{
	extent_client *ec;
#ifdef _LOCK_
	lock_client *lc;
#endif

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
	struct syminfo
	{
		unsigned long long size;
		unsigned long atime;
		unsigned long mtime;
		unsigned long ctime;
	};
	struct dirent
	{
		std::string name;
		yfs_client::inum inum;
	};

private:
	static string filename(const char *);
	static inum n2i(string);
	static string i2n(inum);
	static string entry(const char *, inum);

	void LOCK(inum);
	void UNLOCK(inum);

public:
	yfs_client(string, string);

	bool isfile(inum);
	bool isdir(inum);
	bool issymlink(inum);

	int getfile(inum, fileinfo &);
	int getdir(inum, dirinfo &);
	int getsymlink(inum, syminfo &);

	int setattr(inum, size_t);
	int lookup(inum, const char *, bool &, inum &);
	int lookvp(inum, const char *, bool &, inum &);
	int create(inum, const char *, mode_t, inum &);
	int readdir(inum, std::list<dirent> &);
	int write(inum, size_t, off_t, const char *, size_t &);
	int read(inum, size_t, off_t, std::string &);
	int unlink(inum, const char *);
	int mkdir(inum, const char *, mode_t, inum &);
	int symlink(inum, const char *, inum &, const char *);
	int readlink(inum, string &);
};

#endif
