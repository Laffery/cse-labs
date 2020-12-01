#ifndef ydb_server_2pl_h
#define ydb_server_2pl_h

#include <string>
#include <map>
#include <vector>
#include "extent_client.h"
#include "lock_protocol.h"
#include "lock_client.h"
#include "lock_client_cache.h"
#include "ydb_protocol.h"
#include "ydb_server.h"

// #define COMPLEX
#ifdef COMPLEX
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
 *  origin_value - origin value of inode
 *  change_id - id who has changed this inode
 *  trans_stat - transaction status table
 *  depend - transaction dependency table, for detecting deadlock
 *  trans_mutex - mutex for each transaction for lifecycle
 *  ino_mutex/ino_cv - for inode lock acquire & release
 *  ino_owner - who own inode's lock
 *  trans_own - inodes owned by transaction
 *  trans_stat_mutex - mutex for trans_stat, keep consistance of transaction status 
 */
class ydb_server_2pl: public ydb_server 
{
private:
	ydb_protocol::transaction_id curr_id;
	map<uint32_t, string> origin_value;
	map<uint32_t, ydb_protocol::transaction_id> change_id;
	map<ydb_protocol::transaction_id, ydb_protocol::xxstat> trans_stat;
	map<ydb_protocol::transaction_id, vector<ydb_protocol::transaction_id> > depend;
	map<ydb_protocol::transaction_id, pthread_mutex_t> trans_mutex;
	map<uint32_t, pthread_mutex_t> ino_mutex;
	map<uint32_t, pthread_cond_t> ino_cv;
	map<uint32_t, ydb_protocol::transaction_id> ino_owner;
	map<ydb_protocol::transaction_id, vector<uint32_t> > trans_own;
	pthread_mutex_t trans_stat_mutex;
#ifdef COMPLEX
	pthread_mutex_t debug_mutex;
#endif

public:
	ydb_server_2pl(std::string, std::string);
	~ydb_server_2pl();
	void rollback(ydb_protocol::transaction_id);
	bool detectDL(ydb_protocol::transaction_id, ydb_protocol::transaction_id);
	void acquire(ydb_protocol::transaction_id, uint32_t);
	void release(ydb_protocol::transaction_id, uint32_t);
	ydb_protocol::status transaction_begin(int, ydb_protocol::transaction_id &);
	ydb_protocol::status transaction_commit(ydb_protocol::transaction_id, int &);
	ydb_protocol::status transaction_abort(ydb_protocol::transaction_id, int &);
	ydb_protocol::status get(ydb_protocol::transaction_id, const std::string, std::string &);
	ydb_protocol::status set(ydb_protocol::transaction_id, const std::string, const std::string, int &);
	ydb_protocol::status del(ydb_protocol::transaction_id, const std::string, int &);
};

#endif
