#ifndef Launch_Router_HH
#define Launch_Router_HH
#include <click/element.hh>
#include <click/glue.hh>
#include <click/ipaddress.hh>
#include <click/etheraddress.hh>
#include <clicknet/ether.h>
#include <elements/local/launch.hh>
#include <elements/local/launchrequester.hh>
#include <elements/local/launchlockrequester.hh>
#include <click/confparse.hh>
#include <click/timer.hh>
#include <click/hashmap.hh>
#include <elements/local/launchrtrsender.hh>
#include <list>		// list class library
#include <elements/local/Controller.cc>
using namespace std;
CLICK_DECLS

class LaunchRouter : public Element { public:

	LaunchRouter();
	~LaunchRouter();

	const char *class_name() const		{ return "LaunchRouter"; }
	const char *port_count() const		{ return PORTS_1_1; }
	const char *processing() const		{ return AGNOSTIC; }
	
	int configure(Vector<String> &, ErrorHandler *);
	int initialize(ErrorHandler *);
	
	Packet * simple_action(Packet *p_in);

	void insert_route(const IPAddress &nip,
	      uint32_t nlat, uint32_t nlong, 
	       uint8_t * ne, 	uint8_t chl,
	       	uint32_t pub,uint8_t chl1,
	       	uint32_t pub1,uint8_t chl2,
	       	uint32_t pub2, 	uint32_t swt);
	void update_route(const IPAddress &nip, uint8_t chl, uint32_t pub,uint8_t chl1, uint32_t pub1,uint8_t chl2, uint32_t pub2);
	
	void cant_use_channel(int channel_number);
	
	void set_channel_loc_positive();
	void set_channel_loc_negative();
	

	void set_ready_tosend_positive();
	void set_rtr_received_positive();

	void set_ready_for_another_packet_negative();

	bool can_use_11;
	bool can_use_6;
	bool can_use_1;

	int _already_has_lock;

private:

	list<Packet *> packets_holded;

	Packet * _holded_packet;
	IPAddress _dst_ip;

	IPAddress _ip;
	EtherAddress _ether_address_eth;
	uint8_t _eth [6];


	double _pu_behavior;

	bool _channel_lock_positive;
	class IPAddress locked_neighbor_ip;

	bool _routingtable_available;

	bool _rtr_received;

	bool _ready_for_another_packet;


	bool _ready_tosend;
	uint32_t my_lat;			// this node's Latitude.
	uint32_t my_long;			// this node's Longitude.


	/* destination node's addresses */
	IPAddress dst_ip;
	EtherAddress dst_eth;

	uint32_t dst_lat;			// this node's Latitude.
	uint32_t dst_long;			// this node's Longitude.

	
	/* timers 
	1. _respone_waiting_timer for waiting on responses to reach this node from its nieghbors
	2. _lock_waiting_timer for waiting on channel lock to reach this node from the node it chose as the next hop
	3. _routing_table_entry_timer for making route table expire */
	Timer _respone_waiting_timer;
	Timer _lock_waiting_timer;
	Timer _rtr_waiting_timer;
	Timer _routing_table_entry_timer;

	uint32_t _repsonse_waiting_ms;
	uint32_t _lock_waiting_ms;	
	uint32_t _routing_table_entry_ms;


	static void static_use_responses(Timer *, void *e) { ((LaunchRouter *) e)->use_responses(); }
	void use_responses();

	static void static_use_lock(Timer *, void *e) { ((LaunchRouter *) e)->use_lock(); }
	void use_lock();

	static void static_make_routetable_expire(Timer *, void *e) { ((LaunchRouter *) e)->make_routetable_expire(); }
	void make_routetable_expire();

	static void static_use_rtr(Timer *, void *e) { ((LaunchRouter *) e)->use_rtr(); }
	void use_rtr();
	

	LaunchCtrlRequester *_requester;
	LaunchLockRequester * _lock_requester;
	LaunchRTRSender * _rtr_sender;
  
	//Metric Calculation Function

	double distance(double lat1, double lon1, double lat2, double lon2, char unit);
	double deg2rad(double deg);
	double rad2deg(double deg);

	//Routing Table Entry

	class RouteEntry {

	public:
		class IPAddress  neighbor_ip;      // IP address of this destination

		uint32_t neighbor_lat;			// Sender's Latitude.
		uint32_t neighbor_long;			// Sender's Longitude.

		uint8_t   neighbor_eth[6]; // hardware address of next hop

		uint8_t channel;
		uint32_t pu_behavior;
		
		uint8_t channel1;
		uint32_t pu_behavior1;

