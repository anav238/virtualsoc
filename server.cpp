#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <pthread.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "rapidjson/rapidjson.h"
#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/filereadstream.h"
#include "rapidjson/filewritestream.h"

#define PORT 2024
#define MAX_CLIENTS 500
#define MAX_UNAME 20
#define MAX_PASS 20
#define MAX_CMD 1200
#define MAX_MSG 1000 
#define MAX_RES 200
#define MAX_PROFILE 100000

using namespace rapidjson;

extern int errno;
int connectedClients;
int clients[MAX_CLIENTS];
char usernames[MAX_CLIENTS][MAX_UNAME];
pthread_mutex_t varLock, fileLock;

int checkCredentials(char* username, char* password) {
	char filename[MAX_UNAME + 10] = "users/";
	strcat(filename, username);
	strcat(filename, ".json");

	pthread_mutex_lock(&fileLock);
	FILE *fd = fopen(filename, "r");
	if (fd == NULL) 
		return 0;

	char rbuff[MAX_PROFILE];
	FileReadStream rs(fd, rbuff, sizeof(rbuff));
		
	Document doc;
	doc.ParseStream(rs);
	assert(doc.IsObject());
	assert(doc.HasMember("password"));

	assert(doc["password"].IsString());
	char actualPassword[MAX_PASS];
	strcpy(actualPassword, doc["password"].GetString());
	fclose(fd);
	pthread_mutex_unlock(&fileLock);
	if(strcmp(password, actualPassword) == 0) 
		return 1;
	return 0;
}

int userExists(char *username) {
	return 1;
}

int getOnlineUserId(char *username) {
	pthread_mutex_lock(&varLock);
	for (int i = 0; i < connectedClients; i++) 
		if (strcmp(usernames[i], username) == 0)
			return i;
	pthread_mutex_unlock(&varLock);
	return -1;
}

int viewProfile(char *username, char *requestFrom, char *profile) {
	char filename[MAX_UNAME + 10] = "users/";
	strcat(filename, author);
	strcat(filename, ".json");

	pthread_mutex_lock(&fileLock);
	FILE *fd = fopen(filename, "r");
	if (fd == NULL) 
		return 0;

	char rbuff[MAX_PROFILE];
	FileReadStream rs(fd, rbuff, sizeof(rbuff));
		
	Document doc;
	doc.ParseStream(rs);
	assert(doc.IsObject());
	assert(doc.HasMember("posts"));

	
	return 1;
}

int post(char *author, char *visibility, char *text) {
	char filename[MAX_UNAME + 10] = "users/";
	strcat(filename, author);
	strcat(filename, ".json");

	pthread_mutex_lock(&fileLock);
	FILE *fd = fopen(filename, "r");
	if (fd == NULL) 
		return 0;

	char rbuff[MAX_PROFILE];
	FileReadStream rs(fd, rbuff, sizeof(rbuff));
		
	Document doc;
	doc.ParseStream(rs);
	assert(doc.IsObject());
	assert(doc.HasMember("posts"));
	
	Document::AllocatorType& allocator = doc.GetAllocator();

	assert(doc["posts"].IsArray());
	Value postArray(kArrayType);

	Value visibilityValue;
	visibilityValue.SetString(visibility, strlen(visibility), allocator);
	Value textValue;
	textValue.SetString(text, strlen(text), allocator);
	
	postArray.PushBack(visibilityValue, allocator);
	postArray.PushBack(textValue, allocator);

	doc["posts"].PushBack(postArray, allocator);

	fclose(fd);

	fd = fopen(filename, "w");
	if (fd == NULL) 
		return 0;

	char wbuff[MAX_PROFILE];
	FileWriteStream ws(fd, wbuff, sizeof(wbuff));
	Writer<FileWriteStream> writer(ws);
	doc.Accept(writer);

	fclose(fd);
	pthread_mutex_unlock(&fileLock);
	return 1;
}

