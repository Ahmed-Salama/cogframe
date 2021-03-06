#include <click/config.h>
#include <elements/local/launch.hh>
#include <elements/local/launchrouter.hh>
#include <elements/local/launchrequester.hh>
#include <elements/local/launchlockrequester.hh>
#include <click/args.hh>
#include <clicknet/ether.h>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/confparse.hh>
#include <elements/local/launchrtrsender.hh>
#include <unistd.h>
#include <math.h>
#define pi 3.14159265358979323846
CLICK_DECLS


LaunchRouter::LaunchRouter()
: _respone_waiting_timer(static_use_responses, this), _lock_waiting_timer(static_use_lock, this), _routing_table_entry_timer(static_make_routetable_expire, this), _rtr_waiting_timer(static_use_rtr, this)
{
	_routingtable_available = false;
	_channel_lock_positive = false;
	_ready_for_another_packet = true;
	_rtr_received = false;
	_already_has_lock = 0;

	can_use_11= true;
	can_use_6 = true;
	can_use_1 = true;


	
}

LaunchRouter::~LaunchRouter()
{
}

int
LaunchRouter::configure(Vector<String> &conf, ErrorHandler * errh)
{
	//Initializing the controller module.
	//Controller.initialize();
	
	if (Args(conf, this, errh)
      .read_mp("ETH", _ether_address_eth)
	  .read_mp("PU", _pu_behavior)
	  .read_mp("RES_T", _repsonse_waiting_ms)
	  .read_mp("LOCK_T", _lock_waiting_ms)
	  .read_mp("RT_T", _routing_table_entry_ms)
	  .read_p("REQUESTER", reinterpret_cast<Element *&>(_requester))
	  .read_p("LOCK_REQUESTER", reinterpret_cast<Element *&>(_lock_requester))
 	  .read_mp("LAUNCHRTRSENDER", reinterpret_cast<Element *&>(_rtr_sender))
	  .complete() < 0)
      return -1;
      
      
	//RouteEntry routeEntry = Controller.get_sensed_location(lch.neighbor_ip);
	//_pu_behavior = routeEntry.pu_behavior;

	memcpy(_eth ,_ether_address_eth.data(),sizeof(_eth));
	return 0;
}

int 
LaunchRouter::initialize(ErrorHandler *)
{
	_respone_waiting_timer.initialize(this);
	_lock_waiting_timer.initialize(this);
	_routing_table_entry_timer.initialize(this);
	_rtr_waiting_timer.initialize(this);
}

Packet *
LaunchRouter::simple_action(Packet *p_in)
{
	_holded_packet = p_in->uniqueify();
	
	//if router has been initialized with a lock and a routing table
	//the the packet is directly processed 
	if(_ready_for_another_packet)
	{
		//get dst ip from packet's annotations for later use	
		_dst_ip = _holded_packet->dst_ip_anno();
		
		
		//Forward packet if routingtable is available and we have lock 
		//call launchlocrequester if routingtable is available and we don't have lock
		//call launchctrlrequester to send REQ
		if(_routingtable_available &&  _channel_lock_positive)
		{
			click_chatter("choosing_best_neighbor");
			RouteEntry current_best_neighbor = choose_bestneighbor(_dst_ip,_rtes);
			IPAddress current_neighbor_ip  = current_best_neighbor.neighbor_ip;
			click_chatter("neighbor IP %s \n", current_neighbor_ip.unparse().c_str());
			click_chatter("neighbor IP 2%s \n", current_best_neighbor.neighbor_ip.unparse().c_str());
			click_chatter("chose_best_neighbor");
			if(current_neighbor_ip == locked_neighbor_ip)
			{
				click_chatter("check passed");
				_holded_packet->set_dst_ip_anno(locked_neighbor_ip /*from calculating the metric*/);
				return _holded_packet;
			}
			else
			{
				click_chatter("check didn't pass passed");
				_lock_requester->send_lock_request(current_best_neighbor.channel_used/*channel selected*/, current_best_neighbor.neighbor_ip/*lock distantion ip*/, current_best_neighbor.neighbor_eth/*lock distantion eth*/,_eth);
				_lock_waiting_timer.schedule_after_msec(_lock_waiting_ms);
				packets_holded.push_back(_holded_packet);
			}
		}
		//if only the routing table became available then send for
		//a LOCK REQ again
		else if(_routingtable_available)
		{
			click_chatter("dddddddddddddddd$$$$$$$$$$ lock negative");
			RouteEntry current_best_neighbor = choose_bestneighbor(_dst_ip,_rtes);
			IPAddress current_neighbor_ip  = current_best_neighbor.neighbor_ip;
			click_chatter("neighbor IP %s \n", current_neighbor_ip.unparse().c_str());
			click_chatter("neighbor IP 2%s \n", current_best_neighbor.neighbor_ip.unparse().c_str());
			click_chatter("dddddddddddddddd$$$$$$$$$$ obtained best neighbor information");
			_lock_requester->send_lock_request(current_best_neighbor.channel_used/*channel selected*/, current_best_neighbor.neighbor_ip/*lock distantion ip*/, current_best_neighbor.neighbor_eth/*lock distantion eth*/,_eth/*my ethernet*/);
			_lock_waiting_timer.schedule_after_msec(_lock_waiting_ms);
			click_chatter("dddddddddddddddd$$$$$$$$$$ LOCK REQUEST SENT");
			packets_holded.push_back(_holded_packet);
		}
		//if neither the lock nor the routing table are available 
		//then send CTRL REQ to all neighbours
		else
		{
			_requester->send_request();
			_respone_waiting_timer.schedule_after_msec(_repsonse_waiting_ms);
			_ready_for_another_packet = false;
			packets_holded.push_back(_holded_packet);
		}
	}
	else
	{	
		packets_holded.push_back(_holded_packet);
	}
	return 0;
}




