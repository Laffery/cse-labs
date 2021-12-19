// the caching lock server implementation

#include "lock_server_cache.h"
#include <sstream>
#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "lang/verify.h"
#include "handle.h"
#include "tprintf.h"

lock_server_cache::lock_server_cache()
{
	pthread_mutex_init(&mutex, NULL);
}

lock_protocol::status
lock_server_cache::stat(lock_protocol::lockid_t lid, int &r)
{
	lock_protocol::status ret = lock_protocol::OK;
	tprintf("stat request\n");
	r = nacquire;
	return ret;
}

/* 
 * There are 3 return val.
 * OK: success, lid never locked or no client own it now
 * RETRY: clt must wait until no client before it
 * RPCERR: cannot safe bind
 */
int lock_server_cache::acquire(lock_protocol::lockid_t lid, string id, int &r)
{
	rlock_protocol::status ret = rlock_protocol::OK;
	// Your lab2 part3 code goes here

	pthread_mutex_lock(&mutex);

	// if lock lid don't have any client to own
	if (locks_lists.find(lid) == locks_lists.end())
	{
		queue<string> clque;
		clque.push(id);
		locks_lists[lid] = clque;
		tprintf("lock_server_cache: %s lock %llu at once\n", id.c_str(), lid);

		pthread_mutex_unlock(&mutex);
		return lock_protocol::OK;
	}

	// there is lock lid in list
	else
	{
		// need not to wait
		if (locks_lists[lid].empty())
		{
			locks_lists[lid].push(id);
			tprintf("lock_server_cache: %s lock %llu at once\n", id.c_str(), lid);
			pthread_mutex_unlock(&mutex);
			return lock_protocol::OK;
		}

		// need to wait
		else
		{
			string last = locks_lists[lid].back();
			locks_lists[lid].push(id);

			handle h(last);
			rpcc *cl = h.safebind();
			if (cl) // safebind success
			{
				int r;
				pthread_mutex_unlock(&mutex); // avoid deadlock
				ret = cl->call(rlock_protocol::revoke, lid, r);
				pthread_mutex_lock(&mutex);

				if (ret != rlock_protocol::OK)
				{
					pthread_mutex_unlock(&mutex);
					tprintf("lock_server_cache: %s acquire %llu -> RETRY\n", id.c_str(), lid);
					return rlock_protocol::RETRY;
				}

				else
				{
					pthread_mutex_unlock(&mutex);
					tprintf("lock_server_cache: %s acquire %llu -> OK\n", id.c_str(), lid);
					return rlock_protocol::OK;
				}
			} 
			
			else
			{
				pthread_mutex_unlock(&mutex);
				tprintf("lock_server_cache: %s acquire -> safebind failed\n", last.c_str());
				return rlock_protocol::RPCERR;	
			}
		}
	}
}

int lock_server_cache::release(lock_protocol::lockid_t lid, string id, int &r)
{
	lock_protocol::status ret = rlock_protocol::OK;
	// Your lab2 part3 code goes here
	pthread_mutex_lock(&mutex);

	// only if client id has lock lid can it call this method, thus just pop it
	locks_lists[lid].pop();
	tprintf("lock_server_cache: %s unlock %llu\n", id.c_str(), lid);

	// tell client in queue re-acquire for lock lid
	if (!locks_lists[lid].empty())
	{
		string next = locks_lists[lid].front();
		pthread_mutex_unlock(&mutex);

		handle h(next);
		rpcc *cl = h.safebind();

		if (cl)
		{
			int r;
			ret = cl->call(rlock_protocol::retry, lid, r);
			tprintf("lock_server_cache: %llu send RETRY to %s\n", lid, id.c_str());
			return ret; // OK or NOENT(nearly never)
		}

		else
		{
			tprintf("lock_server_cache: release %s -> safebind failed\n", next.c_str());
			return rlock_protocol::RPCERR;
		}
	}

	pthread_mutex_unlock(&mutex);
	return rlock_protocol::OK;
}