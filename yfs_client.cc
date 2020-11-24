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

yfs_client::yfs_client(std::string extent_dst, std::string lock_dst)
{
    ec = new extent_client(extent_dst);
    // Lab2: Use lock_client_cache when you test lock_cache
    // lc = new lock_client(lock_dst);
    lc = new lock_client_cache(lock_dst);
    if (ec->put(1, "") != extent_protocol::OK)
        printf("error init root dir\n"); // XYB: init root dir
}

void
yfs_client::LOCK(inum ino)
{
    lc->acquire(ino);
    printf("LOCK %llu\n", ino);
}

void
yfs_client::UNLOCK(inum ino)
{
    lc->release(ino);
    printf("UNLOCK %llu\n", ino);
}

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
yfs_client::n2i(std::string n)
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
    size_t _inum_size_ = _inum_str_.size();

    return _name_str_ + _inum_str_.insert(_inum_size_, DIR_INODE_LEN - _inum_size_, 0);
}

bool
yfs_client::isfile(inum inum)
{
    extent_protocol::attr a;
    LOCK(inum);

    if (ec->getattr(inum, a) != extent_protocol::OK)
    {
        UNLOCK(inum);
        printf("error getting attr\n");
        return false;
    }

    if (a.type == extent_protocol::T_FILE)
    {
        UNLOCK(inum);
        printf("isfile: %lld is a file\n", inum);
        return true;
    }

    UNLOCK(inum);
    printf("isfile: %lld is not a file\n", inum);
    return false;
}

bool
yfs_client::isdir(inum inum)
{
    extent_protocol::attr a;

    LOCK(inum);
    if (ec->getattr(inum, a) != extent_protocol::OK)
    {
        UNLOCK(inum);
        printf("error getting attr\n");
        return false;
    }

    if (a.type == extent_protocol::T_DIR)
    {
        UNLOCK(inum);
        printf("isdir: %lld is a dir\n", inum);
        return true;
    } 

    UNLOCK(inum);
    printf("isdir: %lld is not a dir\n", inum);
    return false;
}

bool
yfs_client::issymlink(inum inum)
{
    extent_protocol::attr a;
    LOCK(inum);

    if (ec->getattr(inum, a) != extent_protocol::OK)
    {
        UNLOCK(inum);
        printf("error getting attr\n");
        return false;
    }

    if (a.type == extent_protocol::T_SYMLINK)
    {
        UNLOCK(inum);
        printf("issymlink: %lld is a symlink\n", inum);
        return true;
    }

    UNLOCK(inum);
    printf("issymlink: %lld is not a symlink\n", inum);
    return false;
}


int
yfs_client::getfile(inum inum, fileinfo &fin)
{
    int r = OK;

    LOCK(inum);
    printf("getfile %016llx\n", inum);
    extent_protocol::attr a;
    if (ec->getattr(inum, a) != extent_protocol::OK) {
        r = IOERR;
        goto release;
    }

    fin.atime = a.atime;
    fin.mtime = a.mtime;
    fin.ctime = a.ctime;
    fin.size = a.size;
    printf("getfile %016llx -> sz %llu\n", inum, fin.size);

release:
    UNLOCK(inum);
    return r;
}

int
yfs_client::getdir(inum inum, dirinfo &din)
{
    int r = OK;

    LOCK(inum);
    printf("getdir %016llx\n", inum);
    extent_protocol::attr a;
    if (ec->getattr(inum, a) != extent_protocol::OK) {
        r = IOERR;
        goto release;
    }
    din.atime = a.atime;
    din.mtime = a.mtime;
    din.ctime = a.ctime;

release:
    UNLOCK(inum);
    return r;
}

