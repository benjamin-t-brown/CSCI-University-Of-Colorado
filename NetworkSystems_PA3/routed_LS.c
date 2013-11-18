/* routed_LS.c */

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>   /* memset() */
#include <sys/time.h> /* select() */
#include <signal.h>
#include <unistd.h>
#include <math.h>
#include <pthread.h>

#include "rlist.h"

#define MAXROUTERS 20

void verify_usage( int argc );
int build_addr( char* ip, int port, struct sockaddr_in* remote );
int create_tcp_socket_in( int port, int* insock, int* resock);
int create_tcp_socket_out( int port, int* sock );
int read_init_file( char* path, RouterNode** head, char me );
int create_and_bind_sockets( RouterNode* head, char me );
int init_wait_set( RouterNode* curr, char me, fd_set* fds );
int recv_topography(  char me, fd_set* fds);
int foreward_topography( RouterNode* head, char me, char sender );
int dykstra( );
void logfile(char* str, FILE* fp);

int seq = 0;
RouterNode* table = NULL;
RouterNode* gtopo[MAXROUTERS];
char id;
int seqtable[MAXROUTERS]; 

int running = 1;
FILE *ofp;

int main(int argc, char *argv[]) {  
	srand(0);                     
	int err = 0;	
	void* broadcast_table();

	for( int i = 0; i < MAXROUTERS; ++i ) gtopo[i] = NULL;
	
	for( int i = 0; i < MAXROUTERS; ++i ) seqtable[i] = 0;

	verify_usage( argc );
	
	ofp = fopen(argv[2], "w");
	if (ofp == NULL) {
		fprintf(stderr, "Can't open output file %s!\n",
			  argv[2]);
		exit(1);
	}	
	
	err = read_init_file(argv[3], &table, argv[1][0] ); if( err == 0 ) return 1;
	id = argv[1][0];

	gtopo[argv[1][0] - 65] = table; 

	RLIST_print( table );

	create_and_bind_sockets( table, argv[1][0] );

	//----------------------------------------------------------------

	logfile("INITIAL ROUTING TABLE\n", ofp);
	char line[100];
	RouterNode* hh = gtopo[ id - 65 ];
	while( hh != NULL ){
		sprintf(line, " %c -> %c thru %c cost %d \n", id, hh->entry->dest, hh->entry->dest, hh->entry->cost );
		printf("%s",line);
		logfile(line, ofp);
		hh = hh->next;
	}

	printf("Listen for topology changes or quit cmd...\n");fflush(stdout);
	fd_set fds;
	struct timeval timeout;
	int rc, result, threadid;
	
	pthread_t broadcaster;
	pthread_create(&broadcaster,NULL,broadcast_table,&threadid);
	
	while( 1 ){
		timeout.tv_sec = 3; timeout.tv_usec = 0;
		init_wait_set( table, argv[1][0], &fds );

		rc = select(sizeof(fds)*8, &fds, NULL, NULL, &timeout);
		if (rc==-1) {
			printf("Select failed \n");
			break;
		}

		if (rc > 0){
			if( recv_topography( argv[1][0], &fds) == 0 ) break;	
		} 
	}

	running = 0;
	printf("cleanup\n");
	
	fclose( ofp );
	pthread_join(broadcaster,NULL);
	for( int i = 0; i < MAXROUTERS; i++ )
		RLIST_cleanup( gtopo[i] );
}

void* broadcast_table(){
	fd_set fds;
	struct timeval timeout;
	int rc, result, threadid, ran;

	while( running ){
		
		//Wait a random amount of microseconds so that there is less probability of 
		//double writing to a socket.
		ran = randr(0, 50);
		timeout.tv_sec = 1; timeout.tv_usec = 10000*ran;
		
		rc = select(sizeof(fds)*8, NULL, NULL, NULL, &timeout);
		if (rc==-1) {
			printf("Select broadcast failed \n");
		}
		
		//Flood the routing table for this router to all adjacent routers
		char send[1024]; char topo_str[1024]; bzero(send, 1024); bzero(topo_str, 1024);
		RLIST_to_string( table, topo_str);
		sprintf(send, "%c,%d@%s#!", id, seq++, topo_str);
		RLIST_write_except( &table, send, id, 'z', strlen(send) );	
		
		//RLIST_print( gtopo[id-65] );	
	}
}

