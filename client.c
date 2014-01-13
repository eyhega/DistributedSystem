/**
 * \file client.c
 * \author Antoine DUFAURE & Gaetan EYHERAMONO
 * */
#include "entete.h"

#include <stdio.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <pthread.h>
#include <string.h>


#define NB_CO_WORKER 2
#define BUFFER_SIZE 128

/**
 * Represent the different client state
 * */
typedef enum {
	OUTSIDE,	///< The client is not into the critical section
	INSIDE,		///< The client is into the critical section
	WAITING		///< The client is waiting for the token
} state_t;

/**
 * Represent the token state
 * */
typedef enum {
	PRESENT = 2,	///< The client has the token
	NON_PRESENT = 1,///< The client has not the token
	ERROR = 0		///< Error during the token transaction
} jeton_state_t;

/**
 * Represent the two kind of requests that a client is able to send to the other
 * */
typedef enum {
	REQ,	///< The client is requesting the token to perform job in critical section
	JETON	///< The client is sending the token to another client
} diffuse_t;


int socket_num;							///< The socket id (fd)
char buffer[BUFFER_SIZE];				///< The buffer to use for exchange messages via the socket
struct couple clients[NB_CO_WORKER+1];	///< All the clients data (other clients and me)
jeton_state_t jeton = ERROR;			///< The current token state
unsigned short V[NB_CO_WORKER+1];  	///< The local V table of the client
unsigned short LN[NB_CO_WORKER+1]; 	///< The token values (updated upon the token reception)
state_t status = OUTSIDE;				///< The current client status
pthread_t SCThread;					///< The thread to perform the critical section
pthread_mutex_t mutexV = PTHREAD_MUTEX_INITIALIZER;///< The mutex to lock V & LN ressources (between the current thread and the SCThread)
int isLogOn = 0;						///< Boolean to know if the program has to log or not

/**
 * \brief Test if two clients are equals
 * 
 * \param cl1 Pointer on the first client
 * \param cll2 Pointer on the second client
 * 
 * \return 1 if the clients are equal, 0 otherwise. 
 * */
int areClientsEqual(couple * cl1, couple * cl2)
{	
	return ( (strcmp(cl1->_name,cl2->_name) == 0) && (cl1->_port == cl2->_port) );
}

/**
 * \brief Get the specific index in local tables corresponding to the client cl
 * 
 * \param cl Pointer to the client to search
 * 
 * \return the client index
 * */
unsigned short getVIndex(couple * cl)
{
	unsigned short currentIndex = 0;
	
	while(currentIndex < NB_CO_WORKER+1 && !areClientsEqual(cl,clients+currentIndex)) { ++currentIndex; }
	
	return currentIndex;
}

/**
 * \brief Save the clients info into the local tables from the buffer
 * The data corresponding to "me" as a client, are added at the end of each tables
 * 
 * \param self pointer to self describing data
 * */
void saveClientsInfos(struct couple * self)
{
	int counter = 0;
	struct couple tmp;
	char * offset = buffer;
	
	sscanf(buffer,"%*c%s %u | ",tmp._name,&tmp._port);
	do
	{
		int cmp = strcmp(tmp._name,self->_name);
		if(cmp != 0 || (cmp == 0 && tmp._port != self->_port))
		{
			strcpy(clients[counter]._name,tmp._name);
			clients[counter++]._port = tmp._port;
		}
		while(*offset != '|') { ++offset; } // go on next '|'
		offset+=2; // go on first string char
	}while(sscanf(offset,"%s %d |",tmp._name,&tmp._port) == 2 && counter<NB_CO_WORKER);
	
	if(isLogOn)
	{
		printf("Les clients ont ete enregistres\n");
			
		for(counter = 0 ; counter < NB_CO_WORKER ; ++counter)
		{
			printf("Name %s\nPort %d\n==========\n",clients[counter]._name,clients[counter]._port);
		}
	}
	
	/* Add self at the array end */
	clients[NB_CO_WORKER]._port = self->_port;
	strcpy(clients[NB_CO_WORKER]._name,self->_name);
}

/**
 * \brief Asynchronize receive command.
 * Actually, we are waiting for data from server. This function is executed by a thread.
 * */
