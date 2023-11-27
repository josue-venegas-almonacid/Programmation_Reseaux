/* Libraries */
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <unistd.h>

#include "server2.h"
#include "client2.h"

/**
 * Initializes the necessary components for network communication.
 * If the operating system is Windows, it uses Winsock.
 */
static void init(void)
{
#ifdef WIN32
   WSADATA wsa;
   int err = WSAStartup(MAKEWORD(2, 2), &wsa);
   if(err < 0)
   {
      puts("WSAStartup failed !");
      exit(EXIT_FAILURE);
   }
#endif
}

/**
 * Cleans up any resources used by the server. On Windows, 
 * it calls WSACleanup() to clean up the Winsock library.
 */
static void end(void)
{
#ifdef WIN32
   WSACleanup();
#endif
}

/**
 * The main application function. It initializes the server,
*/
static void app(void)
{
   // The socket for the server
   int sock = init_connection();

   // The buffer for the messages
   char buffer[BUF_SIZE];
   char command[BUF_SIZE];
   char user_id[BUF_SIZE];
   char message[BUF_SIZE];
   char party_id[BUF_SIZE];

   // The number of clients
   int clients_size = 0;
   // The maximum number of clients
   int max = sock;
   // The list of clients
   Client clients[MAX_CLIENTS];

   // The set of file descriptors
   fd_set rdfs;

   while(1)
   {
      // Monitor the file descriptors

      // Initialize the set of file descriptors to zero
      FD_ZERO(&rdfs);
      // Add the standard input to the set
      FD_SET(STDIN_FILENO, &rdfs);
      // Add the connection socket to the set
      FD_SET(sock, &rdfs);

      // Add socket of each client to the set
      for(int i = 0; i < clients_size; i++)
      {
         FD_SET(clients[i].sock, &rdfs);
      }

      // If we overflow the maximum number of clients, we exit
      if(select(max + 1, &rdfs, NULL, NULL, NULL) == -1)
      {
         perror("select()");
         exit(errno);
      }

      // Stop process when type on keyboard
      if(FD_ISSET(STDIN_FILENO, &rdfs)) break;

      // New client
      else if(FD_ISSET(sock, &rdfs))
      {
         SOCKADDR_IN csin = { 0 };
         size_t sinsize = sizeof csin;

         // Accept the connection
         int csock = accept(sock, (SOCKADDR *)&csin, &sinsize);
         
         // If the connection failed, we continue
         if(csock == SOCKET_ERROR)
         {
            perror("accept()");
            continue;
         }

         /* after connecting the client sends its name */
         /* not implemented yed, but the user sends it when executes the game*/
         if(read_client(csock, buffer) == -1)
         {
            /* disconnected */
            continue;
         }

         /* what is the new maximum fd ? */
         max = csock > max ? csock : max;

         FD_SET(csock, &rdfs);

         Client c = { csock };
         strncpy(c.name, buffer, BUF_SIZE - 1);
         clients[clients_size] = c;
         clients_size++;

         printf("New client connected as %s with csok %i\n", c.name, csock);
      }
      else
      {
         int i = 0;
         for(i = 0; i < clients_size; i++)
         {
            /* a client is talking */
            if(FD_ISSET(clients[i].sock, &rdfs))
            {
               Client client = clients[i];
               int client_id = clients[i].sock;

               int c = read_client(client_id, buffer);
               /* client disconnected */
               if(c == 0)
               {
                  printf("Client %s disconnected\n", client.name);

                  // if the client is in a party, leave it, stop the game and send a message to the other players
                  // not implemened yet

                  closesocket(clients[i].sock);
                  remove_client(clients, i, &clients_size);
                  strncpy(buffer, client.name, BUF_SIZE - 1);
                  strncat(buffer, " disconnected !", BUF_SIZE - strlen(buffer) - 1);

                  for(i=0; i<clients_size; i++) {
                     int receiver_id = clients[i].sock;
                     send_message_to_client(clients, clients_size, 0, receiver_id, buffer);
                  }
               }
               else
               {
                  /* client sends a message */
                  /* if message starts with "/" it's a command */
                  if (buffer[0] == '/'){
                     if (strcmp(buffer, "/disconnect") == 0){
                        printf("Client %s disconnected\n", client.name);

                        // if the client is in a party, leave it, stop the game and send a message to the other players
                        // not implemened yet

                        closesocket(clients[i].sock);
                        remove_client(clients, i, &clients_size);
                        strncpy(buffer, client.name,                 BUF_SIZE - 1);
                        strncat(buffer, " disconnected !",           BUF_SIZE - strlen(buffer) - 1);

                        for(i=0; i<clients_size; i++) {
                           int receiver_id = clients[i].sock;
                           send_message_to_client(clients, clients_size, 0, receiver_id, buffer);
                        }
                     }

                     else if (strcmp(buffer, "/list_clients") == 0){
                        printf("Client %s asked for a list of connected clients\n", client.name);
                        for(i=0; i<clients_size; i++) {
                           char message[BUF_SIZE];
                           message[0] = 0;
                           strncpy(message, clients[i].name, BUF_SIZE - 1);
                           strncat(message, "\n", BUF_SIZE - strlen(message) - 1);
                           send_message_to_client(clients, clients_size, 0, client_id, message);
                        }
                     }
                     
                     else if (sscanf(buffer, "%s %s %[^\n]", command, user_id, message) == 3 && strncmp(command, "/send", strlen("/send")) == 0) {
                        printf("Client %s sent a private message\n", client.name);
                        printf("To user_id: %s\n", user_id);
                        printf("Message: %s\n", message);
                        continue;
                     }

                     else if (strcmp(buffer, "/create_party") == 0){
                        continue;
                     }

                     else if (strcmp(buffer, "/list_parties") == 0){
                        continue;
                     }

                     else if (sscanf(buffer, "%s %s", command, party_id) == 2 && strncmp(command, "/join_party", strlen("/join_party")) == 0) {
                        printf("Client %s joined a party\n", client.name);
                        printf("Party_id: %s\n", party_id);
                        // only can join a party if the user is in the lobby
                        continue;
                     }

                     else if (strcmp(buffer, "/leave_party") == 0){
                        // only can leave a party if the user is in a party
                        continue;
                     }

                     else if (strcmp(buffer, "/help") == 0){
                        printf("Client %s asked for help\n", client.name);
                        strncpy(buffer, "Available commands\n",                                                         BUF_SIZE - 1);
                        strncat(buffer, "/disconnect: disconnect from server\n",                                        BUF_SIZE - strlen(buffer) - 1);
                        strncat(buffer, "/list_clients: list connected clients\n",                                      BUF_SIZE - strlen(buffer) - 1);
                        strncat(buffer, "/send <<client_id>> <<message>>: send a private message to a client\n",        BUF_SIZE - strlen(buffer) - 1);
                        strncat(buffer, "/create_party: create a party\n",                                              BUF_SIZE - strlen(buffer) - 1);
                        strncat(buffer, "/list_parties: list available parties\n",                                      BUF_SIZE - strlen(buffer) - 1);
                        strncat(buffer, "/join_party <<party_id>>: join a party\n",                                     BUF_SIZE - strlen(buffer) - 1);
                        strncat(buffer, "/leave_party: leave a party\n",                                                BUF_SIZE - strlen(buffer) - 1);
                        send_message_to_client(clients, clients_size, 0, client_id, buffer);
                     }

                     else{
                        printf("Client %s sent an unknown command\n", client.name);
                        strncpy(buffer, "Unknown command\n",                               BUF_SIZE - 1);
                        strncat(buffer, "Type /help for a list of available commands\n",   BUF_SIZE - strlen(buffer) - 1);
                        send_message_to_client(clients, clients_size, 0, client_id, buffer);
                     }
                  }

                  /* if buffer does not start with "/" then it's just a message */
                  else{

                     // if the user is in the lobby
                     /** Send the message to everyone but himself
                      * for(i=0; i<clients_size; i++) {
                      * int receiver_id = clients[i].sock;
                      * if (receiver_id != client_id) send_message_to_client(clients, clients_size, client_id, receiver_id, buffer);
                     } **/

                     // if the user is in a party
                     /** Send the message to everyone but himself
                      * for(i=0; i<party_size; i++) {
                      * int receiver_id = party_clients[i].sock;
                      * if (receiver_id != client_id) send_message_to_client(clients, clients_size, client_id, receiver_id, buffer);
                     } **/

                     // hard-coded messaging between two clients
                     if (client_id == 5) send_message_to_client(clients, clients_size, client_id, 4, buffer);
                     if (client_id == 4) send_message_to_client(clients, clients_size, client_id, 5, buffer);
                  }
               }
               break;
            }
         }
      }
   }

   clear_clients(clients, clients_size);
   end_connection(sock);
}

