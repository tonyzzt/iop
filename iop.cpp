/* iop.cpp */

#include "iop.h"

#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <signal.h>
#include <string>
#include <string.h>
#include <getopt.h>
#include <stdarg.h>

using namespace std;
#include <vector>
#include <algorithm>

#include <netinet/ip.h>
#include <netinet/ip6.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>

#include "cui.h"

extern "C" {
	#include "decpcap.h"
}

#include "packet.h"
#include "connection.h"
#include "process.h"
#include "refresh.h"
#include "devices.h"

extern Process * unknownudp;
extern ConnList* connections;

unsigned refreshdelay = 1;
bool tracemode = false;
bool bughuntmode = false;
bool needrefresh = true;
bool needrecord = false;
char* needrecord_filename;

bool needlongsave = false;
char* needlongsave_filename;

bool emptyMode = false;

bool processMode = false;
char* strPIDs;
vector<int> vPIDs;


//packet_type packettype = packet_ethernet;
//dp_link_type linktype = dp_link_ethernet;
const char version[] = " version " VERSION "." SUBVERSION "." MINORVERSION;

const char * currentdevice = NULL;

timeval curtime;

extern int sortMode;

bool local_addr::contains (const in_addr_t & n_addr) {
	if ((sa_family == AF_INET)
	    && (n_addr == addr))
		return true;
	if (next == NULL)
		return false;
	return next->contains(n_addr);
}

bool local_addr::contains(const struct in6_addr & n_addr) {
	if (sa_family == AF_INET6)
	{
		/*
		if (DEBUG) {
			char addy [50];
			std::cerr << "Comparing: ";
			inet_ntop (AF_INET6, &n_addr, addy, 49);
			std::cerr << addy << " and ";
			inet_ntop (AF_INET6, &addr6, addy, 49);
			std::cerr << addy << std::endl;
		}
		*/
		//if (addr6.s6_addr == n_addr.s6_addr)
		if (memcmp (&addr6, &n_addr, sizeof(struct in6_addr)) == 0)
		{
			if (DEBUG)
				std::cerr << "Match!" << std::endl;
			return true;
		}
	}
	if (next == NULL)
		return false;
	return next->contains(n_addr);
}

struct dpargs {
	int sa_family;
	in_addr ip_src;
	in_addr ip_dst;
	in6_addr ip6_src;
	in6_addr ip6_dst;
};

int process_tcp (u_char * userdata, const dp_header * header, const u_char * m_packet) {
	struct dpargs * args = (struct dpargs *) userdata;
	struct tcphdr * tcp = (struct tcphdr *) m_packet;

	curtime = header->ts;

	/* get info from userdata, then call getPacket */
	Packet * packet;
	switch (args->sa_family)
	{
		case (AF_INET):
			packet = new Packet (args->ip_src, ntohs(tcp->source), args->ip_dst, ntohs(tcp->dest), header->len, header->ts);//
			break;
		case (AF_INET6):
			packet = new Packet (args->ip6_src, ntohs(tcp->source), args->ip6_dst, ntohs(tcp->dest), header->len, header->ts);
			break;
	}

	Connection * connection = findConnection(packet); //retrieve the existed connection

	if (connection != NULL)
	{	
		/* add packet to the connection */
		connection->add(packet);
	} else {
		
		/* else: unknown connection, create new */
		connection = new Connection (packet);
		Process * ret = getProcess(connection, currentdevice); //getProcess (Connection * connection, const char * devicename), the purpose is adding to the processes linklist 

		if (ret != NULL) //when a process get, then it means a valid connection and add to connections list
		{
			connections = new ConnList (connection, connections); //global connections to judge if such connection is known or unknown
		}
		else
		{
			if (tracemode)
				std::cout << "innode is not found in getProcess since /proc/net/tcp delay";
			delete connection;
		}
			
	}
	delete packet;

	if (needrefresh)
	{
		do_refresh(); //render info in UI
		needrefresh = false;
	}

	/* we're done now. */
	return true;
}