void* asyncRcv(void * arg)
{
	int recus;
	int lg_app;
	struct sockaddr_in appelant;
	
	if(isLogOn)
		printf("I am listening\n");
	
	lg_app=sizeof(struct sockaddr_in);
	recus=recvfrom(socket_num,buffer,sizeof(buffer),0,(struct sockaddr *) &appelant,&lg_app);
	if (recus <=0)
		printf("bug\n");
	else 
	{
		if(isLogOn)
			printf("Message : %s\n",buffer);
	}
	
	return NULL;
}

/**
 * \brief Create the socket with the specific port filled into the current couple.
 * 
 * \param current The current couple which contains the hostname and the port to communicate.
 * 
 * \return NULL if the binding fails, a pointer to a thread otherwise.
 * */
pthread_t * createSocket(struct couple * current)
{
	int s_ecoute;
	struct sockaddr_in adr;
	static pthread_t thread;

	s_ecoute=socket(AF_INET, SOCK_DGRAM,0);

	adr.sin_family=AF_INET;
	adr.sin_port=htons(current->_port);
	adr.sin_addr.s_addr=INADDR_ANY;

	if(bind(s_ecoute,(struct sockaddr *) &adr, sizeof (struct sockaddr_in)) !=0)
	{
	  return NULL;
	}

	socket_num = s_ecoute;
	pthread_create(&thread,NULL,asyncRcv,NULL);
	return &thread;
}

/**
 * \brief Send a message to a specific client.
 * The message has to be already write into the current buffer.
 * 
 * \param clientIndex The client index into the clients table which defines the message destination.
 * */
void sendMsgToClient(int clientIndex)
{
	int emission =0;
	struct sockaddr_in target;
	struct hostent *entree;
	
	target.sin_family=AF_INET;
	target.sin_port=htons(clients[clientIndex]._port);
	
	entree= (struct hostent *)gethostbyname(clients[clientIndex]._name);
	bcopy((char *) entree->h_addr, (char *)&target.sin_addr, entree->h_length);
	
	if(isLogOn)
		printf("Sending %s to client [ %s %d ]\n",buffer,clients[clientIndex]._name,clients[clientIndex]._port);
		
	emission = sendto(socket_num,buffer,sizeof(buffer),0,(struct sockaddr *)&target,sizeof(target));
	if(emission <= 0)
	{
		puts("Gros pblm");
	}
}

/**
 * \brief Inform all the other clients that the current client is requested the token.
 * 
 * \param type The request type to share
 * */
void diffuse(diffuse_t type)
{
	int index = 0;
	
	if(isLogOn)
		printf("Diffusing REQ\n");
		
	sprintf(buffer,"[ %s %d ] REQ %d %d",clients[NB_CO_WORKER]._name,clients[NB_CO_WORKER]._port,type,V[NB_CO_WORKER]);
	for( ; index < NB_CO_WORKER ; ++index)
	{
		sendMsgToClient(index);
	}
}

/**
 * \brief Send the token to a specific client.
 * 
 * \param clientIndex The client index into the clients table which defines the message destination.
 * */
void sendToken(int clientIndex)
{
	int currentIndex = 0;
	char tmpBuffer[BUFFER_SIZE];
	sprintf(buffer,"[ %s %d ] REQ %d",clients[NB_CO_WORKER]._name,clients[NB_CO_WORKER]._port,JETON);
	
	for( ; currentIndex < NB_CO_WORKER +1 ; ++currentIndex)
	{
		sprintf(tmpBuffer," { %s | %d %d }",clients[currentIndex]._name,clients[currentIndex]._port,LN[currentIndex]);
		strcat(buffer,tmpBuffer);
	}
	jeton = NON_PRESENT;
	
	if(isLogOn)
		printf("Sending token : %s",buffer);
		
	sendMsgToClient(clientIndex);
}

/**
 * \brief Job to do during the critical section.
 * This process is dedicated in a thread.
 * */