/**
 * Given the number of arguments passed, print the usage information if incorrect and exit
 * otherwise, continue.
 * @param argc The number of arguments.
 */
void verify_usage( int argc ){
	if (argc != 4) {
		printf("usage : %s <RouderID> <LogFileName> <Initialization File>\n", "routed_LS");
		exit(1);
	} else {
		printf("\n--- CSCI 4273 Network Systems: Programming Assignment 3 ---\n\n");
	}
}

/**
 * Create a tcp socket in linux
 * @param  port  The port of the socket
 * @param  local [out] The sockaddress struct which will be modified with socket information
 * @return       sockfd - a socked file descriptor
 */
int create_tcp_socket_in( int port, int* insock, int* resock ){
	int sockfd, portno=port, on=1;
	socklen_t clilen;
	char buffer[256];
	struct sockaddr_in serv_addr, cli_addr;
	
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0) 
		perror("ERROR opening socket\n");
	bzero((char *) &serv_addr, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = INADDR_ANY;
	serv_addr.sin_port = htons(portno);
	if (bind(sockfd, (struct sockaddr *) &serv_addr,
		  sizeof(serv_addr)) < 0) 
		  perror("ERROR on binding\n");
		  
	
	listen(sockfd,5);
	*resock = accept(sockfd, 
			 (struct sockaddr *) &cli_addr, 
			 &clilen);

	printf("Accepted at port %d\n", port);

	setsockopt( sockfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on) );
	setsockopt( *resock, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on) );

	*insock = sockfd;

	return 0;
}

int create_tcp_socket_out( int port, int* sock ){
	int sockfd, portno = port;
	struct sockaddr_in serv_addr;

	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0) 
		perror("ERROR opening socket\n");

	*sock = sockfd;

	build_addr( "localhost", port, &serv_addr);

	if (connect(sockfd,(struct sockaddr *) &serv_addr,sizeof(serv_addr)) < 0){
		perror("ERROR connecting!\n");
		return -1;
	}
	
	printf(" Connected!\n");
	
	
	return 0;
}

/**
 * Build a sockaddr_in struct with information about the server you are
 * connecting to so you can use it in sendto
 * @param  ip     The ip address of the server
 * @param  port   The port the server is listening at
 * @param  remote [out] A reference to the sockaddress you want to build
 * @return        1 on success, 0 on error
 */
int build_addr( char* ip, int port, struct sockaddr_in* remote ){
	struct hostent *server;

    server = gethostbyname(ip);
    if (server == NULL) {
        printf("ERROR: no such host\n");
        return 0;
    }

	bzero( remote, sizeof( struct sockaddr_in ));   
	remote->sin_family = AF_INET;            
    bcopy((char *)server->h_addr, 
         (char *) &(remote->sin_addr.s_addr),
         server->h_length);
	remote->sin_port = htons( port );     

	return 1;
}

//: <source router, source TCP port, destination router, destination TCP port, link cost>
int read_init_file( char* path, RouterNode** head, char me ){
	size_t n;
	FILE *fp;
    char  *line = NULL;
    
    char src, dest;
    int port_in, port_out, cost;
    
    int numlinks = 0;
   
    fp = fopen(path,"r");
    if( fp == NULL ){
    	printf("Error loading init file: No file '%s' exists in local directory.\n", path); fflush(stdout);
    	return 0;
    } else {
		RLIST_add_node( head, me, 0, 0, 0 );
		while( getline(&line, &n, fp) != -1 ){
			//sscanf( line, "<%10[^,],%d,%10[^,],%d,%d>", 
			sscanf( line, "<%c,%d,%c,%d,%d>", 
				&src, 
				&port_in,
				&dest,
				&port_out,
				&cost );
				
			//don't read topography of other routers
			if( src == me ){
				RLIST_add_node( head, dest, cost, port_out, port_in );
				numlinks++;
			}
		}
	    fclose( fp );
	}
	
	if( line )
		free( line );

    return numlinks;	
}