int sendOfflineMessage(char *sender, char *recipient, char *message) {
	char filename[MAX_UNAME + 10] = "users/";
	strcat(filename, recipient);
	strcat(filename, ".json");

	pthread_mutex_lock(&fileLock);
	FILE *fd = fopen(filename, "r");
	if (fd == NULL) 
		return 0;

	char rbuff[MAX_PROFILE];
	FileReadStream rs(fd, rbuff, sizeof(rbuff));
		
	Document doc;
	doc.ParseStream(rs);
	assert(doc.IsObject());
	assert(doc.HasMember("inbox"));
	
	Document::AllocatorType& allocator = doc.GetAllocator();

	assert(doc["inbox"].IsArray());
	Value messageArray(kArrayType);

	Value senderValue;
	senderValue.SetString(sender, strlen(sender), allocator);
	Value messageValue;
	messageValue.SetString(message, strlen(message), allocator);
	
	messageArray.PushBack(senderValue, allocator);
	messageArray.PushBack(messageValue, allocator);

	doc["inbox"].PushBack(messageArray, allocator);

	fclose(fd);

	fd = fopen(filename, "w");
	if (fd == NULL) 
		return 0;

	char wbuff[MAX_PROFILE];
	FileWriteStream ws(fd, wbuff, sizeof(wbuff));
	Writer<FileWriteStream> writer(ws);
	doc.Accept(writer);

	fclose(fd);
	pthread_mutex_unlock(&fileLock);
	return 1;
}

int sendMessage(char *sender, char *recipients, char *message) {
	char *currentRecipient = strtok(recipients, " "); 
	int succeses = 0;
	while (currentRecipient) {
		int recipientId = getOnlineUserId(currentRecipient);

		if (recipientId >= 0) {
			char toSend[MAX_MSG + 200] = "[Message from ";
			strcat(toSend, sender);
			strcat(toSend, "] ");
			strcat(toSend, message);
			if (write(clients[recipientId], toSend, strlen(toSend) + 1) <= 0)
				perror("[server]Eroare la scriere in client.\n");
			else
				succeses++;
		}
		else succeses += sendOfflineMessage(sender, currentRecipient, message);
		currentRecipient = strtok(NULL, " ");
	}
	return succeses;
}

int registerUser(char* username, char* password) {
	char filename[MAX_UNAME + 10] = "users/";
	strcat(filename, username);
	strcat(filename, ".json");

	pthread_mutex_lock(&fileLock);
	FILE *fd = fopen(filename, "r");
	if (fd != NULL) 
		return 0;

	fd = fopen("users/template.json","r");
	char rbuff[MAX_PROFILE];
	FileReadStream rs(fd, rbuff, sizeof(rbuff));
		
	Document doc;
	doc.ParseStream(rs);
	assert(doc.IsObject());

	Document::AllocatorType& allocator = doc.GetAllocator();
	doc["password"].SetString(password, allocator);
	fclose(fd);

	fd = fopen(filename, "w");

	char wbuff[MAX_PROFILE];
	FileWriteStream ws(fd, wbuff, sizeof(wbuff));
	Writer<FileWriteStream> writer(ws);
	doc.Accept(writer);

	fclose(fd);
	pthread_mutex_unlock(&fileLock);
	return 1;
}

int logout(char* username, int* loggedIn) {
	if (*loggedIn == 0)
		return 0;
	*loggedIn = 0;
	pthread_mutex_lock(&varLock);
	strcpy(username, "");
	pthread_mutex_unlock(&varLock);
	return 1;
}

int login(char* username, int* loggedIn, char* usernameTry, char* passwordTry) {
	if (!usernameTry || !passwordTry)
		return 0;
	if (*loggedIn == 1)
		return 0;
	if (checkCredentials(usernameTry, passwordTry)) {
		pthread_mutex_lock(&varLock);
		strcpy(username, usernameTry);
		pthread_mutex_unlock(&varLock);
		*loggedIn = 1;
		return 1;	
	} 
	return 0;
}

int checkPermissions(char* commandName, int* loggedIn) {
	if ((*loggedIn) == 0
		&& strcmp(commandName, "login") != 0 
		&& strcmp(commandName, "register") != 0
		&& strcmp(commandName, "profile") != 0) 
		return 0;
	return 1;
}

