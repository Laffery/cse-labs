// RPC stubs for clients to talk to lock_server, and cache the locks
// see lock_client.cache.h for protocol details.

#include "lock_client_cache.h"
#include "rpc.h"
#include <sstream>
#include <iostream>
#include <time.h>
#include <stdio.h>
#include "tprintf.h"

int lock_client_cache::last_port = 0;

lock_client_cache::lock_client_cache(string xdst, class lock_release_user *_lu)
	: lock_client(xdst), lu(_lu)
{
	srand(time(NULL) ^ last_port);
	rlock_port = ((rand() % 32000) | (0x1 << 10));
	const char *hname;
	// VERIFY(gethostname(hname, 100) == 0);
	hname = "127.0.0.1";
	ostringstream host;
	host << hname << ":" << rlock_port;
	id = host.str();
	
	last_port = rlock_port;
	rpcs *rlsrpc = new rpcs(rlock_port);
	rlsrpc->reg(rlock_protocol::revoke, this, &lock_client_cache::revoke_handler);
	rlsrpc->reg(rlock_protocol::retry, this, &lock_client_cache::retry_handler);

	pthread_mutex_init(&mutex, NULL);
}

lock_protocol::status
lock_client_cache::acquire(lock_protocol::lockid_t lid)
{
	int r;
	int ret = lock_protocol::OK;
	// Your lab2 part3 code goes here

	pthread_mutex_lock(&mutex);
	tprintf("lock_client_cache: %s acquiring %llu ...\n", id.c_str(), lid);

	// client never have lock lid
	if (locks_clstatus.find(lid) == locks_clstatus.end())
	{
		pthread_cond_t cond;
		pthread_cond_init(&cond, NULL);

		locks_cond[lid] = cond;
		locks_clstatus[lid] = rlock_protocol::NONE; // unfound == NONE
		locks_revoke[lid] = false;
	}

	// client does not have ownership of lock lid
	if (locks_clstatus[lid] == rlock_protocol::NONE)
	{
acquiring:
		locks_clstatus[lid] = rlock_protocol::ACQUIRING;
		
		// acquire
		pthread_mutex_unlock(&mutex); // avoid deadlock
		tprintf("lock_client_cache: %s send acquire %llu to server\n", id.c_str(), lid);
		ret = cl->call(lock_protocol::acquire, lid, id, r);
		pthread_mutex_lock(&mutex);

		switch (ret)
		{
		case rlock_protocol::RETRY:		// waited, now it's this client turn
			tprintf("lock_client_cache: %s acquire %llu -> RETRY\n", id.c_str(), lid);
			while (locks_clstatus[lid] != rlock_protocol::FREE 
					&& locks_clstatus[lid] != rlock_protocol::LOCKED)
			{
				/* 
				 * there is situation that before this thread's aquiring success, 
				 * unluckily this client lose the ownership of lock lid, thus we
				 * need to re-acquire to the server
				 */ 
				// client does not own lock lid, re-acquire
				if (locks_clstatus[lid] == rlock_protocol::NONE)
				{
					goto acquiring;
				}

				// wait for other thread release 
				pthread_cond_wait(&(locks_cond[lid]), &mutex);
			}
		case rlock_protocol::OK:		// acquire sucess
			// locks_clstatus[lid] = rlock_protocol::FREE;
			tprintf("lock_client_cache: %s acquire %llu -> LOCKED\n", id.c_str(), lid);
			locks_clstatus[lid] = rlock_protocol::LOCKED;
			pthread_mutex_unlock(&mutex);
			return rlock_protocol::OK;
		case rlock_protocol::RPCERR:	// safe bind failed
		default:
			pthread_mutex_unlock(&mutex);
			return rlock_protocol::RPCERR;
		}
	}

	// own lock lid, ACQURING and RELEASING won't be here, so FREE or LOCKED
	else
	{
		// wait lid FREE
		while (locks_clstatus[lid] != rlock_protocol::FREE)
		{
			if (locks_clstatus[lid] == rlock_protocol::NONE)
			{
				goto acquiring;
			}
			tprintf("lock_client_cache: %s acquire %llu -> wait free\n", id.c_str(), lid);
			pthread_cond_wait(&(locks_cond[lid]), &mutex);
		}

		locks_clstatus[lid] = rlock_protocol::LOCKED;
		pthread_mutex_unlock(&mutex);
		tprintf("lock_client_cache: %s acquire %llu -> LOCKED at once\n", id.c_str(), lid);
		return rlock_protocol::OK;
	}
}

