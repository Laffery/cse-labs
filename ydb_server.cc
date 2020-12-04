#include "ydb_server.h"
#include "extent_client.h"

//#define DEBUG 1

static long timestamp(void) {
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return (tv.tv_sec*1000 + tv.tv_usec/1000);
}

ydb_server::ydb_server(std::string extent_dst, std::string lock_dst) {
	ec = new extent_client(extent_dst);
	// lc = new lock_client(lock_dst);
	lc = new lock_client_cache(lock_dst);

	long starttime = timestamp();
	
	for(int i = 2; i < 1024; i++) {    // for simplicity, just pre alloc all the needed inodes
		extent_protocol::extentid_t id;
		ec->create(extent_protocol::T_FILE, id);
	}
	
	long endtime = timestamp();
	printf("time %ld ms\n", endtime-starttime);
}

ydb_server::~ydb_server() {
	delete lc;
	delete ec;
}

uint32_t
ydb_server::hash(string str)
{
	uint32_t res = 0;
	for (size_t i = 0; i < str.length(); ++i)
	{
		res = (res * 131) + (str.at(i) - '0');
	}

	return 2 + (res % 1022);
}


ydb_protocol::status 
ydb_server::transaction_begin(int, ydb_protocol::transaction_id &out_id) 
{    
	// the first arg is not used, it is just a hack to the rpc lib
	// no imply, just return OK
	printf("transaction begin\n");
	return ydb_protocol::OK;
}

ydb_protocol::status 
ydb_server::transaction_commit(ydb_protocol::transaction_id id, int &) 
{
	// no imply, just return OK
	printf("transaction commit\n");
	return ydb_protocol::OK;
}

ydb_protocol::status 
ydb_server::transaction_abort(ydb_protocol::transaction_id id, int &) 
{
	// no imply, just return OK
	printf("transaction abort\n");
	return ydb_protocol::OK;
}

ydb_protocol::status 
ydb_server::get(ydb_protocol::transaction_id id, const std::string key, std::string &out_value) 
{
	// lab3: your code here
	ec->get(hash(key), out_value);
	return ydb_protocol::OK;
}

ydb_protocol::status 
ydb_server::set(ydb_protocol::transaction_id id, const std::string key, const std::string value, int &) 
{
	// lab3: your code here
	ec->put(hash(key), value);
	return ydb_protocol::OK;
}

ydb_protocol::status 
ydb_server::del(ydb_protocol::transaction_id id, const std::string key, int &) 
{
	// lab3: your code here
	return ydb_protocol::OK;
}

