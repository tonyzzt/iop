/* iop console UI */
#include <string>
#include <pwd.h>
#include <sys/types.h>
#include <stdlib.h>
#include <cstdlib>
#include <algorithm>

#include <ncurses.h>
#include "iop.h"
#include "process.h"

#include <time.h>
#include "CSVFileWriter.h"

std::string * caption;
//extern char [] version;
const char version[] = " version " VERSION "." SUBVERSION "." MINORVERSION;
extern ProcList * processes;
extern timeval curtime;

extern Process * unknowntcp;
extern Process * unknownudp;
extern Process * unknownip;

// sort on sent or received?
//bool sortRecv = true;

int sortMode = sort_Recv;


// viewMode: kb/s or total
int VIEWMODE_KBPS = 0;
int VIEWMODE_TOTAL_KB = 1;
int VIEWMODE_TOTAL_B = 2;
int VIEWMODE_TOTAL_MB = 3;
int viewMode = VIEWMODE_KBPS;
int nViewModes = 4;

extern bool needrecord;
extern char* needrecord_filename;
extern bool needlongsave;
extern char* needlongsave_filename;

CSVFileWriter writerCSV;
CSVFileWriter writerCSVlongsave;

extern bool processMode;
extern bool emptyMode;

class Line
{
public:
	Line (const char * name, double n_recv_value, double n_sent_value, double n_packet_recv_value,double n_packet_sent_value,pid_t pid, uid_t uid, const char * n_devicename)
	{
		assert (pid >= 0);
		m_name = name;
		sent_value = n_sent_value;
		recv_value = n_recv_value;
		devicename = n_devicename;
		packet_sent_value = n_packet_sent_value;
		packet_recv_value = n_packet_recv_value;
		m_pid = pid;
		m_uid = uid;
		assert (m_pid >= 0);
	}

	void show (int row, unsigned int proglen);

	double sent_value;
	double recv_value;

	double packet_sent_value;
	double packet_recv_value;
	
	pid_t m_pid; //typedef short           pid_t; 	
	const char * m_name;

private:
	const char * devicename;
	uid_t m_uid;
};

char * uid2username (uid_t uid)
{
	struct passwd * pwd = NULL;
	/* getpwuid() allocates space for this itself,
	 * which we shouldn't free */
	pwd = getpwuid(uid);

	if (pwd == NULL)
	{
		assert(false);
		return strdup ("unlisted");
	} else {
		return strdup(pwd->pw_name);
	}
}


void Line::show (int row, unsigned int proglen)
{
	assert (m_pid >= 0);
	assert (m_pid <= 100000);

	if (emptyMode) return;

	if (DEBUG || tracemode )
	{
		std::cout << m_name << '/' << m_pid << '/' << m_uid << "\t" << sent_value << "\t" << recv_value << std::endl;
		return;
	}

	if (m_pid == 0)
		mvprintw (3+row, 0, "?");
	else
		mvprintw (3+row, 0, "%d", m_pid);
	char * username = uid2username(m_uid);
	mvprintw (3+row, 6, "%s", username);
	free (username);
	if (strlen (m_name) > proglen) {
		// truncate oversized names
		char * tmp = strdup(m_name);
		char * start = tmp + strlen (m_name) - proglen;
		start[0] = '.';
		start[1] = '.';
		mvprintw (3+row, 6 + 9, "%s", start);
		free (tmp);
	} else {
		mvprintw (3+row, 6 + 9, "%s", m_name);
	}
	mvprintw (3+row, 6 + 9 + proglen + 2, "%s", devicename);
	mvprintw (3+row, 6 + 9 + proglen + 2 + 6, "%10.3f", sent_value);
	mvprintw (3+row, 6 + 9 + proglen + 2 + 6 + 9 + 3, "%10.3f", recv_value);

	mvprintw (3+row, 6 + 9 + proglen + 2 + 6 + 9 + 3 + 18, "%10.0f", packet_sent_value);
	mvprintw (3+row, 6 + 9 + proglen + 2 + 6 + 9 + 3 + 18 + 14, "%10.0f", packet_recv_value); //lenght is 10
	
	if (viewMode == VIEWMODE_KBPS)
	{
		//mvprintw (3+row, 6 + 9 + proglen + 2 + 6 + 11, "KB/sec");
		mvprintw (3+row, 6 + 9 + proglen + 2 + 6 + 9 + 3 + 11, "KB/sec");
		//mvprintw (3+row, 6 + 9 + proglen + 2 + 6 + 9 + 3 + 9 + 9 + 11, "packets/sec");
		mvprintw (3+row, 6 + 9 + proglen + 2 + 6 + 9 + 3 + 18 + 14 + 10, "/sec");
	}
	else if (viewMode == VIEWMODE_TOTAL_MB)
	{
		mvprintw (3+row, 6 + 9 + proglen + 2 + 6 + 9 + 3 + 11, "MB    ");
	}
	else if (viewMode == VIEWMODE_TOTAL_KB)
	{
		mvprintw (3+row, 6 + 9 + proglen + 2 + 6 + 9 + 3 + 11, "KB    ");
	}
	else if (viewMode == VIEWMODE_TOTAL_B)
	{
		mvprintw (3+row, 6 + 9 + proglen + 2 + 6 + 9 + 3 + 11, "B     ");
	}
}

