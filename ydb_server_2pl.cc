#include "ydb_server_2pl.h"
#include "extent_client.h"

//#define DEBUG 1

ydb_server_2pl::ydb_server_2pl(std::string extent_dst, std::string lock_dst) : ydb_server(extent_dst, lock_dst) 
{
	curr_id = 0;
	pthread_mutex_init(&trans_id_mutex, NULL);
	pthread_mutex_init(&trans_stat_mutex, NULL);
}

ydb_server_2pl::~ydb_server_2pl() {
}

bool
ydb_server_2pl::detectDL(ydb_protocol::transaction_id targ, ydb_protocol::transaction_id curr)
{
	if (!depend[curr].empty())
	{
		for (size_t i = 0; i < depend[curr].size(); ++i)
		{
			ydb_protocol::transaction_id next = depend[curr].at(i);
			if (trans_stat[next] != ydb_protocol::BEGIN)
				continue;
			if (next == targ)
				return true;
			if (detectDL(targ, next))
				return true;
		}
	}

	return false;
}

ydb_protocol::status 
ydb_server_2pl::transaction_begin(int, ydb_protocol::transaction_id &out_id) 
{ 
	// the first arg is not used, it is just a hack to the rpc lib
	// lab3: your code here
	pthread_mutex_lock(&trans_id_mutex);
	printf("transaction %d begin\n", curr_id);
	out_id = (curr_id++);
	trans_stat[out_id] = ydb_protocol::BEGIN;
	pthread_mutex_unlock(&trans_id_mutex);
	return ydb_protocol::OK;
}

ydb_protocol::status 
ydb_server_2pl::transaction_commit(ydb_protocol::transaction_id id, int &) 
{
	// lab3: your code here
	printf("transaction %d commit\n", id);
	pthread_mutex_lock(&trans_stat_mutex);
	trans_stat[id] = ydb_protocol::COMMIT;
	pthread_mutex_unlock(&trans_stat_mutex);
	return ydb_protocol::OK;
}

ydb_protocol::status 
ydb_server_2pl::transaction_abort(ydb_protocol::transaction_id id, int &) 
{
	// lab3: your code here
	printf("transaction %d abort\n", id);

	// rollback
	for (uint32_t i = 2; i < 1024; ++i)
	{
		if (change_id[i] == id)
		{
			ec->put(i, origin_value[i]);
		}
	}

	pthread_mutex_lock(&trans_stat_mutex);
	trans_stat[id] = ydb_protocol::ABORTED;
	pthread_mutex_unlock(&trans_stat_mutex);
	return ydb_protocol::OK;
}

ydb_protocol::status 
ydb_server_2pl::get(ydb_protocol::transaction_id id, const std::string key, std::string &out_value) 
{
	// lab3: your code here
	printf("get %d, %s, %s ...\n", id, key.c_str(), out_value.c_str());

	if (id == -1)
		return ydb_protocol::TRANSIDINV;

	uint32_t ino = hash(key);
	// lc->acquire(ino);
	ec->get(ino, out_value);
	printf("get %d, %s, %s\n", id, key.c_str(), out_value.c_str());
	// lc->release(ino);
	return ydb_protocol::OK;
}

ydb_protocol::status 
ydb_server_2pl::set(ydb_protocol::transaction_id id, const std::string key, const std::string value, int &) 
{
	// lab3: your code here
	printf("set %d, %s, %s ...\n", id, key.c_str(), value.c_str());

	if (id == -1)
		return ydb_protocol::TRANSIDINV;

	uint32_t ino = hash(key);
	// lc->acquire(ino);
	
	// ensure only set origin_value once, or it's not the origin value
	pthread_mutex_lock(&trans_stat_mutex);
	ydb_protocol::transaction_id tid = change_id[ino];
	if (tid != id)
	{
		printf("transaction %d check transaction %d with status %d\n", id, tid, trans_stat[tid]);
		if (trans_stat[tid] == ydb_protocol::BEGIN)
		{
			if (detectDL(id, tid))
			{
				depend.erase(id);

				trans_stat[id] = ydb_protocol::ABORTED;
				pthread_mutex_unlock(&trans_stat_mutex);
				// lc->release(ino);
				return ydb_protocol::ABORT;
			}

			depend[id].push_back(tid);
		}

		string buf;
		ec->get(ino, buf);
		origin_value[ino] = buf;
		change_id[ino] = id;
	}
	pthread_mutex_unlock(&trans_stat_mutex);

	ec->put(ino, value);
	printf("set %d, %s, %s\n", id, key.c_str(), value.c_str());
	// lc->release(ino);
	return ydb_protocol::OK;
}

ydb_protocol::status 
ydb_server_2pl::del(ydb_protocol::transaction_id id, const std::string key, int &) 
{
	// lab3: your code here
	if (id == -1)
		return ydb_protocol::TRANSIDINV;

	return ydb_protocol::OK;
}

