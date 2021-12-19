// lock protocol

#ifndef lock_protocol_h
#define lock_protocol_h

#include "rpc.h"

class lock_protocol
{
public:
	enum xxstatus
	{
		OK,
		RETRY,
		RPCERR,
		NOENT,
		IOERR
	};
	enum lock_status
	{
		LOCKED,
		UNLOCK
	};
	typedef int status;
	typedef unsigned long long lockid_t;
	enum rpc_numbers
	{
		acquire = 0x7001,
		release,
		stat
	};
};

class rlock_protocol
{
public:
	enum xxstatus
	{
		OK,
		RETRY,
		RPCERR,
		NOENT,
		IOERR,
		REVOKE
	};
	enum client_status
	{
		NONE, 
		FREE, 
		LOCKED, 
		ACQUIRING, 
		RELEASING,
		XNULL
	};
	typedef int status;
	typedef unsigned long long lockid_t;
	enum rpc_numbers
	{
		revoke = 0x8001,
		retry = 0x8002
	};
};

#endif