int create_and_bind_sockets( RouterNode* head, char me ){
	struct sockaddr_in* local = NULL;
	char buff[100];
	
	if( head == NULL ){
		return 0;
	}
		
	if( head->entry->dest == me ){
		create_and_bind_sockets( head->next, me );	
	} else {
		int tmp1, tmp2, tmp3, conn=0;
		printf("Blindly attempting to connect to %c (%d)...", head->entry->dest, head->entry->port_out);fflush(stdout);
		if( create_tcp_socket_out( head->entry->port_out, &tmp1 ) == 0  ){
			conn = 1;
			head->entry->out_fd = tmp1; 
		} else {
			shutdown(tmp1, 2);
		}
		
		printf("Accepting from %c (%d)...", head->entry->dest, head->entry->port_out);fflush(stdout);
		create_tcp_socket_in( head->entry->port_in, &tmp2, &tmp3 ); 
		head->entry->in_fd = tmp2;
		head->entry->recv_fd = tmp3;

		if( conn == 0 ){
			printf("Retry connect to %c (%d)...\n", 'B', head->entry->port_out);fflush(stdout);
			sleep(1);
			if( create_tcp_socket_out( head->entry->port_out, &tmp1 ) == 0  ){
				conn = 1;
				head->entry->out_fd = tmp1;
			} else {
				shutdown(tmp1, 2);			
			}
		}	
		
		create_and_bind_sockets( head->next, me );		
		
	}
	return 0;
}

int init_wait_set( RouterNode* curr, char me, fd_set* fds ){

	FD_ZERO(&(*fds));
	while( curr != NULL ){
		if( curr->entry->dest == me || curr->entry->recv_fd < 0 ){
			curr = curr->next;
			continue;	
		}

		//printf("Adding to wait set: %d\n", curr->entry->recv_fd);
		FD_SET( curr->entry->recv_fd, &(*fds));
		//FD_SET( curr->entry->in_fd, &(*fds));
		curr = curr->next;
	}	
	
	//std in
	FD_SET( 0, &(*fds));
	
	return 1;
}

