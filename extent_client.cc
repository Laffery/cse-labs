// RPC stubs for clients to talk to extent_server

#include "extent_client.h"
#include <sstream>
#include <iostream>
#include <stdio.h>
#include <unistd.h>
#include <time.h>

int extent_client::last_port = 1;

extent_client::extent_client(std::string dst)
{
	sockaddr_in dstsock;
	make_sockaddr(dst.c_str(), &dstsock);
	cl = new rpcc(dstsock);
	if (cl->bind() != 0)
	{
		printf("extent_client: bind failed\n");
	}

#ifdef CACHE
	srand(time(NULL) ^ last_port);
	ec_port = ((rand() % 32000) | (0x1 << 10));
	const char *hname;
	// VERIFY(gethostname(hname, 100) == 0);
	hname = "127.0.0.1";
	ostringstream host;
	host << hname << ":" << ec_port;
	id = host.str();
	
	last_port = ec_port;
	rpcs *rlsrpc = new rpcs(ec_port);
	rlsrpc->reg(extent_protocol::update, this, &extent_client::update);
	rlsrpc->reg(extent_protocol::rmcache, this, &extent_client::remove_cache);
#endif
}

extent_protocol::status
extent_client::create(uint32_t type, extent_protocol::extentid_t &eid)
{
	extent_protocol::status ret = extent_protocol::OK;
	eid = 4;
#ifdef CACHE
	ret = cl->call(extent_protocol::create, id, type, eid);
#else
	ret = cl->call(extent_protocol::create, type, eid);
#endif
	// printf("create with type %u, id %llu\n", type, eid);
	return ret;
}

extent_protocol::status
extent_client::get(extent_protocol::extentid_t eid, std::string &buf)
{
	extent_protocol::status ret = extent_protocol::OK;
#ifdef CACHE
	// if (cache.count(eid)) {
		// buf = cache[eid].data;
		// printf("get cache %llu, %s\n", eid, buf.c_str());
	// }
	// else
		ret = cl->call(extent_protocol::get, id, eid, buf);
#else
	ret = cl->call(extent_protocol::get, eid, buf);
#endif
	// printf("get %llu, %s\n", eid, buf.c_str());
	// printf("get %llu\n", eid);
	return ret;
}

extent_protocol::status
extent_client::getattr(extent_protocol::extentid_t eid, extent_protocol::attr &attr)
{
	extent_protocol::status ret = extent_protocol::OK;
#ifdef CACHE
	if (cache.count(eid))
		attr = cache[eid].attr;
	else
		ret = cl->call(extent_protocol::getattr, id, eid, attr);
#else
	ret = cl->call(extent_protocol::getattr, eid, attr);
#endif
	// printf("getattr of %llu\n", eid);
	return ret;
}

extent_protocol::status
extent_client::put(extent_protocol::extentid_t eid, std::string buf)
{
	int r;
	extent_protocol::status ret = extent_protocol::OK;
#ifdef CACHE
	ret = cl->call(extent_protocol::put, id, eid, buf, r);
#else
	ret = cl->call(extent_protocol::put, eid, buf, r);
#endif
	// printf("put %llu, %s\n", eid, buf.c_str());
	// printf("put %llu\n", eid);
	return ret;
}

extent_protocol::status
extent_client::remove(extent_protocol::extentid_t eid)
{
	int r;
	extent_protocol::status ret = extent_protocol::OK;
#ifdef CACHE
	ret = cl->call(extent_protocol::remove, id, eid, r);
#else
	ret = cl->call(extent_protocol::remove, eid, r);
#endif
	// printf("remove %llu\n", eid);
	return ret;
}

extent_protocol::status 
extent_client::update(extent_protocol::extentid_t eid, string data, extent_protocol::attr attr, int &r)
{
	// printf("update %llu\n", eid);
	cache[eid].data = data;
	cache[eid].attr = attr;
	return extent_protocol::OK;
}

extent_protocol::status 
extent_client::remove_cache(extent_protocol::extentid_t eid, int &r)
{
	// printf("remove %llu\n", eid);
	cache.erase(eid);
	return extent_protocol::OK;
}
