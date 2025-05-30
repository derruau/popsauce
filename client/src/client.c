// client.c
#include "pse.h"
#include "common.h"
typedef struct player{
  char pseudo [100]; 
  int uiid; 
  char text[100]; 
  int sock; 

} player; 
int fin =FAUX;
void send_connect(int sock, const char *usrname, uint32_t id ){
  Connect connect_payload; 
  strncopy(connect_payload.username,usrname, sizeof(connect_payload)-1 );
  connect_payload.username[sizeof(connect_payload.username)-1]='\0';

  Message *msg=payload_to_message(CONNECT, &connect_payload, id);
  uint8_t *buffer; 
  uint32_t buffer_size; 
  if (serialize_message(msg, &buffer, &buffer_size)!=0){
    perror("serialize message"); 
    free(msg); 
  }
  send_message(sock, buffer, buffer_size, NULL); 

}

int affichage_lobby_libres(int sock, uint32_t client_id){
  Message *req =payload_to_message(LOBBYLIST, NULL, client_id);
  uint8_t *buffer; 
  uint32_t buffer_size; 
  serialize_message(req,&buffer, &buffer_size );
  free(req); 
  
  uint32_t buffer2_size; 
  uint8_t buffer2=receive_message(sock, &buffer_size, MAX_PAYLOAD_LENGTH);
  Message *resp=deserialize_message(&buffer2);
  free(buffer2); 
  LobbyList *lobies=(LobbyList *) resp->payload; 
  for (int i=0; i<lobies->__quantity; i++){
    printf ("Lobby no %d id %d nom %s \n", i+1, lobies->ids[i], lobies->names[i]);

  }
  if (lobies->__quantity=0){
    printf("Aucun Lobby disponnible"); 
  }
}

void join_lobby(int sock,uint32_t client_id,  uint32_t lobby_id){
  JoinLobby jpayload; 
  jpayload.lobby_id= lobby_id; 
  Message * req= payload_to_message(JOIN_LOBBY, &jpayload, client_id); 
  uint8_t buffer; 
  uint32_t buffer_size; 
  serialize_message(req, buffer, buffer_size); 
  send_message(sock, &buffer, buffer_size, NULL);
  free(buffer);  
  free(req);
  uint32_t buffer2_size; 
  uint8_t buffer2=receive_message(sock, &buffer2_size, MAX_PAYLOAD_LENGTH); 
  Message *resp=deserialize_message(buffer2);
  if (resp->type==SUCCESS){
    printf("Lobby rejoint"); 
  }
  else{
    printf("Echect de la connection au lobby"); 
  }

}

void create_lobby(int sock, int client_id, const char * name , uint32_t nbmax){
  CreateLobby payload; 
  payload.max_players=nbmax; 
  strncpy(payload.name, name, sizeof((payload.name)-1)); 
  payload.name[sizeof(payload)-1]='\0';
  Message *req=payload_to_message(CREATE_LOBBY, &payload, client_id); 
  uint8_t buffer; 
  uint32_t buffer_size; 
  serialize(req, &buffer, &buffer_size); 
  send_message(sock, &buffer, buffer_size, req); 

  uint32_t buffer2_size; 
  uint8_t buffer2=receive_message(sock, &buffer2_size, MAX_PAYLOAD_LENGTH);
  Message *resp=deserialize_message(buffer2) ;
  if (resp->type==SUCCESS){
    printf("Lobby créé avec succes"); 
  }
  else {
    printf("Echec de la creation du lobby"); 
  }

  
  
  
}

void start_game(int sock, uint32_t client_id){
  Message *req= payload_to_message(START_GAME, NULL, client_id ); 
  uint8_t buffer; 
  uint32_t buffer_size; 
  serialize_message(req, &buffer, &buffer_size); 
  send_message(sock, buffer, buffer_size, NULL); 
  uint32_t buffer2_size; 
  uint8_t buffer2=receive_message(sock, &buffer2_size, MAX_PAYLOAD_LENGTH);
  Message *resp=deserialize_message(&buffer2); 

  if (resp->type==SUCCESS){
    printf("Lancement de la partie"); 
  }


}
 
