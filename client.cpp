#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <pthread.h>
#include <string.h>
#define MAX_CMD 1200
#define MAX_RES 100
#define MAX_MSG 1000

extern int errno;
int sd;			
struct sockaddr_in server;	
int port;
pthread_mutex_t lock;
int ended = 0;
char inbox[MAX_MSG] = "";

void* getServerMessages(void* args) {
    while (!ended) {
        char message[MAX_MSG] = "";
        read(sd, message, MAX_MSG);
        if (strlen(message)) {
            if (message[0] == '[') 
                strcpy(inbox, message);
            else {
                printf("%s\n", message);
                if (strlen(inbox)) {
                    printf("%s\n", inbox);
                    strcpy(inbox, "");
                }
            }
        }
    }
}

void printHelp() {
    printf("Aici va fi meniul de help.\n");
}

void processCommands() {
    while (1) {
        char full_command[MAX_CMD];
        bzero(full_command, MAX_CMD);
        fgets(full_command, MAX_CMD, stdin);

        char *command = strtok(full_command, "\n");
        if (!command) {
            if (strlen(inbox)) {
                printf("%s\n", inbox);
                strcpy(inbox, "");
            }
            continue;
        }
        if (strcmp(command, "quit") == 0) 
            break;

        if (write (sd, command, strlen(full_command) + 1) <= 0) 
            perror ("[client]Eroare la write() spre server.\n");

    }
}

int main (int argc, char *argv[])
{
    if (argc != 3) {
        printf ("Sintaxa: %s <adresa_server> <port>\n", argv[0]);
        return -1;
    }

    port = atoi (argv[2]);

    if ((sd = socket (AF_INET, SOCK_STREAM, 0)) == -1) {
        perror ("Eroare la socket().\n");
        return errno;
    }

    server.sin_family = AF_INET;
    server.sin_addr.s_addr = inet_addr(argv[1]);
    server.sin_port = htons (port);
  
    if (connect (sd, (struct sockaddr *) &server,sizeof (struct sockaddr)) == -1) {
        perror ("[client]Eroare la connect().\n");
        return errno;
    }
  
    if (pthread_mutex_init(&lock, NULL) != 0) { 
        perror("[client]Eroare la crearea mutex!");
        return errno; 
    } 

    printf ("Bine ai venit pe Instabook! Pentru ajutor, tasteaza comanda help.\n ");
    
    pthread_t thread_id;
	pthread_create(&thread_id, NULL, getServerMessages, NULL);
    processCommands();
    
    pthread_mutex_destroy(&lock);
    close (sd);
    return 0;
}