int
yfs_client::getsymlink(inum inum, syminfo &sin)
{
    int r = OK;

    LOCK(inum);
    printf("getsymlink %016llx\n", inum);
    extent_protocol::attr a;
    if (ec->getattr(inum, a) != extent_protocol::OK) {
        r = IOERR;
        goto release;
    }

    sin.atime = a.atime;
    sin.mtime = a.mtime;
    sin.ctime = a.ctime;
    sin.size = a.size;
    printf("getsymlink %016llx -> sz %llu\n", inum, sin.size);

release:
    UNLOCK(inum);
    return r;
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
    // printf("YFS: _set_attr(ino,sz) %llu %lu\n", ino, size);
    int r = OK;
    
    if (size < 0)
    {
        printf("YFS: _setattr Error: size %lu less than 0\n", size);
        return IOERR;
    }

    string buf;

    // read_file(ino, buf)
    LOCK(ino);
    if ((r = ec->get(ino, buf)) != extent_protocol::OK)
    {
        printf("YFS: _setattr Error: cannot get the content of inode %llu\n", ino);
        UNLOCK(ino);
        return r;
    }

    // if size < _size_ need to erase some bits, else add some new bits 0
    size_t _size_ = buf.size();
    buf = (_size_ > size) ? buf.substr(0, size) : buf.insert(_size_, size - _size_, 0);

    // store back, attr of inode will be correct by write_inode
    if ((r = ec->put(ino, buf)) != extent_protocol::OK)
    {
        printf("YFS: _setattr Error: cannot store modified content to inode %llu\n", ino);
        UNLOCK(ino);
        return r;
    }

    // printf("YFS: _setattr: set attr of inode %llu\n", ino);
    UNLOCK(ino);
    return OK;
}

int
yfs_client::create(inum parent, const char *name, mode_t mode, inum &ino_out)
{
    // printf("YFS: _create '%s' in %llu\n", name, parent);
    int r = OK;
    bool found = false;

    LOCK(parent);
    if ((r = lookup(parent, name, found, ino_out)) != OK)
    {
        printf("YFS: _create Error: cannot open dir inode %llu", parent);
        UNLOCK(parent);
        return r;
    }

    if (found)
    {
        printf("YFS: _create Error: file '%s' has been existed\n", filename(name).c_str());
        UNLOCK(parent);
        return EXIST;
    }


    // LOCK(parent);
    // alloc inode for the new file
    if ((r = ec->create(extent_protocol::T_FILE, ino_out)) != extent_protocol::OK)
    {
        printf("YFS: _create Error: cannot allocate inode for file '%s'\n", filename(name).c_str());
        UNLOCK(parent);
        return r;
    }

    /* add entry to the dir */
    // get dir
    string buf;
    if ((r = ec->get(parent, buf)) != extent_protocol::OK)
    {
        printf("YFS: _create Error: cannot open dir inode %llu\n", parent);
        UNLOCK(parent);
        return r;
    }
    
    // write back to parent
    string _entry_;
    if ((r = ec->put(parent, buf.insert(buf.size(), (_entry_ = entry(name, ino_out))))) != extent_protocol::OK)
    {
        printf("YFS: _create Error: connot write entry '%s' to dir\n", _entry_.c_str());
        UNLOCK(parent);
        return r;
    }

    // printf("\tYFS: _create: create file '%s' with inode %llu in dir\n", name, ino_out);
    UNLOCK(parent);
    return OK;
}

int
yfs_client::mkdir(inum parent, const char *name, mode_t mode, inum &ino_out)
{
    // printf("YFS: _mkdir '%s' in %llu\n", name, parent);
    int r = OK;
    bool found = false;

    LOCK(parent);
    if ((r = lookup(parent, name, found, ino_out)) != extent_protocol::OK)
    {
        printf("YFS: _mkdir: look up failed\n");
        UNLOCK(parent);
        return r;
    }

    if (found)
    {
        printf("YFS: _create: file '%s' is existed in %llu\n", name, parent);
        UNLOCK(parent);
        return EXIST;
    }

    // LOCK(parent);
    if ((r = ec->create(extent_protocol::T_DIR, ino_out)) != extent_protocol::OK)
    {
        printf("YFS: _mkdir: failed to create dir in %llu\n", parent);
        UNLOCK(parent);
        return r;
    }

    string buf;

    // get parent dir
    if ((r = ec->get(parent, buf)) != extent_protocol::OK)
    {
        printf("YFS: _mkdir: cannot open dir %llu\n", parent);
        UNLOCK(parent);
        return r;
    }

    buf += entry(name, ino_out);

    if ((r = ec->put(parent, buf)) != extent_protocol::OK)
    {
        printf("YFS: _mkdir: failed to write back %llu\n", parent);
        UNLOCK(parent);
        return r;
    }

    UNLOCK(parent);
    // printf("YFS: _mkdir '%s' with inode %llu in %llu\n", name, ino_out, parent);
    return OK;
}