int GreatestFirst (const void * ma, const void * mb)
{
	Line ** pa = (Line **)ma;
	Line ** pb = (Line **)mb;
	Line * a = *pa;
	Line * b = *pb;
	double aValue;
	if (sortMode == sort_Recv)
	{
		aValue = a->recv_value;
	}
	else if (sortMode == sort_Sent)
	{
		aValue = a->sent_value;
	}
	else 
	{
		aValue = (double)a->m_pid;
	}

	double bValue;
	if (sortMode == sort_Recv)
	{
		bValue = b->recv_value;
	}
	else if (sortMode == sort_Sent)
	{
		bValue = b->sent_value;
	}
	else
	{
		bValue = (double)b->m_pid;
	}

	if (aValue > bValue)
	{
		return -1;
	}
	if (aValue == bValue)
	{
		return 0;
	}
	return 1;
}

int LeastFirst (const void * ma, const void * mb)
{
	Line ** pa = (Line **)ma;
	Line ** pb = (Line **)mb;
	Line * a = *pa;
	Line * b = *pb;
	double aValue;
	if (sortMode == sort_Recv)
	{
		aValue = a->recv_value;
	}
	else if (sortMode == sort_Sent)
	{
		aValue = a->sent_value;
	}
	else 
	{
		aValue = (double)a->m_pid;
	}

	double bValue;
	if (sortMode == sort_Recv)
	{
		bValue = b->recv_value;
	}
	else if (sortMode == sort_Sent)
	{
		bValue = b->sent_value;
	}
	else
	{
		bValue = (double)b->m_pid;
	}

	if (aValue < bValue)
	{
		return -1;
	}
	if (aValue == bValue)
	{
		return 0;
	}
	return 1;
}


void init_ui ()
{
	WINDOW * screen = initscr();
	raw();
	noecho();
	cbreak();
	nodelay(screen, TRUE);
	caption = new std::string ("iop");
	caption->append(version);
	//caption->append(", running at ");
}

void exit_ui ()
{
	clear();
	endwin();
	delete caption;
}

void ui_tick ()
{
	switch (getch()) {
		case 'q':
			/* quit */
			quit_cb(0);
			break;
		case 's':
			/* sort on 'sent' */
			sortMode = sort_Sent;
			break;
		case 'r':
			/* sort on 'received' */
			sortMode = sort_Recv;
			break;	
		case 'p':
			/* sort on 'pid' */
			sortMode = sort_Pid;
			break;				
		case 'm':
			/* switch mode: total vs kb/s */
			viewMode = (viewMode + 1) % nViewModes;
			break;
	}
}

float tomb (u_int32_t bytes)
{
	return ((double)bytes) / 1024 / 1024;
}
float tokb (u_int32_t bytes)
{
	return ((double)bytes) / 1024;
}
float tokbps (u_int32_t bytes)
{
	return (((double)bytes) / PERIOD) / 1024; //PERIOD means last PERIOD seconds
}
float toPacketPerSecond (u_int32_t packets)
{
	return (((double)packets) / PERIOD);
}
/** Get the kb/s values for this process */
void getkbps (Process * curproc, float * recvd, float * sent, float * recvdPacket,float * sentPacket)
{
	u_int32_t sum_sent = 0,
	  	sum_recv = 0;
	u_int32_t sum_packet_sent = 0,
	  	sum_packet_recv = 0;

	/* walk though all this process's connections, and sum
	 * them up */
	ConnList * curconn = curproc->connections;
	ConnList * previous = NULL;
	while (curconn != NULL) //enuermate different connection
	{
 		if (curconn->getVal()->getLastPacket() <= curtime.tv_sec - CONNTIMEOUT)
		{
			/* stalled connection, remove. */
			ConnList * todelete = curconn;
			Connection * conn_todelete = curconn->getVal();
			curconn = curconn->getNext();
			if (todelete == curproc->connections)
				curproc->connections = curconn;
			if (previous != NULL)
				previous->setNext(curconn);
			delete (todelete);
			delete (conn_todelete);
		}
		else 
		{
			u_int32_t sent = 0, recv = 0;
			u_int32_t packet_recv = 0, packet_sent = 0;
			curconn->getVal()->sumanddel(curtime, &recv, &sent, &packet_recv, &packet_sent); //get the difference
			sum_sent += sent; 
			sum_recv += recv;
			sum_packet_sent += packet_sent; 
			sum_packet_recv += packet_recv;
			previous = curconn;
			curconn = curconn->getNext();
		}
	}
	*recvd = tokbps(sum_recv);
	*sent = tokbps(sum_sent);
	*recvdPacket = toPacketPerSecond(sum_packet_recv); //test purpose
	*sentPacket = toPacketPerSecond(sum_packet_sent); //test purpose
}