void* doJob(void* args)
{
	int tmpVect[NB_CO_WORKER+1];
	int index = 0,minIndex = 0, minValue=INT_MAX;
	
	++LN[NB_CO_WORKER]; // increase token value
	printf("==I AM IN THE CRITICAL SECTION==\n");
	sleep(10);
	printf("==I AM LEAVING THE CRITICAL SECTION==\n");
	
	
	 /* Exit SC */
	 status = OUTSIDE;
	 pthread_mutex_lock(&mutexV);
	 for(; index < NB_CO_WORKER ; ++index)
	 {
		 tmpVect[index] = V[index] - LN[index];
		 if(tmpVect[index] > 0) // someone is requesting SC
		 {
			if(LN[index] < minValue)
			{ 
			 minIndex = index;
			 minValue = LN[index];
			}
		 }
	 }
	 pthread_mutex_unlock(&mutexV);
	 if(minValue != INT_MAX) /* there is a diff between LN & V */
	 {
		 sendToken(minIndex);
	 }
}

/**
 * \brief Prepare the critical section process (create the thread)
 * */
void performSC()
{
	pthread_create(&SCThread,NULL,doJob,NULL);
}

/**
 * \brief Request the token to perform the critical section process.
 * If the client has already the token, he performs the SC.
 * If he has not, he diffuses his request to other clients.
 * */
void requestSC()
{
	if(status == OUTSIDE)
	{
		++V[NB_CO_WORKER]; /* increase my V number */
		if (jeton == PRESENT)
		{
			status = INSIDE;
			performSC();
		}
		else
		{
			/*Diffusion (req,Vi,Si) */
			diffuse(REQ);
			status = WAITING;
		}
	}
}

/**
 * \brief Handle the data coming from STDIN file descriptor.
 * 
 * \return 0 if the program still has to continue, 1 otherwise.
 * */
int processDataFromSTDIN()
{
	int retVal = 0;
	ssize_t size = 0;
	size = read(0,buffer,BUFFER_SIZE);
	buffer[size-1] = '\0'; // remove \n
	
	switch(buffer[0])
	{
		case 'e':
		case 'E':
			printf("Request SC\n");
			requestSC();
			break;
		
		case 'q':
		case 'Q':
			retVal = 1;
			break;
	}
	
	return retVal;
}
/**
 * \brief Extract the couple info from the current buffer.
 * 
 * \param offset The pointer to the string which contains data
 * \param dest Pointer to the couple to fill
 * */
void extractCoupleFromBuffer(char * offset,couple * dest)
{
	sscanf(offset,"[ %s %d ]",dest->_name,&(dest->_port));
}

/**
 * \brief Handle the critical section request coming from another client (through the network)
 * 
 * \param send The sender couple
 * \param offset Pointer to the current string which contains data
 * */
void processSCRequest(couple sender,char * offset)
{
	int vNumber,vIndex;
	while(*offset != ' ') ++offset; //go after REQ
	while(*offset != ' ') ++offset; //go after req type
	
	sscanf(offset," %d",&vNumber);
	
	vIndex = getVIndex(&sender);
	
	pthread_mutex_lock(&mutexV);
	++V[vIndex];
	pthread_mutex_unlock(&mutexV);
	if(status == OUTSIDE && jeton == PRESENT)
	{
		sendToken(vIndex);
	}
}

/**
 * \brief Handle a token reception.
 * If the client was waiting the token to perform the SC, he will go INSIDE the SC.
 * 
 * \param sender The sender data
 * \param offset Pointer to the string which contains preformated data
 * */
void processTokenReception(couple sender,char * bufferOffset)
{
	unsigned short tmpValue;
	couple tmpCouple;
	int currentIndex = 0,vIndex=0;
	char * offset = bufferOffset;
	char * endOffset = NULL;
	char tmpBuffer[BUFFER_SIZE];
	
	
	while(*offset != '{') ++offset;
	
	
	//get token values
	for(; currentIndex < NB_CO_WORKER+1 ; ++currentIndex)
	{
		endOffset = offset;
		while(*endOffset != '}') ++endOffset;
		strncpy(tmpBuffer,offset,endOffset-offset);
		
		sscanf(tmpBuffer,"{ %s | %d %u }",tmpCouple._name,&tmpCouple._port,&tmpValue);
		vIndex = getVIndex(&tmpCouple);
		LN[vIndex] = tmpValue; // update token
		++offset;
		while(*offset != '{' && *offset != '\0') ++offset;
	}
	puts("Token received");
	jeton = PRESENT;
	
	if(status == WAITING)
	{
		status = INSIDE;
		performSC();
	}
}

/**
 * \brief Handle the data received on the socket.
 * 
 * \return 0 if the program still has to continue, 1 otherwise.
 * */
