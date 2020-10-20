// lock client interface.

#ifndef lock_client_cache_h
#define lock_client_cache_h

#include <string>
#include <map>
#include "lock_protocol.h"
#include "rpc.h"
#include "lock_client.h"
#include "lang/verify.h"
#include <pthread.h>

using namespace std;

/* 
 * Classes that inherit lock_release_user can override dorelease so 
 * that they will be called when lock_client releases a lock.
 * You will not need to do anything with this class until Lab 6.
*/
class lock_release_user
{
public:
	virtual void dorelease(lock_protocol::lockid_t) = 0;
	virtual ~lock_release_user(){};
};

/*
 * Class that keep information of a lock kept in this client, 
 * including status of lock in this client cache, condition 
 * variable and a boolean flag which means whether the lock 
 * has received REVOKE from server.
 */
class lock_info
{
public:
	rlock_protocol::client_status status;
	pthread_cond_t cond;
	bool revoked;

public: 
	lock_info()
	{
		status = rlock_protocol::NONE;
		pthread_cond_init(&cond, NULL);
		revoked = false;
	}

	~lock_info() {}
};

class lock_client_cache : public lock_client
{
private:
	class lock_release_user *lu;
	int rlock_port;
	string hostname;
	string id;

private:
	map<rlock_protocol::lockid_t, lock_info *> client_locks;
	pthread_mutex_t mutex;

public:
	string getId() { return id; }

	static int last_port;
	lock_client_cache(string xdst, class lock_release_user *l = 0);
	virtual ~lock_client_cache(){};
	lock_protocol::status acquire(lock_protocol::lockid_t);
	lock_protocol::status release(lock_protocol::lockid_t);
	rlock_protocol::status revoke_handler(lock_protocol::lockid_t, int &);
	rlock_protocol::status retry_handler(lock_protocol::lockid_t, int &);
};

#endif