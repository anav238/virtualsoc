int sendOfflineMessage(char *sender, char *recipient, char *message) {
	char filename[MAX_UNAME + 10] = "users/";
	strcat(filename, recipient);
	strcat(filename, ".json");
	//Pentru a păstra consistența fișierelor și a evita scrierea simultană a unor date în același fișier de către thread-uri multiple, am folosit un mutex. 
	pthread_mutex_lock(&fileLock);
	FILE *fd = fopen(filename, "r");
	if (fd == NULL) 
		return 0;
	
	//MAX_PROFILE este o constantă ce reprezintă numărul maxim de caractere pe care le poate include un profil
	char rbuff[MAX_PROFILE];
	//pentru parsarea fișierului .json am utilizat biblioteca RapidJSON, care are nevoie de un obiect de tip FileReadStream și de un buffer pentru citirea din fișier
	FileReadStream rs(fd, rbuff, sizeof(rbuff));
		
	//Document este un tip de dată definit în RapidJSON ce reprezintă DOM-ul
	Document doc;
	//Funcția ParseStream aduce datele citite cu ajutorul FileReadStream în doc, oferind posibilitatea accesării și modificării lor cu ușurință
	doc.ParseStream(rs);
	//Assert-urile au rolul de a verifica că fișierele JSON sunt în formatul corect.
	assert(doc.IsObject());
	assert(doc.HasMember("inbox"));
	
	//Deoarece în această bibliotecă nu există conceptul de copiere, ci doar de mutare cu alocare și dealocare de memorie, vom avea nevoie de un obiect de tip alocator. Acesta este creat separat din motive de eficiență, pentru a nu fi nevoie să se creeze câte un obiect nou de tip Allocator de fiecare dată când se alocă memorie pentru un obiect.
	Document::AllocatorType& allocator = doc.GetAllocator();

	assert(doc["inbox"].IsArray());
	//Obiectele de tip Value reprezintă atributele din fișierul JSON
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
	//FileWriteStream este un stream folosit pentru scrierea în fișiere, care este folosit de un obiect de tip Writer ce poate fi acceptat de către document, pe care îl va prelua și îl va scrie în fișierul din FileWriteStream
	FileWriteStream ws(fd, wbuff, sizeof(wbuff));
	Writer<FileWriteStream> writer(ws);
	doc.Accept(writer);

	fclose(fd);
	pthread_mutex_unlock(&fileLock);
}

int sendMessage(char *sender, char *recipients, char *message) {
	//Lista persoanelor la care avem de trimis mesajul este transmisă ca parametru în recipients, sub formă de username-uri separate de spații
	char *currentRecipient = strtok(recipients, " "); 
	//În variabila succeses vom reține numărul de mesaje transmise cu succes
	int succeses = 0;
	while (currentRecipient) {
		//Funcția getOnlineUserId caută în vectorul global usernames index-ul utilizatorului cu username-ul currentRecipient. Dacă nu îl găsește, returnează -1.
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
