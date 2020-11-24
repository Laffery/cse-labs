// the lock server implementation

#include "lock_server.h"
#include <sstream>
#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>

lock_server::lock_server() : nacquire(0)
{
	pthread_mutex_init(&mutex, NULL);
	pthread_cond_init(&cond, NULL);
}

lock_protocol::status
lock_server::stat(int clt, lock_protocol::lockid_t lid, int &r)
{
	lock_protocol::status ret = lock_protocol::OK;
	printf("stat request from clt %d\n", clt);
	r = nacquire;
	return ret;
}

lock_protocol::status
lock_server::acquire(int clt, lock_protocol::lockid_t lid, int &r)
{
	lock_protocol::status ret = lock_protocol::OK;
	// Your lab2 part2 code goes here

	pthread_mutex_lock(&mutex);

	if (locks_status.find(lid) != locks_status.end())
	{
		// if lock lid has been locked, wait until unlock, then lock it
		bool flag = false;
	
		while (locks_status[lid] == lock_protocol::LOCKED) {
			flag = true;
			pthread_cond_wait(&cond, &mutex);
		}

		if (flag)
			printf("lock_server: %d waiting for %llu\n", clt, lid);
		
		locks_status[lid] = lock_protocol::LOCKED;
	} 
	
	// if lock table don't have a lock lid, directly lock it
	else {
		locks_status[lid] = lock_protocol::LOCKED;
	}
	
	printf("lock_server: %d lock %llu\n", clt, lid);
	r = lock_protocol::OK;
	pthread_mutex_unlock(&mutex);
	
	return ret;
}

lock_protocol::status
lock_server::release(int clt, lock_protocol::lockid_t lid, int &r)
{
	lock_protocol::status ret = lock_protocol::OK;
	// Your lab2 part2 code goes here

	pthread_mutex_lock(&mutex);

	if (locks_status.find(lid) != locks_status.end())
	{
		locks_status[lid] = lock_protocol::UNLOCK;
		printf("lock_server: %d unlock %llu\n", clt, lid);
		r = lock_protocol::OK;
		pthread_cond_signal(&cond);
	} 
	
	// no lock lid
	else {
		r = lock_protocol::NOENT;
	}
	
	pthread_mutex_unlock(&mutex);

	return ret;
}