int
yfs_client::lookup(inum parent, const char *name, bool &found, inum &ino_out)
{
    // printf("YFS: _lookup '%s' in %llu\n", name, parent);
    int r = OK;
    string buf;
    
    // read parent dir
    // LOCK(parent);
    if ((r = ec->get(parent, buf)) != extent_protocol::OK)
    {
        printf("Lookup Error: cannot open dir inode %llu\n", parent);
        // UNLOCK(parent);
        return r;
    }

    // UNLOCK(parent);

    string _name_str_ = filename(name);  

    // loop for every entry in the parent dir
    for (size_t i = 0; i < buf.size(); i += DIR_ENTRY_LEN)
    {
        // the ith filename
        string _file_name_ = buf.substr(i, DIR_FNAME_LEN);
        
        // find it!
        if (!strcmp(_file_name_.c_str(), _name_str_.c_str()))
        {
            ino_out = n2i(buf.substr(i + DIR_FNAME_LEN, DIR_INODE_LEN));
            printf("YFS: _lookup: file '%s' is found with inode %llu\n", _name_str_.c_str(), ino_out);
            found = true;
            return OK;
        }
    }

    // not found
    found = false;
    // printf("YFS: _lookup: file '%s' does not exist\n", _name_str_.c_str());
    return OK;
}

int
yfs_client::readdir(inum dir, std::list<dirent> &list)
{
    // printf("\nYFS: _read_dir %llu\n", dir);
    int r = OK;

    if (dir <= 0 || dir > INODE_NUM)
    {
        printf("YFS: _readdir Error: dir inode %llu out of range\n", dir);
        return IOERR;
    }

    string buf;
    LOCK(dir);
    if ((r = ec->get(dir, buf)) != extent_protocol::OK)
    {
        printf("YFS: _readdir Error: cannot open dir %llu\n", dir);
        UNLOCK(dir);
        return r;
    }

    UNLOCK(dir);

    // printf("YFS: _readdir: read entried of dir %llu\n", dir);

    for (size_t i = 0; i < buf.size(); i += DIR_ENTRY_LEN)
    {
        printf("i: %lu, buf.size: %lu\n", i, buf.size());
        struct dirent _entry_;
        _entry_.name = buf.substr(i, DIR_FNAME_LEN);
        _entry_.inum = n2i(buf.substr(i + DIR_FNAME_LEN, DIR_INODE_LEN));
        printf("\tentry(ino, name): %llu, %s\n", _entry_.inum, _entry_.name.c_str());
        list.push_back(_entry_);
    }

    return OK;
}

int
yfs_client::read(inum ino, size_t size, off_t off, std::string &data)
{
    // printf("YFS: _read %llu sz %lu off %ld\n", ino, size, off);
    int r = OK;
    string buf;

    LOCK(ino);
    if ((r = ec->get(ino, buf)) != extent_protocol::OK)
    {
        printf("YFS: _read: cannot open file %llu\n", ino);
        UNLOCK(ino);
        return r;
    }

    UNLOCK(ino);

    size_t _size_ = buf.size();
    if (off < 0 || off > (long)_size_)
    {
        data = "";
        printf("YFS: _read: off %ld is out of range %lu\n", off, _size_);
        return IOERR;
    }

    if (size + off > _size_) {
        data = buf.substr(off, _size_ - off);
    } else {
        data = buf.substr(off, size);
    }

    // printf("YFS: _read: data: %s\n", data.c_str());

    return OK;
}

int
yfs_client::write(inum ino, size_t size, off_t off, const char *data, size_t &bytes_written)
{
    // printf("YFS: _write %llu\n", ino);
    int r = OK;
    string buf;
    string add(size, 0);

    for (size_t i = 0; i < size; ++i)
        add.at(i) = data[i];

    LOCK(ino);
    if ((r = ec->get(ino, buf)) != extent_protocol::OK)
    {
        printf("YFS: _write: cannot open file %llu\n", ino);
        UNLOCK(ino);
        return r;
    }

    size_t _org_file_size_ = buf.size();
    size_t _max_file_size_ = MAXFILE * BLOCK_SIZE;

    // out of maxium file size
    if (off >= (long)_max_file_size_)
    {
        // fill to maxsize
        bytes_written = _max_file_size_ - _org_file_size_;
        buf.insert(_org_file_size_, bytes_written, '\0');
    }

    else
    {
        size_t _new_file_size_ = off + size;
        if (_new_file_size_ > _max_file_size_)
        {
            bytes_written = _max_file_size_ - (off + size);
            buf.insert(_org_file_size_, bytes_written, '\0');
            buf += add;
            bytes_written += size;
        }

        else if ((unsigned long)off > _org_file_size_)
        {
            bytes_written = off - _org_file_size_;
            buf.insert(_org_file_size_, bytes_written, '\0');
            buf += add;
            bytes_written += size;
        }
        
        else if (_new_file_size_ > _org_file_size_)
        {
            buf = buf.substr(0, off) + add;
            bytes_written = size;
        }
        
        else
        {
            bytes_written = size;
            buf.replace(off, size, add);
        }
    }
    

    if ((r = ec->put(ino, buf)) != extent_protocol::OK)
    {
        printf("YFS: _write: failed to write back to %llu\n", ino);
    }

    UNLOCK(ino);
    return r;
}

