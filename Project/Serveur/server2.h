#ifndef SERVER_H
#define SERVER_H

#ifdef WIN32

#include <winsock2.h>

#elif defined (linux)

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h> /* close */
#include <netdb.h> /* gethostbyname */

#define INVALID_SOCKET -1
#define SOCKET_ERROR -1
#define closesocket(s) close(s)
typedef int SOCKET;
typedef struct sockaddr_in SOCKADDR_IN;
typedef struct sockaddr SOCKADDR;
typedef struct in_addr IN_ADDR;

#else

#error not defined for this platform

#endif

#define CRLF        "\r\n"
#define PORT         1977
#define MAX_CLIENTS     100

#define BUF_SIZE    1024

#include "client2.h"
#include "awale.h"

typedef struct
{
   // ID of the party
   int id;

   // ID of the player one
   Client* player_one;

   // ID of the player two
   Client* player_two;

   // ID of the spectators
   int spectators[MAX_CLIENTS];

   // TODO: change the above structure and save everyone in a list

   // Number of spectators
   int spectators_size;

   // Party mode
   // 0 = public
   // 1 = private
   int mode;

   // Game
   Awale* game;
   int game_started;
   int turn;
}Party;

// List of functions
static void init(void);
static void end(void);
static void app(void);

int user_exists(Client* clients, int clients_size, char* username);

Client* get_client_by_id(Client *clients, int clients_size, int client_id);
Client* get_client_by_username(Client *clients, int clients_size, char* username);
Party*  get_party_by_id(Party *parties, int parties_size, int party_id);
int is_friend(Client* client, Client* friend);
int getRandomValue(int val1, int val2);

void send_message_to_client(Client *clients, int clients_size, int sender_id, int receiver_socket, char *buffer, char *color);
void broadcast_message(Client *clients, int clients_size, int sender_id, int room_id, char *buffer, char *color);

static void clear_clients(Client *clients, int clients_size);
static void remove_client(Client *clients, int to_remove, int *clients_size);
static int init_connection(void);
static void end_connection(int sock);
static int read_client(SOCKET sock, char *buffer);
static void write_client(SOCKET sock, const char *buffer);



#endif /* guard */
