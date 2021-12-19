// yfs client.  implements FS operations using extent and lock server
#include "yfs_client.h"
#include "extent_client.h"
#include <sstream>
#include <iostream>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

using namespace std;

yfs_client::yfs_client()
{
    ec = new extent_client();
    cache_init();
}

yfs_client::yfs_client(string extent_dst, string lock_dst)
{
    ec = new extent_client();
    if (ec->put(1, "") != extent_protocol::OK)
        printf("Init Error: failed to init root dir\n");

    cache_init();
}

yfs_client::~yfs_client()
{
    delete [] cache;
    delete [] incache;
}

#define MIN(a, b) ((a) < (b) ? (a) : (b))

string
yfs_client::filename(const char *name)
{
    size_t _name_size_ = strlen(name);
    string _name_str_(name, MIN(DIR_FNAME_LEN, _name_size_));

    if (_name_size_ < DIR_FNAME_LEN)
        _name_str_.insert(_name_size_, (DIR_FNAME_LEN - _name_size_), 0);

    return _name_str_;
}

yfs_client::inum
yfs_client::n2i(string n)
{
    istringstream ist(n);
    unsigned long long finum;
    ist >> finum;
    return finum;
}

string
yfs_client::i2n(inum inum)
{
    ostringstream ost;
    ost << inum;
    return ost.str();
}

string
yfs_client::entry(const char *name, inum inum)
{
    string _name_str_ = filename(name);
    string _inum_str_ = i2n(inum);
    size_t _inum_size = _inum_str_.size();

    return _name_str_ + _inum_str_.insert(_inum_size, DIR_INODE_LEN - _inum_size, 0);
}

/****************************/

bool 
yfs_client::inumCheck(inum inum)
{
    return (inum >= 0 && inum < INODE_NUM);
}


void
yfs_client::cache_init()
{
    incache = new inum[YFS_CACHE_NUM];
    cache = new string[YFS_CACHE_NUM];
}

void
yfs_client::cache_put(inum num, string data)
{
    int index = num%YFS_CACHE_NUM;
    incache[index] = num;
    cache[index] = data;
}

int
yfs_client::cache_get(inum num, string &data)
{
    int index = num%YFS_CACHE_NUM;
    if (incache[index] != num)
        return 0;
    else
    {
        data = cache[index];
        return 1;
    }
}

void
yfs_client::cache_remove(inum num)
{
    incache[num%YFS_CACHE_NUM] = -1;
}

int
yfs_client::yfs_get(inum num, string &data)
{
    if (!cache_get(num, data))
        return ec->get(num, data);

    return OK;
}

int
yfs_client::yfs_put(inum num, string data)
{
    int r;
    if ((r = ec->put(num, data)) == OK)
        cache_put(num, data);
    return r;
}

int
yfs_client::yfs_remove(inum num)
{
    int r;
    if ((r = ec->remove(num)) == OK)
        cache_remove(num);
    return r;
}

extent_protocol::types
yfs_client::getType(inum inum)
{
    // if (!inumCheck(inum))
    //     return extent_protocol::T_NONE;

    extent_protocol::attr a;
    if (ec->getattr(inum, a) != extent_protocol::OK)
        return extent_protocol::T_NONE;

    return (extent_protocol::types) a.type;
}

int
yfs_client::getAttr(inum inum, info &in)
{
    // if (!inumCheck(inum))
    //     return IOERR;

    extent_protocol::attr a;

    if (ec->getattr(inum, a) != extent_protocol::OK)
        return IOERR;

    in.type  = (extent_protocol::types) a.type;
    in.atime = a.atime;
    in.mtime = a.mtime;
    in.ctime = a.ctime;
    in.size  = a.size;

    return OK;
}

#define EXT_RPC(xx) do { \
    if ((xx) != extent_protocol::OK) { \
        printf("EXT_RPC Error: %s:%d \n", __FILE__, __LINE__); \
        r = IOERR; \
        goto release; \
    } \
} while (0)

// Only support set size of attr
int
yfs_client::setattr(inum ino, size_t size)
{
    // if (!inumCheck(ino) || size < 0)
    //     return IOERR;

    int r = OK;
    string buf;

    // read_file(ino, buf)
    // if ((r = ec->get(ino, buf)) != extent_protocol::OK)
    if ((r = yfs_get(ino, buf)) != OK)
        return r;

    // if size < _size_ need to erase some bits, else add some new bits 0
    size_t _size_ = buf.size();
    buf = (_size_ > size) ? buf.substr(0, size) : buf.insert(_size_, size - _size_, 0);

    // store back, attr of inode will be correct by write_inode
    // return ec->put(ino, buf);
    return yfs_put(ino, buf);
}

int
yfs_client::lookup(inum parent, const char *name, bool &found, inum &ino_out)
{
    list<dirent> ls = dir_cache[parent];
    for (list<dirent>::iterator it = ls.begin(); it != ls.end(); ++it)
    {
        if ((*it).name == string(name))
        {
            found = true;
            ino_out = (*it).inum;
            break;
        }
    }
    
    return OK;
}

