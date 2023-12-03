#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <unistd.h>
#include <time.h>

#include "server2.h"
#include "client2.h"
#include "awale.h"

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

static void end(void)
{
#ifdef WIN32
   WSACleanup();
#endif
}

static void app(void)
{
   // The socket for the server
   int sock = init_connection();
   int max = sock;
   fd_set rdfs;

   // The buffer for the messages
   char buffer[BUF_SIZE];
   char command[BUF_SIZE];
   char other_username[BUF_SIZE];
   char mode[BUF_SIZE];
   char message[BUF_SIZE];

   // ASCII color codes for the messages
   char red[BUF_SIZE]   = "\033[0;31m";
   char green[BUF_SIZE] = "\033[0;32m";
   char yellow[BUF_SIZE]= "\033[0;33m";
   char blue[BUF_SIZE]  = "\033[0;34m";
   char white[BUF_SIZE] = "\033[0;37m";

   // The list of clients
   Client* clients = malloc(MAX_CLIENTS * sizeof(Client));
   int clients_size = 0;

   // The list of parties
   Party* parties = malloc(MAX_CLIENTS * sizeof(Party));
   int parties_size = 0;
   Awale* game;
   Party* party;
   int move;
   int turn;

   while(1)
   {
      // Monitor the file descriptors

      // Initialize the set of file descriptors to zero
      FD_ZERO(&rdfs);
      // Add the standard input to the set
      FD_SET(STDIN_FILENO, &rdfs);
      // Add the connection socket to the set
      FD_SET(sock, &rdfs);

      // Add socket of each connected client to the set
      for(int s = 0; s < clients_size; s++)
      {
         if(clients[s].sock != -1) FD_SET(clients[s].sock, &rdfs);
      }

      // If we overflow the maximum number of clients, we exit
      if(select(max + 1, &rdfs, NULL, NULL, NULL) == -1)
      {
         perror("select()");
         exit(errno);
      }

      // Stop process when type on keyboard
      if(FD_ISSET(STDIN_FILENO, &rdfs)) continue;

      // New client
      else if(FD_ISSET(sock, &rdfs))
      {
         printf("New client\n");
         SOCKADDR_IN csin = { 0 };
         uid_t sinsize = sizeof csin;

         // Accept the connection
         int csock = accept(sock, (SOCKADDR *)&csin, &sinsize);
         
         // If the connection failed, we continue
         if(csock == SOCKET_ERROR)
         {
            perror("accept()");
            continue;
         }

         if(read_client(csock, buffer) == -1)
         {
            /* disconnected */
            printf("Client disconnected\n");
            continue;
         }

         // Check the maximum socket number
         max = csock > max ? csock : max;

         FD_SET(csock, &rdfs);

         // Check if the user already exists
         int id = user_exists(clients, clients_size, buffer);

         // If the user exists, reconnect him
         if(id != -1)
         {
            Client* client = get_client_by_id(clients, clients_size, id);

            // Update the socket
            client->sock = csock;

            printf("User connected as %s with ID %i and socket %i\n", client->name, client->id, client->sock);

            // Send a message to all the users in the lobby
            strncpy(buffer, client->name, BUF_SIZE - 1);
            strncat(buffer, " connected !", BUF_SIZE - strlen(buffer) - 1);
            broadcast_message(clients, clients_size, 0, -1, buffer, yellow);
         }

         // If the user does not exist, try to create a new user
         else
         {
            if (clients_size == MAX_CLIENTS)
            {
               strncpy(buffer, "The server is full\n", BUF_SIZE - 1);
               send_message_to_client(clients, clients_size, 0, csock, buffer, red);
               continue;
            }

            else
            {
               // If the list of users is not full, add the new user
               Client client = { 
                  sock: csock,
                  id: clients_size + 1,
                  party_id: -1,
                  name: "",
                  bio: "",
                  challengers: { 0 },
                  challengers_size: 0,
                  friends: { 0 },
                  friends_size: 0,
                  replay_party_id: -1,
                  replay_position: 0,
                  ranking: 0
               };

               strncpy(client.name, buffer, BUF_SIZE - 1);
               clients[clients_size] = client;
               
               printf("User connected as %s with ID %i and socket %i\n", clients[clients_size].name, clients[clients_size].id, clients[clients_size].sock);
               clients_size++;

               // Send a message to all the users in the lobby
               strncpy(buffer, clients[clients_size - 1].name, BUF_SIZE - 1);
               strncat(buffer, " connected !", BUF_SIZE - strlen(buffer) - 1);
               broadcast_message(clients, clients_size, 0, -1, buffer, yellow);
            }
         }
      }

      // Listen to all clients
      else
      {
         for(int i = 0; i < clients_size; i++)
         {
            if(FD_ISSET(clients[i].sock, &rdfs))
            {
               
               int c = read_client(clients[i].sock, buffer);

               // Client disconnected
               if(c == 0)
               {
                  printf("The user %s has disconnected\n", clients[i].name);

                  // Change the player party id to the lobby
                  clients[i].party_id = -1;

                  // Change the player replay party id to -1
                  clients[i].replay_party_id = -1;
                  clients[i].replay_position = 0;
                 
                  // Close the socket
                  closesocket(clients[i].sock);
                  clients[i].sock = -1;

                  // Send a message to all the users in the lobby
                  strncpy(buffer, clients[i].name, BUF_SIZE - 1);
                  strncat(buffer, " disconnected !", BUF_SIZE - strlen(buffer) - 1);

                  broadcast_message(clients, clients_size, 0, -1, buffer, yellow);
               }

               // Client sent a message
               else
               {
                  // If the buffer starts with "/" it's a command
                  if(buffer[0] == '/')
                  {
                     if(strcmp(buffer, "/list_users") == 0)
                     {
                        printf("The user %s tried to list the users\n", clients[i].name);

                        strncpy(buffer, "List of users:\n",             BUF_SIZE - 1);
                        for(int u = 0; u < clients_size; u++)
                        {
                           char str[BUF_SIZE];
                           sprintf(str, "%d", clients[u].party_id);


                           strncat(buffer, clients[u].name,             BUF_SIZE - strlen(buffer) - 1);
                           
                           if(clients[u].sock == -1)
                           {
                              strncat(buffer, " (DISCONNECTED)\n",      BUF_SIZE - strlen(buffer) - 1);
                           }

                           else if(clients[u].party_id == -1)
                           {
                              strncat(buffer, " (LOBBY)\n",             BUF_SIZE - strlen(buffer) - 1);
                           }
                           
                           else
                           {
                              strncat(buffer, " (PARTY ID: ",           BUF_SIZE - strlen(buffer) - 1);
                              strncat(buffer, str,                      BUF_SIZE - strlen(buffer) - 1);
                              strncat(buffer, ")\n",                    BUF_SIZE - strlen(buffer) - 1);
                           }
                        }
                        send_message_to_client(clients, clients_size, 0, clients[i].sock, buffer, yellow);
                     }

                     else if(sscanf(buffer, "%s %[^\n]", command, message) == 2 && strncmp(command, "/chat", strlen("/chat")) == 0)
                     {
                        printf("The user %s tried to send a message to the room %d\n", clients[i].name, clients[i].party_id);
                        
                        // Send the message to all the users in the room (lobby/party)
                        broadcast_message(clients, clients_size, clients[i].id, clients[i].party_id, message, blue);
                     }

                     else if(sscanf(buffer, "%s %s %[^\n]", command, other_username, message) == 3 && strncmp(command, "/send", strlen("/send")) == 0)
                     {
                        printf("The user %s tried to send a message to %s\n", clients[i].name, other_username);

                        // The user can not send a message to himself
                        if(strcmp(clients[i].name, other_username) == 0){
                           strncpy(buffer, "You can not send a message to yourself\n", BUF_SIZE - 1);
                           send_message_to_client(clients, clients_size, 0, clients[i].sock, buffer, red);
                        }

                        // Try to send the message
                        else
                        {
                           Client* receiver = get_client_by_username(clients, clients_size, other_username);

                           // If the receiver does not exist, display an error
                           if(receiver == NULL) 
                           {
                              strncpy(buffer, "User not found\n", BUF_SIZE - 1);
                              send_message_to_client(clients, clients_size, 0, clients[i].sock, buffer, red);
                           }

                           // If the receiver exists, try to send the message
                           else
                           {
                              if(receiver->sock == -1)
                              {
                                 strncpy(buffer, "User is disconnected\n", BUF_SIZE - 1);
                                 send_message_to_client(clients, clients_size, 0, clients[i].sock, buffer, red);
                              }
                              else
                              {
                                 send_message_to_client(clients, clients_size, clients[i].id, receiver->sock, message, yellow);

                                 strncpy(buffer, "Message sent\n", BUF_SIZE - 1);
                                 send_message_to_client(clients, clients_size, 0, clients[i].sock, buffer, green);

                              }
                           }
                        }
                     }

                     else if(sscanf(buffer, "%s %s", command, mode) == 2 && strncmp(command, "/create_party", strlen("/create_party")) == 0)
                     {
                        // Check if the user is in a party already
                        if(clients[i].party_id != -1)
                        {
                           strncpy(buffer, "You are already in a party\n", BUF_SIZE - 1);
                           send_message_to_client(clients, clients_size, 0, clients[i].sock, buffer, red);
                        }

                        else
                        {
                           int party_visibility = atoi(mode);

                           if (party_visibility != 0 && party_visibility != 1)
                           {
                              strncpy(buffer, "Invalid party mode\n", BUF_SIZE - 1);
                              send_message_to_client(clients, clients_size, 0, clients[i].sock, buffer, red);
                              continue;
                           }

                           printf("The user %s tried to create a %s party\n", clients[i].name, party_visibility == 0 ? "public" : "private");

                           if(parties_size == MAX_CLIENTS)
                           {
                              strncpy(buffer, "The list of parties is full\n", BUF_SIZE - 1);
                              send_message_to_client(clients, clients_size, 0, clients[i].sock, buffer, red);
                              continue;
                           }

                           else
                           {
                              // Create a new game
                              Awale game_init = { {4, 4, 4, 4, 4, 4}, {4, 4, 4, 4, 4, 4}, 0, 0, 0, 0 };
                              game = &game_init;
                              Party party = {
                                 id: parties_size,
                                 player_one: &clients[i],
                                 player_two: NULL,
                                 spectators: { 0 },
                                 spectators_size: 0,
                                 mode: party_visibility,
                                 game: game,
                                 status: 0,
                                 turn: clients[i].sock
                              };
                              
                              // Add the party to the list of parties
                              parties[parties_size] = party;
                              parties_size++;

                              // Join the party
                              // Save the party id to the client room attribute
                              clients[i].party_id = party.id;

                              // Display party to the player
                              send_message_to_client(clients, clients_size, 0, clients[i].sock, display(*game, party.player_one->name, "Second player"), yellow);
                           }
                        }

                     }
                     // Replay party
                     else if(sscanf(buffer, "%s %s", command, message) == 2 && strncmp(command, "/replay_party", strlen("/replay_party")) == 0)
                     {
                        // Check if the user is in a party already
                        if(clients[i].party_id != -1)
                        {
                           strncpy(buffer, "You are already in a party\n", BUF_SIZE - 1);
                           send_message_to_client(clients, clients_size, 0, clients[i].sock, buffer, red);
                        }
                        else
                        {
                           int party_id = atoi(message);
                           // Check if the party exists
                           party = get_party_by_id(parties, parties_size, party_id);
                           if(party == NULL)
                           {
                              strncpy(buffer, "Party not found\n", BUF_SIZE - 1);
                              send_message_to_client(clients, clients_size, 0, clients[i].sock, buffer, red);
                           }
                           else
                           {
                              clients[i].replay_party_id = party->id;
                              clients[i].replay_position = 0;
                              clients[i].party_id = -2;
                              send_message_to_client(clients, clients_size, 0, clients[i].sock, party->replay[clients[i].replay_position++], blue);
                              if (clients[i].replay_position == party->replay_size)
                              {
                                 clients[i].replay_position = 0;
                                 clients[i].replay_party_id = -1;
                                 clients[i].party_id = -1;
                              }
                               
                           }
                        }
                     }
                     else if(strcmp(buffer, "/list_parties") == 0)
                     {
                        printf("The user %s tried to list the parties\n", clients[i].name);

                        char str[BUF_SIZE];
                        strncpy(buffer, "List of parties:\n", BUF_SIZE - 1);
                        for(int p = 0; p < parties_size; p++)
                        {
                           party = get_party_by_id(parties, parties_size, p);
                           sprintf(str, "%d", party->id);
                           strncat(buffer, "Party ID: ",                                BUF_SIZE - strlen(buffer) - 1);
                           strncat(buffer, str,                                         BUF_SIZE - strlen(buffer) - 1);
                           strncat(buffer, "\n",                                        BUF_SIZE - strlen(buffer) - 1);

                           strncat(buffer, "  - Owner: ",                               BUF_SIZE - strlen(buffer) - 1);
                           strncpy(str, party->player_one->name,                        BUF_SIZE - 1);
                           strncat(buffer, str,                                         BUF_SIZE - strlen(buffer) - 1);
                           strncat(buffer, "\n",                                        BUF_SIZE - strlen(buffer) - 1);

                           strncat(buffer, "  - Second player: ",                       BUF_SIZE - strlen(buffer) - 1);
                           strncpy(str, party->player_two == NULL ? "" : party->player_two->name,                        BUF_SIZE - 1);
                           strncat(buffer, str,                                         BUF_SIZE - strlen(buffer) - 1);
                           strncat(buffer, "\n",                                        BUF_SIZE - strlen(buffer) - 1);

                           strncat(buffer, "  - Visibility: ",                          BUF_SIZE - strlen(buffer) - 1);
                           strncpy(str, (party->mode == 0)? "Public\n":"Private\n",     BUF_SIZE - 1);
                           strncat(buffer, str,                                         BUF_SIZE - strlen(buffer) - 1);
                        }
                        send_message_to_client(clients, clients_size, 0, clients[i].sock, buffer, yellow);
                     }

                     else if(sscanf(buffer, "%s %s %s", command, message, mode) == 3 && strncmp(command, "/join_party", strlen("/join_party")) == 0)
                     {
                        // Check if the user is in a party already
                        if(clients[i].party_id != -1)
                        {
                           strncpy(buffer, "You are already in a party\n", BUF_SIZE - 1);
                           send_message_to_client(clients, clients_size, 0, clients[i].sock, buffer, red);
                        }

                        else
                        {
                           int party_id = atoi(message);
                           int join_mode = atoi(mode);

                           // Check if the join mode is valid
                           if(join_mode != 0 && join_mode != 1)
                           {
                              strncpy(buffer, "Invalid join mode\n", BUF_SIZE - 1);
                              send_message_to_client(clients, clients_size, 0, clients[i].sock, buffer, red);
                           }
                           else
                           {
                              // Check if the party exists
                              party = get_party_by_id(parties, parties_size, party_id);
                              if(party == NULL)
                              {
                                 strncpy(buffer, "Party not found\n", BUF_SIZE - 1);
                                 send_message_to_client(clients, clients_size, 0, clients[i].sock, buffer, red);
                              }
                              else if(party->status == 2)
                              {
                                 strncpy(buffer, "The party is over\n", BUF_SIZE - 1);
                                 send_message_to_client(clients, clients_size, 0, clients[i].sock, buffer, red);
                              }
                              else
                              {
                                 game = party->game;
                                 // Check if the party is public or, if it's private, if the user is a friend of the owner
                                 if(party->mode == 0 || (party->mode == 1 && is_friend(party->player_one, &clients[i])))
                                 {
                                    // If join mode is 0, the user is joining as a player
                                    if (join_mode == 0 && party->player_two == NULL)
                                    {
                                       party->player_two = &clients[i];
                                       party->status = 1;
                                       party->turn = getRandomValue(party->player_one->sock, party->player_two->sock);
                                       // Print turn
                                       printf("The turn is %d\n", party->turn);
                                       game->turn = (party->turn == party->player_one->sock) ? 1 : 2;
                                       printf("The game turn is %d\n", game->turn);
                                       clients[i].party_id = party->id;
                                       printf("The user %s joined the party %d as a player\n", clients[i].name, party->id);
                                       broadcast_message(clients, clients_size, 0, clients[i].party_id, display(*game, party->player_one->name, party->player_two->name), blue);
                                    }
                                    else if (join_mode == 0 && party->player_two != NULL)
                                    {
                                       strncpy(buffer, "You can not join this party as a player, try as a spectator\n", BUF_SIZE - 1);
                                       send_message_to_client(clients, clients_size, 0, clients[i].sock, buffer, red);
                                    }
                                    else if (join_mode == 1)
                                    { 
                                       if (party->spectators_size == MAX_CLIENTS)
                                       {
                                          strncpy(buffer, "The party is full\n", BUF_SIZE - 1);
                                          send_message_to_client(clients, clients_size, 0, clients[i].sock, buffer, red);
                                          continue;
                                       }
                                       party->spectators[party->spectators_size] = clients[i].sock;
                                       party->spectators_size++;
                                       clients[i].party_id = party->id;
                                       printf("User %s joined the party %d as a spectator\n", clients[i].name, clients[i].party_id);
                                       // If game not started, send message to user to wait
                                       if (party->status == 0)
                                       {
                                          strncpy(buffer, "Waiting for the second player to join\n", BUF_SIZE - 1);
                                          send_message_to_client(clients, clients_size, 0, clients[i].sock, buffer, yellow);
                                       }
                                       else
                                       {
                                          send_message_to_client(clients, clients_size, 0, clients[i].sock, display(*game, party->player_one->name, party->player_two->name), blue);
                                       }
                                       
                                    }
                                    
                                 }
                                 else
                                 {
                                    strncpy(buffer, "You are not allowed to join this party, send a friend reqeust to the owner.\n", BUF_SIZE - 1);
                                    send_message_to_client(clients, clients_size, 0, clients[i].sock, buffer, red);
                                 }
                           
                              }

                           }
                        }
                     } 

                     else if(strcmp(buffer, "/leave_party") == 0)
                     {
                        printf("The user %s tried to left the party %d\n", clients[i].name, clients[i].party_id);
                        // Check if the user is in a party
                        if(clients[i].party_id == -1)
                        {
                           strncpy(buffer, "You are not in a party\n", BUF_SIZE - 1);
                           send_message_to_client(clients, clients_size, 0, clients[i].sock, buffer, red);
                        }

                        else
                        {
                           // Save the lobby id to the client room attribute
                           clients[i].party_id = -1;

                           // Send a message to the user
                           strncpy(buffer, "You left the party\n", BUF_SIZE - 1);
                           send_message_to_client(clients, clients_size, 0, clients[i].sock, buffer, yellow);

                        }
                     }

                     else if(sscanf(buffer, "%s %s", command, other_username) == 2 && strncmp(command, "/add_friend", strlen("/add_friend")) == 0)
                     {
                        printf("The user %s tried to send a friend request to %s\n", clients[i].name, other_username);

                        // The user can not add himself as a friend
                        if(strcmp(clients[i].name, other_username) == 0){
                           strncpy(buffer, "You can not add yourself as a friend\n", BUF_SIZE - 1);
                           send_message_to_client(clients, clients_size, 0, clients[i].sock, buffer, red);
                        }

                        // Try to send the friend request
                        else
                        {
                           Client* receiver = get_client_by_username(clients, clients_size, other_username);
                           
                           // If the receiver does not exist, display an error
                           if(receiver == NULL) 
                           {
                              strncpy(buffer, "User not found\n", BUF_SIZE - 1);
                              send_message_to_client(clients, clients_size, 0, clients[i].sock, buffer, red);
                           }

                           // If the receiver exists, try to send the friend request
                           else
                           {
                              int can_add = 1;

                              // If the receiver is already a friend, display an error
                              for(int f = 0; f < clients[i].friends_size; f++)
                              {
                                 if(clients[i].friends[f] == receiver->id)
                                 {
                                    strncpy(buffer, "User is already a friend\n", BUF_SIZE - 1);
                                    send_message_to_client(clients, clients_size, 0, clients[i].sock, buffer, red);
                                    can_add = 0;
                                 }
                              }

                              // If the receiver has already a pending friend request from the sender, display an error
                              for(int f = 0; f < receiver->friend_requests_size; f++)
                              {
                                 if(receiver->friend_requests[f] == clients[i].id)
                                 {
                                    strncpy(buffer, "Friend request already sent\n", BUF_SIZE - 1);
                                    send_message_to_client(clients, clients_size, 0, clients[i].sock, buffer, red);
                                    can_add = 0;
                                 }
                              }

                              if(can_add == 1)
                              {
                                 // Add the client id into the receiver friend requests
                                 receiver->friend_requests[receiver->friend_requests_size] = clients[i].id;
                                 receiver->friend_requests_size++;

                                 // Send a message to the receiver
                                 strncpy(buffer, "You have a new friend request from: ", BUF_SIZE - 1);
                                 strncat(buffer, clients[i].name, BUF_SIZE - strlen(buffer) - 1);
                                 send_message_to_client(clients, clients_size, 0, receiver->sock, buffer, yellow);

                                 strncpy(buffer, "Friend request sent\n", BUF_SIZE - 1);
                                 send_message_to_client(clients, clients_size, 0, clients[i].sock, buffer, green);
                              }
                           }
                        }
                     }

                     else if(strcmp(buffer, "/list_friend_requests") == 0)
                     {
                        printf("The user %s tried to list the friend requests\n", clients[i].name);

                        strncpy(buffer, "List of friend requests:\n",         BUF_SIZE - 1);
                        for(int u = 0; u < clients[i].friend_requests_size; u++)
                        {
                           Client* friend = get_client_by_id(clients, clients_size, clients[i].friend_requests[u]);
                           strncat(buffer, friend->name,                      BUF_SIZE - strlen(buffer) - 1);
                        }
                        send_message_to_client(clients, clients_size, 0, clients[i].sock, buffer, yellow);
                     }

                     else if(sscanf(buffer, "%s %s", command, other_username) == 2 && strncmp(command, "/accept_friend", strlen("/accept_friend")) == 0)
                     {
                        printf("The user %s tried to accept a friend request from %s\n", clients[i].name, other_username);

                        // Check if the user exists
                        Client* other_user = get_client_by_username(clients, clients_size, other_username);

                        // If the user does not exist, display an error
                        if(other_user == NULL)
                        {
                           strncpy(buffer, "User not found\n", BUF_SIZE - 1);
                           send_message_to_client(clients, clients_size, 0, clients[i].sock, buffer, red);
                        }

                        // If the user exists, try to accept the friend request
                        else
                        {
                           int found = 0;
                           for(int f = 0; f < clients[i].friend_requests_size; f++)
                           {
                              // If the user has a pending friend request from the other user, accept it
                              if(clients[i].friend_requests[f] == other_user->id)
                              {
                                 found = 1;

                                 // Add the other user id into the user friends
                                 clients[i].friends[clients[i].friends_size] = other_user->id;
                                 clients[i].friends_size++;

                                 // Add the user id into the other user friends
                                 other_user->friends[other_user->friends_size] = clients[i].id;
                                 other_user->friends_size++;

                                 // Remove the friend request from the user list
                                 memmove(clients[i].friend_requests + f, clients[i].friend_requests + f + 1, (clients[i].friend_requests_size - f - 1) * sizeof(int));
                                 clients[i].friend_requests_size--;

                                 // If this user also sent a friend request to the other user, remove that friend request in his list
                                 for(int f = 0; f < other_user->friend_requests_size; f++)
                                 {
                                    if(other_user->friend_requests[f] == clients[i].id)
                                    {
                                       // Remove the friend request in the other user list
                                       memmove(other_user->friend_requests + f, other_user->friend_requests + f + 1, (other_user->friend_requests_size - f - 1) * sizeof(int));
                                       other_user->friend_requests_size--;
                                       break;
                                    }
                                 }

                                 // Send a message to the user
                                 strncpy(buffer, "Friend request accepted\n", BUF_SIZE - 1);
                                 send_message_to_client(clients, clients_size, 0, clients[i].sock, buffer, green);

                                 // Send a message to the other user
                                 strncpy(buffer, clients[i].name, BUF_SIZE - 1);
                                 strncat(buffer, " has accepted your friend request\n", BUF_SIZE - strlen(buffer) - 1);
                                 send_message_to_client(clients, clients_size, 0, other_user->sock, buffer, yellow);

                                 break;
                              }
                           }

                           // If the user does not have a pending friend request from the other user, display an error
                           if(found == 0)
                           {
                              strncpy(buffer, "You don't have a friend request from this user\n", BUF_SIZE - 1);
                              send_message_to_client(clients, clients_size, 0, clients[i].sock, buffer, red);
                           }  
                        }
                     }
                     
                     else if(sscanf(buffer, "%s %s", command, other_username) == 2 && strncmp(command, "/reject_friend", strlen("/reject_friend")) == 0)
                     {
                        printf("The user %s tried to reject a friend request from %s\n", clients[i].name, other_username);

                        // Check if the user exists
                        Client* other_user = get_client_by_username(clients, clients_size, other_username);

                        // If the user does not exist, display an error
                        if (other_user == NULL)
                        {
                           strncpy(buffer, "User not found\n", BUF_SIZE - 1);
                           send_message_to_client(clients, clients_size, 0, clients[i].sock, buffer, red);
                        }

                        // If the user exists, try to reject his friend request
                        else
                        {
                           int found = 0;
                           for(int f = 0; f < clients[i].friend_requests_size; f++)
                           {
                              if(clients[i].friend_requests[f] == other_user->id)
                              {
                                 found = 1;

                                 // Remove the friend request from the user list
                                 memmove(clients[i].friend_requests + f, clients[i].friend_requests + f + 1, (clients[i].friend_requests_size - f - 1) * sizeof(int));
                                 clients[i].friend_requests_size--;

                                 // Send a message to the user
                                 strncpy(buffer, "Friend request rejected\n", BUF_SIZE - 1);
                                 send_message_to_client(clients, clients_size, 0, clients[i].sock, buffer, green);

                                 break;
                              }
                           }

                           if(found == 0)
                           {
                              // If the user is not a friend, display an error
                              strncpy(buffer, "You don't have a friend request from this user\n", BUF_SIZE - 1);
                              send_message_to_client(clients, clients_size, 0, clients[i].sock, buffer, red);
                           }
                        }
                     }

                     else if(strcmp(buffer, "/list_friends") == 0)
                     {
                        printf("The user %s tried to list the friends\n", clients[i].name);

                        strncpy(buffer, "List of friends:\n",                 BUF_SIZE - 1);
                        for(int u = 0; u < clients[i].friends_size; u++)
                        {
                           Client* friend = get_client_by_id(clients, clients_size, clients[i].friends[u]);
                           char str[BUF_SIZE];
                           sprintf(str, "%d", friend->party_id);

                           strncat(buffer, friend->name,                      BUF_SIZE - strlen(buffer) - 1);
                           
                           if(friend->sock == -1)
                           {
                              strncat(buffer, " (DISCONNECTED)\n",      BUF_SIZE - strlen(buffer) - 1);
                           }

                           else if(friend->party_id == -1)
                           {
                              strncat(buffer, " (LOBBY)\n",             BUF_SIZE - strlen(buffer) - 1);
                           }
                           
                           else
                           {
                              strncat(buffer, " (PARTY ID: ",           BUF_SIZE - strlen(buffer) - 1);
                              strncat(buffer, str,                      BUF_SIZE - strlen(buffer) - 1);
                              strncat(buffer, ")\n",                    BUF_SIZE - strlen(buffer) - 1);
                           }
                        }
                        send_message_to_client(clients, clients_size, 0, clients[i].sock, buffer, yellow);
                     }

                     else if(sscanf(buffer, "%s %s", command, other_username) == 2 && strncmp(command, "/delete_friend", strlen("/delete_friend")) == 0)
                     {
                        printf("The user %s tried to delete a friend: %s\n", clients[i].name, other_username);

                        // Check if the user exists
                        Client* other_user = get_client_by_username(clients, clients_size, other_username);

                        // If the user does not exist, display an error
                        if (other_user == NULL)
                        {
                           strncpy(buffer, "User not found\n", BUF_SIZE - 1);
                           send_message_to_client(clients, clients_size, 0, clients[i].sock, buffer, red);
                        }

                        // If the user exists, try to delete him
                        else
                        {
                           int found = 0;
                           for(int f = 0; f < clients[i].friends_size; f++)
                           {
                              if(clients[i].friends[f] == other_user->id)
                              {
                                 found = 1;

                                 // Remove the friend from the user list
                                 memmove(clients[i].friends + f, clients[i].friends + f + 1, (clients[i].friends_size - f - 1) * sizeof(int));
                                 clients[i].friends_size--;

                                 // Try to delete this user from the other user friends
                                 for(int f = 0; f < other_user->friends_size; f++)
                                 {
                                    if(other_user->friends[f] == clients[i].id)
                                    {
                                       memmove(other_user->friends + f, other_user->friends + f + 1, (other_user->friends_size - f - 1) * sizeof(int));
                                       other_user->friends_size--;
                                       break;
                                    }
                                 }

                                 // Send a message to the user
                                 strncpy(buffer, "Friend deleted\n", BUF_SIZE - 1);
                                 send_message_to_client(clients, clients_size, 0, clients[i].sock, buffer, green);

                                 break;
                              }
                           }

                           if(found == 0)
                           {
                              // If the user is not a friend, display an error
                              strncpy(buffer, "User is not a friend\n", BUF_SIZE - 1);
                              send_message_to_client(clients, clients_size, 0, clients[i].sock, buffer, red);
                           }
                        }
                     }
                     
                     else if(sscanf(buffer, "%s %s", command, other_username) == 2 && strncmp(command, "/read_bio", strlen("/read_bio")) == 0)
                     {
                        printf("The user %s tried to read the bio of %s\n", clients[i].name, other_username);

                        // Check if the user exists
                        Client* other_user = get_client_by_username(clients, clients_size, other_username);
                        
                        if(other_user == NULL)
                        {
                           strncpy(buffer, "User not found\n", BUF_SIZE - 1);
                           send_message_to_client(clients, clients_size, 0, clients[i].sock, buffer, red);
                        }

                        else
                        {
                           strncpy(buffer, other_user->bio, BUF_SIZE - 1);
                           send_message_to_client(clients, clients_size, 0, clients[i].sock, buffer, yellow);
                        }
                     }

                     else if(sscanf(buffer, "%s %[^\n]", command, message) == 2 && strncmp(command, "/update_bio", strlen("/update_bio")) == 0)
                     {
                        printf("The user %s tried to update his bio to: %s\n", clients[i].name, message);

                        // Update the bio
                        strncpy(clients[i].bio, message, BUF_SIZE - 1);

                        // Send a message to the user
                        strncpy(buffer, "Bio updated\n", BUF_SIZE - 1);
                        send_message_to_client(clients, clients_size, 0, clients[i].sock, buffer, green);
                     }

                     else if(sscanf(buffer, "%s %s", command, other_username) == 2 && strncmp(command, "/challenge", strlen("/challenge")) == 0)
                     {
                        printf("The user %s tried to challenge %s\n", clients[i].name, other_username);

                        // The user can not challenge himself
                        if(strcmp(clients[i].name, other_username) == 0){
                           strncpy(buffer, "You can not challenge yourself\n", BUF_SIZE - 1);
                           send_message_to_client(clients, clients_size, 0, clients[i].sock, buffer, red);
                        }

                        else
                        {
                           if(clients[i].party_id != -1)
                           {
                              strncpy(buffer, "You can not challenge someone while you are in a party\n", BUF_SIZE - 1);
                              send_message_to_client(clients, clients_size, 0, clients[i].sock, buffer, red);
                           }

                           else
                           {
                              // Check if the user exists
                              Client* other_user = get_client_by_username(clients, clients_size, other_username);

                              if (other_user == NULL)
                              {
                                 strncpy(buffer, "User not found\n", BUF_SIZE - 1);
                                 send_message_to_client(clients, clients_size, 0, clients[i].sock, buffer, red);
                              }

                              else
                              {
                                 // If the user is disconnected, display an error
                                 if(other_user->sock == -1)
                                 {
                                    strncpy(buffer, "User is disconnected\n", BUF_SIZE - 1);
                                    send_message_to_client(clients, clients_size, 0, clients[i].sock, buffer, red);
                                 }

                                 // If the user is already in a party, display an error
                                 else if(other_user->party_id != -1)
                                 {
                                    strncpy(buffer, "User is already in a party\n", BUF_SIZE - 1);
                                    send_message_to_client(clients, clients_size, 0, clients[i].sock, buffer, red);
                                 }

                                 else
                                 {
                                    // Check if the user is already in the list of challengers
                                    int found = 0;
                                    for(int c = 0; c < other_user->challengers_size; c++)
                                    {
                                       if(other_user->challengers[c] == clients[i].id)
                                       {
                                          found = 1;

                                          // If the user is already in the list of challengers, display an error
                                          strncpy(buffer, "You have already challenged this user\n", BUF_SIZE - 1);
                                          send_message_to_client(clients, clients_size, 0, clients[i].sock, buffer, red);

                                          break;
                                       }
                                    }

                                    // If the user is not in the list of challengers, send the challenge
                                    if(found == 0)
                                    {
                                       // Add the user to the list of challengers
                                       other_user->challengers[other_user->challengers_size] = clients[i].id;
                                       other_user->challengers_size++;

                                       // Send a message to the user
                                       strncpy(buffer, "Challenge sent\n", BUF_SIZE - 1);
                                       send_message_to_client(clients, clients_size, 0, clients[i].sock, buffer, green);

                                       // Send a message to the other user
                                       strncpy(buffer, "You have a new challenge from: ", BUF_SIZE - 1);
                                       strncat(buffer, clients[i].name, BUF_SIZE - strlen(buffer) - 1);
                                       send_message_to_client(clients, clients_size, 0, other_user->sock, buffer, yellow);
                                    }
                                 }
                              }
                           }
                        }
                     }

                     else if(strcmp(buffer, "/list_challenges") == 0)
                     {
                        printf("The user %s tried to list the challenges\n", clients[i].name);

                        strncpy(buffer, "List of challenges:\n",                 BUF_SIZE - 1);
                        for(int u = 0; u < clients[i].challengers_size; u++)
                        {
                           Client* challenger = get_client_by_id(clients, clients_size, clients[i].challengers[u]);

                           strncat(buffer, challenger->name,                      BUF_SIZE - strlen(buffer) - 1);
                        }
                        send_message_to_client(clients, clients_size, 0, clients[i].sock, buffer, yellow);
                     }

                     else if(sscanf(buffer, "%s %s", command, other_username) == 2 && strncmp(command, "/accept_challenge", strlen("/accept_challenge")) == 0)
                     {
                        printf("The user %s tried to accept a challenge from %s\n", clients[i].name, other_username);

                        if(clients[i].party_id != -1)
                        {
                           strncpy(buffer, "You can not accept a challenge while you are in a party\n", BUF_SIZE - 1);
                           send_message_to_client(clients, clients_size, 0, clients[i].sock, buffer, red);
                        }

                        else
                        {
                           // Check if the user exists
                           Client* other_user = get_client_by_username(clients, clients_size, other_username);

                           // If the user does not exist, display an error
                           if(other_user == NULL)
                           {
                              strncpy(buffer, "User not found\n", BUF_SIZE - 1);
                              send_message_to_client(clients, clients_size, 0, clients[i].sock, buffer, red);
                           }

                           // If the user exists, try to accept the challenge
                           else
                           {
                              int found = 0;
                              for(int f = 0; f < clients[i].challengers_size; f++)
                              {
                                 // If the user has a pending challenge from the other user, try to accept it
                                 if(clients[i].challengers[f] == other_user->id)
                                 {
                                    found = 1;

                                    // If the user who sent the challenge is disconnected, display an error
                                    if(other_user->sock == -1)
                                    {
                                       strncpy(buffer, "User is disconnected\n", BUF_SIZE - 1);
                                       send_message_to_client(clients, clients_size, 0, clients[i].sock, buffer, red);
                                    }

                                    // If the user who sent the challenge is in a party, display an error
                                    else if(other_user->party_id != -1)
                                    {
                                       strncpy(buffer, "User is already in a party\n", BUF_SIZE - 1);
                                       send_message_to_client(clients, clients_size, 0, clients[i].sock, buffer, red);
                                    }

                                    // If the user who sent the challenge is not in a party, accept the challenge
                                    else
                                    {
                                       // Remove the challenge from the user list
                                       memmove(clients[i].challengers + f, clients[i].challengers + f + 1, (clients[i].challengers_size - f - 1) * sizeof(int));
                                       clients[i].challengers_size--;

                                       // If this user also sent a challenge to the other user, remove that challenge in his list
                                       for(int c = 0; c < other_user->challengers_size; c++)
                                       {
                                          if(other_user->challengers[c] == clients[i].id)
                                          {
                                             // Remove the challenge in the other user list
                                             memmove(other_user->challengers + c, other_user->challengers + c + 1, (other_user->challengers_size - c - 1) * sizeof(int));
                                             other_user->challengers_size--;
                                             break;
                                          }
                                       }

                                       // Send a message to the user
                                       strncpy(buffer, "Challenge accepted\n", BUF_SIZE - 1);
                                       send_message_to_client(clients, clients_size, 0, clients[i].sock, buffer, green);

                                       // Send a message to the other user
                                       strncpy(buffer, clients[i].name, BUF_SIZE - 1);
                                       strncat(buffer, " has accepted your challenge\n", BUF_SIZE - strlen(buffer) - 1);
                                       send_message_to_client(clients, clients_size, 0, other_user->sock, buffer, yellow);

                                       // Create party and game
                                       Awale game_init = { {4, 4, 4, 4, 4, 4}, {4, 4, 4, 4, 4, 4}, 0, 0, 0, 0 };
                                       game = &game_init;
                                       Party party_init = {
                                          id: parties_size,
                                          player_one: &clients[i],
                                          player_two: other_user,
                                          spectators: { 0 },
                                          spectators_size: 0,
                                          mode: 0,
                                          game: game,
                                          status: 0,
                                          turn: 0
                                       };
                                       if(parties_size == MAX_CLIENTS)
                                       {
                                          strncpy(buffer, "The list of parties is full\n", BUF_SIZE - 1);
                                          send_message_to_client(clients, clients_size, 0, clients[i].sock, buffer, red);
                                          send_message_to_client(clients, clients_size, 0, other_user->sock, buffer, red);
                                          continue;
                                       }

                                       else
                                       {
                                          // Create party and game
                                          Awale game_init = { {4, 4, 4, 4, 4, 4}, {4, 4, 4, 4, 4, 4}, 0, 0, 0, 0 };
                                          game = &game_init;
                                          Party party_init = {
                                             id: parties_size,
                                             player_one: &clients[i],
                                             player_two: other_user,
                                             spectators: { 0 },
                                             spectators_size: 0,
                                             mode: 0,
                                             game: game,
                                             status: 0,
                                             turn: 0
                                          };

                                          // Add the party to the list of parties
                                          parties[parties_size] = party_init;
                                          parties_size++;
                                    
                                          party = get_party_by_id(parties, parties_size, parties_size-1);
                                          // Save the party id to the client room attribute
                                          clients[i].party_id = party->id;
                                          other_user->party_id = party->id;
                                          
                                          game = party->game;
                                          party->status = 1;
                                          party->turn = getRandomValue(party->player_one->sock, party->player_two->sock);
                                          // Print turn
                                          printf("The turn is %d\n", party->turn);
                                          game->turn = (party->turn == party->player_one->sock) ? 1 : 2;
                                          printf("The game turn is %d\n", game->turn);
                                          broadcast_message(clients, clients_size, 0, clients[i].party_id, display(*game, party->player_one->name, party->player_two->name), blue);
                                       }
                                    } 

                                    break;
                                 }
                              }

                              // If the user does not have a pending challenge from the other user, display an error
                              if(found == 0)
                              {
                                 strncpy(buffer, "You don't have a pending challenge from this user\n", BUF_SIZE - 1);
                                 send_message_to_client(clients, clients_size, 0, clients[i].sock, buffer, red);
                              }  

                           }
                        }
                     }

                     else if(sscanf(buffer, "%s %s", command, other_username) == 2 && strncmp(command, "/reject_challenge", strlen("/reject_challenge")) == 0)
                     {
                        printf("The user %s tried to reject a challenge from %s\n", clients[i].name, other_username);

                        // Check if the user exists
                        Client* other_user = get_client_by_username(clients, clients_size, other_username);

                        // If the user does not exist, display an error
                        if(other_user == NULL)
                        {
                           strncpy(buffer, "User not found\n", BUF_SIZE - 1);
                           send_message_to_client(clients, clients_size, 0, clients[i].sock, buffer, red);
                        }

                        // If the user exists, try to reject the challenge
                        else
                        {
                           int found = 0;
                           for(int f = 0; f < clients[i].challengers_size; f++)
                           {
                              // If the user has a pending challenge from the other user, try to reject it
                              if(clients[i].challengers[f] == other_user->id)
                              {
                                 found = 1;
                                 
                                 // Remove the challenge from the user list
                                 memmove(clients[i].challengers + f, clients[i].challengers + f + 1, (clients[i].challengers_size - f - 1) * sizeof(int));
                                 clients[i].challengers_size--;


                                 // Send a message to the user
                                 strncpy(buffer, "Challenge rejected\n", BUF_SIZE - 1);
                                 send_message_to_client(clients, clients_size, 0, clients[i].sock, buffer, green);

                                 break;
                              }
                           }

                           // If the user does not have a pending challenge from the other user, display an error
                           if(found == 0)
                           {
                              strncpy(buffer, "You don't have a pending challenge from this user\n", BUF_SIZE - 1);
                              send_message_to_client(clients, clients_size, 0, clients[i].sock, buffer, red);
                           }  
                        }

                     }

                     // List the ranking of users, sorted by user's ranking points
                     else if(strcmp(buffer, "/ranking") == 0)
                     {
                        printf("The user %s tried to list the ranking\n", clients[i].name);

                        // Sort the users by ranking points
                        qsort(clients, clients_size, sizeof(Client), compare_ranking);

                        char str[BUF_SIZE];                          

                        strncpy(buffer, "Ranking:\n", BUF_SIZE - 1);
                        for(int u = 0; u < clients_size; u++)
                        {
                           sprintf(str, "%d", clients[u].ranking);

                           strncat(buffer, clients[u].name, BUF_SIZE - strlen(buffer) - 1);
                           strncat(buffer, " - Points: ", BUF_SIZE - strlen(buffer) - 1);
                           strncat(buffer, str, BUF_SIZE - strlen(buffer) - 1);
                           strncat(buffer, "\n", BUF_SIZE - strlen(buffer) - 1);
                        }
                        send_message_to_client(clients, clients_size, 0, clients[i].sock, buffer, yellow);
                     }

                     else if(strcmp(buffer, "/help_1") == 0)
                     {
                        strncpy(buffer, "Available commands\n",                                                                 BUF_SIZE - 1);
                        strncat(buffer, "/update_bio <<bio>>: update your bio\n",                                               BUF_SIZE - strlen(buffer) - 1);
                        strncat(buffer, "/read_bio <<username>>: read the bio of an user\n",                                    BUF_SIZE - strlen(buffer) - 1);
                        strncat(buffer, "/list_users: list all the users connected\n",                                          BUF_SIZE - strlen(buffer) - 1);
                        strncat(buffer, "/chat <<message>>: send a message to all users in the lobby or party\n",               BUF_SIZE - strlen(buffer) - 1);
                        strncat(buffer, "/send <<username>> <<message>>: send a private message to an user\n",                  BUF_SIZE - strlen(buffer) - 1);
                        strncat(buffer, "/create_party <<visibility>>: create a party. Visibility: 0 = public, 1 = private\n",  BUF_SIZE - strlen(buffer) - 1);
                        strncat(buffer, "/list_parties: list all the parties\n",                                                BUF_SIZE - strlen(buffer) - 1);
                        strncat(buffer, "/join_party <<party_id>> <<mode>>: join a party. Mode: 0 = player, 1 = spectator\n",   BUF_SIZE - strlen(buffer) - 1);
                        strncat(buffer, "/leave_party: leave the current party\n",                                              BUF_SIZE - strlen(buffer) - 1);
                        
                        send_message_to_client(clients, clients_size, 0, clients[i].sock, buffer, yellow);
                     }

                     else if(strcmp(buffer, "/help_2") == 0)
                     {
                        strncpy(buffer, "Available commands\n",                                                                 BUF_SIZE - 1);
                        strncat(buffer, "/add_friend <<username>>: send a friend request to an user\n",                         BUF_SIZE - strlen(buffer) - 1);
                        strncat(buffer, "/list_friend_requests: list all the friend requests\n",                                BUF_SIZE - strlen(buffer) - 1);
                        strncat(buffer, "/accept_friend <<username>>: accept a friend request from an user\n",                  BUF_SIZE - strlen(buffer) - 1);
                        strncat(buffer, "/reject_friend <<username>>: reject a friend request from an user\n",                  BUF_SIZE - strlen(buffer) - 1);
                        strncat(buffer, "/list_friends: list all the friends\n",                                                BUF_SIZE - strlen(buffer) - 1);
                        strncat(buffer, "/delete_friend <<username>>: delete a friend\n",                                       BUF_SIZE - strlen(buffer) - 1);
                        strncat(buffer, "/challenge <<username>>: challenge an user\n",                                         BUF_SIZE - strlen(buffer) - 1);
                        strncat(buffer, "/list_challenges: list all the challenges\n",                                          BUF_SIZE - strlen(buffer) - 1);
                        strncat(buffer, "/accept_challenge <<username>>: accept a challenge from an user\n",                    BUF_SIZE - strlen(buffer) - 1);
                        strncat(buffer, "/reject_challenge <<username>>: reject a challenge from an user\n",                    BUF_SIZE - strlen(buffer) - 1);
                        strncat(buffer, "/replay_party <<party_id>>: replay a party\n",                                         BUF_SIZE - strlen(buffer) - 1);
                        strncat(buffer, "/ranking: list the ranking of users, sorted by user's ranking points\n",               BUF_SIZE - strlen(buffer) - 1);
                        
                        send_message_to_client(clients, clients_size, 0, clients[i].sock, buffer, yellow);
                     }

                     else
                     {
                        strncpy(buffer, "Unknown command\n",                                             BUF_SIZE - 1);
                        strncat(buffer, "Type /help_1 and /help_2 for a list of available commands\n",   BUF_SIZE - strlen(buffer) - 1);
                        send_message_to_client(clients, clients_size, 0, clients[i].sock, buffer, red);
                     }
                  }

                  // If the buffer does not start with "/" then it's a move or a chat in the lobby
                  else
                  {
                     // if the user is in the lobby, he can only chat
                     if(clients[i].party_id == -1)
                        broadcast_message(clients, clients_size, clients[i].id, clients[i].party_id, buffer, blue);
                     // user replaying a party
                     else if(clients[i].party_id == -2)
                     {
                        party = get_party_by_id(parties, parties_size, clients[i].replay_party_id);
                        send_message_to_client(clients, clients_size, 0, clients[i].sock, party->replay[clients[i].replay_position++], blue);
                        if (clients[i].replay_position == party->replay_size)
                        {
                           clients[i].replay_position = 0;
                           clients[i].replay_party_id = -1;
                           clients[i].party_id = -1;
                           send_message_to_client(clients, clients_size, 0, clients[i].sock, "Replay finished!", blue);
                        }
                     }
                     else
                     {
                        party = get_party_by_id(parties, parties_size, clients[i].party_id);
                        game = party->game;

                        // Waite for game to start
                        if ( !party->status ) {
                           strncpy(buffer, "Waiting for game to start\n", BUF_SIZE - 1);
                           write_client(clients[i].sock, buffer);
                        }
                        else if ( !game->finished && clients[i].sock == party->turn) {

                           // Add move to replay
                           strncpy(party->replay[party->replay_size], display(*game, party->player_one->name, party->player_two->name), BUF_SIZE - 1);
                           party->replay_size++;
                           
                           move = atoi(buffer);
                           if (check_move(game, game->turn, move, message)) {
                                 write_client(clients[i].sock, message);
                                 continue;
                           }

                           broadcast_message(clients, clients_size, 0, clients[i].party_id, display(*game, party->player_one->name, party->player_two->name), blue);

                           if (is_game_over(game))
                              game->finished = 1;
                           
                           party->turn = (party->turn == party->player_one->sock) ? party->player_two->sock : party->player_one->sock;
                           
                           // Send message to the other player
                           snprintf(message, BUF_SIZE, "Your move: ");
                           write_client(party->turn, message);

                        }
                        else if ( !game->finished && clients[i].sock != party->turn) {
                           // Player Two's move
                           snprintf(message, BUF_SIZE, "Waiting for the other player move...");
                           write_client(clients[i].sock, message);
                        }
                        else {
                           // Finish the game and display the winner
                           finish_game(game);
                           party->status = 2;
                           party->player_one->party_id = -1;
                           party->player_two->party_id = -1;

                           // Add end to replay
                           strncpy(party->replay[party->replay_size], display_winner(*game, party->player_one->name, party->player_two->name), BUF_SIZE - 1);
                           party->replay_size++;
                           broadcast_message(clients, clients_size, 0, clients[i].party_id, display_winner(*game, party->player_one->name, party->player_two->name), blue);
                           if (game->score_one > game->score_two)
                              party->player_one->ranking++;
                           else if (game->score_one < game->score_two)
                              party->player_two->ranking++;
                              
                        } 
                        
                     }
                  }
               }

               continue;
            }
         }
      }
   }

   clear_clients(clients, clients_size);
   end_connection(sock);
}

