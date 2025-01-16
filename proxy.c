#include  <stdio.h>
#include  <stdlib.h>
#include  <sys/socket.h>
#include  <netdb.h>
#include  <string.h>
#include  <unistd.h>
#include  <stdbool.h>
#include "./simpleSocketAPI.h"


#define SERVADDR "127.0.0.1"        // Définition de l'adresse IP d'écoute
#define SERVPORT "0"                // Définition du port d'écoute, si 0 port choisi dynamiquement
#define LISTENLEN 1                 // Taille de la file des demandes de connexion
#define MAXBUFFERLEN 1024           // Taille du tampon pour les échanges de données
#define MAXHOSTLEN 64               // Taille d'un nom de machine
#define MAXPORTLEN 64               // Taille d'un numéro de port


void readProcess(int ecode, int descSockCOM, char* buffer );
void writeProcess(int ecode, int descSockCOM, char* buffer );

int main(){
    int ecode;                       // Code retour des fonctions
    char serverAddr[MAXHOSTLEN];     // Adresse du serveur
    char serverPort[MAXPORTLEN];     // Port du server
    int descSockRDV;                 // Descripteur de socket de rendez-vous
    int descSockCOM;                 // Descripteur de socket de communication
    int descSockCTRLserv;            // Descripteur pour s'assurer de la bonne connexion
    int descSockTEMPclient;              // Descripteur pour la connexion spécialement créer pour LS côté client
    int descSockTEMPserv;              // Descripteur pour la connexion spécialement créer pour LS côté serveur
    struct addrinfo hints;           // Contrôle la fonction getaddrinfo
    struct addrinfo *res;            // Contient le résultat de la fonction getaddrinfo
    struct sockaddr_storage myinfo;  // Informations sur la connexion de RDV
    struct sockaddr_storage from;    // Informations sur le client connecté
    socklen_t len;                   // Variable utilisée pour stocker les 
				                     // longueurs des structures de socket
    char buffer[MAXBUFFERLEN];       // Tampon de communication entre le client et le serveur
    
    // Initialisation de la socket de RDV IPv4/TCP
    descSockRDV = socket(AF_INET, SOCK_STREAM, 0);
    if (descSockRDV == -1) {
         perror("Erreur création socket RDV\n");
         exit(2);
    }
    // Publication de la socket au niveau du système
    // Assignation d'une adresse IP et un numéro de port
    // Mise à zéro de hints
    memset(&hints, 0, sizeof(hints));
    // Initialisation de hints
    hints.ai_flags = AI_PASSIVE;      // mode serveur, nous allons utiliser la fonction bind
    hints.ai_socktype = SOCK_STREAM;  // TCP
    hints.ai_family = AF_UNSPEC;      

     // Récupération des informations du serveur
     ecode = getaddrinfo(SERVADDR, SERVPORT, &hints, &res);
     if (ecode) {
         fprintf(stderr,"getaddrinfo: %s\n", gai_strerror(ecode));
         exit(1);
     }
     // Publication de la socket
     ecode = bind(descSockRDV, res->ai_addr, res->ai_addrlen);
     if (ecode == -1) {
         perror("Erreur liaison de la socket de RDV");
         exit(3);
     }
     // Nous n'avons plus besoin de cette liste chainée addrinfo
     freeaddrinfo(res);

     // Récuppération du nom de la machine et du numéro de port pour affichage à l'écran
     len=sizeof(struct sockaddr_storage);
     ecode=getsockname(descSockRDV, (struct sockaddr *) &myinfo, &len);
     if (ecode == -1)
     {
         perror("SERVEUR: getsockname");
         exit(4);
     }
     ecode = getnameinfo((struct sockaddr*)&myinfo, sizeof(myinfo), serverAddr,MAXHOSTLEN, 
                         serverPort, MAXPORTLEN, NI_NUMERICHOST | NI_NUMERICSERV);
     if (ecode != 0) {
             fprintf(stderr, "error in getnameinfo: %s\n", gai_strerror(ecode));
             exit(4);
     }
     printf("L'adresse d'ecoute est: %s\n", serverAddr);
     printf("Le port d'ecoute est: %s\n", serverPort);

     // Definition de la taille du tampon contenant les demandes de connexion
     ecode = listen(descSockRDV, LISTENLEN);
     if (ecode == -1) {
         perror("Erreur initialisation buffer d'écoute");
         exit(5);
     }

	len = sizeof(struct sockaddr_storage);

    char Ending[3];
    Ending[0] = '\r';
    Ending[1] = '\n';
    Ending[2] = '\0';

    while(true){
        // Attente connexion du client
        // Lorsque demande de connexion, creation d'une socket de communication avec le client
        descSockCOM = accept(descSockRDV, (struct sockaddr *) &from, &len);
        if (descSockCOM == -1){
            perror("Erreur accept\n");
            exit(6);
        }
        // Echange de données avec le client connecté

        switch (fork()){
            case -1: 
                perror("erreur création fils");
                exit(8);
                break;
            
            case 0:{
                strcpy(buffer, "220 Connecté au proxy, entrez votre username@server\n");
                writeProcess(ecode,descSockCOM, buffer);
                
                //Formatage de la commande USER
                readProcess(ecode,descSockCOM,buffer);
                char login[MAXHOSTLEN], server[MAXHOSTLEN];
                char commandeUser[5]; // récupère la chaine USER
                char data[31]; //Contient anonymous@NomHôte
                sscanf(buffer, "%s %s", commandeUser, data);
                sscanf(data, "%[^@]@%[^\n]", login, server);
                printf("Login : %s, Server name : %s\n",login,server);

                //Connexion au serveur FTP
                //On crée un socket vers le serveur pour recevoir ses informations
                //On se connecte sur le Port 21 car c'est le port réservé à la connexion de contrôle
                char port21[5];
                strcpy(port21, "21");
                ecode = connect2Server(server,port21,&descSockCTRLserv);
                if (ecode == -1) {perror("Problème de connexion au serveur ftp\n");exit(6);}
                printf("Connexion établie\n");
                bzero(buffer, MAXBUFFERLEN);
                readProcess(ecode, descSockCTRLserv, buffer);
                 if(ecode == -1) {
                    perror("Problème pour lire les réponses du serveur.\n");
                    exit(3);
                }

                //On envoie le login de l'utilisateur
                char requeteLogin[MAXHOSTLEN];
                bzero(requeteLogin, MAXHOSTLEN-1);
                strncat(requeteLogin, commandeUser, strlen(commandeUser));
                strncat(requeteLogin," ", 2);
                strncat(requeteLogin, login, strlen(login));
                strncat(requeteLogin,Ending,strlen(Ending));
                writeProcess(ecode,descSockCTRLserv,requeteLogin);
                if (ecode < 0){
                    perror("Erreur initialisation login\n");
                    exit(42);
                }
                
                bzero(buffer, MAXBUFFERLEN);
                //Lecture de login et regarde si un mot de passe est nécessaire
                readProcess(ecode, descSockCTRLserv, buffer);
                if (ecode < 0){
                    perror("Erreur lecture du login par le serveur\n");
                    exit(42);
                }

                //On envoie la demande de mot de passe (331)
                writeProcess(ecode,descSockCOM,buffer);
                if (ecode < 0){
                    perror("Erreur envoi demande de mot de passe\n");
                    exit(42);
                }
                printf("Envoi de demande de mot de passe \n");
                  
                bzero(buffer, MAXBUFFERLEN);
                //On lit le mdp et je l 'envoie au serveur distant
                readProcess(ecode, descSockCOM, buffer);
                if (ecode < 0){
                    perror("Erreur lecture du mot de passe\n");
                    exit(42);
                }

                //On envoie le mot de passe au serveur
                writeProcess(ecode,descSockCTRLserv,buffer);
                if (ecode < 0){
                    perror("Erreur envoi du mot de passe au serveur\n");
                    exit(42);
                }
                printf("Mot de passe correctement envoyé au serveur");
                
                //Le serveur vérifie la connexion 
                readProcess(ecode, descSockCTRLserv, buffer);
                if (ecode < 0){
                    perror("Erreur vérification mot de passe\n");
                    exit(42);
                }

                //Envoie si la connexion est vérifiée
                writeProcess(ecode,descSockCOM,buffer);
                 if (ecode < 0){
                    perror("Erreur envoi validation mot de passe\n");
                    exit(42);
                }

                /*
                On regarde la commande SYST du client pour faire tourner correctement
                le proxy FTP sur n'importe quelle machine
                */
                readProcess(ecode, descSockCOM, buffer);
                if (ecode < 0){
                    perror("Erreur lecture commande SYST\n");
                    exit(42);
                }

                //On envoie la commande au serveur
                writeProcess(ecode,descSockCTRLserv,buffer);
                if (ecode < 0){
                    perror("Erreur envoi commande SYST\n");
                    exit(42);
                }

                //Ecoute la réponse à SYST du serveur (215)
                readProcess(ecode, descSockCTRLserv, buffer);
                if (ecode < 0){
                    perror("Erreur lecture de SYST dans le serveur\n");
                    exit(42);
                }

                //Renvoi le type de système du serveur
                writeProcess(ecode,descSockCOM,buffer);
                 if (ecode < 0){
                    perror("Erreur renvoi type système\n");
                    exit(42);
                }

                //Commande PORT du Client
                readProcess(ecode, descSockCOM, buffer);
                if (ecode < 0){
                    perror("Erreur lecture commande PORT\n");
                    exit(42);
                }
                int IP1, IP2, IP3, IP4, PORT1, PORT2;
                sscanf(buffer, "PORT %d,%d,%d,%d,%d,%d", &IP1, &IP2, &IP3, &IP4, &PORT1, &PORT2); //Découpage du port pour communiquer spécialement créer pour la durée du transfert

                //Permet d'écrire le port et l'ip du client à partir du découpage précédent
                char ipClient[MAXHOSTLEN];
                char portClient[MAXPORTLEN];
                sprintf(ipClient, "%d.%d.%d.%d", IP1, IP2, IP3, IP4);
                sprintf(portClient, "%d", PORT1 * 256 + PORT2);

                ecode=connect2Server(ipClient,portClient,&descSockTEMPclient);
                if (ecode<0){
                    perror("Problème de connexion avec le nouveau port et adresse ip\n");
                    exit(42);
                }else {
                    printf(" Connecté au serveur en tant que %s.\n", ipClient);
                }

                 //On ajout PASV à la fin du buffer pour préciser au serveur que l'on veut communiquer en mode passif
                strcpy(buffer, "PASV\r\n");
                writeProcess(ecode,descSockCTRLserv,buffer);
                if(ecode<0){
                    perror("Erreur envoi mode passif\n");
                    exit(42);
                }

                //Le serveur lit la demande pour passer en mode passif
                readProcess(ecode,descSockCTRLserv,buffer);
                if (ecode<0){
                    perror("Erreur lecture mode passif\n");
                    exit(42);
                }
                // decoupage de l'ip et du port
                sscanf(buffer, "227 Entering Passive Mode (%d,%d,%d,%d,%d,%d)", &IP1, &IP2, &IP3, &IP4, &PORT1, &PORT2);

                //Nouvelle connexion en mode passif pour le serveur
                char ipServeur[MAXHOSTLEN];
                char portServeur[MAXPORTLEN];
                sprintf(ipServeur, "%d.%d.%d.%d", IP1, IP2, IP3, IP4);
                sprintf(portServeur, "%d", PORT1 * 256 + PORT2);
                printf(ipServeur);
                printf(portServeur);
                ecode=connect2Server(ipServeur,portServeur,&descSockTEMPserv);
                if (ecode<0){
                    perror("Erreur de connexion passive\n");
                    exit(42);
                } else {
                    printf("Connexion passive établie\n");
                }

                char message[50] = "200 Rentrer List\n";
                //Envoie d'une confirmation au client
                writeProcess(ecode,descSockCOM,message);
                if(ecode<0){
                    perror("Erreur réception message de connexion\n");
                    exit(42);
                }

                //Ecoute commande LIST du Client
                readProcess(ecode,descSockCOM,buffer);
                if (ecode<0){
                    perror("Erreur de lecture de la commande LIST");
                    exit(42);
                }

                //Envoie de la commande au SERVEUR FTP et non à la connexion passive
                writeProcess(ecode,descSockCTRLserv,buffer);

                //On récupère la réponse de la commande LIST sur le serveur FTP
                readProcess(ecode,descSockCTRLserv,buffer);

                //Envoie la vérification d'ouverture de connexion en mode 150
                writeProcess(ecode,descSockCOM,buffer);

                //Lire tous les éléments du résultat de la commande ls
                while (read(descSockTEMPserv, buffer, MAXBUFFERLEN - 1) != 0){
                    // lecture de donnees du serveur
                    readProcess(ecode,descSockTEMPserv,buffer);
                    if (ecode == -1)
                    {
                        perror("Probleme de lecture dans LS");
                        exit(1);
                    }

                    // envoie des donnees au client
                    writeProcess(ecode,descSockTEMPclient,buffer);
                    if (ecode == -1)
                    {
                        perror("Probleme d'envoie de donnees dans LS");
                        exit(1);
                    }
                };

                close(descSockTEMPclient);
                close(descSockTEMPserv);
            }
        }
        close(descSockCTRLserv);
        close(descSockCOM);
    }
    close(descSockRDV);
}

void readProcess(int ecode, int descSock, char* buffer ){
    ecode = read(descSock, buffer, MAXBUFFERLEN-1);
	if (ecode == -1) {perror("Problème de lecture\n"); exit(3);}
	buffer[ecode] = '\0';
	printf("MESSAGE RECU : %s \n",buffer);
}

void writeProcess(int ecode, int descSockCOM, char* buffer ) {
  ecode = write(descSockCOM, buffer, strlen(buffer));
  if (ecode == -1) {perror("Problème d'écriture\n"); exit(3);}
  printf("MESSAGE ENVOYE : %s \n",buffer);
}

