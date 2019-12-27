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
#define MAX_POST 300
#define MAX_PROFILE 100000

using namespace rapidjson;

extern int errno;
int connectedClients;
int clients[MAX_CLIENTS];
char usernames[MAX_CLIENTS][MAX_UNAME];
pthread_mutex_t varLock, fileLock;

char masterPassword[] = "admin";

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

void getRelationship(Document &doc, char *username, char *relationship) {
	Document::AllocatorType& allocator = doc.GetAllocator();
	assert(doc.HasMember("friends"));

	assert(doc["friends"].IsArray());
	for (SizeType i = 0; i < doc["friends"].Size(); i++) {
		if (strcmp(username, doc["friends"][i].GetString()) == 0) {
			strcpy(relationship, "friends");
			return;
		}
	}

	assert(doc["acquaintances"].IsArray());
	for (SizeType i = 0; i < doc["acquaintances"].Size(); i++) {
		if (strcmp(username, doc["acquaintances"][i].GetString()) == 0) {
			strcpy(relationship, "acquaintances");
			return;
		}
	}

	strcpy(relationship, "strangers");
}

int addToGroup(char *owner, char *userToAdd, char *group) {
	if (strcmp(group, "friends") != 0 && strcmp(group, "acquaintances") != 0)
		return 0;

	char filename[MAX_UNAME + 10];
	sprintf(filename, "users/%s.json", userToAdd);

	pthread_mutex_lock(&fileLock);
	FILE *fd = fopen(filename, "r");
	if (fd == NULL) 
		return 0;

	sprintf(filename, "users/%s.json", owner);
	if (fd == NULL) 
		return 0;

	char rbuff[MAX_PROFILE];
	FileReadStream rs(fd, rbuff, sizeof(rbuff));
		
	Document doc;
	doc.ParseStream(rs);
	assert(doc.IsObject());
	assert(doc.HasMember(group));
	
	Document::AllocatorType& allocator = doc.GetAllocator();

	assert(doc[group].IsArray());
	Value addedUser;
	addedUser.SetString(userToAdd, allocator);

	doc[group].PushBack(addedUser, allocator);

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

int viewProfile(char *username, char *requestFrom, char *profile) {
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
	Document::AllocatorType& allocator = doc.GetAllocator();

	char relationship[20];
	getRelationship(doc, requestFrom, relationship);
	assert(doc.HasMember("visibility"));
	assert(doc["visibility"].IsString());
	char profileVisibility[20];
	strcpy(profileVisibility, doc["visibility"].GetString());
	if (strcmp(profileVisibility, "friends") == 0 && strcmp(relationship, "friends") != 0)
		return 0;
	if (strcmp(profileVisibility, "acquaintances") == 0 && strcmp(relationship, "strangers") == 0)
		return 0;
	assert(doc.HasMember("posts"));

	assert(doc["posts"].IsArray());
	for (SizeType i = 0; i < doc["posts"].Size(); i++) {
		assert(doc["posts"][i].IsArray());
		char visibility[20], text[MAX_POST];
		strcpy(visibility, doc["posts"][i][0].GetString());
		strcpy(text, doc["posts"][i][1].GetString());

		if (strcmp(visibility, "public") == 0 || strcmp(relationship, "friends") == 0
			|| (strcmp(visibility, "acquaintances") == 0 && strcmp(relationship, "acquaintances") == 0)) {
			strcat(profile, text);
			strcat(profile, "\n");
		}
	}
	fclose(fd);
	pthread_mutex_unlock(&fileLock);
	return 1;
}

int post(char *author, char *visibility, char *text) {
	if (strcmp(visibility, "public") != 0 && strcmp(visibility, "friends") != 0
		&& strcmp(visibility, "acquaintances") != 0) 
		return 0;

	if (strlen(text) > MAX_POST)
		return 0;

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

void getOfflineInbox(char *username, char *result) {
	char filename[MAX_UNAME + 10] = "users/";
	strcat(filename, username);
	strcat(filename, ".json");

	pthread_mutex_lock(&fileLock);
	FILE *fd = fopen(filename, "r");
	if (fd == NULL) 
		return;

	char rbuff[MAX_PROFILE];
	FileReadStream rs(fd, rbuff, sizeof(rbuff));
		
	Document doc;
	doc.ParseStream(rs);
	assert(doc.IsObject());
	assert(doc.HasMember("inbox"));
	
	Document::AllocatorType& allocator = doc.GetAllocator();

	assert(doc["inbox"].IsArray());
	Value emptyArray(kArrayType);

	char inbox[MAX_MSG * 100];
	sprintf(inbox, "%d new messages while you were away.\n", (int)doc["inbox"].Size());
	for (SizeType i = 0; i < doc["inbox"].Size(); i++) {
		assert(doc["inbox"][i].IsArray());
		char sender[MAX_UNAME], message[MAX_MSG];
		strcpy(sender, doc["inbox"][i][0].GetString());
		strcpy(message, doc["inbox"][i][1].GetString());

		char newMessage[MAX_MSG + MAX_UNAME + 20];
		sprintf(newMessage, "Message from %s: %s\n", sender, message);
		strcat(inbox, newMessage);
	}
	strcat(result, inbox);
	doc["inbox"].SetArray();

	fclose(fd);

	fd = fopen(filename, "w");
	if (fd == NULL) 
		return;

	char wbuff[MAX_PROFILE];
	FileWriteStream ws(fd, wbuff, sizeof(wbuff));
	Writer<FileWriteStream> writer(ws);
	doc.Accept(writer);

	fclose(fd);
	pthread_mutex_unlock(&fileLock);
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

int registerUser(char* username, char* password, char* masterPasswordTry) {
	if (strlen(username) == 0)
		return -4;

	if (masterPasswordTry && strcmp(masterPasswordTry, masterPassword) != 0)
		return -3;

	if (strlen(username) > MAX_UNAME || strlen(password) > MAX_PASS) 
		return -2;

	char filename[MAX_UNAME + 10] = "users/";
	strcat(filename, username);
	strcat(filename, ".json");

	pthread_mutex_lock(&fileLock);
	FILE *fd = fopen(filename, "r");
	if (fd != NULL) 
		return -1;

	fd = fopen("users/template.json","r");
	char rbuff[MAX_PROFILE];
	FileReadStream rs(fd, rbuff, sizeof(rbuff));
		
	Document doc;
	doc.ParseStream(rs);
	assert(doc.IsObject());

	Document::AllocatorType& allocator = doc.GetAllocator();
	doc["password"].SetString(password, allocator);

	if (masterPasswordTry) 
		doc["userType"].SetString("admin");
	else 
		doc["userType"].SetString("regular");

	assert(doc["friends"].IsArray());
	Value usernameValue;
	usernameValue.SetString(username, strlen(username), allocator);
	doc["friends"].PushBack(usernameValue, allocator);

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
		if (!usernameTry) {
			strcpy(result, "Incorrect syntax!");
			return;
		}
		char* passwordTry = strtok(NULL, " ");
		if (!passwordTry) {
			strcpy(result, "Incorrect syntax!");
			return;
		}
		char* others = strtok(NULL, " ");
		if (others)
			strcpy(result, "Incorrect syntax!");
		else if (login(username, loggedIn, usernameTry, passwordTry)) {
			strcpy(result, "Succesful login!\n");
			getOfflineInbox(username, result);
		}
		else 
			strcpy(result, "Login failed!");
	}

	else if (strcmp(commandName, "logout") == 0) {
		char* others = strtok(NULL, " ");
		if (others)
			strcpy(result, "Incorrect syntax!");
		else if (logout(username, loggedIn)) 
			strcpy(result, "Succesful logout!\n");
		else 
			strcpy(result, "You are already logged out!\n");
	}

	else if (strcmp(commandName, "register") == 0) {
		char* newUsername = strtok(NULL, " ");
		if (!newUsername) {
			strcpy(result, "Incorrect syntax.");
			return;
		}
		char* newPassword = strtok(NULL, " ");
		if (!newPassword) {
			strcpy(result, "Incorrect syntax.");
			return;
		}
		char* masterPasswordTry = strtok(NULL, " ");
		char* others;
		if (masterPasswordTry)
			others = strtok(NULL, " ");
		if (others)
			strcpy(result, "Incorrect syntax.");
		else {
			int returned = registerUser(newUsername, newPassword, masterPasswordTry);
			if (returned == 1) 
				strcpy(result, "User created succesfully.\n");
			else if (returned == -1)
				strcpy(result, "Username already exists.");
			else if (returned == -2) 
				strcpy(result, "Too long username/password.");
			else if (returned == -3)
				strcpy(result, "Wrong master password.");
			else if (returned == -4) 
				strcpy(result, "You can't register another account while logged in.");
		}
	}

	else if (strcmp(commandName, "message") == 0) {
		char* recipients = strtok(NULL, ":");
		if (!recipients) {
			strcpy(result, "Incorrect syntax.");
			return;
		}
		char* message = strtok(NULL, "\n");
		if (!message) {
			strcpy(result, "You can't send an empty message.");
			return;
		}
		sprintf(result, "%d message(s) successfully sent.\n", sendMessage(username, recipients, message));
	}
	else if (strcmp(commandName, "post") == 0) {
		char* visibility = strtok(NULL, " ");
		if (!visibility) {
			strcpy(result, "Incorrect syntax.");
			return;
		}
		char* message = strtok(NULL, "\n");
		if (!message) {
			strcpy(result, "Empty post.");
			return;
		}
		if (post(username, visibility,  message)) 
			strcpy(result, "Post published succesfully.\n");
		else 
			strcpy(result, "Post could not be published.\n");
	}
	else if (strcmp(commandName, "profile") == 0) {
		char* user = strtok(NULL, " ");
		char profile[MAX_PROFILE];
		bzero(&profile, sizeof(profile));
		if (!user) { 
			if (!viewProfile(username, username, profile))
				strcpy(result, "Error viewing own profile.\n");
			else 
				strcpy(result, profile);
		}
		else {
			if(!viewProfile(user, username, profile))
				strcpy(result, "Access forbidden!\n");
			else 
				strcpy(result, profile);
		}
	}
	else if (strcmp(commandName, "addFriend") == 0) {
		char *userToAdd = strtok(NULL, " ");
		if (!userToAdd) {
			strcpy(result, "Incorrect syntax.\n");
			return;
		}
		if (!addToGroup(username, userToAdd, "friends")) 
			strcpy(result, "Could not add friend.\n");
		else 
			strcpy(result, "Friend succesfully added.\n");
	}
	else if (strcmp(commandName, "addAcquaintance") == 0) {
		char *userToAdd = strtok(NULL, " ");
		if (!userToAdd) {
			strcpy(result, "Incorrect syntax.\n");
			return;
		}
		if (!addToGroup(username,userToAdd, "acquaintances")) 
			strcpy(result, "Could not add acquaintance.\n");
		else 
			strcpy(result, "Acquaintance succesfully added.\n");
	}
	else 
		strcpy(result, "Command doesn't exist.\n");
}

void* processClientCommands(void *args) {
	int clientId = *((int*)args);
	int client = clients[clientId];
	int loggedIn = 0;

	bzero(&usernames[clientId], sizeof(usernames[clientId]));
	strcpy(usernames[clientId], "");

	while(1) {
		char command[MAX_CMD];
		char result[MAX_RES];
		fflush(stdout);

		bzero(&command, sizeof(command));
		bzero(&result, sizeof(result));

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
	strcpy(usernames[clientId], "");	
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