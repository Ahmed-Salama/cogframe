#ifndef LAUNCH_HH
#define LAUNCH_HH
#include <click/glue.hh>
#include <click/string.hh>
#include <click/ipaddress.hh>


struct launch_ctrl_hdr
{
	uint8_t type;
	enum Type
	{
		LAUNCH_REQ = 1,
		LAUNCH_RES = 2,
		LAUNCH_UPDATE = 3,
		LAUNCH_LOCK_REQ = 4,
		LAUNCH_LOCK_RES = 5,
		LAUNCH_READY_TORECIEVE = 6
	};
	uint32_t my_lat;			// Sender's Latitude.
	uint32_t my_long;			// Sender's Longitude.
	
	uint8_t channel;
	uint32_t pu_behavior;
	uint8_t channel1;
	uint32_t pu_behavior1;
	uint8_t channel2;
	uint32_t pu_behavior2;



	uint32_t switching_time;

	uint8_t lock_response;
	IPAddress neighbor_ip;		/* Neighbor response with its IP		     */
	IPAddress dst_ip;
  
};


#endif