/** get total values for this process */
void gettotal(Process * curproc, u_int32_t * recvd, u_int32_t * sent,u_int32_t * recvdPacket, u_int32_t * sentPacket)
{
	u_int32_t sum_sent = 0,
	  	sum_recv = 0;
	u_int32_t sum_packet_sent = 0,
	  	sum_packet_recv = 0;
	
	ConnList * curconn = curproc->connections;
	while (curconn != NULL) //sum up all the connection's sumSent and sumRecv
	{
		Connection * conn = curconn->getVal();
		sum_sent += conn->sumSent;
		sum_recv += conn->sumRecv;
		sum_packet_sent += conn->sumPacketSent;
		sum_packet_recv += conn->sumPacketRecv;
		curconn = curconn->getNext();
	}
	//std::cout << "Sum sent: " << sum_sent << std::endl;
	//std::cout << "Sum recv: " << sum_recv << std::endl;
	*recvd = sum_recv;
	*sent = sum_sent;
	*recvdPacket = sum_packet_recv;
	*sentPacket = sum_packet_sent;
}

void gettotalmb(Process * curproc, float * recvd, float * sent, float * recvdPacket,float * sentPacket)
{
	u_int32_t sum_sent = 0,
	  	sum_recv = 0;
	u_int32_t sum_packet_sent = 0,
	  	sum_packet_recv = 0;
	gettotal(curproc, &sum_recv, &sum_sent,&sum_packet_recv,&sum_packet_sent);
	*recvd = tomb(sum_recv);
	*sent = tomb(sum_sent);
	*recvdPacket = sum_packet_recv;
	*sentPacket = sum_packet_sent;
	
}

/** get total values for this process */
void gettotalkb(Process * curproc, float * recvd, float * sent, float * recvdPacket,float * sentPacket)
{
	u_int32_t sum_sent = 0,
	  	sum_recv = 0;
	u_int32_t sum_packet_sent = 0,
	sum_packet_recv = 0;
	gettotal(curproc, &sum_recv, &sum_sent,&sum_packet_recv,&sum_packet_sent);
	*recvd = tokb(sum_recv);
	*sent = tokb(sum_sent);
	*recvdPacket = sum_packet_recv;
	*sentPacket = sum_packet_sent;
}

void gettotalb(Process * curproc, float * recvd, float * sent, float * recvdPacket,float * sentPacket)
{
	u_int32_t sum_sent = 0,
	  	sum_recv = 0;
	u_int32_t sum_packet_sent = 0,
	sum_packet_recv = 0;	
	gettotal(curproc, &sum_recv, &sum_sent,&sum_packet_recv,&sum_packet_sent);
	//std::cout << "Total sent: " << sum_sent << std::endl;
	*sent = sum_sent;
	*recvd = sum_recv;
	*recvdPacket = sum_packet_recv;
	*sentPacket = sum_packet_sent;	
}