void *receive_response(void *arg){
  player *user=arg;
  uint32_t buffer2_size; 
  uint8_t buffer2=0;
  
  while(!fin){
  uint8_t buffer2=receive_message(user->sock, &buffer2_size, MAX_PAYLOAD_LENGTH);
  Message *resp=deserialize_message(&buffer2); 
  if (resp->type==GAME_ENDED){
    printf("Fin de la partie");
    fin=VRAI;
  }
  QuestionSent * q= (QuestionSent*) resp->payload;
  printf("Question : %s", q->question);
  }
}
void *send_response(void *arg){
  player *user= arg; 
  
  SendResponse payload; 
  while(!fin){
  fgets(user->text, LIGNE_MAX, stdin); 
  strncpy(payload.response, user->text, sizeof(payload)-1); 
  Message *req=payload_to_message(SEND_RESPONSE, &payload, user->uiid);
  uint8_t buffer; 
  uint32_t buffer_size; 
  serialize_message(req, &buffer, &buffer_size); 
  send_message(user->sock, buffer, buffer_size, NULL); 
  }
}



    

int fin =!FAUX; 
void *reception_client(void* canal, void *ligne); 
void *ecriture_client(void *canal, void *ligne); 
void menu(player *user, int soc)
{
  user=malloc(sizeof(player)); 
  printf ("\nVeuillez entrer un nom d'utilisateur \n"); 
  scanf("%s", user->pseudo); 
  user->uiid=(int) (user->pseudo[0]+user->pseudo[1]+user->pseudo[2]); 
  send_connect(soc, user->pseudo, user->uiid ); 

  printf("1/Créer un lobby\n"); 
  printf("2/Rejoindre un lobby\n"); 

  int n; 
  scanf("%d", &n); 
  switch (n){
    case 1: 
         char name[100]; 
         int nmax; 
         printf("Entrez le nom de votre lobby"); 
         scanf("%s", name); 
         printf("Quel est le nombre de joueur maximum autorisés?"); 
         scanf("%d", &nmax); 
         create_lobby(soc, user->uiid, name, nmax) ;
         printf("Voulez vous demarrer?"); 
         int resp; 
         scanf("%d", &resp ); 
         if (resp==1){
          start_game(soc, user->uiid); 
         }

    case 2: 
      affichage_lobby_libres(soc, user->uiid);
      printf("entrez le nom et l'id du lobby que vous voulez rejoindre"); 
      uint32_t lobby_id ;
      scanf("%d", &lobby_id);
      join_lobby(soc, user->uiid, lobby_id); 
      break; 

    
  }


}
int main(int argc, char *argv[]) {
  int sock, ret;
  struct sockaddr_in *adrServ;
  int fin = FAUX;
  char ligneecr[LIGNE_MAX], lignelect[LIGNE_MAX];
  int lgEcr;
  pthread_t idlecture; 
  pthread_t idecriture; 


  signal(SIGPIPE, SIG_IGN);

  if (argc != 3)
    erreur("usage: %s machine port\n", argv[0]);

  sock = socket (AF_INET, SOCK_STREAM, 0);
  if (sock < 0)
    erreur_IO("socket");

  adrServ = resolv(argv[1], argv[2]);
  if (adrServ == NULL)
    erreur("adresse %s port %s inconnus\n", argv[1], argv[2]);
  printf("resolution DNS : serveur adr %s, port %hu\n",
	       stringIP(ntohl(adrServ->sin_addr.s_addr)),
	       ntohs(adrServ->sin_port));

  printf("connexion au serveur\n");
  ret = connect(sock, (struct sockaddr *)adrServ, sizeof(struct sockaddr_in));
  if (ret < 0)
    erreur_IO("connect");


  player *user; 
  
  menu(user, sock)  ; 
 user->sock=sock; 
pthread_create(idecriture, NULL, send_response, user); 
pthread_create(idlecture, NULL, receive_response, user ); 
  if (close(sock) == -1)
    erreur_IO("fermeture socket");

  exit(EXIT_SUCCESS);
}





