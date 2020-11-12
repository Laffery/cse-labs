#ifndef yfs_client_h
#define yfs_client_h

#include <string>
//#include "yfs_protocol.h"
#include "extent_client.h"
#include <vector>

using namespace std;

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

	struct info
	{
		extent_protocol::types type;
		unsigned long long size;
		unsigned long atime;
		unsigned long mtime;
		unsigned long ctime;
	};

	// struct fileinfo
	// {
	// 	unsigned long long size;
	// 	unsigned long atime;
	// 	unsigned long mtime;
	// 	unsigned long ctime;
	// };

	// struct dirinfo
	// {
	// 	unsigned long atime;
	// 	unsigned long mtime;
	// 	unsigned long ctime;
	// };

	// struct syminfo
	// {
	// 	unsigned long long size;
	// 	unsigned long atime;
	// 	unsigned long mtime;
	// 	unsigned long ctime;
	// };

	struct dirent
	{
		string name;
		yfs_client::inum inum;
	};

private:
	static string filename(const char *);
	static inum n2i(string);
	static string i2n(inum);
	static string entry(const char *, inum);

public:
	yfs_client();
	yfs_client(string, string);

	extent_protocol::types getType(inum);
	int getAttr(inum, info &);

	int setattr(inum, size_t);
	int lookup(inum, const char *, bool &, inum &);
	int create(inum, const char *, mode_t, inum &);
	int readdir(inum, list<dirent> &);
	int write(inum, size_t, off_t, const char *, size_t &);
	int read(inum, size_t, off_t, string &);
	int unlink(inum, const char *);
	int mkdir(inum, const char *, mode_t, inum &);

	/* you may need to add symbolic link related methods here */
	int symlink(inum, const char *, inum &, const char *);
	int readlink(inum, string &);
};

#endif