/*
 * release
 * There is a thread who own lock lid sending release to client.
 * Without REVOKE: FREE at once
 * With	   REVOKE: send release to server and this client will lose lock lid.
 */
lock_protocol::status
lock_client_cache::release(lock_protocol::lockid_t lid)
{
	int r;
	int ret = lock_protocol::OK;
	// Your lab2 part3 code goes here
	pthread_mutex_lock(&mutex);
	tprintf("lock_client_cache: %s releasing %llu ...\n", id.c_str(), lid);

	if (locks_clstatus.find(lid) == locks_clstatus.end() 
		|| locks_clstatus[lid] != rlock_protocol::LOCKED)
	{
		pthread_mutex_unlock(&mutex);
		tprintf("lock_client_cache: %s release %llu -> NOENT\n", id.c_str(), lid);
		return rlock_protocol::NOENT;
	}

	// there is REVOKE arrive this client
	if (locks_revoke[lid])
	{
		pthread_mutex_unlock(&mutex);
		ret = cl->call(lock_protocol::release, lid, id, r);
		pthread_mutex_lock(&mutex);

		if (ret == rlock_protocol::OK) {
			locks_clstatus[lid] = rlock_protocol::NONE;
			locks_revoke[lid] = false;
			/*
			 * must wake cond up otherwise there is thread thd waiting for lock, but the 
			 * client lose lock lid, thd will never own lock, which will cause RPC_LOSSY
			 */
			pthread_cond_signal(&(locks_cond[lid])); 
		}
		
		pthread_mutex_unlock(&mutex);
		tprintf("lock_client_cache: %s release %llu -> REVOKE\n", id.c_str(), lid);
		return ret; // OK | RPCERR | NOENT
	}

	// no REVOKE, just turns to FREE
	else
	{
		locks_clstatus[lid] = rlock_protocol::FREE;
		pthread_cond_signal(&(locks_cond[lid]));
		pthread_mutex_unlock(&mutex);
		tprintf("lock_client_cache: %s release %llu -> FREE\n", id.c_str(), lid);
		return ret;
	}
}

/*
 * rlock_protocol::revoke
 * In lock_server_cache.cc, revoke will be called only if lock lid is locked 
 * and the queue has some client, the server call this method to tell client 
 * clt which before the new acquiring client, then clt will receive REVOKE:  
 * locks_revoke[lid] turns into true. Actually, while client releases lock 
 * lid and locks_revoke[lid] is true, till lock lid is FREE, clt will send 
 * release request to server at once, then clt lose ownership of lock lid.
 */
rlock_protocol::status
lock_client_cache::revoke_handler(lock_protocol::lockid_t lid, int &)
{
	int ret = rlock_protocol::OK;
	// Your lab2 part3 code goes here

	pthread_mutex_lock(&mutex);
	tprintf("lock_client_cache: %s revoking %llu ...\n", id.c_str(), lid);
	// locks_revoke[lid] = true; // cl->release might cause data dependency

	if (locks_clstatus[lid] == rlock_protocol::FREE)
	{
		int r;
		pthread_mutex_unlock(&mutex);
		ret = cl->call(lock_protocol::release, lid, id, r);
		
		pthread_mutex_lock(&mutex);
		if (ret == rlock_protocol::OK) {
			locks_clstatus[lid] = rlock_protocol::NONE;
			// locks_revoke[lid] = false; // unnecessary
			pthread_cond_signal(&(locks_cond[lid]));
			pthread_mutex_unlock(&mutex);
			tprintf("lock_client_cache: %s revoke %llu -> RELEASE\n", id.c_str(), lid);
			return rlock_protocol::OK;
		}
		
		pthread_mutex_unlock(&mutex);
		tprintf("lock_client_cache: %s revoke %llu -> ERR\n", id.c_str(), lid);
		return ret;
	}

	locks_revoke[lid] = true;
	pthread_mutex_unlock(&mutex);
	tprintf("lock_client_cache: %s revoke %llu -> REVOKE\n", id.c_str(), lid);
	return rlock_protocol::REVOKE;
}