		uint8_t channel2;
		uint32_t pu_behavior2;
		

		uint32_t switching_time;
		uint8_t channel_used;


		RouteEntry(const IPAddress &nip,
			  uint32_t nlat, uint32_t nlong, 
			   uint8_t * ne, 	uint8_t chl, uint32_t pu,uint8_t chl1,uint32_t pu1,uint8_t chl2,uint32_t pu2,
			   	 	uint32_t swt) :
		  neighbor_ip(nip), neighbor_lat(nlat), neighbor_long(nlong), 
		  channel(chl),  pu_behavior(pu),channel1(chl1),  pu_behavior1(pu1),channel2(chl2),  pu_behavior2(pu2), switching_time(swt)
		{memcpy(neighbor_eth,ne,6);}

		RouteEntry() {}
		~RouteEntry() {}
	};



	typedef HashMap<IPAddress, RouteEntry> RTable;
	typedef RTable::const_iterator RTIter;
	RTable _rtes;
	
	double calculate_metric(RouteEntry r, LocationEntry l_dst, LocationEntry l_neighbor);
	
	RouteEntry choose_bestneighbor(IPAddress _current_dst_addr, HashMap<IPAddress, RouteEntry>  _rtes)
	{
		click_chatter("ENTERED CHOOSING BEST NEIGHBOR!!\n");

		if(_rtes.findp(_current_dst_addr) != 0)
		{
			printf("STILL HERE \n");
			click_chatter("DST_ADDR_IP %s\n",_current_dst_addr.unparse().c_str());
			RouteEntry * editable = _rtes.findp(_current_dst_addr);
			printf("PUs %d %d %d %s\n", editable->pu_behavior, editable->pu_behavior1, editable->pu_behavior2,
			editable->neighbor_ip.unparse().c_str());
			
			if(!can_use_1)
			{
				if(editable->pu_behavior2 < editable->pu_behavior1 && can_use_11)
				{
					editable->channel_used = 11;
				}
				else if(editable->pu_behavior1 < editable->pu_behavior2 && can_use_6)
				{
					editable->channel_used = 6;
				}				
				else if( !can_use_6 && can_use_11 )
				{
					editable->channel_used = 11;
				}
				else if( can_use_6 && !can_use_11 )
				{
					editable->channel_used = 6;
				}				
				
				else
				{
					printf("I CAN'T BE YOUR HOP ... \n");
					printf("I'LL KILL MYSELF BECAUSE I CAN'T BE YOUR HOP ... \n");
					printf("BY DARLING ... X-( ....\n");
					printf("... ....\n");
					printf("X-( .... R.I.P ..\n");
					exit(0);
				}
			}
			else if(!can_use_6)
			{
				if(editable->pu_behavior2 < editable->pu_behavior && can_use_11)
				{
					editable->channel_used = 11;
				}
				else if(editable->pu_behavior < editable->pu_behavior2 && can_use_1)
				{
					editable->channel_used = 1;
				}				
				else if( !can_use_1 && can_use_11 )
				{
					editable->channel_used = 11;
				}
				else if( can_use_1 && !can_use_11 )
				{
					editable->channel_used = 1;
				}								else
				{
					printf("I CAN'T BE YOUR HOP ... \n");
					printf("I'LL KILL MYSELF BECAUSE I CAN'T BE YOUR HOP ... \n");
					printf("BY DARLING ... X-( ....\n");
					printf("... ....\n");
					printf("X-( .... R.I.P ..\n");
					exit(0);
				}

			}
			else if(!can_use_11)
			{
				if(editable->pu_behavior < editable->pu_behavior1 && can_use_1)
				{
					editable->channel_used = 1;					
				}
				else if(editable->pu_behavior1 < editable->pu_behavior && can_use_6)
				{
					editable->channel_used = 6;
				}				
				else if( !can_use_6 && can_use_1 )
				{
					editable->channel_used = 1;
				}
				else if( can_use_6 && !can_use_1 )
				{
					editable->channel_used = 6;
				}								else
				{
					printf("I CAN'T BE YOUR HOP ... \n");
					printf("I'LL KILL MYSELF BECAUSE I CAN'T BE YOUR HOP ... \n");
					printf("BY DARLING ... X-( ....\n");
					printf("... ....\n");
					printf("X-( .... R.I.P ..\n");
					exit(0);
				}

			}
			else
			{
				if( editable->pu_behavior <= editable->pu_behavior1 && editable->pu_behavior <= editable->pu_behavior2)
				{
					printf("STILL HERE 2\n");
					editable->channel_used = 1;
					//editable->channel_used = 1;
				}
				else if( editable->pu_behavior1 <= editable->pu_behavior && editable->pu_behavior1 <= editable->pu_behavior2)
				{
					printf("STILL HERE 2\n");
					//rte.channel_used = 6;
					editable->channel_used = 6;
				}
				else if(editable->pu_behavior2 <= editable->pu_behavior1 && editable->pu_behavior2 <= editable->pu_behavior)
				{
					printf("STILL HERE 2\n");
					//rte.channel_used = 11;
					editable->channel_used = 11;
				}
			}
					 
			printf("STILL HERE 3\n");
			click_chatter("EDITABLE_IP %s\n",(*editable).neighbor_ip.unparse().c_str()); 
			return *editable;
		}
		
		
		Controller controller = Controller::getInstance();
		click_chatter("Start debugging..........\n");
		
		double last_metric = 10000;
		IPAddress best_ip;
		double current_metric;

		for (RTIter iter = _rtes.begin(); iter.live(); iter++) {
			RouteEntry rte = iter.value();
			RouteEntry * editable = _rtes.findp(rte.neighbor_ip);
			
			LocationEntry lentry_dst =  controller.get_sensed_location(string(_current_dst_addr.unparse().mutable_c_str()));//rte.neighbor_ip);
			
			//click_chatter("%d %d %d %d %d %d %d %d", rte.neighbor_lat, rte.neighbor_long, rte.channel, rte.pu_behavior, rte.channel1, rte.pu_behavior1, rte.channel2, rte.pu_behavior2);
			
			click_chatter("Sensed .......... \n");
			LocationEntry lentry_neighbor =  controller.get_sensed_location(string(rte.neighbor_ip.unparse().mutable_c_str()));
			click_chatter("Sensed  neighbor.......... \n");
			
			if(can_use_1 && rte.pu_behavior <= rte.pu_behavior1 && rte.pu_behavior <= rte.pu_behavior2)
			{
				rte.channel_used = 1;
				editable->channel_used = 1;
			}
			else if(can_use_6 && rte.pu_behavior1 <= rte.pu_behavior && rte.pu_behavior1 <= rte.pu_behavior2)
			{
				rte.channel_used = 6;
				editable->channel_used = 6;
			}
			else if(can_use_11 && rte.pu_behavior2 <= rte.pu_behavior && rte.pu_behavior2 <= rte.pu_behavior1)
			{
				rte.channel_used = 11;
				editable->channel_used = 11;
			}
			else
			{
				
				click_chatter("error.......... \n"); 
				rte.channel_used = 0;
				continue;
			}

			
			current_metric = calculate_metric(rte,lentry_dst, lentry_neighbor);
			click_chatter("currentMetric ... %.f\n", current_metric); 
			printf("CALCULATING DISTANCE BETWEEN %s AND %s AND IT IS :: %f ON CHANNEL %d \n", rte.neighbor_ip.unparse().c_str(),lentry_dst.identifier.c_str(),current_metric, rte.channel_used);


/*			RouteEntry rte = iter.value();
			
			LocationEntry lentry =  _ltable.find(rte.neighbor_ip);

			current_metric = calculate_metric(rte, lentry);
			if(can_use_1 && rte.pu_behavior > rte.pu_behavior1 && rte.pu_behavior>rte.pu_behavior2 && rte.pu_behavior >0)
			{
				rte.channel_used = 1;
			}
			else if(can_use_6 && rte.pu_behavior1 > rte.pu_behavior && rte.pu_behavior1>rte.pu_behavior2 && rte.pu_behavior1 >0)
			{
				rte.channel_used = 6;
			}
			else if(can_use_11 && rte.pu_behavior2 > rte.pu_behavior1 && rte.pu_behavior2>rte.pu_behavior && rte.pu_behavior2 >0)
			{
				rte.channel_used = 11;
			}
			else
			{
				rte.channel_used = 0;
			}
*/

			
//			switch(rte.channel)
//			{
//				case 1:
//					if (!can_use_1)
//						continue;
//				break;
//				case 6:
//					if (!can_use_6)
//						continue;
//				break;
//				case 11:
//					if (!can_use_11)
//						continue;
//				break;
//			}


			if(current_metric < last_metric){

				click_chatter("bestIP modified.......... \n"); 
				last_metric = current_metric;
				best_ip = rte.neighbor_ip;
			}	
		}
		//_rtes.findp(best_ip)->channel_used = channel_used ;
		
			click_chatter("Method ended.......... \n"); 
			//click_chatter(best_ip); 
		return *_rtes.findp(best_ip);
	};


	uint8_t channel_used ;
	
};
CLICK_ENDDECLS
#endif