int
yfs_client::create(inum parent, const char *name, mode_t mode, inum &ino_out)
{
    // if (!inumCheck(parent))
    //     return IOERR;

    int r = OK;
    bool found = false;

    if ((r = lookup(parent, name, found, ino_out)) != OK)
        return r;

    if (found)
        return EXIST;

    // alloc inode for the new file
    if ((r = ec->create(extent_protocol::T_FILE, ino_out)) != extent_protocol::OK)
        return r;
    
    /* add entry to the dir */
    // get dir
    string buf;
    // if ((r = ec->get(parent, buf)) != extent_protocol::OK)
    if ((r = yfs_get(parent, buf)) != OK)
        return r;
    
    // write back to parent
    buf = buf.insert(buf.size(), entry(name, ino_out));
    // if ((r = ec->put(parent, buf)) != extent_protocol::OK)
    if ((r = yfs_put(parent, buf)) != OK)
        return r;

    dirent file;
    file.inum = ino_out;
    file.name = string(name);
    dir_cache[parent].push_back(file);
    return OK;
}

int
yfs_client::mkdir(inum parent, const char *name, mode_t mode, inum &ino_out)
{
    // if (!inumCheck(parent))
    //     return IOERR;

    int r = OK;
    bool found = false;
    if ((r = lookup(parent, name, found, ino_out)) != extent_protocol::OK)
        return r;

    if (found)
        return EXIST;

    if ((r = ec->create(extent_protocol::T_DIR, ino_out)) != extent_protocol::OK)
        return r;

    string buf;

    // get parent dir
    // if ((r = ec->get(parent, buf)) != extent_protocol::OK)
    if ((r = yfs_get(parent, buf)) != OK)
        return r;

    // add entry in parent dir
    buf += entry(name, ino_out);

    // if ((r = ec->put(parent, buf)) != extent_protocol::OK)
    if ((r = yfs_get(parent, buf)) != OK)
        return r;
    
    dirent dir;
    dir.inum = ino_out;
    dir.name = string(name);
    dir_cache[parent].push_back(dir);
    list<dirent> files;
    dir_cache.insert(pair<inum, list<dirent> >(ino_out, files));
    return OK;
}

int
yfs_client::readdir(inum dir, list<dirent> &list)
{
    list = dir_cache[dir];
    return OK;
}

int
yfs_client::read(inum ino, size_t size, off_t off, string &data)
{
    // if (!inumCheck(ino))
    //     return IOERR;

    int r = OK;

    string buf;
    // if ((r = ec->get(ino, buf)) != extent_protocol::OK)
    if ((r = yfs_get(ino, buf)) != OK)
        return r;

    size_t _size_ = buf.size();
    if (off < 0 || off > (long)_size_)
    {
        data = "";
        return IOERR;
    }

    data = buf.substr(off, min(size, _size_ - off));

    return OK;
}

int 
yfs_client::write(inum ino, size_t size, off_t off, const char *data, size_t &bytes_written)
{
    // if (!inumCheck(ino) || size < 0)
    //     return IOERR;

    int r = OK;
    string buf;
    // if ((r = ec->get(ino, buf)) != extent_protocol::OK)
    if ((r = yfs_get(ino, buf)) != OK)
        return r;
    
    // off + write size <= file size
    if (off + size > buf.size()) 
        buf.resize(off + size);

    for (off_t i = off; i < off + (off_t)size; i++)
        buf[i] = data[i - off];

    bytes_written = size;
    // return ec->put(ino, buf);
    return yfs_put(ino, buf);
}

int yfs_client::unlink(inum parent, const char *name)
{
    // if (!inumCheck(parent))
    //     return IOERR;

    int r = OK;

    inum ino;
    bool found = false;
    if ((r = lookup(parent, name, found, ino)) != extent_protocol::OK)
        return r;

    if (!found)
        return NOENT;

    // remove file
    // if ((r = ec->remove(ino)) != extent_protocol::OK)
    if ((r = yfs_remove(ino)) != OK)
        return r;

    // remove entry in parent dir
    string buf;
    // if ((r = ec->get(parent, buf)) != extent_protocol::OK)
    if ((r = yfs_get(parent, buf)) != OK)
        return r;

    string _name_str_ = filename(name);
    for (size_t i = 0; i < buf.size(); i += DIR_ENTRY_LEN)
    {
        if (buf.substr(i, DIR_FNAME_LEN) == _name_str_)
        {
            buf.erase(i, DIR_ENTRY_LEN);
            break;
        }
    }

    // r = ec->put(parent, buf);
    r = yfs_put(parent, buf);

    list<dirent> ls = dir_cache[parent];
    for (list<dirent>::iterator it = ls.begin(); it != ls.end(); ++it)
    {
        if ((*it).name == string(name))
        {
            ls.erase(it);
            break;
        }
    }
    dir_cache[parent] = ls;
    return r;
}

int
yfs_client::symlink(inum parent, const char *link, inum &ino_out, const char *name)
{
    // if (!inumCheck(parent))
    //     return IOERR;

    int r = OK;

    // create a new inode
    if ((r = ec->create(extent_protocol::T_LINK, ino_out)) != extent_protocol::OK)
        return r;

    // save linked path to inode ino_out
    // if ((r = ec->put(ino_out, string(link))) != extent_protocol::OK)
    if ((r = yfs_put(ino_out, string(link))) != OK)
        return r;

    string buf;
    // if ((r = ec->get(parent, buf)) != extent_protocol::OK)
    if ((r = yfs_get(parent, buf)) != OK)
        return r;

    buf += entry(name, ino_out);

    // if ((r = ec->put(parent, buf)) != extent_protocol::OK)
    if ((r = yfs_put(parent, buf)) != OK)
        return r;

    dirent file;
    file.inum = ino_out;
    file.name = string(name);
    dir_cache[parent].push_back(file);
    return OK;
}

int
yfs_client::readlink(inum ino, string &data)
{
    // if (!inumCheck(ino))
    //     return IOERR;

    // return ec->get(ino, data);
    return yfs_get(ino, data);
}
