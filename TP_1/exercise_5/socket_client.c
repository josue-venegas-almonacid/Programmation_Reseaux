/* Client pour les sockets
 *    socket_client ip_server port
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

int main(int argc, char** argv) {
  if (argc != 2) {
    printf("Usage: port_scanner hostname\n");
    exit(1);
  }

  char* hostname = argv[1];
  struct hostent* host = gethostbyname(hostname);
  if (host == NULL) {
    printf("Failed to resolve hostname\n");
    exit(1);
  }

  printf("Scanning ports for %s (%s):\n", hostname, inet_ntoa(*(struct in_addr*)host->h_addr_list[0]));

  int sockfd;
  struct sockaddr_in serv_addr;
  for (int port = 1; port <= 65535; port++) {
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
      printf("Failed to create socket\n");
      exit(1);
    }

    bzero((char*)&serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    serv_addr.sin_addr = *((struct in_addr*)host->h_addr_list[0]);

    if (connect(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) == 0) {
      printf("Port %d is open\n", port);
    }

    close(sockfd);
  }

  return 0;
}
