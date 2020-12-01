#include "ydb_server_occ.h"
#include "extent_client.h"

//#define DEBUG 1

ydb_server_occ::ydb_server_occ(std::string extent_dst, std::string lock_dst) : ydb_server(extent_dst, lock_dst) 
{
	curr_id = 0;
	pthread_mutex_init(&trans_mutex, NULL);

	for (uint32_t i = 2; i < 1024; ++i)
	{
		ino_version[i] = 0;
	}

#ifdef COMPLEX3
	pthread_mutex_init(&debug_mutex, NULL);
#endif
}

ydb_server_occ::~ydb_server_occ() {
}

ydb_protocol::status 
ydb_server_occ::transaction_begin(int, ydb_protocol::transaction_id &out_id) 
{ 
	// the first arg is not used, it is just a hack to the rpc lib
	// lab3: your code here
	pthread_mutex_lock(&trans_mutex);
	printf("transaction %d begin\n", curr_id);
	out_id = (curr_id++);
	trans_stat[out_id] = ydb_protocol::BEGIN;
	pthread_mutex_unlock(&trans_mutex);
	return ydb_protocol::OK;
}

ydb_protocol::status 
ydb_server_occ::transaction_commit(ydb_protocol::transaction_id id, int &) 
{
	// lab3: your code here
	
	// valid
	pthread_mutex_lock(&trans_mutex);
	map<uint32_t, uint32_t>::iterator i = trans_read[id].begin();
	for (; i != trans_read[id].end(); ++i)
	{
		uint32_t ino = i->first;
		uint32_t version = i->second;
		if (trans_write[id].count(ino) && ino_version[ino] > version)
		{
			printf("transaction %d conflict\n", id);
			trans_stat[id] = ydb_protocol::ABORTED;
			pthread_mutex_unlock(&trans_mutex);
			return ydb_protocol::ABORT;
		}
	}

	map<uint32_t, string>::iterator j = trans_write[id].begin();
	for (; j != trans_write[id].end(); ++j)
	{
		ec->put(j->first, j->second);
		ino_version[j->first]++;
	}

	printf("transaction %d commit\n", id);
	trans_stat[id] = ydb_protocol::COMMIT;
	pthread_mutex_unlock(&trans_mutex);
	return ydb_protocol::OK;
}

ydb_protocol::status 
ydb_server_occ::transaction_abort(ydb_protocol::transaction_id id, int &) 
{
	// lab3: your code here
	printf("transaction %d abort\n", id);
	pthread_mutex_lock(&trans_mutex);
	trans_stat[id] = ydb_protocol::ABORTED;
	pthread_mutex_unlock(&trans_mutex);
	return ydb_protocol::OK;
}

ydb_protocol::status 
ydb_server_occ::get(ydb_protocol::transaction_id id, const std::string key, std::string &out_value) 
{
	// lab3: your code here
	if (id == -1)
		return ydb_protocol::TRANSIDINV;

	// printf("get %d, %s ...\n", id, key.c_str());

	uint32_t ino = hash(key);

	if (trans_write[id].count(ino))
		out_value = trans_write[id][ino];
	
	else {
		pthread_mutex_lock(&trans_mutex);
		ec->get(ino, out_value);
		trans_read[id][ino] = ino_version[ino];
		pthread_mutex_unlock(&trans_mutex);
	}

#ifdef COMPLEX3
	pthread_mutex_lock(&debug_mutex);
	printf("get %d, %s with ", id, key.c_str());
	if (key.at(0) == 'p')
	{
		product p;
		memcpy(&p, out_value.c_str(), sizeof(product));
		printf("(%d, %d)\n", p.count, p.price);
	}
	else
	{
		user u;
		memcpy(&u, out_value.c_str(), sizeof(user));
		for (int i = 0; i < 5; ++i)
			printf("[%d:%d] ", i, u.buyed_product_count_list[i]);
		printf("---%d\n", u.money);
	}
	pthread_mutex_unlock(&debug_mutex);
#else
	printf("get %d, %s, %s\n", id, key.c_str(), out_value.c_str());
#endif

	return ydb_protocol::OK;
}

ydb_protocol::status 
ydb_server_occ::set(ydb_protocol::transaction_id id, const std::string key, const std::string value, int &) 
{
	// lab3: your code here
	if (id == -1)
		return ydb_protocol::TRANSIDINV;

	// printf("set %d, %d, %s, %s ...\n", id, hash(key), key.c_str(), value.c_str());

	uint32_t ino = hash(key);
	trans_write[id][ino] = value;

#ifdef COMPLEX3
	pthread_mutex_lock(&debug_mutex);
	printf("set %d, %s with ", id, key.c_str());
	if (key.at(0) == 'p')
	{
		product p;
		memcpy(&p, value.c_str(), sizeof(product));
		printf("(%d, %d)\n", p.count, p.price);
	}
	else
	{
		user u;
		memcpy(&u, value.c_str(), sizeof(user));
		for (int i = 0; i < 5; ++i)
			printf("[%d:%d] ", i, u.buyed_product_count_list[i]);
		printf("---%d\n", u.money);
	}
	pthread_mutex_unlock(&debug_mutex);
#else
	printf("set %d, %s, %s\n", id, key.c_str(), value.c_str());
#endif

	return ydb_protocol::OK;
}

ydb_protocol::status 
ydb_server_occ::del(ydb_protocol::transaction_id id, const std::string key, int &) 
{
	// lab3: your code here
	if (id == -1)
		return ydb_protocol::TRANSIDINV;

	return ydb_protocol::OK;
}

