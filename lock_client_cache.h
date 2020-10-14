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

class lock_client_cache : public lock_client
{
private:
	class lock_release_user *lu;
	int rlock_port;
	string hostname;
	string id;

/*
  enum client_status { NONE=10, FREE, LOCKED, ACQUIRING, RELEASING };
  std::map<lock_protocol::lockid_t, client_status> lock_keeper;
  std::map<lock_protocol::lockid_t, pthread_cond_t> lock_manager;
  pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;
  std::map<lock_protocol::lockid_t, bool> revoke_has_arrived;
 */

private:
	map<rlock_protocol::lockid_t, rlock_protocol::client_status> locks_clstatus;
	map<rlock_protocol::lockid_t, pthread_cond_t> locks_cond;
	map<rlock_protocol::lockid_t, bool> locks_revoke;
	pthread_mutex_t mutex;

public:
	string getId() { return id; }
	string getStat(lock_protocol::lockid_t lid)
	{
		rlock_protocol::client_status stat = rlock_protocol::XNULL;
		if (locks_clstatus.find(lid) != locks_clstatus.end())
			stat = locks_clstatus[lid];
		switch (stat)
		{
		case rlock_protocol::NONE:
			return "NONE";
		case rlock_protocol::ACQUIRING:
			return "ACQUIRING";
		case rlock_protocol::LOCKED:
			return "LOCKED";
		case rlock_protocol::RELEASING:
			return "RELEASING";
		case rlock_protocol::FREE:
			return "FREE";
		default:
			return "NULL";
		}
	}
	lock_protocol::status ACQUIRE(lock_protocol::lockid_t, int);
	lock_protocol::status RELEASE(lock_protocol::lockid_t, int);

	static int last_port;
	lock_client_cache(string xdst, class lock_release_user *l = 0);
	virtual ~lock_client_cache(){};
	lock_protocol::status acquire(lock_protocol::lockid_t);
	lock_protocol::status release(lock_protocol::lockid_t);
	rlock_protocol::status revoke_handler(lock_protocol::lockid_t, int &);
	rlock_protocol::status retry_handler(lock_protocol::lockid_t, int &);
};

#endif