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

class ydb_server_2pl: public ydb_server 
{
private:
	ydb_protocol::transaction_id curr_id;
	map<uint32_t, string> origin_value;
	map<uint32_t, ydb_protocol::transaction_id> change_id;
	map<ydb_protocol::transaction_id, ydb_protocol::xxstat> trans_stat;
	map<ydb_protocol::transaction_id, vector<ydb_protocol::transaction_id> > depend;

	pthread_mutex_t trans_id_mutex;
	pthread_mutex_t trans_stat_mutex;
public:
	ydb_server_2pl(std::string, std::string);
	~ydb_server_2pl();
	bool detectDL(ydb_protocol::transaction_id, ydb_protocol::transaction_id);
	ydb_protocol::status transaction_begin(int, ydb_protocol::transaction_id &);
	ydb_protocol::status transaction_commit(ydb_protocol::transaction_id, int &);
	ydb_protocol::status transaction_abort(ydb_protocol::transaction_id, int &);
	ydb_protocol::status get(ydb_protocol::transaction_id, const std::string, std::string &);
	ydb_protocol::status set(ydb_protocol::transaction_id, const std::string, const std::string, int &);
	ydb_protocol::status del(ydb_protocol::transaction_id, const std::string, int &);
};

#endif
