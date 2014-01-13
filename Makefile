COMPILER=gcc
flags=-lpthread
RPC=-l tirpc

all: client serveur

client:entete.o client.o
	$(COMPILER) client.o entete.o -o client $(flags)

serveur: serveur.o entete.o
	$(COMPILER) serveur.o entete.o -o serveur $(flags) $(RPC)

entete.o: entete.h entete.c
	$(COMPILER) entete.c -c

client.o: entete.c client.c
	$(COMPILER) client.c -c
	
serveur.o: entete.c serveur.c
	$(COMPILER) serveur.c -c

clean:
	rm client serveur *.o
	

