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


#define MAXBUFLEN 256

LIST *MessageReceiveQueue;
LIST *MessageSendQueue;

pthread_t receiveMessage, sendMessage, readKeyboard, writeScreen;

pthread_mutex_t receiveLock;
pthread_mutex_t sendLock;
pthread_mutex_t emptyReceiveMutex;
pthread_mutex_t emptySendMutex;
pthread_cond_t emptyReceiveList;
pthread_cond_t emptySendList;

int MessageCounter = 0;

typedef struct sendParams {
    int socketID;
    struct sockaddr *addr;
    size_t addr_len;
} mystruct;

void printList(LIST *list) {
  Node *head = list->head;
  printf("List is:\n");
  while(head) {
    printf("%s\n", (char *)head->item);
    head = head->next;
  }
}

void *receiveUDP (void *arg) {
    socklen_t addrlen;
    struct sockaddr_storage their_addr;
    int *number;
    char message[MAXBUFLEN];
    int id= *(int *)arg;

    while (1) {
        recvfrom(id, message, MAXBUFLEN, 0,(struct sockaddr *)&their_addr, &addrlen);
        pthread_mutex_lock(&receiveLock);
        ListPrepend(MessageReceiveQueue, message);
        pthread_mutex_unlock(&receiveLock);
        if(ListCount(MessageReceiveQueue) == 1) {
            pthread_mutex_lock(&emptyReceiveMutex);
            pthread_cond_signal(&emptyReceiveList);
            pthread_mutex_unlock(&emptyReceiveMutex);
        }
        if (message[0] == '!') {
            break;
        }
    }
    sleep(0.5);
    pthread_exit(NULL);

}

void *sendUDP (void *arg) {
    mystruct *params = (mystruct *)arg;
    int numbytes;
    char *message;
    int exit = 0;
    while (1) {
        pthread_mutex_lock(&emptySendMutex);
        if (ListCount(MessageSendQueue) == 0) {
            pthread_cond_wait(&emptySendList, &emptySendMutex);
        }
        pthread_mutex_unlock(&emptySendMutex);
        pthread_mutex_lock(&sendLock);
        message = ListTrim(MessageSendQueue);
        if (*message == '!') {
            exit = 1;
        }
        MessageCounter--;
        pthread_mutex_unlock(&sendLock);
        numbytes = sendto(params->socketID,  message, MAXBUFLEN, 0, params->addr, params->addr_len);
        if (exit) {
            
            break;
        }
    }
    pthread_cancel(writeScreen);
    pthread_cancel(receiveMessage);   
    pthread_exit(NULL);

}

void *readMessage (void *arg) {
    char message[10][MAXBUFLEN];
    int i = 0;
    int exit = 0;
    while (1) {
        if (fgets(message[MessageCounter], MAXBUFLEN, stdin) == NULL) {
            continue;
        }
        if(message[MessageCounter][0] == '\n') {
            continue;
        }
        if(message[MessageCounter][0] == '!') {
            exit = 1;
        }
        pthread_mutex_lock(&sendLock);
        ListPrepend(MessageSendQueue, &message[MessageCounter++]);
        pthread_mutex_unlock(&sendLock);
        if (ListCount(MessageSendQueue) == 1) {
            pthread_mutex_lock(&emptySendMutex);
            pthread_cond_signal(&emptySendList);
            pthread_mutex_unlock(&emptySendMutex);
        }
        if (exit) {
            break;
        }
    }
    pthread_exit(NULL);
}

void *printMessage (void *arg) {
    char *message;
    while (1) {
      pthread_mutex_lock(&emptyReceiveMutex);
      if (ListCount(MessageReceiveQueue) == 0) {
          pthread_cond_wait(&emptyReceiveList, &emptyReceiveMutex);
      }
      pthread_mutex_unlock(&emptyReceiveMutex);
      pthread_mutex_lock(&receiveLock);
      message = ListTrim(MessageReceiveQueue);
      pthread_mutex_unlock(&receiveLock);
      if(message[0] == '!') {
        break;
      }
      fputs(message, stdout);
    }
    pthread_cancel(sendMessage);
    pthread_cancel(readKeyboard);
    pthread_exit(NULL);

}



int main(int argc, char *argv[])
{

    int thread1, thread2, thread3, thread4;

    fcntl(0, F_SETFL, O_NONBLOCK);  // set to non-blocking
    fcntl(1, F_SETFL, O_NONBLOCK);
    MessageSendQueue = ListCreate();
    MessageReceiveQueue = ListCreate();

    int sockfd;
    struct addrinfo hints, *servinfo, *p, *q;
    int rv;
    struct sockaddr_storage their_addr;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = AI_PASSIVE;

    if ((rv = getaddrinfo(argv[2], argv[3], &hints, &servinfo)) != 0) {
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

    if ((rv = getaddrinfo(NULL, argv[1], &hints, &servinfo)) != 0) {
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


    thread1 = pthread_create(&receiveMessage, NULL, receiveUDP, (void *)&sockfd);
    thread2 = pthread_create(&sendMessage, NULL, sendUDP, (void *)params);
    thread3 = pthread_create(&readKeyboard, NULL, readMessage, NULL);
    thread4 = pthread_create(&writeScreen, NULL, printMessage, NULL);

    pthread_join(receiveMessage, NULL);
    pthread_join(readKeyboard, NULL);
    pthread_join(writeScreen, NULL);
    pthread_join(sendMessage, NULL);

    printf("Thanks for chatting!\n");

    close(sockfd);

    return 0;
}
