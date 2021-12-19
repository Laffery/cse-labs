/* extent client interface. */

#ifndef extent_client_h
#define extent_client_h

#include <string>
#include "extent_protocol.h"
#include "extent_server.h"

using namespace std;

class extent_client
{
private:
	rpcc *cl;

public:
	extent_client(string dst);

	extent_protocol::status create(uint32_t, extent_protocol::extentid_t &);
	extent_protocol::status get(extent_protocol::extentid_t, string &);
	extent_protocol::status getattr(extent_protocol::extentid_t, extent_protocol::attr &);
	extent_protocol::status put(extent_protocol::extentid_t, string);
	extent_protocol::status remove(extent_protocol::extentid_t);
};

#endif