// Display all processes and relevant network traffic using show function
void do_refresh() //
{	
	int row; // number of terminal rows
	int col; // number of terminal columns
	unsigned int proglen; // max length of the "PROGRAM" column

	getmaxyx(stdscr, row, col);	 /* find the boundaries of the screeen */
	/*
	if (col < 60) {
		clear();
		mvprintw(0,0, "The terminal is too narrow! Please make it wider.\nI'll wait...");
		return;
	}
*/

	if (col > PROGNAME_WIDTH) col = PROGNAME_WIDTH;

	proglen = col - 82;

	//refreshconninode();
	if (emptyMode)
	{
		//TODO Nothing.
	}
	else if (DEBUG || tracemode)
	{
		std::cout << "\nRefreshing:\n";
	}
	else
	{
		clear();
		mvprintw (0, 0, "%s", caption->c_str());
		attron(A_REVERSE);
		mvprintw (2, 0, "  PID USER     %-*.*s  DEV        SENT        RECEIVED        PK_SENT      PK_RECEIVED", proglen, proglen, "PROGRAM");
		attroff(A_REVERSE);
	}

	if (!processes) return; //in case the processes is not initalized, then return with no show
	
	ProcList * curproc = processes; //current process
	ProcList * previousproc = NULL;
	int nproc = processes->size(); //max size is 65535
	/* initialise to null pointers */
	Line * lines [nproc]; //the number of rows. depending on the count of processes
	int n = 0, i = 0;
	double sent_global = 0;
	double recv_global = 0;

	double packet_sent_global = 0;
	double packet_recv_global = 0;

#ifndef NDEBUG
	// initialise to null pointers
	for (int i = 0; i < nproc; i++)
		lines[i] = NULL;
#endif

	while (curproc != NULL) //enumerate the global processes
	{
		// walk though its connections, summing up their data, and
		// throwing away connections that haven't received a package
		// in the last PROCESSTIMEOUT seconds.
		assert (curproc != NULL);
		assert (curproc->getVal() != NULL);
		assert (nproc == processes->size());

		/* remove timed-out processes (unless it's one of the the unknown process) */

		//comment remove timed-out processes  
		/*
		if ((curproc->getVal()->getLastPacket() + PROCESSTIMEOUT <= curtime.tv_sec)
				&& (curproc->getVal() != unknowntcp) //remain unknow process
				&& (curproc->getVal() != unknownudp)
				&& (curproc->getVal() != unknownip))
		{

			if (DEBUG)
				std::cout << "PROC: Deleting process\n";
			ProcList * todelete = curproc;
			Process * p_todelete = curproc->getVal();
			if (previousproc)
			{
				previousproc->next = curproc->next;
				curproc = curproc->next;
			} else {
				processes = curproc->getNext();
				curproc = processes;
			}
			delete todelete; //delete current ProcList 
			delete p_todelete; //delete current Process
			nproc--;
			//continue;
			
			
		}
		else
		*/
		{
			// add a non-timed-out process to the list of stuff to show
			float value_sent = 0, 
				value_recv = 0;

			float value_packet_sent = 0, 
				value_packet_recv = 0;


			//retrieve the current process's network io
			if (viewMode == VIEWMODE_KBPS)
			{
				//std::cout << "kbps viemode" << std::endl;
				getkbps (curproc->getVal(), &value_recv, &value_sent, &value_packet_recv, &value_packet_sent);
			}
			else if (viewMode == VIEWMODE_TOTAL_KB)
			{
				//std::cout << "total viemode" << std::endl;
				gettotalkb(curproc->getVal(), &value_recv, &value_sent, &value_packet_recv, &value_packet_sent);
			}
			else if (viewMode == VIEWMODE_TOTAL_MB)
			{
				//std::cout << "total viemode" << std::endl;
				gettotalmb(curproc->getVal(), &value_recv, &value_sent, &value_packet_recv, &value_packet_sent);
			}
			else if (viewMode == VIEWMODE_TOTAL_B)
			{
				//std::cout << "total viemode" << std::endl;
				gettotalb(curproc->getVal(), &value_recv, &value_sent, &value_packet_recv, &value_packet_sent);
			}
			else
			{
				forceExit("Invalid viewmode");
			}
			uid_t uid = curproc->getVal()->getUid();
#ifndef NDEBUG
			struct passwd * pwuid = getpwuid(uid);
			assert (pwuid != NULL);
			// value returned by pwuid should not be freed, according to
			// Petr Uzel.
			//free (pwuid);
#endif
			assert (curproc->getVal()->pid >= 0);
			assert (n < nproc);

			lines[n] = new Line (curproc->getVal()->name, value_recv, value_sent,value_packet_recv,value_packet_sent,
					curproc->getVal()->pid, uid, curproc->getVal()->devicename); //init the line
			previousproc = curproc;
			curproc = curproc->next; //caculate the next process
			n++; //n represent one process, corresponding to the curproc
#ifndef NDEBUG
			assert (nproc == processes->size());
			if (curproc == NULL)
				assert (n-1 < nproc);
			else
				assert (n < nproc);
#endif
		}
	}

	/* sort the accumulated lines */
	if (sortMode == sort_Pid)
	{
		qsort (lines, nproc, sizeof(Line *), LeastFirst);
	}
	else
	{
		qsort (lines, nproc, sizeof(Line *), GreatestFirst);
	}

	if (needrecord)
	{
		//char pathName[ZT_MAX_PATH] = "test1.csv";	
		writerCSV.Open( needrecord_filename);	 //w+t, rewrite the file
		writerCSV.WriteString("PID",4);
		writerCSV.WriteString("SENT",5);
		writerCSV.WriteString("RECEIVED",9);
		writerCSV.WriteString("PK_SENT",8);
		writerCSV.WriteString("PK_RECEIVED",12);	
		writerCSV.WriteString("PROGRAM",8);	
		writerCSV.NextLine();	
	}

	if (needlongsave)
	{
		writerCSVlongsave.OpenA( needlongsave_filename);
	}
	

	/* print them */
	for (i=0; i<nproc; i++)
	{
		lines[i]->show(i, proglen);
		recv_global += lines[i]->recv_value; //the sum for the CURRENT lines
		sent_global += lines[i]->sent_value;

		packet_recv_global += lines[i]->packet_recv_value; //the sum for the CURRENT lines
		packet_sent_global += lines[i]->packet_sent_value;		

		if (needrecord)
		{
			writerCSV.WriteInt(lines[i]->m_pid);
			writerCSV.WriteFloat(lines[i]->sent_value);
			writerCSV.WriteFloat(lines[i]->recv_value);
			writerCSV.WriteFloat(lines[i]->packet_sent_value);
			writerCSV.WriteFloat(lines[i]->packet_recv_value);				
			writerCSV.WriteString(lines[i]->m_name,strlen(lines[i]->m_name));
			writerCSV.NextLine();		
		}

		if (needlongsave)
		{
/*
			time_t rawtime;
			struct tm * timeinfo;
			time (&rawtime);
			timeinfo = localtime (&rawtime);
			char strTime[100];
			sprintf(strTime, "%s", asctime(timeinfo));

			writerCSVlongsave.WriteString(strTime,strlen(strTime));
			*/
			
			writerCSVlongsave.WriteInt(lines[i]->m_pid);
			writerCSVlongsave.WriteFloat(lines[i]->sent_value);
			writerCSVlongsave.WriteFloat(lines[i]->recv_value);
			writerCSVlongsave.WriteFloat(lines[i]->packet_sent_value);
			writerCSVlongsave.WriteFloat(lines[i]->packet_recv_value);				
			writerCSVlongsave.WriteString(lines[i]->m_name,strlen(lines[i]->m_name));
			time_t timep;
			time(&timep);
			char* strTime = asctime(gmtime(&timep));
			strTime[strlen(strTime)-1] = '\0';
			writerCSVlongsave.WriteString(strTime,strlen(strTime));
			writerCSVlongsave.NextLine();		
		}
				
		delete lines[i];
	}

	if (needrecord)
	{	
		writerCSV.Close();
	}

	if (needlongsave)
	{	
		writerCSVlongsave.Close();
	}
		
	if (tracemode || DEBUG) {
		/* print the 'unknown' connections, for debugging */
		if (unknowntcp)
		{
			ConnList * curr_unknownconn = unknowntcp->connections;
			while (curr_unknownconn != NULL) {
				std::cout << "Unknown connection: " <<
					curr_unknownconn->getVal()->refpacket->gethashstring() << std::endl;

				curr_unknownconn = curr_unknownconn->getNext();
			}
		}
	}

	if ((!tracemode) && (!DEBUG) && !emptyMode ){
		attron(A_REVERSE);
		mvprintw (3+1+i, 0, "  TOTAL        %-*.*s        %10.3f  %10.3f        %10.0f    %10.0f ", proglen, proglen, " ", sent_global, recv_global,packet_sent_global,packet_recv_global);
		if (viewMode == VIEWMODE_KBPS)
		{
			mvprintw (3+1+i, col - 36, "KB/sec ");
			mvprintw (3+1+i, col - 5, "/sec ");
		} else if (viewMode == VIEWMODE_TOTAL_B) {
			mvprintw (3+1+i, col - 36, "B      ");
		} else if (viewMode == VIEWMODE_TOTAL_KB) {
			mvprintw (3+1+i, col - 36, "KB     ");
		} else if (viewMode == VIEWMODE_TOTAL_MB) {
			mvprintw (3+1+i, col - 36, "MB     ");
		}
		attroff(A_REVERSE);
		mvprintw (4+1+i, 0, "");
		refresh();
	}
}


