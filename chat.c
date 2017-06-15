#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <pthread.h>
#include "list.h"
#include <fcntl.h>


#define MAXBUFLEN 100

LIST *MessageReceiveQueue;
LIST *MessageSendQueue;

pthread_mutex_t receiveLock;
pthread_mutex_t sendLock;
pthread_mutex_t emptyReceiveMutex;
pthread_mutex_t emptySendMutex;
pthread_cond_t emptyReceiveList;
pthread_cond_t emptySendList;

typedef struct sendParams {
    int socketID;
    struct sockaddr *addr;
    size_t addr_len;
} mystruct;

void printList(LIST *list) {
  Node *head = list->head;
  while(head) {
    printf("%s\n", (char *)head->item);
    head = head->next;
  }
}

void *receiveUDP (void *arg) {
    socklen_t addrlen;
    struct sockaddr_storage their_addr;
    int *number;
    char message[100];
    int id= *(int *)arg;

    while (1) {
        printf("Waiting to receive message\n");
        recvfrom(id, message, 100, 0,(struct sockaddr *)&their_addr, &addrlen);
        printf("Received message to print\n");
        pthread_mutex_lock(&receiveLock);
        ListPrepend(MessageReceiveQueue, &message);
        pthread_mutex_unlock(&receiveLock);
        pthread_mutex_lock(&emptyReceiveMutex);
        pthread_cond_signal(&emptyReceiveList);
        pthread_mutex_unlock(&emptyReceiveMutex);
    }
    pthread_exit(NULL);

}

void *sendUDP (void *arg) {
    mystruct *params = (mystruct *)arg;
    int numbytes;
    char *message;
    while (1) {
        pthread_mutex_lock(&emptySendMutex);
        if (ListCount(MessageSendQueue) == 0) {
            printf("Waiting on message to send\n");
            pthread_cond_wait(&emptySendList, &emptySendMutex);
        }
        pthread_mutex_unlock(&emptySendMutex);
        printf("Received message to send\n");
        pthread_mutex_lock(&sendLock);
        message = ListTrim(MessageSendQueue);
        pthread_mutex_unlock(&sendLock);
        numbytes = sendto(params->socketID,  message, 100, 0, params->addr, params->addr_len);
        printf("Sent message\n");
    }
    pthread_exit(NULL);

}

void *readMessage (void *arg) {
    char message[100];
    int i = 0;
    while (1) {
      //printf("Waiting on input\n");
      if (fgets(message, 100, stdin) == NULL) {
        continue;
      }
      if(message[0] == '\n') {
        continue;
      }
      pthread_mutex_lock(&sendLock);
      ListPrepend(MessageSendQueue, &message);
      printf("Put message on send queue\n");
      pthread_mutex_unlock(&sendLock);
      pthread_mutex_lock(&emptySendMutex);
      pthread_cond_signal(&emptySendList);
      pthread_mutex_unlock(&emptySendMutex);
    }
    pthread_exit(NULL);
}

void *printMessage (void *arg) {
    char *message;
    int j = 0;
    while (1) {
      pthread_mutex_lock(&emptyReceiveMutex);
      if (ListCount(MessageReceiveQueue) == 0) {
          printf("Waiting on message to print\n");
          pthread_cond_wait(&emptyReceiveList, &emptyReceiveMutex);
      }
      pthread_mutex_unlock(&emptyReceiveMutex);
      printf("Recevied message to print\n");
      pthread_mutex_lock(&receiveLock);
      message = ListTrim(MessageReceiveQueue);
      pthread_mutex_unlock(&receiveLock);
      fputs(message, stdout);
    }

    pthread_exit(NULL);

}



int main(int argc, char *argv[])
{

    pthread_t recvfrom, sendto, read, write;
    int thread1, thread2, thread3, thread4;

    fcntl(0, F_SETFL, O_NONBLOCK);  // set to non-blocking
    fcntl(1, F_SETFL, O_NONBLOCK);
    MessageSendQueue = ListCreate();
    MessageReceiveQueue = ListCreate();

    int sockfd;
    struct addrinfo hints, *servinfo, *p, *q;
    int rv;
    struct sockaddr_storage their_addr;
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC; // set to AF_INET to force IPv4
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = AI_PASSIVE; // use my IP

    if ((rv = getaddrinfo(argv[1], argv[2], &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    for(q = servinfo; q != NULL; q = q->ai_next) {
        if ((sockfd = socket(q->ai_family, q->ai_socktype,
                q->ai_protocol)) == -1) {
            perror("listener: socket");
            continue;
        }
        break;
    }

    if ((rv = getaddrinfo(NULL, argv[3], &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    for(p = servinfo; p != NULL; p = p->ai_next) {
         if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("listener: bind");
            continue;
        }
        break;

    }


    if (p == NULL) {
        fprintf(stderr, "listener: failed to bind socket\n");
        return 2;
    }

    freeaddrinfo(servinfo);

    mystruct *params = malloc(sizeof(mystruct));
    params->socketID = sockfd;
    params->addr = q->ai_addr;
    params->addr_len = q->ai_addrlen;


    thread1 = pthread_create(&recvfrom, NULL, receiveUDP, (void *)&sockfd);
    thread2 = pthread_create(&sendto, NULL, sendUDP, (void *)params);
    thread3 = pthread_create(&read, NULL, readMessage, NULL);
    thread4 = pthread_create(&write, NULL, printMessage, NULL);

    pthread_join(recvfrom, NULL);
    pthread_join(sendto, NULL);
    pthread_join(read, NULL);
    pthread_join(write, NULL);


    close(sockfd);

    return 0;
}