int process_udp (u_char * userdata, const dp_header * header, const u_char * m_packet) {
	struct dpargs * args = (struct dpargs *) userdata;
	//struct tcphdr * tcp = (struct tcphdr *) m_packet;
	struct udphdr * udp = (struct udphdr *) m_packet;

	curtime = header->ts;

	/* TODO get info from userdata, then call getPacket */
	Packet * packet;
	switch (args->sa_family)
	{
		case (AF_INET):
			packet = new Packet (args->ip_src, ntohs(udp->source), args->ip_dst, ntohs(udp->dest), header->len, header->ts);
			break;
		case (AF_INET6):
			packet = new Packet (args->ip6_src, ntohs(udp->source), args->ip6_dst, ntohs(udp->dest), header->len, header->ts);
			break;
	}

	//if (DEBUG)
	//	std::cout << "Got packet from " << packet->gethashstring() << std::endl;

	Connection * connection = findConnection(packet);

	if (connection != NULL)
	{
		/* add packet to the connection */
		connection->add(packet);
	} else {
		/* else: unknown connection, create new */
		//std::cout << "udp hit" <<endl;
		connection = new Connection (packet);
		getProcess(connection, currentdevice);
	}
	delete packet;

	if (needrefresh)
	{
		do_refresh();
		needrefresh = false;
	}

	/* we're done now. */
	return true;
}

int process_ip (u_char * userdata, const dp_header * /* header */, const u_char * m_packet) {
	struct dpargs * args = (struct dpargs *) userdata;
	struct ip * ip = (struct ip *) m_packet;
	args->sa_family = AF_INET;
	args->ip_src = ip->ip_src;
	args->ip_dst = ip->ip_dst;

	/* we're not done yet - also parse tcp :) */
	return false;
}

int process_ip6 (u_char * userdata, const dp_header * /* header */, const u_char * m_packet) {
	struct dpargs * args = (struct dpargs *) userdata;
	const struct ip6_hdr * ip6 = (struct ip6_hdr *) m_packet;
	args->sa_family = AF_INET6;
	args->ip6_src = ip6->ip6_src;
	args->ip6_dst = ip6->ip6_dst;

	/* we're not done yet - also parse tcp :) */
	return false;
}

void quit_cb (int /* i */)
{
	procclean();
	if ((!tracemode) && (!DEBUG))
		exit_ui();
	exit(0);
}

void forceExit(const char *msg, ...)
{
	if ((!tracemode)&&(!DEBUG)){
		exit_ui();
	}

	va_list argp;
	va_start(argp, msg);
	vfprintf(stderr, msg, argp);
	va_end(argp);
	std::cerr << std::endl;

    exit(0);
}

static void versiondisplay(void)
{
	std::cerr << version << "\n";
}

static void help(void)
{
	//std::cerr << "usage: iop [-V] [-b] [-d seconds] [-t] [-p] [-f (eth|ppp))] [device [device [device ...]]]\n";
	std::cerr << "usage: iop [-V] [-b] [-d seconds] [-t] [-c] [-p pid1,pid2...] [-r filename] [-s filename] [device [device [device ...]]]\n";
	std::cerr << "		-V : prints version.\n";
	std::cerr << "		-d : delay for update refresh rate in seconds. default is 1.\n";
	std::cerr << "		-t : tracemode.\n";
	std::cerr << "		-e : emtpy output mode. It does not show any data in UI, usually combine use with -r or -s switch\n";
	//std::cerr << "		-f : format of packets on interface, default is eth.\n";
	std::cerr << "		-b : bughunt mode - implies tracemode.\n";
	std::cerr << "		-c : sniff in promiscious mode (not recommended).\n";
	std::cerr << "		-p : only list the select process by PID.\n";
	std::cerr << "		-r : recording the process data realtime (ONLY the last second) to file .\n";	
	std::cerr << "		-s : recording the process data for a long run to a file.\n";		
	std::cerr << "		device : device(s) to monitor. default is eth0\n";
	std::cerr << std::endl;
	std::cerr << "When iop is running, press:\n";
	std::cerr << " q: quit\n";
	std::cerr << " m: switch between total and kb/s mode\n";

}

class handle {
public:
	handle (dp_handle * m_handle, const char * m_devicename = NULL,
			handle * m_next = NULL) {
		content = m_handle; next = m_next; devicename = m_devicename;
	}
	dp_handle * content;
	const char * devicename;
	handle * next;
};

