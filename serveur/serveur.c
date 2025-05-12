#include "pse.h"

// serveur.c
#include "pse.h"

#define NOM_JOURNAL   "journal.log"
#define nb_workers 2
#define nocanal -1
typedef struct {
  int canal;
  int no_connect; 
  sem_t sem; 
} DATA_THREAD;

void *threadSessionClient(void *arg);
void sessionClient(int canal, DATA_THREAD *dataThread);
void remiseAZeroJournal(void);
DATA_THREAD datathread [nb_workers];
pthread_t idthreads[nb_workers]; 
int retour[nb_workers];
int fdJournal;
void create_threads();
int next_worker(); 
sem_t sem_workers; 
int nb_connect=0; 
int main(int argc, char *argv[]) {
    short port;
    int ecoute, ret, canal;
    struct sockaddr_in adrEcoute, adrClient;
    unsigned int lgAdrClient;
    sem_init(&sem_workers,0,nb_workers);
  
    create_threads(); 
    if (argc != 2)
    erreur("usage: %s port\n", argv[0]);

    port = (short)atoi(argv[1]);

    fdJournal = open(NOM_JOURNAL, O_CREAT|O_WRONLY|O_APPEND, 0600);
    if (fdJournal == -1)
    erreur_IO("ouverture journal");

    ecoute = socket (AF_INET, SOCK_STREAM, 0);
    if (ecoute < 0)
    erreur_IO("socket");
  
    adrEcoute.sin_family = AF_INET;
    adrEcoute.sin_addr.s_addr = INADDR_ANY;
    adrEcoute.sin_port = htons(port);
    ret = bind (ecoute,  (struct sockaddr *)&adrEcoute, sizeof(adrEcoute));
    if (ret < 0)
    erreur_IO("bind");
  
    ret = listen (ecoute, 5);
    if (ret < 0)
        erreur_IO("listen");

    while (VRAI) {
        printf("Serveur. attente connexion\n");
        lgAdrClient = sizeof(adrClient);
        sem_wait(&sem_workers); 
        canal=accept(ecoute, (struct sockaddr *)&adrClient, &lgAdrClient);
        nb_connect++; 
        int next=next_worker(); 
        datathread[next].canal = canal; 
        datathread[next].no_connect=nb_connect; 
        sem_post(&datathread[next].sem);
   
    
        if (datathread[next].canal < 0)
        erreur_IO("accept");
        printf("reveil \n");
        printf("Serveur. connexion recue : client adr %s, port %hu\n",
                stringIP(ntohl(adrClient.sin_addr.s_addr)),
                ntohs(adrClient.sin_port));

        if (ret != 0)
        erreur_IO("creation thread");
    } 

    if (close(ecoute) == -1)
        erreur_IO("fermeture socket ecoute");  

    if (close(fdJournal) == -1)
        erreur_IO("fermeture journal");  

    exit(EXIT_SUCCESS);
}


void *threadSessionClient(void *arg) {
 
  DATA_THREAD *dataThread = (DATA_THREAD *)arg;
  while (1){
  sem_wait(&dataThread->sem);
  sessionClient(dataThread->canal, dataThread);
  dataThread->canal=nocanal; 
  nb_connect--; 
  sem_post(&sem_workers); 
  }
  pthread_exit(NULL);
}

// session de dialogue avec un client
// fermeture du canal a la fin de la session
void sessionClient(int canal, DATA_THREAD *dataThread) {
  int fin = FAUX;
  char ligne[LIGNE_MAX];
  int lgLue, lgEcr;

  while (!fin) {
    lgLue = lireLigne(canal, ligne);
    if (lgLue == -1)
      erreur_IO("lecture ligne");

    if (lgLue == 0) { // connexion fermee, donc arret du client
      printf("Serveur. arret du client\n");
      fin = VRAI;
    }
    else if (strcmp(ligne, "fin") == 0) {
      printf("Serveur. fin demandee\n");
      fin = VRAI;
    }
    else if (strcmp(ligne, "init") == 0) { 
      printf("Serveur. remise a zero du journal\n");
      remiseAZeroJournal();
    }
    else {
      printf("Serveur. %d octets recus de client no %d. ligne: %s\n", lgLue, dataThread->no_connect,  ligne );

      lgEcr = ecrireLigne(fdJournal, ligne); 
      if (lgEcr == -1)
        erreur_IO("ecriture journal");
    }
  }

  if (close(canal) == -1)
    erreur_IO("fermeture canal");
}

void remiseAZeroJournal(void) {
  // on ferme le fichier et on le rouvre en mode O_TRUNC
  if (close(fdJournal) == -1)
    erreur_IO ("fermeture journal pour remise a zero");

  fdJournal = open(NOM_JOURNAL, O_TRUNC|O_WRONLY|O_APPEND, 0600);
  if (fdJournal == -1)
    erreur_IO ("ouverture journal pour remise a zero");
}

void create_threads(){
	for (int i=0; i<nb_workers; i++){
		datathread[i].canal=nocanal; 
		sem_init(&datathread[i].sem,0,0);
		retour[i]= pthread_create(&idthreads[i], NULL, threadSessionClient, &datathread[i]);			
	}	

}

int next_worker(){
	for (int i=0; i<nb_workers; i++){
		if (datathread[i].canal==nocanal){
			return i;
		}
	}
	return nocanal; 
}