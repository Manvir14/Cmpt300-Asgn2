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
        if (message[0] == '!' && strlen(message) == 2) {
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
        if (*message == '!' && strlen(message) == 2) {
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
        // Used fgets because stdin is non-blocking now
        if (fgets(message[MessageCounter], MAXBUFLEN, stdin) == NULL) {
            continue;
        }
        if(message[MessageCounter][0] == '\n') {
            continue;
        }
        if(message[MessageCounter][0] == '!' && strlen(message[MessageCounter]) == 2) {
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
    char remote[4] = "R: ";
    while (1) {
      pthread_mutex_lock(&emptyReceiveMutex);
      if (ListCount(MessageReceiveQueue) == 0) {
          pthread_cond_wait(&emptyReceiveList, &emptyReceiveMutex);
      }
      pthread_mutex_unlock(&emptyReceiveMutex);
      pthread_mutex_lock(&receiveLock);
      message = ListTrim(MessageReceiveQueue);
      pthread_mutex_unlock(&receiveLock);
      if(message[0] == '!' && strlen(message) == 2) {
        break;
      }
      fputs(remote, stdout);
      fputs(message, stdout);
    }
    pthread_cancel(sendMessage);
    pthread_cancel(readKeyboard);
    pthread_exit(NULL);

}



int main(int argc, char *argv[])
{

    int thread1, thread2, thread3, thread4;

    fcntl(0, F_SETFL, O_NONBLOCK);  // set stdin and stdout to non-blocking
    fcntl(1, F_SETFL, O_NONBLOCK);

    MessageSendQueue = ListCreate();
    MessageReceiveQueue = ListCreate();

    int sockfd;
    struct addrinfo hints, *servinfo, *local, *remote;
    int rv;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = AI_PASSIVE;

    if ((rv = getaddrinfo(argv[2], argv[3], &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    for(remote = servinfo; remote != NULL; remote = remote->ai_next) {
        if ((sockfd = socket(remote->ai_family, remote->ai_socktype,
                remote->ai_protocol)) == -1) {
            perror("Error creating socket");
            continue;
        }
        break;
    }

    if ((rv = getaddrinfo(NULL, argv[1], &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    for(local = servinfo; local != NULL; local = local->ai_next) {
         if (bind(sockfd, local->ai_addr, local->ai_addrlen) == -1) {
            close(sockfd);
            perror("Error binding socket");
            continue;
        }
        break;

    }


    if (local == NULL) {
        fprintf(stderr, "Couldn't bind socket\n");
        return 2;
    }

    freeaddrinfo(servinfo);

    mystruct *params = malloc(sizeof(mystruct));
    params->socketID = sockfd;
    params->addr = remote->ai_addr;
    params->addr_len = remote->ai_addrlen;

    printf("Welcome to s-talk\n");
    printf("To close the chat, enter '!'\n");
    printf("Message beginning with R: is from remote host\n");
    printf("Begin chatting by typing a message:\n");
    thread1 = pthread_create(&receiveMessage, NULL, receiveUDP, (void *)&sockfd);
    thread2 = pthread_create(&sendMessage, NULL, sendUDP, (void *)params);
    thread3 = pthread_create(&readKeyboard, NULL, readMessage, NULL);
    thread4 = pthread_create(&writeScreen, NULL, printMessage, NULL);



    pthread_join(receiveMessage, NULL);
    pthread_join(readKeyboard, NULL);
    pthread_join(writeScreen, NULL);
    pthread_join(sendMessage, NULL);

    printf("Terminating chat\n");

    close(sockfd);

    return 0;
}