void processClientCommand(char* command, int *loggedIn, char* username, char* result) {
	char* commandName = strtok(command, " ");

	if (!checkPermissions(commandName, loggedIn)) {
		strcpy(result, "You don't have permission to do this right now.");
		return;
	}

	if (strcmp(commandName, "login") == 0) {
		char* usernameTry = strtok(NULL, " ");
		char* passwordTry = strtok(NULL, " ");
		if (login(username, loggedIn, usernameTry, passwordTry)) 
			strcpy(result, "Succesful login!");
		else 
			strcpy(result, "Login failed!");
	}

	else if (strcmp(commandName, "logout") == 0) {
		if (logout(username, loggedIn)) 
			strcpy(result, "Succesful logout!\n");
		else 
			strcpy(result, "You are already logged out!\n");
	}

	else if (strcmp(commandName, "register") == 0) {
		char* newUsername = strtok(NULL, " ");
		char* newPassword = strtok(NULL, " ");
		if (registerUser(newUsername, newPassword) == 1) 
			strcpy(result, "User created succesfully.\n");
		else 
			strcpy(result, "Username already exists.\n");
	}

	else if (strcmp(commandName, "message") == 0) {
		char* recipients = strtok(NULL, ":");
		char* message = strtok(NULL, "\n");
		sprintf(result, "%d message(s) sent.\n", sendMessage(username, recipients, message));
	}
	else if (strcmp(commandName, "post") == 0) {
		char* visibility = strtok(NULL, " ");
		char* message = strtok(NULL, "\n");
		if (post(username, visibility,  message)) 
			strcpy(result, "Post published succesfully.\n");
		else 
			strcpy(result, "Post could not be published.\n");
	}

	else 
		strcpy(result, "Command doesn't exist.");
	
}

void* processClientCommands(void *args) {
	int clientId = *((int*)args);
	int client = clients[clientId];
	int loggedIn = 0;

	strcpy(usernames[clientId], "");

	while(1) {
		char command[MAX_CMD];
		char result[MAX_RES];
		fflush(stdout);

		if (read(client, command, MAX_CMD) <= 0) {
			perror("[server]Eroare la citire de la client.\n");	
			break;		
		}

		if (strcmp(command, "quit") == 0)
			break;
		
		processClientCommand(command, &loggedIn, usernames[clientId], result);
		
		if (write(client, result, strlen(result) + 1) <= 0) {
			perror("[server]Eroare la scriere in client.\n");
			break;
		}
	}
	close(client);	
}

int main()
{
	struct sockaddr_in server;	
	struct sockaddr_in from;
	char command[100];		
	char msgrasp[100] = " ";        
	int sd;
	connectedClients = 0;			
	int clientId = -1;

	if ((sd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		perror("[server]Eroare la crearea socket-ului.\n");
		return errno;
	}

	bzero(&server, sizeof(server));
	bzero(&from, sizeof(from));

	server.sin_family = AF_INET;
	server.sin_addr.s_addr = htonl(INADDR_ANY);
	server.sin_port = htons(PORT);

	if (bind(sd, (struct sockaddr *) &server, sizeof(struct sockaddr)) == -1) {
		perror("[server]Eroare la apelul functiei bind.\n");
		return errno;
	}

	if (listen(sd, 1) == -1) {
		perror("[server]Eroare la apelul functiei listen.\n");
		return errno;
	}

	if (pthread_mutex_init(&varLock, NULL) != 0 || pthread_mutex_init(&fileLock, NULL) != 0) {
        perror("[server]Eroare la initializarea mutex");
        return errno;
    }

	while (1)	{
		int client;
		socklen_t length = sizeof(from);

		fflush(stdout);

		client = accept(sd, (struct sockaddr *) &from, &length);

		if (client < 0) {
			perror("[server]Eroare la conexiunea cu clientul.\n");
			continue;
		}

		connectedClients++, clientId++;
		clients[clientId] = client;
		pthread_t thread_id;
		pthread_create(&thread_id, NULL, processClientCommands, (void*)&clientId);
	}
	pthread_mutex_destroy(&fileLock);
	pthread_mutex_destroy(&varLock);
	return 0;
}