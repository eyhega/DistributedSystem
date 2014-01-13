/**
 * \file serveur.c
 * \author Antoine DUFAURE & Gaetan EYHERAMONO
 * */
#include <stdio.h>
#include <string.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/socket.h>
#include <pthread.h>
#include <math.h>
#include "entete.h"
#define NB_CLIENT 3

struct couple data_client[NB_CLIENT];	///< clients infos
unsigned short nbClientRequest = 0;	///< number of clients connected

/**
 * \brief Format the data into a buffer.
 * 
 * \param buff The buffer to write into.
 * */
void prepareData(char * buff)
{
	int i = 0;
	for(i ; i < NB_CLIENT ; ++i) 
	{
		sprintf(buff + strlen(buff),"%s %u | ",data_client[i]._name,data_client[i]._port);
	}
}

/**
 * \brief Send the clients informations to all clients.
 * This code is dedicated for a thread.
 * */
void sendInfosClient()
{
	int s_com,emis;
	char mes[100];
	struct sockaddr_in adr;
	struct hostent *entree;
	
	prepareData(mes);

	int i = 0;
	for(i ;  i < NB_CLIENT ; ++i)
	{
		s_com=socket(AF_INET, SOCK_DGRAM,0);
		printf("La socket est cree\n");

		adr.sin_family=AF_INET;
		adr.sin_port=htons(data_client[i]._port);

		entree= (struct hostent *)gethostbyname(data_client[i]._name);
		bcopy((char *)entree->h_addr,(char *)&adr.sin_addr,entree->h_length);

		printf("Envoie du message sur %s port num %u\n",data_client[i]._name,data_client[i]._port);
		emis=sendto(s_com,mes,sizeof(mes),0,(struct sockaddr *)&adr,sizeof(adr));

		if (emis <=0)
		{
			printf("Gros probleme\n");
		}
		
		close(s_com);
	}
	exit(0);
}

int main()
{
	int rep;
	rep = registerrpc(ARITH_PROG, ARITH_VERS1, REGISTER_PROC,
			registerClient,xdr_couple,xdr_char);

	if (rep==-1)
	{ 
		printf("Erreur d'enregistrement\n");
		exit(2); 
	}
	
	svc_run();
}

/**
 * \brief RPC method which permits to register a new client.
 * 
 * \param p Pointer to the new client.
 * 
 * \return '2' if it's the first client (then we give him the token), '1' otherwise.
 * */
char * registerClient(struct couple * p)
{
	static char res;
	static pthread_t thread;
	strncpy(data_client[nbClientRequest]._name,p->_name,BUFF_SIZE);
	data_client[nbClientRequest]._port = p->_port;
	
	if(nbClientRequest == 0)
	{
		res = '2';
	}
	else
	{
		res = '1';
	}
	++nbClientRequest;
	printf("Nouveau client : %s\n",p->_name);
	
	if(nbClientRequest == NB_CLIENT)
	{
		//launch thread for sending infos clients
		pthread_create(&thread,NULL,sendInfosClient,NULL);
	}
	
	return &res;
}