/*
 * rlock_protocol::retry
 * In lock_server_cache.cc, retry will be called only if lock lid is released 
 * and the queue has some client waiting, the server call this method to tell 
 * the front client clt to re-acquire. Actually, clt kept acquiring thus just 
 * to turn it into LOCKED status.
 */
rlock_protocol::status
lock_client_cache::retry_handler(lock_protocol::lockid_t lid, int &)
{
	pthread_mutex_lock(&mutex);
	tprintf("lock_client_cache: %s retrying %llu ...\n", id.c_str(), lid);
	switch(locks_clstatus[lid])
	{
		case rlock_protocol::FREE: 
		case rlock_protocol::LOCKED: 
		case rlock_protocol::RELEASING:
			pthread_mutex_unlock(&mutex);
			tprintf("lock_client_cache: %s retry -> wrong client_status\n", id.c_str());
			return rlock_protocol::NOENT;
		case rlock_protocol::ACQUIRING:
			locks_clstatus[lid] = rlock_protocol::LOCKED;
			tprintf("lock_client_cache: %s retry -> %llu\n", id.c_str(), lid);
			pthread_cond_signal(&(locks_cond[lid]));
		case rlock_protocol::NONE:
		default:
			pthread_mutex_unlock(&mutex);
			return rlock_protocol::OK;
	}
}

/* 
 * debug helper version method: with thread id
 */

// lock_protocol::status
// lock_client_cache::ACQUIRE(lock_protocol::lockid_t lid, int thd)
// {
// 	int r;
// 	int ret = lock_protocol::OK;
// 	// Your lab2 part3 code goes here

// 	pthread_mutex_lock(&mutex);
// 	tprintf("lock_client_cache: %s--%d acquiring %llu ...\n", id.c_str(), thd, lid);

// 	// client never have lock lid
// 	if (locks_clstatus.find(lid) == locks_clstatus.end())
// 	{
// 		pthread_cond_t cond;
// 		pthread_cond_init(&cond, NULL);

// 		locks_cond[lid] = cond;
// 		locks_clstatus[lid] = rlock_protocol::NONE; // unfound == NONE
// 		locks_revoke[lid] = false;
// 	}

// 	// client does not have ownership of lock lid
// 	if (locks_clstatus[lid] == rlock_protocol::NONE)
// 	{
// acquiring:
// 		locks_clstatus[lid] = rlock_protocol::ACQUIRING;
		
// 		// acquire
// 		pthread_mutex_unlock(&mutex); // avoid deadlock
// 		tprintf("lock_client_cache: %s--%d send acquire %llu to server\n", id.c_str(), thd, lid);
// 		ret = cl->call(lock_protocol::acquire, lid, id, r);
// 		pthread_mutex_lock(&mutex);

// 		switch (ret)
// 		{
// 		case rlock_protocol::RETRY:		// waited, now it's this client turn
// 			tprintf("lock_client_cache: %s--%d acquire %llu -> RETRY\n", id.c_str(), thd, lid);
// 			while (locks_clstatus[lid] != rlock_protocol::FREE 
// 					&& locks_clstatus[lid] != rlock_protocol::LOCKED)
// 			{
// 				/* 
// 				 * there is situation that before this thread's aquiring success, 
// 				 * unluckily this client lose the ownership of lock lid, thus we
// 				 * need to re-acquire to the server
// 				 */ 
// 				// client does not own lock lid, re-acquire
// 				if (locks_clstatus[lid] == rlock_protocol::NONE)
// 				{
// 					goto acquiring;
// 				}