// Called by the _respone_waiting_timer when it times out
// to chech if the routingtable was loaded or not
void 
LaunchRouter::use_responses()
{
	
	if(_routingtable_available)
	{
		//lookup table and calculate the metric to choose next hop
		//issue lock request
		
		click_chatter("Print in the launch router .. cc\n");
		click_chatter("available routing table \n");
		RouteEntry best_neighbor = choose_bestneighbor(_dst_ip,_rtes);	
		
		locked_neighbor_ip = best_neighbor.neighbor_ip;
		click_chatter("neighbor IP %s \n", locked_neighbor_ip.unparse().c_str());
		click_chatter("neighbor IP 2%s \n", best_neighbor.neighbor_ip.unparse().c_str());
		_lock_requester->send_lock_request(best_neighbor.channel_used/*channel selected*/, best_neighbor.neighbor_ip/*lock distantion ip*/, best_neighbor.neighbor_eth/*lock distantion eth*/,_eth);
		_lock_waiting_timer.schedule_after_msec(_lock_waiting_ms);
		
		
		
	}
	else
	{
		_requester->send_request();
		_respone_waiting_timer.schedule_after_msec(_repsonse_waiting_ms);
	}
}

// Called by the _lock_waiting_timer when it times out to check if lock was obtained
void 
LaunchRouter::use_lock()
{
	//check whether lock response is positive or negative
	//if positive push packet
	//if negative wait again
	if(_channel_lock_positive && _routingtable_available)
	{
		//annotate packet with distenation and output packet
		//sleep(2);

		Packet * value = packets_holded.front();
		click_chatter("_channel_lock_positive && _routingtable_available");
		RouteEntry current_best_neighbor = choose_bestneighbor(value->dst_ip_anno(),_rtes);
		IPAddress current_neighbor_ip  = current_best_neighbor.neighbor_ip;
		click_chatter("NEIGHBOR IP %s\n",current_neighbor_ip.unparse().c_str());
		click_chatter("NEIGHBOR IP 2%s\n",current_best_neighbor.neighbor_ip.unparse().c_str());
		if(current_neighbor_ip == locked_neighbor_ip)
		{
			value->set_dst_ip_anno(locked_neighbor_ip /*from calculating the metric*/);
			output(0).push(value);
			packets_holded.pop_front();
			_ready_for_another_packet = true;
		//	_rtr_sender->send_RTR();				
		}
		else
		{
			_dst_ip = value->dst_ip_anno();
			_lock_requester->send_lock_request(current_best_neighbor.channel_used/*channel selected*/, current_best_neighbor.neighbor_ip/*lock distantion ip*/, current_best_neighbor.neighbor_eth/*lock distantion eth*/,_eth);
			_lock_waiting_timer.schedule_after_msec(_lock_waiting_ms);
			return;
		}
		_rtr_waiting_timer.schedule_after_msec(_repsonse_waiting_ms);
	}
	else
	{
		_respone_waiting_timer.schedule_after_msec(_repsonse_waiting_ms);
	}
}