int yfs_client::unlink(inum parent,const char *name)
{
    // printf("YFS: _unlink '%s' in %llu\n", name, parent);
    int r = OK;
    inum ino;
    bool found = false;

    LOCK(parent);
    if ((r = lookup(parent, name, found, ino)) != extent_protocol::OK)
    {
        printf("YFS: _unlink: some error in lookup '%s' in %llu\n", name, parent);
        UNLOCK(parent);
        return r;
    }

    if (!found)
    {
        printf("YFS: _unlink: '%s' is not in %llu\n", name, parent);
        UNLOCK(parent);
        return NOENT;
    }

    LOCK(ino);
    // remove file
    if ((r = ec->remove(ino)) != extent_protocol::OK)
    {
        printf("YFS: _unlink: failed to remove inode %llu\n", ino);
        UNLOCK(parent);
        UNLOCK(ino);
        return r;
    }

    UNLOCK(ino);

    // remove entry in parent dir
    // LOCK(parent);
    string buf;
    if ((r = ec->get(parent, buf)) != extent_protocol::OK)
    {
        printf("YFS: _unlink: failed to open %llu\n", parent);
        UNLOCK(parent);
        return r;
    }

    string _name_str_ = filename(name);
    for (size_t i = 0; i < buf.size(); i += DIR_ENTRY_LEN)
    {
        if (buf.substr(i, DIR_FNAME_LEN) == _name_str_)
        {
            buf.erase(i, DIR_ENTRY_LEN);
            break;
        }
    }

    if ((r = ec->put(parent, buf)) != extent_protocol::OK)
    {
        printf("YFS: _unlink: failed to write back %llu\n", parent);
    }

    UNLOCK(parent); 
    // printf("YFS: _unlink '%s' in %llu\n", name, parent);
    return r;
}

int
yfs_client::symlink(inum parent, const char *link, inum &ino_out, const char *name)
{
    // printf("YFS: _symlink(link, name): '%s' '%s' in %llu\n", link, name, parent);
    int r = OK;

    // create a new inode
    LOCK(parent);
    if ((r = ec->create(extent_protocol::T_SYMLINK, ino_out)) != extent_protocol::OK)
    {
        printf("YFS: _symlink: cannot create a new inode\n");
        UNLOCK(parent);
        return r;
    }

    LOCK(ino_out);
    // save linked path to inode ino_out
    if ((r = ec->put(ino_out, string(link))) != extent_protocol::OK)
    {
        printf("YFS: _symlink: failed to write link contene\n");
        UNLOCK(ino_out);
        UNLOCK(parent);
        return r;
    }
    UNLOCK(ino_out);

    string buf;
    if ((r = ec->get(parent, buf)) != extent_protocol::OK)
    {
        printf("YFS: _symlink: cannot open dir %llu\n", parent);
        UNLOCK(parent);
        return r;
    }

    buf += entry(name, ino_out);

    if ((r = ec->put(parent, buf)) != extent_protocol::OK)
    {
        printf("YFS: _symlink: failed to write back to dir %llu\n", parent);
        UNLOCK(parent);
        return r;
    }

    UNLOCK(parent);
    // printf("YFS: _symlink: create '%s' link to '%s' in %llu\n", link, name, parent);
    return OK;
}

int
yfs_client::readlink(inum ino, string &data)
{
    // printf("YFS: _readlink %llu\n", ino);
    int r = OK;

    LOCK(ino);
    if ((r = ec->get(ino, data)) != extent_protocol::OK)
    {
        UNLOCK(ino);
        printf("YFS: _readlink: some error in read\n");
        return r;
    }

    UNLOCK(ino);
    // printf("YFS: _readlink %llu success\n", ino);
    return OK;
}