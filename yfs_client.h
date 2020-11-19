#ifndef yfs_client_h
#define yfs_client_h

#include <string>
#include <bitset>
#include <map>
//#include "yfs_protocol.h"
#include "extent_client.h"
#include <vector>

using namespace std;

#define YFS_CACHE_NUM 32

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
	} info_t;

	typedef struct ent
	{
		string data;
	} entry_t;

	struct dirent
	{
		string name;
		yfs_client::inum inum;
	};

private:
	bitset<INODE_NUM> imap; // whether an inode exists
	inum *incache; // whether an inode in cache
	string *cache;
	int cache_size;

private:
	static string filename(const char *);
	static inum n2i(string);
	static string i2n(inum);
	static string entry(const char *, inum);

public:
	yfs_client();
	yfs_client(string, string);
	~yfs_client();

	bool inumCheck(inum);

	void cache_init();
	void cache_put(inum, string);
	int cache_get(inum, string &);
	void cache_remove(inum);

	int yfs_get(inum, string &);
	int yfs_put(inum, string);

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
