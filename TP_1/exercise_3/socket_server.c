/* Serveur sockets TCP */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

void handleConnection(int newsockfd) {
  char c;
  while (1) {
    while (read(newsockfd, &c, 1) != 1);
    printf("%c", c);
  }
}

int main(int argc, char** argv) {
  char datas[] = "hello\n";
  int sockfd, newsockfd, clilen, nleft, nbwriten;
  struct sockaddr_in cli_addr, serv_addr;

  if (argc != 2) {
    printf("usage: socket_server port\n");
    exit(0);
  }

  printf("server starting...\n");

  /* ouverture du socket */
  sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd < 0) {
    printf("impossible d'ouvrir le socket\n");
    exit(0);
  }

  /* initialisation des parametres */
  bzero((char*)&serv_addr, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  serv_addr.sin_port = htons(atoi(argv[1]));

  /* effecture le bind */
  if (bind(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
    printf("impossible de faire le bind\n");
    exit(0);
  }

  /* petit initialisation */
  listen(sockfd, 1);

  /* attend la connection d'un client */
  clilen = sizeof(cli_addr);

  while (1) {
    newsockfd = accept(sockfd, (struct sockaddr*)&cli_addr, &clilen);
    if (newsockfd < 0) {
      printf("accept error\n");
      exit(0);
    }

    printf("connection accepted\n");

    int pid = fork();
    if (pid < 0) {
      printf("fork error\n");
      exit(0);
    } else if (pid == 0) {
      // child process
      printf("process ID: %d\n", getpid());
      close(sockfd);
      handleConnection(newsockfd);
      exit(0);
    } else {
      // parent process
      close(newsockfd);
    }
  }

  /*  attention il s'agit d'une boucle infinie
   *  le socket n'est jamais ferme !
   */

  return 1;
}