// 				// wait for other thread release 
// 				pthread_cond_wait(&(locks_cond[lid]), &mutex);
// 			}
// 		case rlock_protocol::OK:		// acquire sucess
// 			// locks_clstatus[lid] = rlock_protocol::FREE;
// 			tprintf("lock_client_cache: %s--%d acquire %llu -> LOCKED\n", id.c_str(), thd, lid);
// 			locks_clstatus[lid] = rlock_protocol::LOCKED;
// 			pthread_mutex_unlock(&mutex);
// 			return rlock_protocol::OK;
// 		case rlock_protocol::RPCERR:	// safe bind failed
// 		default:
// 			pthread_mutex_unlock(&mutex);
// 			return rlock_protocol::RPCERR;
// 		}
// 	}

// 	// own lock lid, ACQURING and RELEASING won't be here, so FREE or LOCKED
// 	else
// 	{
// 		// wait lid FREE
// 		while (locks_clstatus[lid] != rlock_protocol::FREE)
// 		{
// 			if (locks_clstatus[lid] == rlock_protocol::NONE)
// 			{
// 				goto acquiring;
// 			}
// 			tprintf("lock_client_cache: %s--%d acquire %llu -> wait free\n", id.c_str(), thd, lid);
// 			pthread_cond_wait(&(locks_cond[lid]), &mutex);
// 		}

// 		locks_clstatus[lid] = rlock_protocol::LOCKED;
// 		pthread_mutex_unlock(&mutex);
// 		tprintf("lock_client_cache: %s--%d acquire %llu -> LOCKED at once\n", id.c_str(), thd, lid);
// 		return rlock_protocol::OK;
// 	}
// }

// lock_protocol::status
// lock_client_cache::RELEASE(lock_protocol::lockid_t lid, int thd)
// {
// 	int r;
// 	int ret = lock_protocol::OK;
// 	// Your lab2 part3 code goes here
// 	pthread_mutex_lock(&mutex);
// 	tprintf("lock_client_cache: %s--%d releasing %llu ...\n", id.c_str(), thd, lid);

// 	if (locks_clstatus.find(lid) == locks_clstatus.end() 
// 		|| locks_clstatus[lid] != rlock_protocol::LOCKED)
// 	{
// 		pthread_mutex_unlock(&mutex);
// 		tprintf("lock_client_cache: %s--%d release %llu -> NOENT\n", id.c_str(), thd, lid);
// 		return rlock_protocol::NOENT;
// 	}

// 	// there is REVOKE arrive this client
// 	if (locks_revoke[lid])
// 	{
// 		pthread_mutex_unlock(&mutex);
// 		ret = cl->call(lock_protocol::release, lid, id, r);
// 		pthread_mutex_lock(&mutex);

// 		if (ret == rlock_protocol::OK) {
// 			locks_clstatus[lid] = rlock_protocol::NONE;
// 			locks_revoke[lid] = false;
// 			/*
// 			 * must wake cond up otherwise there is thread thd waiting for lock, but the 
// 			 * client lose lock lid, thd will never own lock, which will cause RPC_LOSSY
// 			 */
// 			pthread_cond_signal(&(locks_cond[lid])); 
// 		}
		
// 		pthread_mutex_unlock(&mutex);
// 		tprintf("lock_client_cache: %s--%d release %llu -> REVOKE\n", id.c_str(), thd, lid);
// 		return ret;
// 	}

// 	// no REVOKE, just turns to FREE
// 	else
// 	{
// 		locks_clstatus[lid] = rlock_protocol::FREE;
// 		pthread_cond_signal(&(locks_cond[lid]));
// 		pthread_mutex_unlock(&mutex);
// 		tprintf("lock_client_cache: %s--%d release %llu -> FREE\n", id.c_str(), thd, lid);
// 		return ret;
// 	}
// }