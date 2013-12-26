DistributedSystem
=================

To compile client :
gcc client.c entete.c -o client -lphtread

To compile server (if tirpc is installed) :
gcc serveur.c entete.c -o serveur -lpthread -lm -l tirpc
