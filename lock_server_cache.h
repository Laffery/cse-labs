#ifndef lock_server_cache_h
#define lock_server_cache_h

#include <string>
#include <map>
#include <queue>
#include "lock_protocol.h"
#include "rpc.h"
#include "lock_server.h"

using namespace std;

class lock_server_cache
{
private:
	int nacquire;
	map<lock_protocol::lockid_t, queue<string> > locks_lists;

protected:
	pthread_mutex_t mutex;

public:
	lock_server_cache();
	lock_protocol::status stat(lock_protocol::lockid_t, int &);
	int acquire(lock_protocol::lockid_t, string id, int &);
	int release(lock_protocol::lockid_t, string id, int &);
};

#endif