Client get_client(Client *clients, char client_id)
{
   int i = 0;
   for(i = 0; i < MAX_CLIENTS; i++)
   {
      if(clients[i].sock == client_id)
      {
         return clients[i];
      }
   }

   Client no_client = { 0, "No clients" };
   return no_client;
}

void send_message_to_client(Client *clients, int clients_size, char sender_id, char receiver_id, const char *buffer)
{
   int i = 0;
   char message[BUF_SIZE];
   message[0] = 0;
   for(i = 0; i < clients_size; i++)
   {
      /* we send message only to the receiver */
      if(receiver_id == clients[i].sock)
      {
         /* if sender_id == 0 then the sender is the server */
         if(sender_id == 0)
         {
            strncpy(message, "server", BUF_SIZE - 1);
         }
         /* if sender_id != 0 then get the sender name */
         else
         {
            Client sender = get_client(clients, sender_id);
            strncpy(message, sender.name, BUF_SIZE - 1);
         }

         strncat(message, ": ", sizeof message - strlen(message) - 1);
         strncat(message, buffer, sizeof message - strlen(message) - 1);
         write_client(receiver_id, message);
      }
   }
}

static void clear_clients(Client *clients, int clients_size)
{
   int i = 0;
   for(i = 0; i < clients_size; i++)
   {
      closesocket(clients[i].sock);
   }
}

