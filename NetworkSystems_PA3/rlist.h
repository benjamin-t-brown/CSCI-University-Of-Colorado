/* rlist.h by Benjamin Brown
 * A way to store the forewarding table as a linked list
 */
#ifndef RLIST_H
#define RLIST_H

#include <sys/types.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

typedef struct {
		char dest;
		int cost;
		int port_out;
		int port_in;
		int in_fd;
		int out_fd;
		int out_connected;
		int recv_fd;
		struct sockaddr_in* instruct;
		struct sockaddr_in* outstruct;
} RouterEntry;

void RE_to_string( RouterEntry* r, char* dest ){
	if( r->cost != -1 )
		sprintf(dest, "%c,%d,%d,%d#", r->dest, r->cost, r->port_out, r->port_in );
	else {
		sprintf(dest, " ");
	}
}

typedef struct{
	RouterEntry* entry;
	void* next;
} RouterNode;

int RLIST_extract_entry(char* buff, char* ret, int start, int max){
	int i;
	
	for( i = start; i < max; ++i ){
		if( buff[i] == '#' || buff[i] == '\n' || buff[i] == '\0' ){
			strncpy( ret, &buff[start], i - start ); 
			//printf("Extracting from %d to %d", start, i );			
			break;
		} else if( i == max-1  ){
			strncpy( ret, &buff[start], i - start + 1 ); 
			//printf("Extracting from %d to %d", start, i + 1 );					
		}
	}	
	
	
	return i+1;
}

int RLIST_add_node(RouterNode** head, char dest, int cost, int port_out, int port_in){
	RouterEntry* t = malloc( sizeof(RouterEntry) );
	t->dest = dest;
	t->cost = cost;
	t->port_out = port_out;
	t->port_in = port_in;
	t->in_fd = -1;
	t->out_fd = -1;
	t->recv_fd = -1;
	t->instruct = NULL;
	t->outstruct = NULL;
	t->out_connected = 0;
	
	if( *head == NULL ){
		RouterNode* newnode = (RouterNode*) malloc( sizeof(RouterNode) );
		newnode->entry = t;
		newnode->next = NULL;
		*head = newnode;
		return 0;
	}
	
	if( (*head)->next == NULL ){
		RouterNode* newnode = (RouterNode*) malloc( sizeof(RouterNode) );
		newnode->entry = t;
		newnode->next = NULL;		
		(*head)->next = newnode;
		return 0;
	}
	
	RouterNode* curr = *head;
	while( curr->next != NULL ){
		curr = curr->next;
	}

	RouterNode* newnode = (RouterNode*) malloc( sizeof(RouterNode) );
	newnode->entry = t;
	newnode->next = NULL;	
		
	curr->next = newnode;
	
	return 0;
}

int RLIST_cleanup(RouterNode* head){
	if( head == NULL ){
		return 0;
	}
	
	RLIST_cleanup(head->next);
	if( head->entry->instruct != NULL ) free( head->entry->instruct );
	if( head->entry->outstruct != NULL ) free( head->entry->outstruct );
	shutdown(head->entry->in_fd, 2);
	shutdown(head->entry->out_fd, 2);
	shutdown(head->entry->recv_fd, 2);	
	free(head->entry);
	free(head);
	return 0;
}

int RLIST_to_string(RouterNode* head, char* dest){
	char s[100];
	bzero(s, 100);
	
	if( head == NULL ){
		strcpy(dest, " ");
		return 0;
	}
	
	if( head->next == NULL ){
		RE_to_string( head->entry, s );
		strcat(dest, s);
		return 0;
	}
	
	if( head->entry->cost != -1 ){
		RE_to_string( head->entry, s );
		strcat(dest, s);
		//strcat(dest, "#");
	}
	
	RLIST_to_string( head->next, dest );
	return 0;
} 

RouterEntry* RLIST_get_entry( RouterNode** head, char id ){
	RouterNode* curr = *head;
	
	if( curr == NULL ){
		printf("ERROR: Head null in RLIST_get_entry\n");
		return NULL;
	}	
	
	while( curr != NULL ){
		if( curr->entry->dest == id ){
			return curr->entry;
		} 	
		curr = curr->next;
	}
	
	return NULL;
}

int RLIST_get_entry_set( RouterNode** head, fd_set* fds, char* recvd){
	RouterNode* curr = *head;
	int ctr = 0;
	if( head == NULL ){
		return 0;
	}
	
	while( curr != NULL ){
		if( FD_ISSET( curr->entry->recv_fd, &(*fds) )){
			recvd[ctr] = curr->entry->dest;
			ctr++;
		}	
		curr = curr->next;
	}
	
	return ctr;	
}

int RLIST_get_adj_nodes( RouterNode* head, char* recvd){
	RouterNode* curr = head;
	int ctr = 0;
	if( head == NULL ){
		return 0;
	}
	
	while( curr != NULL ){
		if( curr->entry->cost != -1 ){
			recvd[ctr] = curr->entry->dest;
			ctr++;
		}	
		curr = curr->next;
	}
	
	return ctr;	
}

int RLIST_write_except( RouterNode** head, char* msg, char me, char except, int len){
	RouterNode* curr = *head;
	
	if( head == NULL ){
		return 0;
	}
	
	while( curr != NULL ){
		if( curr->entry->dest != me && curr->entry->dest != except  ){
			if( curr->entry->out_fd > 0 ){				
				write( curr->entry->out_fd, msg, len );				
			}
		} 
		curr = curr->next;
	}
	
	return 0;	
}

int RLIST_print( RouterNode* head ){
	if( head == NULL ){
		printf(" DEST | COST | OUT  | IN   \n");
		printf("---------------------------\n");
		return 0;
	}
	
	if( head->next == NULL ){
		RLIST_print(NULL);
		printf(" %c    | %d    | %d | %d \n", 
			head->entry->dest,
			head->entry->cost,
			head->entry->port_out,
			head->entry->port_in);
		return 0;
	}
	
	RLIST_print( head->next );
	if( head->entry->cost == 0 ){
		printf(" %c    | %d    | %d    | %d \n", 
			head->entry->dest,
			head->entry->cost,
			head->entry->port_out,
			head->entry->port_in);			
	} else {
		printf(" %c    | %d    | %d | %d \n", 
			head->entry->dest,
			head->entry->cost,
			head->entry->port_out,
			head->entry->port_in);		
	}

	return 0;	
}

unsigned int randr(unsigned int min, unsigned int max){
       double scaled = (double)rand()/RAND_MAX;

       return (max - min +1)*scaled + min;
}

#endif
