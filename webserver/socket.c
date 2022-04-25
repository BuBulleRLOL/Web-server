
#include "socket.h"

int creer_serveur(int port) {
  /* CREATION SOCKET */
  int socketfd;
  int optval = 1;

  socketfd = socket(AF_INET, SOCK_STREAM, 0);
  if (socketfd == -1)
  {
        perror("socketfd error");
	return -1;
  }

  /* CONNECTION SOCKET <-> INTERFACE(S) */
  struct sockaddr_in saddr;
  saddr.sin_family = AF_INET; /* Socket ipv4 */
  saddr.sin_port = htons(port); /* Port d'écoute */
  saddr.sin_addr.s_addr = INADDR_ANY; /* écoute sur toutes les interfaces */

  if (setsockopt(socketfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(int)) == -1) {
	perror("Can not set SO_REUSEADDR option");
	return -1;
  }

  if (bind(socketfd, (struct sockaddr * )&saddr, sizeof(saddr)) == -1)
  {
        perror("bind socketfd error");	
	return -1;
  }

  /* MISE EN ECOUTE */
  if (listen(socketfd, 10) == -1)
  {
        perror("listen socketfd error");
	return -1;
  }
  return socketfd;
}