int processDataFromSocket()
{
	int retVal = 0;
	char * offset = buffer;
	couple sender;
	ssize_t size = 0;
	diffuse_t req_type;
	size = read(socket_num,buffer,BUFFER_SIZE);
	
	if(isLogOn)
		printf("Received message : %s\n",buffer);
	
	extractCoupleFromBuffer(buffer,&sender);
	while(*offset != ']') ++offset; // positionning cursor
	offset+=2; //go to first char on REQ
	
	sscanf(offset,"REQ %d",&req_type);
	if(req_type == REQ)
	{
		processSCRequest(sender,offset);
	}
	else if(req_type == JETON)
	{
		processTokenReception(sender,offset);
	}
	else
	{
		puts("Suspect received message");
		retVal = 1;
	}
	
	return retVal;
}

/**
 * \brief Handle data from the socket or STDIN.
 * 
 * \param fds The file descriptors to inspect.
 * 
 * \return 0 if the program still has to continue, 0 otherwise.
 * */
int processData(fd_set * fds)
{
	int retValue = 0;
	
	if(FD_ISSET(socket_num,fds))
	{
		retValue = processDataFromSocket();
	}
	
	if(FD_ISSET(0,fds))
	{
		retValue = processDataFromSTDIN();
	}
	
	return retValue;
}

/**
 * \brief Launch the system between clients and begin the Suzuki/Kasami algorithm.
 * 
 * \param self Data about "me" (as a client).
 * */
void launchSystem(couple * self)
{
	int quit = 0,retval = -1;
	
	/* initialize V & LN */
	memset(V,0,(NB_CO_WORKER+1)*sizeof(unsigned short));
	memset(LN,0,(NB_CO_WORKER+1)*sizeof(unsigned short));
	printf("System is launched.\n");
	
	while(!quit)
	{
		fd_set fds; // file descriptors to be listen by the select
		struct timeval tv;
		tv.tv_sec = 5;/* 5 scd time out */
		tv.tv_usec = 0;
		
		FD_ZERO(&fds);
		FD_SET(socket_num,&fds);
		FD_SET(0,&fds);
		
		retval = select(socket_num+1,&fds,NULL,NULL,&tv);

		if (retval == -1)
		   perror("select()");
		else if (retval)
		{
			if(isLogOn)
				printf("Data is available now.\n");
				
			quit = processData(&fds);
		}
		else
		{
			if(isLogOn)
			{
				printf("No data within five seconds.\n\n");

				int count = 0;

				for(; count < NB_CO_WORKER + 1 ; ++count)
				{
				   printf("[ %s %d ] %d\n",clients[count]._name,clients[count]._port,V[count]);
				}
				printf("\n");
				if(jeton == PRESENT)
				{
					for(count = 0; count < NB_CO_WORKER + 1 ; ++count)
					{
					   printf("Token [ %s %d ] %d\n",clients[count]._name,clients[count]._port,LN[count]);
					}   
				}
				printf("\n");
			}
		}
	}
	
	close(socket_num);
}

void handleCommand(int nbParameter,char ** argv)
{
	if(nbParameter > 1)
	{
		int index =1;
		while(index < nbParameter)
		{
			if(strcmp(argv[index],"-log") == 0)
			{
				isLogOn = 1;
			}
			++index;
		}
	}
}


int main(int argc, char ** argv)
{
	char x;
	struct couple don;
	int m,nbTry = 0;
	pthread_t * thread = NULL;
	
	handleCommand(argc,argv);
	
	printf("Please, fill the gap:\n");
	gethostname(don._name,BUFF_SIZE);
	printf("Name : %s\n",don._name);
	
	do
	{
		if(nbTry) puts("Can not create the socket on this port.");
		printf("Port number: ");
		scanf("%u%*c",&don._port);
		thread = createSocket(&don);
		++nbTry;
	}while(thread == NULL);
	
	
	m=callrpc("localhost", ARITH_PROG, ARITH_VERS1, REGISTER_PROC,
		xdr_couple,&don,xdr_char,&x);
		
	if (m==0)
	{
		jeton = x-48;
		if(jeton == PRESENT)
			puts("===I have the token===");
	}
	else
	{
		printf("pb\n");
	}
	
	pthread_join(*thread,NULL);
	saveClientsInfos(&don);
	
	launchSystem(&don);
	
	return 0;
}
