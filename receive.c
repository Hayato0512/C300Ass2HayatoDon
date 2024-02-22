#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include "boss.h"
#include "list.h"
#include "receive.h"

static List* list_for_in;
//Synchronization
static pthread_t receive_thread;
static pthread_cond_t condVar_in = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t mutex_in = PTHREAD_MUTEX_INITIALIZER;

void* Receiver(void* unused) {
	int socketDescriptor = Manager_Getsocket();

	while(1) {
		struct sockaddr_in sin_Remote_addr; 
		unsigned int sin_length = sizeof(sin_Remote_addr);
		char buffer[1024];
		int bytes_rx = recvfrom(socketDescriptor, buffer, 1024, 0, (struct sockaddr*) &sin_Remote_addr, &sin_length);
		int terminater = (bytes_rx < 1024)?bytes_rx:1024 - 1;
		buffer[terminater] = '\0';
		int receiveLength = strlen(buffer);

		while(1) {
			char* msg_Rx = (char*)malloc(receiveLength);
			strcpy(msg_Rx, buffer);

			if (Manager_append(list_for_in, msg_Rx) == -1) {
				printf("Error: Message could't be added to the list\n");
				free(msg_Rx);
				Manager_wait();
			}else {
				pthread_mutex_lock(&mutex_in);
				{
					pthread_cond_signal(&condVar_in);
				}
				pthread_mutex_unlock(&mutex_in);
				break;
			}
		}
	}
	return NULL;
}

void Receiver_init(List* list) {
	list_for_in = list;
	pthread_create(&receive_thread, NULL, Receiver, NULL);
}

void Receiver_signal(void){
	pthread_mutex_lock(&mutex_in); 
	{
		pthread_cond_wait(&condVar_in, &mutex_in);
	}
	pthread_mutex_unlock(&mutex_in);
}

void Receiver_free(void* msg) {
	if (msg) {
		free(msg);
	}
}

void Receiver_shutdown(void) {
	List_free(list_for_in, &Receiver_free);
	pthread_cancel(receive_thread);
	pthread_join(receive_thread, NULL);
	pthread_mutex_destroy(&mutex_in);
	pthread_cond_destroy(&condVar_in);

}