void 
LaunchRouter::use_rtr()
{
	if(_channel_lock_positive && _routingtable_available && _rtr_received)
	{
		while (packets_holded.size() > 0)
		{
			Packet * value = packets_holded.front();
			click_chatter("packets_holded.size() > 0\n");
			RouteEntry current_best_neighbor = choose_bestneighbor(value->dst_ip_anno(),_rtes);
			IPAddress current_neighbor_ip  = current_best_neighbor.neighbor_ip;
			click_chatter("Neighbor IP %s\n",current_neighbor_ip.unparse().c_str());
			click_chatter("Neighbor IP 2%s\n",current_best_neighbor.neighbor_ip.unparse().c_str());
			if(current_neighbor_ip == locked_neighbor_ip)
			{
				value->set_dst_ip_anno(locked_neighbor_ip /*from calculating the metric*/);
				output(0).push(value);
				packets_holded.pop_front();				
			}
			else
			{
				_dst_ip = value->dst_ip_anno();
				_lock_requester->send_lock_request(current_best_neighbor.channel_used/*channel selected*/, current_best_neighbor.neighbor_ip/*lock distantion ip*/, current_best_neighbor.neighbor_eth/*lock distantion eth*/,_eth);
				_lock_waiting_timer.schedule_after_msec(_lock_waiting_ms);
				return;
			}

		}
		_ready_for_another_packet = true;
	}
	else
	{
		_rtr_waiting_timer.schedule_after_msec(_repsonse_waiting_ms);
	}
}



void 
LaunchRouter::make_routetable_expire()
{
	//for (RTIter iter = _rtes.begin(); iter.live(); iter++) {
	//	const RouteEntry &rte = iter.value();
	//	_rtes.remove(rte.neighbor_ip);
	//}
	//_routingtable_available = false;
}



//Should be called by LaunchCtrlResponseHandler


void
LaunchRouter::insert_route(const IPAddress &nip,
	      uint32_t nlat, uint32_t nlong, 
	       uint8_t * ne, 	uint8_t chl,
	       	uint32_t pub,uint8_t chl1,
	       	uint32_t pub1,uint8_t chl2,
	       	uint32_t pub2, 	uint32_t swt)
{
	_routingtable_available = true;
	_routing_table_entry_timer.reschedule_after_msec(_routing_table_entry_ms);
	
	
	RouteEntry * temp = _rtes.findp(nip);
	if(temp != NULL)
	{
  	
		temp->neighbor_lat = nlat;
		temp->neighbor_long = nlong;
		temp->channel = chl;
		temp->pu_behavior = pub;
		temp->switching_time = swt;
	}
	else
	{
		
		RouteEntry r(nip, nlat, nlong, ne, chl, pub, chl1, pub1, chl2, pub2, swt);
		_rtes.insert(nip, r);
	}
}





void
LaunchRouter::update_route(const IPAddress &nip,uint8_t chl,uint32_t pub,uint8_t chl1, uint32_t pub1,uint8_t chl2, uint32_t pub2)
{
	RouteEntry* temp = _rtes.findp(nip);
	printf("=-=-=-=-=-=-=-=-=-=- %d\n",chl);
	temp->channel = chl;
	temp->pu_behavior = pub;
	
	temp->channel1 = chl1;
	temp->pu_behavior1 = pub1;
	
	temp->channel2 = chl2;
	temp->pu_behavior2 = pub2; 
}