int recv_topography( char me, fd_set* fds){
	char active[MAXROUTERS];
	char buff2[1024]; char buff[1024];
	char rlistbuff[1024]; char sublistbuff[1024];
	char from;
	int seqnum;
	int n;
	
	//check for quit command from stdin
	if( FD_ISSET(0, &(*fds) ) ){
		fgets(buff, 100, stdin);
		
		printf("STDIN READS: %s\n", buff);
		
		//Flood quit command if it was received, then quit locally
		if( buff[0] == 'q' && buff[1] == 'u' && buff[2] == 'i' && buff[3] == 't'){
			RLIST_write_except( &gtopo[me - 65], "quit\0", me, 'z', 5 );
			return 0;
		}
		else return 1;
	}
		
	//Load the 'active' array with the names of all routers which have written to this socket
	int nready = RLIST_get_entry_set( &gtopo[me - 65], fds, active );
	
	//for each router that has written to a socket
	for( int i = 0; i < nready; ++i ){
		bzero( buff, 1024 ); bzero( rlistbuff, 1024); bzero( buff2, 1024 ); 
			
		RouterEntry* entry = RLIST_get_entry( &gtopo[me - 65], active[i] );
		
		if( entry == NULL ){
			printf("ERROR, tried to recv topography from null entry at %c\n", active[i] );
			return 0;
		} 
		
		//Read the packet from the socket
		n = read( entry->recv_fd, buff2, 1024 );
		if( n < 1024 ) buff2[n] = '\0';
			
		//check if this packet is a 'quit' packet, if it is, flood it to the rest of the routers and exit
		if( buff2[0] == 'q' && buff2[1] == 'u' && buff2[2] == 'i' && buff2[3] == 't'){
			RLIST_write_except( &gtopo[me - 65], "quit\0", me, 'z', 5 );
			return 0;
		}

		//Look for the end of packet delimeter '!' 
		for( int j = 0; j < n; ++j ){
			if( buff2[j] == '!' ){
				strncpy(buff, buff2, j+1 );
				break;
			}
		}
		
		//seperate the head of the packet from the routing table in the packet
		sscanf( buff, "%c,%d@%s", &from, &seqnum, rlistbuff);	
		
		//Throw packets that this router has already seen
		if( seqnum <= seqtable[from - 65] || from == me ){
			continue;
		}
		
		//keep track of the most recent packet received from this router
		seqtable[from - 65] = seqnum;
		
		//printf("Receiving packet %s\n", buff);
		
		//go through each entry in the forewarding table for this packet and determine if the table
		//is different from the local table
		int lctr = 0; char src, dest; int port_in, port_out, cost; int attach_table = 0; int is_changed = 0;
		RouterEntry* currentry = NULL;
		while( lctr < n ){
			//extract a link from the forewarding table in the packet as a string
			bzero( sublistbuff, 1024 );	
			lctr = RLIST_extract_entry( rlistbuff, (char*) sublistbuff, lctr, n );

			//extract the different elements of the link into variables
			if( strlen(sublistbuff) < 5 ) break;		
			sscanf( sublistbuff, "%c,%d,%d,%d",  
				&dest,
				&cost, 
				&port_in,
				&port_out
				);
				
			//is there a local version of this table on hand?
			if( gtopo[ from - 65 ] == NULL || attach_table == 1 ){ // No? Then build one using this packet.  There was a change.
				attach_table = 1;
				//printf("ttttt at %d %d\n", gtopo[ from - 65 ] == NULL, attach_table);
				
				is_changed = 1;
				RLIST_add_node( &gtopo[ from - 65 ], dest, cost, 0, 0 );
			} else { 		     // Yes? Then see if there is a difference between this packet's table and the local one.

				//If we have don't have a record of the dest router in our table, add it!
				if( RLIST_get_entry( &gtopo[ me - 65 ], dest ) == NULL ){
					is_changed = 1;
					RLIST_add_node( &gtopo[ me - 65 ], dest, -1, 0, 0 );
				}
			
				currentry = RLIST_get_entry( &gtopo[ from - 65 ], dest );
				if( currentry == NULL ){
					printf("ERROR: currentry is null from: %c, dest %c\n", from, dest);
					//RLIST_print(gtopo[ from - 65 ]);
					continue;
				}
				
				//If there was a change in the cost of this link, then update that cost
				if( currentry->cost != cost ){
					is_changed = 1;
					currentry->cost = cost;
				}
				
			}	
			//printf(" ENTRY: %c -> %d\n", dest, cost );
		} 
		
		if( attach_table ){
			printf("Added routing table of: %c\n", from );
			//RLIST_print( gtopo[me-65] );
		}
		
		if( is_changed ){
			printf("Topology change detected: %c\n", from );
			
			//Only perform dykstra if we have topology for every adjacent node
			char nodes[MAXROUTERS];
			int len = RLIST_get_adj_nodes( gtopo[ me - 65 ], nodes );
			int do_dykstra = 1;
			for( int j = 0; j < len; ++j ){
				if( gtopo[ nodes[j] - 65 ] == NULL ){
					do_dykstra = 0;
				}
			} 
			if( do_dykstra ){
				//Log to outputfile (for assignment requirements)
				struct timeval tv;
				gettimeofday(&tv,NULL);
				char output[100];
				sprintf(output,"Topology changed detected: By %c at %d.%d\n", from, (int)tv.tv_sec, (int)tv.tv_usec); 	 
				logfile(output, ofp);	
							
				dykstra();
			}
		}
		
		//only foreward topology if a change has been detected
		//EDIT: For some reason it converges much better if this logic check is commented out
		//if( is_changed )
			RLIST_write_except( &gtopo[me-65], buff, me, from, n );			
	}
	
	return 1;
	
}

//Helper function for dykstra().  Returns the index of the router which has the lowest link cost
int get_smallest_cost_ind(int waiting[MAXROUTERS], int costs[MAXROUTERS]){
	int smallest = 99999999;
	int ind = 0;
	for( int i = 0; i < MAXROUTERS; i++ ){
		if( waiting[i] ){
			if( costs[i] < smallest && costs[i] > 0 ){
				ind = i;
				smallest = costs[i];
			}
		}		
	}
	
	return ind;
}

//Helper function for dykstra().  Returns true if nid is contained in arr.
int in_arr_char( char* arr, int n, char nid ){
	for( int i = 0; i < n; ++i ){
		if( arr[i] == nid ){
			return 1;
		}
	}
	
	return 0;
} 

//Helper function for dykstra().  Gets the min of two numbers.  If one number is -1, returns the opposite number.
int min( int a, int b ){
	if( a == -1 ) return b;
	if( b == -1 ) return a;
	
	if( a < b ){
		return a;
	} else {
		return b;
	}
}

