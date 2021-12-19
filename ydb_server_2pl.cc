#include "ydb_server_2pl.h"
#include "extent_client.h"

//#define DEBUG 1

ydb_server_2pl::ydb_server_2pl(std::string extent_dst, std::string lock_dst) : ydb_server(extent_dst, lock_dst) 
{
	curr_id = 0;
	pthread_mutex_init(&trans_stat_mutex, NULL);

	for(uint32_t i = 2; i < 1024; ++i)
	{
		pthread_cond_t cond;
		pthread_cond_init(&cond, NULL);
		ino_cv[i] = cond;

		pthread_mutex_t mutex;
		pthread_mutex_init(&mutex, NULL);
		ino_mutex[i] = mutex;

		ino_owner[i] = -1;
		// change_id[i] = -1;
	}

#ifdef COMPLEX
	pthread_mutex_init(&debug_mutex, NULL);
#endif
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

void
ydb_server_2pl::rollback(ydb_protocol::transaction_id id)
{
	for (uint32_t i = 2; i < 1024; ++i)
	{
		if (change_id[i] == id)
		{
			ec->put(i, origin_value[i]);
		}

		if (ino_owner[i] == id)
		{
			release(id, i);
		}
	}
	trans_stat[id] = ydb_protocol::ABORTED;
	printf("transaction %d rollback\n", id);
}

void
ydb_server_2pl::acquire(ydb_protocol::transaction_id id, uint32_t ino)
{
	pthread_mutex_lock(&ino_mutex[ino]);

	if (ino_owner[ino] == id)
	{
		pthread_mutex_unlock(&ino_mutex[ino]);
		return;
	}
#ifdef COMPLEX
	printf("transaction %d acquiring %u\n", id, ino);
#endif
	
	while(ino_owner[ino] != -1)
	{
		pthread_cond_wait(&ino_cv[ino], &ino_mutex[ino]);
	}

	ino_owner[ino] = id;
	trans_own[id].push_back(ino);
#ifdef COMPLEX
	printf("transaction %d lock %u\n", id, ino);
#endif
	pthread_mutex_unlock(&ino_mutex[ino]);
}

void
ydb_server_2pl::release(ydb_protocol::transaction_id id, uint32_t ino)
{
	pthread_mutex_lock(&ino_mutex[ino]);

	if (ino_owner[ino] != id)
	{
		pthread_mutex_unlock(&ino_mutex[ino]);
		return;
	}

	ino_owner[ino] = -1;
#ifdef COMPLEX
	printf("transaction %d free %u\n", id, ino);
#endif
	pthread_cond_signal(&ino_cv[ino]);
	pthread_mutex_unlock(&ino_mutex[ino]);
}

ydb_protocol::status 
ydb_server_2pl::transaction_begin(int, ydb_protocol::transaction_id &out_id) 
{ 
	// the first arg is not used, it is just a hack to the rpc lib
	// lab3: your code here
	pthread_mutex_lock(&trans_stat_mutex);
	printf("transaction %d begin\n", curr_id);
	out_id = (curr_id++);
	trans_stat[out_id] = ydb_protocol::BEGIN;

	pthread_mutex_t mutex;
	pthread_mutex_init(&mutex, NULL);
	trans_mutex[out_id] = mutex;
	pthread_mutex_lock(&trans_mutex[out_id]);

	pthread_mutex_unlock(&trans_stat_mutex);
	return ydb_protocol::OK;
}

ydb_protocol::status 
ydb_server_2pl::transaction_commit(ydb_protocol::transaction_id id, int &) 
{
	// lab3: your code here
	printf("transaction %d commit\n", id);
	pthread_mutex_lock(&trans_stat_mutex);
	trans_stat[id] = ydb_protocol::COMMIT;

	for (size_t i = 0; i < trans_own[id].size(); ++i)
		release(id, trans_own[id].at(i));

	pthread_mutex_unlock(&trans_mutex[id]);
	pthread_mutex_unlock(&trans_stat_mutex);
	return ydb_protocol::OK;
}

ydb_protocol::status 
ydb_server_2pl::transaction_abort(ydb_protocol::transaction_id id, int &) 
{
	// lab3: your code here
	printf("transaction %d abort\n", id);
	pthread_mutex_lock(&trans_stat_mutex);
	rollback(id);
	pthread_mutex_unlock(&trans_mutex[id]);
	pthread_mutex_unlock(&trans_stat_mutex);
	return ydb_protocol::OK;
}

ydb_protocol::status 
ydb_server_2pl::get(ydb_protocol::transaction_id id, const std::string key, std::string &out_value) 
{
	// lab3: your code here
	// printf("get %d, %s, %s ...\n", id, key.c_str(), out_value.c_str());

	if (id == -1)
		return ydb_protocol::TRANSIDINV;

	uint32_t ino = hash(key);

	acquire(id, ino);
	ec->get(ino, out_value);

#ifdef COMPLEX
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
ydb_server_2pl::set(ydb_protocol::transaction_id id, const std::string key, const std::string value, int &) 
{
	// lab3: your code here
	// printf("set %d, %d, %s, %s ...\n", id, hash(key), key.c_str(), value.c_str());

	if (id == -1)
		return ydb_protocol::TRANSIDINV;

	uint32_t ino = hash(key);
	
	pthread_mutex_lock(&trans_stat_mutex);

	ydb_protocol::transaction_id tid = change_id.count(ino) ? change_id[ino] : -1;

	if (tid == -1)
		goto common;
	
	if (tid != id)
	{
		if (trans_stat[tid] == ydb_protocol::BEGIN)
		{
			if (detectDL(id, tid))
			{
				printf("transaction %d deadlock\n", id);
				depend.erase(id);
				rollback(id);			
				pthread_mutex_unlock(&trans_mutex[id]);
				pthread_mutex_unlock(&trans_stat_mutex);
				return ydb_protocol::ABORT;
			}

			else
			{
				depend[id].push_back(tid);
				printf("transaction %d wait %d for transaction %d(%d)\n", id, ino, tid, trans_stat[tid]);
				pthread_mutex_unlock(&trans_stat_mutex);
				pthread_mutex_lock(&trans_mutex[tid]);
				printf("transaction %d awake by transaction %d(%d)\n", id, tid, trans_stat[tid]);
				pthread_mutex_lock(&trans_stat_mutex);
				pthread_mutex_unlock(&trans_mutex[tid]);
			}
		}

common:
		acquire(id, ino);
		string buf;
		ec->get(ino, buf);
		origin_value[ino] = buf;
		change_id[ino] = id;
	}
	pthread_mutex_unlock(&trans_stat_mutex);

	ec->put(ino, value);

#ifdef COMPLEX
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
ydb_server_2pl::del(ydb_protocol::transaction_id id, const std::string key, int &) 
{
	// lab3: your code here
	if (id == -1)
		return ydb_protocol::TRANSIDINV;

	return ydb_protocol::OK;
}

