#ifndef ydb_server_h
#define ydb_server_h

#include <string>
#include <map>
#include <vector>
#include <set>
#include <bitset>

#include "extent_client.h"
#include "lock_protocol.h"
#include "lock_client.h"
#include "lock_client_cache.h"
#include "ydb_protocol.h"

using namespace std;

class ydb_server {
protected:
	extent_client *ec;
	lock_client *lc;
	bitset<1024> using_inodes;
	map<string, uint32_t> key_ino;
public:
	ydb_server(string, string);
	virtual ~ydb_server();
	uint32_t hash(string);
	virtual ydb_protocol::status transaction_begin(int, ydb_protocol::transaction_id &);
	virtual ydb_protocol::status transaction_commit(ydb_protocol::transaction_id, int &);
	virtual ydb_protocol::status transaction_abort(ydb_protocol::transaction_id, int &);
	virtual ydb_protocol::status get(ydb_protocol::transaction_id, const string, string &);
	virtual ydb_protocol::status set(ydb_protocol::transaction_id, const string, const string, int &);
	virtual ydb_protocol::status del(ydb_protocol::transaction_id, const string, int &);
};

#endif