/*
// Function to pick the best neighbor in terms of the launch metric
RouteEntry 
LaunchRouter::choose_bestneighbor(IPAddress _current_dst_addr)
{

	if(_rtes.findp(_current_dst_addr) != 0)
	return _rtes.findp(_current_dst_addr);
	
	double last_metric = 10000;
	uint8_t best_ip ;
	double current_metric;
	for (RTIter iter = _rtes.begin(); iter.live(); iter++) {
		if(_current_dst_addr == iter.neighbor_ip)
		{
			best_ip = rte.neighbor_ip;
			break;
		}

		RouteEntry rte = iter.value();

		LocationEntry lentry =  _ltable.find(rte.neighbor_ip);

		current_metric = calculate_metric(rte, lentry);
		if(current_metric < last_metric)
		{
			last_metric = current_metric;
			best_ip = rte.neighbor_ip;
		}	
	}

	return _rtes.findp(best_ip);
}
*/
//Function to calculate the metric for certain neighbor in the table
double 
LaunchRouter::calculate_metric(RouteEntry r, LocationEntry l_dst, LocationEntry l_neighbor)
{


	//double distance_value = distance(((double)l.neighbor_lat/1000.00),((double)l.neighbor_long/1000.00),((double)r.neighbor_lat/1000.00), ((double)r.neighbor_long/1000.00),'M');
	double distance_value = distance(((double)l_dst.neighbor_lat/1000.00),((double)l_dst.neighbor_long/1000.00),((double)l_neighbor.neighbor_lat/1000.00), ((double)l_neighbor.neighbor_long/1000.00),'M');

	click_chatter("CALCULATING DISTANCE BETWEEN %s AND %s AND IT IS :: %f \n",r.neighbor_ip.unparse().c_str(),l_dst.identifier.c_str(),distance_value);
	
	double my_pu;
	if(r.channel_used == 1)
	{
		my_pu = r.pu_behavior;
	}
	else if (r.channel_used == 6)
	{
		my_pu = r.pu_behavior1;
	}
	else if (r.channel_used == 11)
	{
		my_pu = r.pu_behavior2;
	}
	

	printf("SWITCHING TIME %d - N PU %d - MY PU %f \n",r.switching_time, r.pu_behavior, _pu_behavior);








//	printf("###########$$$$$$$$$$$$$$$$################$$$$$$$$$$############\n###########$$$$$$$$$$$$$$$$################$$$$$$$$$$############\n###########$$$$$$$$$$$$$$$$################$$$$$$$$$$############\nCALCULATING DISTANCE BETWEEN %s AND %s AND IT IS :: %f \n###########$$$$$$$$$$$$$$$$################$$$$$$$$$$############\n###########$$$$$$$$$$$$$$$$################$$$$$$$$$$############\n###########$$$$$$$$$$$$$$$$################$$$$$$$$$$############\n",r.neighbor_ip.unparse().c_str(),l.neighbor_ip.unparse().c_str(),distance_value);
//	click_chatter("###########$$$$$$$$$$$$$$$$################$$$$$$$$$$############\n###########$$$$$$$$$$$$$$$$################$$$$$$$$$$############\n###########$$$$$$$$$$$$$$$$################$$$$$$$$$$############\nCALCULATING DISTANCE BETWEEN %s AND %s AND IT IS :: %f \n###########$$$$$$$$$$$$$$$$################$$$$$$$$$$############\n###########$$$$$$$$$$$$$$$$################$$$$$$$$$$############\n###########$$$$$$$$$$$$$$$$################$$$$$$$$$$############\n",r.neighbor_ip.unparse().c_str(),l.neighbor_ip.unparse().c_str(),distance_value);
	return ((distance_value/(3*(10^6)))  + r.switching_time)/(1- r.pu_behavior*_pu_behavior);
}


//Function to calculate the distance between two nodes
double 
LaunchRouter::distance(double lat1, double lon1, double lat2, double lon2, char unit) {
	double theta, dist;
	theta = lon1 - lon2;
	dist = sin(deg2rad(lat1)) * sin(deg2rad(lat2)) + cos(deg2rad(lat1)) * cos(deg2rad(lat2)) * cos(deg2rad(theta));
	dist = acos(dist);
	dist = rad2deg(dist);
	dist = dist * 60 * 1.1515;
	switch(unit) {
		case 'M':
			break;
		case 'K':
			dist = dist * 1.609344;
			break;
		case 'N':
			dist = dist * 0.8684;
			break;
	}
	return (dist);
}


double 
LaunchRouter::deg2rad(double deg) {
	return (deg * pi / 180);
}


double 
LaunchRouter::rad2deg(double rad) {
	return (rad * 180 / pi);
}

void 
LaunchRouter::set_channel_loc_positive(){
	_channel_lock_positive = true;	
}

void 
LaunchRouter::set_channel_loc_negative()
{
	_channel_lock_positive = false;	
}



void 
LaunchRouter::set_rtr_received_positive()
{
	_rtr_received = true;	
}


void 
LaunchRouter::set_ready_for_another_packet_negative()
{
	_routingtable_available = false;
	_channel_lock_positive = false;
	_already_has_lock = 0;
	//_ready_for_another_packet = false;	
}




void 
LaunchRouter::cant_use_channel(int channel_number)
{
	switch(channel_number)
	{
		case 1:
			can_use_1 =false;
			break;
		case 6:
			can_use_6 =false;
			break;
		case 11:
			can_use_11 =false;
			break;
	}
	if(channel_used == channel_number)
		set_channel_loc_negative();
}


CLICK_ENDDECLS
EXPORT_ELEMENT(LaunchRouter)