int compare_ranking(const void *a, const void *b)
{
   Client *client_a = (Client *)a;
   Client *client_b = (Client *)b;

   return client_b->ranking - client_a->ranking;
}

int user_exists(Client* clients, int clients_size, char* username)
{
   // Check in the list of clients. If the username exists, return the ID
   for(int i = 0; i < clients_size; i++)
   {
      if(strcmp(clients[i].name, username) == 0)
      {
         return clients[i].id;
      }
   }

   return -1;
}

Client* get_client_by_id(Client *clients, int clients_size, int client_id)
{
   for(int i = 0; i < clients_size; i++)
   {
      if(clients[i].id == client_id)
      {
         return &clients[i];
      }
   }

   return NULL;
}

Client* get_client_by_username(Client *clients, int clients_size, char* username)
{
   for(int i = 0; i < clients_size; i++)
   {
      if(strcmp(clients[i].name, username) == 0)
      {
         return &clients[i];
      }
   }

   return NULL;
}

Party* get_party_by_id(Party *parties, int parties_size, int party_id)
{
   for(int i = 0; i < parties_size; i++)
   {
      if(parties[i].id == party_id)
      {
         return &parties[i];
      }
   }

   return NULL;
}

void send_message_to_client(Client *clients, int clients_size, int sender_id, int receiver_socket, char *buffer, char *color)
{
   if(receiver_socket == -1) return;

   char message[BUF_SIZE];
   message[0] = 0;

   // Add the color to the message
   strncpy(message, color, BUF_SIZE - 1);
   
   // Add the sender name to the message
   if(sender_id == 0) strncat(message, "[Server]", BUF_SIZE - strlen(message) - 1);

   else
   {
      Client* sender = get_client_by_id(clients, clients_size, sender_id);
      strncat(message, "[", BUF_SIZE - strlen(message) - 1);
      strncat(message, sender->name, BUF_SIZE - strlen(message) - 1);
      strncat(message, "]", BUF_SIZE - strlen(message) - 1);
   }

   strncat(message, ": ", sizeof message - strlen(message) - 1);
   strncat(message, buffer, sizeof message - strlen(message) - 1);

   // Reset the color to white
   strncat(message, "\033[0;37m", BUF_SIZE - strlen(message) - 1);

   write_client(receiver_socket, message);
}

void broadcast_message(Client *clients, int clients_size, int sender_id, int party_id, char *buffer, char *color)
{
   for(int i = 0; i < clients_size; i++)
   {
      if(clients[i].party_id == party_id && clients[i].id != sender_id)
      {
         send_message_to_client(clients, clients_size, sender_id, clients[i].sock, buffer, color);
      }
   }
}

static void clear_clients(Client *clients, int clients_size)
{
   for(int i = 0; i < clients_size; i++)
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

int getRandomValue(int val1, int val2) {
    // Seed the random number generator with the current time
    srand((unsigned int)time(NULL));

    // Generate a random number 0 or 1
    int randomBit = rand() % 2;

    // Return either min or max based on the random bit
    return (randomBit == 0) ? val1 : val2;
}

int is_friend(Client* client, Client* friend) {
   for(int i = 0; i < client->friends_size; i++)
   {
      if(client->friends[i] == friend->sock)
      {
         return 1;
      }
   }

   return 0;
}

int main(int argc, char **argv)
{
   init();

   app();

   end();

   return EXIT_SUCCESS;
}