int dykstra(){
	// Each of these arrays represents a dictionary where (ind+65) = name (as a char)
	// or inversely (name-65) = ind.  
	char spt[MAXROUTERS];    // 'shortest path tree' Entries are either:
							 // '%' - Indicates that dykstra has found a path to this router
							 // '-' - Indicates that dykstra still needs to find a path to this router
							 // '!' - Indicates that there is no router with name corresponding to this ind in the topology
	int waiting[MAXROUTERS]; // A list that contains the boolean of whether or not a certain router is waiting to be added to the spt
	int costs[MAXROUTERS];	 // A list that will contain the costs between links
	char nexthop[MAXROUTERS];// A list that associates a router name with which router it needs to hop to
							 // from this rouder (me) in order to take the shortest route to its destination
	
	//Set all routers in the topology such that they are waiting to be added to the spt
	// also initialize the cost array to what is known from this router's topology
	for( int i = 0; i < MAXROUTERS; ++i ){ 	
		RouterEntry* entry = RLIST_get_entry( &gtopo[id - 65], (char)(i + 65) );
		if( entry != NULL){
			waiting[i] = 1; 
			costs[i] = entry->cost;
			spt[i] = '-'; 
			nexthop[i] = (char)(i + 65);
		} else {
			//If the router does not exist in the topology, ignore the entry
			//and set up the variables such that this algorthm always ignores them
			waiting[i] = 0;
			costs[i] = -1;
			spt[i] = '!'; 
			nexthop[i] = '!';
		}
	}
	//printf("Dykstra initialized...\n");

	//Add this router to the spt (because the cost to get from this router to itself is known as 0)
	spt[ id - 65 ] = id; waiting[ id - 65 ] = 0; costs[ id - 65 ] = 0;

	//Do this loop as long as there are still things we need to add to the spt
	char prouter = id;
	while( in_arr_char( spt, MAXROUTERS, '-' ) ){
		
		//Traverse to the nearest router (whichever router has the shortest path) not in the spt
		char crouter = (char)(get_smallest_cost_ind( waiting, costs ) + 65) ;
		//We know there cannot possibly be a shorter path to this router, so add it to the spt
		waiting[ crouter - 65 ] = 0;
		spt[ crouter - 65 ] = prouter;
		
		//For each of the remaining routers not in the spt...
		for( int i = 0; i < MAXROUTERS; ++i ){
			if( !waiting[i] ) continue; //Ignore nonwaiting routers
			
			//Error case if for some reason there is no topology for a certain router (I haven't hit this yet, but it might happen)
			if( gtopo[crouter - 65] == NULL ){
				printf("For some reason the gtopo at %c is NULL, breaking\n", crouter);
				spt[ i ] = 'Z';
				waiting[ i ] = 0;
				nexthop[ i ] = (char)(i + 65);
				break;
			} 
			
			//printf(" Getting entry for topo %c to %c:\n", crouter, (char)(i + 65) );
			
			//From the the current router's routing table (crouter), get the link between that router 
			//and an adjacent router (i in this case)
			RouterEntry* curr_entry = RLIST_get_entry( &gtopo[crouter - 65], (char)(i + 65) );
			if( curr_entry != NULL ){ //If that link exists (it might not if the network hasn't converged yet)
				int prevcost = costs[i];
				
				//Set the cost to the minimum of the current link cost, and the link cost of going through this router to the destination
				costs[i] = min( costs[i], curr_entry->cost + costs[ crouter - 65 ] );
				
				//Keep track of where the nexthop should be if we changed the cost
				if( prevcost != costs[i] ){
					nexthop[i] = nexthop[ crouter - 65 ];
				}
			} else {
				//printf("Segfault?\n");
			}
		}
		
		prouter = crouter;
	}
	
	printf("NEXTHOP TABLE:\n");
	logfile("ROUTING TABLE\n", ofp);
	char line[100];
	for( int i = 0; i < MAXROUTERS; ++i ){
		if( spt[i] != '!' && spt[i] != '-' ){
			sprintf(line, " %c -> %c thru %c cost %d \n", (char)(id), (char)(i+65), nexthop[i], costs[i] );
			printf("%s",line);
			logfile(line, ofp);
		}
	}
	
	return 0;
}

void logfile(char* str, FILE* fp){
	fputs( str, fp );
}



