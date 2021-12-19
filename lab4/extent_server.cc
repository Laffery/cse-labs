// the extent server implementation

#include "extent_server.h"
#include <sstream>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "handle.h"

extent_server::extent_server() 
{
  im = new inode_manager();
}

#ifdef CACHE
int extent_server::create(extent_protocol::clientid_t clt, uint32_t type, extent_protocol::extentid_t &id)
{
  // alloc a new inode and return inum
  id = im->alloc_inode(type);
  return extent_protocol::OK;
}

int extent_server::put(extent_protocol::clientid_t clt, extent_protocol::extentid_t id, std::string buf, int &)
{
  id &= 0x7fffffff;
  
  const char * cbuf = buf.c_str();
  int size = buf.size();
  im->write_file(id, cbuf, size);

  if (clients.count(clt) == 0)
    clients[clt] = 1;

  extent_protocol::attr attr;
  memset(&attr, 0, sizeof(attr));
  im->getattr(id, attr);

  map<extent_protocol::clientid_t, int>::iterator it = clients.begin();
  for (; it != clients.end(); ++it)
  {
    handle h(it->first);
    rpcc *cl = h.safebind();
    if (cl) {
      string data = cbuf;
      int r;
      cl->call(extent_protocol::update, id, data, attr, r);
    }
  }

  return extent_protocol::OK;
}

int extent_server::get(extent_protocol::clientid_t clt, extent_protocol::extentid_t id, std::string &buf)
{
  id &= 0x7fffffff;

  int size = 0;
  char *cbuf = NULL;

  im->read_file(id, &cbuf, &size);
  if (size == 0)
    buf = "";
  else {
    buf.assign(cbuf, size);
    free(cbuf);
  }

  return extent_protocol::OK;
}

int extent_server::getattr(extent_protocol::clientid_t clt, extent_protocol::extentid_t id, extent_protocol::attr &a)
{
  id &= 0x7fffffff;
  
  extent_protocol::attr attr;
  memset(&attr, 0, sizeof(attr));
  im->getattr(id, attr);
  a = attr;

  return extent_protocol::OK;
}

int extent_server::remove(extent_protocol::clientid_t clt, extent_protocol::extentid_t id, int &)
{
  id &= 0x7fffffff;
  im->remove_file(id);

  map<extent_protocol::clientid_t, int>::iterator it = clients.begin();
  for (; it != clients.end(); ++it)
  {
    handle h(it->first);
    rpcc *cl = h.safebind();
    if (cl) {
      int r;
      cl->call(extent_protocol::rmcache, id, r);
    }
  }
 
  return extent_protocol::OK;
}
#else
int extent_server::create(uint32_t type, extent_protocol::extentid_t &id)
{
  // alloc a new inode and return inum
  id = im->alloc_inode(type);
  return extent_protocol::OK;
}

int extent_server::put(extent_protocol::extentid_t id, std::string buf, int &)
{
  id &= 0x7fffffff;
  
  const char * cbuf = buf.c_str();
  int size = buf.size();
  im->write_file(id, cbuf, size);
  
  return extent_protocol::OK;
}

int extent_server::get(extent_protocol::extentid_t id, std::string &buf)
{
  id &= 0x7fffffff;

  int size = 0;
  char *cbuf = NULL;

  im->read_file(id, &cbuf, &size);
  if (size == 0)
    buf = "";
  else {
    buf.assign(cbuf, size);
    free(cbuf);
  }

  return extent_protocol::OK;
}

int extent_server::getattr(extent_protocol::extentid_t id, extent_protocol::attr &a)
{
  id &= 0x7fffffff;
  
  extent_protocol::attr attr;
  memset(&attr, 0, sizeof(attr));
  im->getattr(id, attr);
  a = attr;

  return extent_protocol::OK;
}

int extent_server::remove(extent_protocol::extentid_t id, int &)
{
  id &= 0x7fffffff;
  im->remove_file(id);
 
  return extent_protocol::OK;
}
#endif
