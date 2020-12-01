#ifndef ydb_server_occ_h
#define ydb_server_occ_h

#include <string>
#include <map>
#include <vector>
#include "extent_client.h"
#include "lock_protocol.h"
#include "lock_client.h"
#include "lock_client_cache.h"
#include "ydb_protocol.h"
#include "ydb_server.h"

// #define COMPLEX3
#ifdef COMPLEX3
struct user {    // in database
	int money;
	short buyed_product_count_list[5];
};

struct product {    // in database
	int price;
	int count;
};
#endif

/*
 * membership:
 * 	curr_id - current transaction id
 *  trans_stat - transaction status table
 *  trans_read/write - transaction read/write set
 *  ino_version - record inode version code
 * 	trans_mutex - transaction mutex
 */
class ydb_server_occ: public ydb_server 
{
private:
	ydb_protocol::transaction_id curr_id;
	map<ydb_protocol::transaction_id, ydb_protocol::xxstat> trans_stat;
	map<ydb_protocol::transaction_id, map<uint32_t, uint32_t> > trans_read;
	map<ydb_protocol::transaction_id, map<uint32_t, string> > trans_write;
	map<uint32_t, uint32_t> ino_version;
	pthread_mutex_t trans_mutex;
#ifdef COMPLEX3
	pthread_mutex_t debug_mutex;
#endif

public:
	ydb_server_occ(std::string, std::string);
	~ydb_server_occ();
	ydb_protocol::status transaction_begin(int, ydb_protocol::transaction_id &);
	ydb_protocol::status transaction_commit(ydb_protocol::transaction_id, int &);
	ydb_protocol::status transaction_abort(ydb_protocol::transaction_id, int &);
	ydb_protocol::status get(ydb_protocol::transaction_id, const std::string, std::string &);
	ydb_protocol::status set(ydb_protocol::transaction_id, const std::string, const std::string, int &);
	ydb_protocol::status del(ydb_protocol::transaction_id, const std::string, int &);
};

#endif