int main (int argc, char** argv)
{
	process_init(); //init the global var unknowntcp and processes

	device * devices = NULL;
	//dp_link_type linktype = dp_link_ethernet;
	int promisc = 0;

	int opt;
	char seps[] = ","; 
	while ((opt = getopt(argc, argv, "Vhbtced:r:s:p:")) != -1) {
		switch(opt) {
			case 'V':
				versiondisplay();
				exit(0);
			case 'h':
				help();
				exit(0);
			case 'b':
				bughuntmode = true;
				tracemode = true;
				break;
			case 't':
				tracemode = true;
				break;
			case 'c':
				promisc = 1;
				break;
			case 'd':
				fprintf(stderr, "Error putting libpcap in nonblocking mode\n");
				refreshdelay=atoi(optarg);
				break;
			case 'r':				
				needrecord_filename = strdup(optarg);
				needrecord = true;
				std::cout << "Saving proc io date (realtime) to: "<<  needrecord_filename <<endl;	
				break;
			case 's':				
				needlongsave_filename = strdup(optarg);
				needlongsave = true;
				std::cout << "Saving proc io date to: "<<  needlongsave_filename <<endl;				
				break;	
			case 'p':				
				strPIDs = strdup(optarg);	
				char *token;					
				token = strtok(strPIDs, seps);
				while( token != NULL )
				{						
					vPIDs.push_back(atoi(token));
					//cout<< atoi(token) << endl;
					token = strtok( NULL, seps );
				}
				processMode = true;
				sortMode = sort_Pid;	
				break;
			case 'e':
				emptyMode = true;
				break;
			/*
			case 'f':
				argv++;
				if (strcmp (optarg, "ppp") == 0)
					linktype = dp_link_ppp;
				else if (strcmp (optarg, "eth") == 0)
					linktype = dp_link_ethernet;
				}
				break;
			*/
			default:
				help();
				exit(EXIT_FAILURE);
		}
	}

	while (optind < argc) {
		devices = new device (strdup(argv[optind++]), devices); //devices linktable, from the new eth to the old eth
	}

	if (devices == NULL)
	{
		devices = determine_default_device(); //eth0
	}

	if ((!tracemode) && (!DEBUG) && !emptyMode){
		init_ui();
	}

	if (NEEDROOT && (getuid() != 0))
		forceExit("You need to be root to run iop!");

	char errbuf[PCAP_ERRBUF_SIZE];

	handle * handles = NULL;
	device * current_dev = devices;
	while (current_dev != NULL) { //add callback function foreach eth
		getLocal(current_dev->name, tracemode);
		if ((!tracemode) && (!DEBUG)){
			//caption->append(current_dev->name);
			//caption->append(" ");
		}

		dp_handle * newhandle = dp_open_live(current_dev->name, BUFSIZ, promisc, 100, errbuf); //handle means eth device
		if (newhandle != NULL)
		{
			dp_addcb (newhandle, dp_packet_ip, process_ip); //add callback function to this handle
			dp_addcb (newhandle, dp_packet_ip6, process_ip6);
			dp_addcb (newhandle, dp_packet_tcp, process_tcp);
			dp_addcb (newhandle, dp_packet_udp, process_udp);

			/* The following code solves sf.net bug 1019381, but is only available
			 * in newer versions (from 0.8 it seems) of libpcap
			 *
			 * update: version 0.7.2, which is in debian stable now, should be ok
			 * also.
			 */
			if (dp_setnonblock (newhandle, 1, errbuf) == -1)
			{
				fprintf(stderr, "Error putting libpcap in nonblocking mode\n");
			}
			handles = new handle (newhandle, current_dev->name, handles); //encapsulate the handle to a linklist
		}
		else
		{
			fprintf(stderr, "Error opening handler for device %s\n", current_dev->name); //
		}

		current_dev = current_dev->next;
	}

	signal (SIGALRM, &alarm_cb); //register the SIGALRM keyboard info etc.
	signal (SIGINT, &quit_cb); 
	alarm (refreshdelay);

	// Main loop:
	//
	//  Walks though the 'handles' list, which contains handles opened in non-blocking mode.
	//  This causes the CPU utilisation to go up to 100%. This is tricky:
	while (1)
	{
		bool packets_read = false;

		handle * current_handle = handles;
		while (current_handle != NULL) //enumerate the eth devices
		{
			struct dpargs * userdata = (dpargs *) malloc (sizeof (struct dpargs));
			userdata->sa_family = AF_UNSPEC;  //not defined
			currentdevice = current_handle->devicename;
			int retval = dp_dispatch (current_handle->content, -1, (u_char *)userdata, sizeof (struct dpargs)); //proactively get the net buffer!
			if (retval == -1 || retval == -2)
			{
				std::cerr << "Error dispatching" << std::endl;
			}
			else if (retval != 0)
			{
				packets_read = true;
			}
			free (userdata);
			current_handle = current_handle->next; //get next eth
		}

		if ((!DEBUG)&&(!tracemode))
		{
			// handle user input
			ui_tick();
		}

		if (needrefresh)
		{
			do_refresh();
			needrefresh = false;
		}

		// If no packets were read at all this iteration, pause to prevent 100%
		// CPU utilisation;
		if (!packets_read)
		{
			usleep(100);
		}
	}
}

