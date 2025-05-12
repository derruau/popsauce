// client.c
#include "pse.h"

int main(int argc, char *argv[]) {
  int sock, ret;
  struct sockaddr_in *adrServ;
  int fin = FAUX;
  char ligne[LIGNE_MAX];
  int lgEcr;

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

  while (!fin) {
    printf("ligne> ");
    if (fgets(ligne, LIGNE_MAX, stdin) == NULL) // saisie de CTRL-D
      fin = VRAI;
    else {
      lgEcr = ecrireLigne(sock, ligne);
      if (lgEcr == -1)
        erreur_IO("ecriture ligne");

      if (strcmp(ligne, "fin\n") == 0)  // ecrireLigne a ajoute \n
        fin = VRAI;
    }
  }

  if (close(sock) == -1)
    erreur_IO("fermeture socket");

  exit(EXIT_SUCCESS);
}