static void remove_client(Client *clients, int to_remove, int *clients_size)
{
   /* we remove the client in the array */
   memmove(clients + to_remove, clients + to_remove + 1, (*clients_size - to_remove - 1) * sizeof(Client));
   /* number client - 1 */
   (*clients_size)--;
}

static int init_connection(void)
{
   SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
   SOCKADDR_IN sin = { 0 };

   if(sock == INVALID_SOCKET)
   {
      perror("socket()");
      exit(errno);
   }

   sin.sin_addr.s_addr = htonl(INADDR_ANY);
   sin.sin_port = htons(PORT);
   sin.sin_family = AF_INET;

   if(bind(sock,(SOCKADDR *) &sin, sizeof sin) == SOCKET_ERROR)
   {
      perror("bind()");
      exit(errno);
   }

   if(listen(sock, MAX_CLIENTS) == SOCKET_ERROR)
   {
      perror("listen()");
      exit(errno);
   }

   return sock;
}

static void end_connection(int sock)
{
   closesocket(sock);
}

static int read_client(SOCKET sock, char *buffer)
{
   int n = 0;

   if((n = recv(sock, buffer, BUF_SIZE - 1, 0)) < 0)
   {
      perror("recv()");
      /* if recv error we disonnect the client */
      n = 0;
   }

   buffer[n] = 0;

   return n;
}

static void write_client(SOCKET sock, const char *buffer)
{
   if(send(sock, buffer, strlen(buffer), 0) < 0)
   {
      perror("send()");
      exit(errno);
   }
}

int main(int argc, char **argv)
{
   init();

   app();

   end();

   return EXIT_SUCCESS;
}
