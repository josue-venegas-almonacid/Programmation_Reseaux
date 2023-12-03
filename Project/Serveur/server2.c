/* Libraries */
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

/** TODO: 
 * - Add bio
 * - Add friends
 * - Add challenges
 * - Add private party
 * - Check cases when the user disconnects and he is in a party (being the owner, the second player or spectator)
 * - Add game
 * - Save a finished game
 * - Replay a saved game
 * - Check cases when the user leaves a party and the game has started
 * - Check cases when the user disconnects and he is in a party and the game has started
 * - Implement dynamic memory allocation (realloc, free) to list of clients and parties
 * - Implement fork() to handle multiple games at the same time
 * - Add ranking
 * - (OPTIONAL) improve the console interface for the client
 * - (OPTIONAL) fix: if the user is typing while a message is received, the message is not displayed correctly
 * - (OPTIONAL) display the /help commands depending on the context (lobby, party, game)
*/

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
   int max = sock;
   fd_set rdfs;

   // The buffer for the messages
   char buffer[BUF_SIZE];
   char command[BUF_SIZE];
   char receiver_username[BUF_SIZE];
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

      // Add socket of each client to the set
      for(int s = 0; s < clients_size; s++)
      {
         FD_SET(clients[s].sock, &rdfs);
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
         printf("New client trying to connect...\n");
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

         /* what is the new maximum socket number ? */
         max = csock > max ? csock : max;

         FD_SET(csock, &rdfs);

         // if the list of users is not full, add the new user
         Client c = { 
            sock: csock,
            room_id: -1,
            name: "",
            bio: "",
            challengers: { 0 },
            challengers_size: 0,
            friends: { 0 },
            friends_size: 0
         };

         strncpy(c.name, buffer, BUF_SIZE - 1);
         clients[clients_size] = c;
         clients_size++;

         printf("Client connected as %s with socket %i\n", c.name, c.sock);

         // TODO: If the list of users is full, realloc
      }

      // Listen to all clients
      else
      {
         for(int i = 0; i < clients_size; i++)
         {
            if(FD_ISSET(clients[i].sock, &rdfs))
            {
               int client_id = clients[i].sock;
               
               int c = read_client(client_id, buffer);

               // Client disconnected
               if(c == 0)
               {
                  printf("The user %s disconnected\n", clients[i].name);

                  // TODO: If the client is in a game, stop it, leave the party and send a message to the other players

                  closesocket(clients[i].sock);
                  remove_client(clients, i, &clients_size);
                  strncpy(buffer, clients[i].name, BUF_SIZE - 1);
                  strncat(buffer, " disconnected !", BUF_SIZE - strlen(buffer) - 1);

                  for(int j = 0; j < clients_size; j++)
                  {
                     int receiver_id = clients[j].sock;
                     send_message_to_client(clients, clients_size, 0, receiver_id, buffer, yellow);
                  }
               }

               // Client sent a message
               else
               {
                  // If the buffer starts with "/" it's a command
                  if(buffer[0] == '/')
                  {
                     if(strcmp(buffer, "/list_users") == 0)
                     {
                        printf("The user %s listed the users\n", clients[i].name);

                        strncpy(buffer, "List of users:\n",             BUF_SIZE - 1);
                        for(int u = 0; u < clients_size; u++)
                        {
                           strncat(buffer, clients[u].name,             BUF_SIZE - strlen(buffer) - 1);
                           if (clients[u].room_id == -1)
                           {
                              strncat(buffer, " (LOBBY)\n",             BUF_SIZE - strlen(buffer) - 1);
                           }
                           else
                           {
                              strncat(buffer, " (PARTY ID: ",           BUF_SIZE - strlen(buffer) - 1);
                              strncat(buffer, "TODO",                   BUF_SIZE - strlen(buffer) - 1);
                              strncat(buffer, ")\n",                    BUF_SIZE - strlen(buffer) - 1);
                           }
                        }
                        send_message_to_client(clients, clients_size, 0, clients[i].sock, buffer, yellow);
                     }

                     else if(sscanf(buffer, "%s %[^\n]", command, message) == 2 && strncmp(command, "/chat", strlen("/chat")) == 0)
                     {
                        printf("The user %s sent a message to the room %d\n", clients[i].name, clients[i].room_id);
                        
                        // Send the message to all the users in the room (lobby/party)
                        broadcast_message(clients, clients_size, clients[i].sock, clients[i].room_id, message, blue);
                     }

                     else if(sscanf(buffer, "%s %s %[^\n]", command, receiver_username, message) == 3 && strncmp(command, "/send", strlen("/send")) == 0)
                     {
                        printf("The user %s sent a message to %s\n", clients[i].name, receiver_username);

                        // The user can not send a message to himself
                        if(strcmp(clients[i].name, receiver_username) == 0){
                           strncpy(buffer, "You can not send a message to yourself\n", BUF_SIZE - 1);
                           send_message_to_client(clients, clients_size, 0, clients[i].sock, buffer, red);
                        }

                        // Try to send the message
                        else
                        {
                           Client* receiver = get_client_by_username(clients, clients_size, receiver_username);

                           // If the receiver does not exist, display an error
                           if(receiver == NULL) 
                           {
                              strncpy(buffer, "User not found\n", BUF_SIZE - 1);
                              send_message_to_client(clients, clients_size, 0, clients[i].sock, buffer, red);
                           }

                           // If the receiver exists, send the message
                           else
                           {
                              send_message_to_client(clients, clients_size, clients[i].sock, receiver->sock, message, yellow);
                              strncpy(buffer, "Message sent\n", BUF_SIZE - 1);
                              send_message_to_client(clients, clients_size, 0, clients[i].sock, buffer, green);
                           }
                        }
                     }

                     else if(sscanf(buffer, "%s %s", command, mode) == 2 && strncmp(command, "/create_party", strlen("/create_party")) == 0)
                     {
                        // Check if the user is in a party already
                        if(clients[i].room_id != -1)
                        {
                           strncpy(buffer, "You are already in a party\n", BUF_SIZE - 1);
                           send_message_to_client(clients, clients_size, 0, clients[i].sock, buffer, red);
                        }

                        else
                        {
                           // TODO: If the list of parties is full, realloc

                           int party_visibility = atoi(mode);

                           if (party_visibility != 0 && party_visibility != 1)
                           {
                              strncpy(buffer, "Invalid party mode\n", BUF_SIZE - 1);
                              send_message_to_client(clients, clients_size, 0, clients[i].sock, buffer, red);
                              continue;
                           }

                           printf("The user %s created a %s party\n", clients[i].name, party_visibility == 0 ? "public" : "private");

                           // hard-coded party
                           // DELETE THIS AFTER ADDING YOUR CODE
                           Awale game_init = { {4, 4, 4, 4, 4, 4}, {4, 4, 4, 4, 4, 4}, 0, 0, 1, 0 };
                           game = &game_init;
                           Party party = {
                              id: parties_size,
                              player_one: &clients[i],
                              player_two: NULL,
                              spectators: { 0 },
                              spectators_size: 0,
                              mode: party_visibility,
                              game: game,
                              game_started: 0,
                              turn: clients[i].sock
                           };
                           
                           // Add the party to the list of parties
                           parties[parties_size] = party;
                           parties_size++;

                           // Join the party
                           // Save the party id to the client room attribute
                           clients[i].room_id = party.id;

                           // Display party to the player
                           send_message_to_client(clients, clients_size, 0, clients[i].sock, display(*game, party.player_one->name, "Second player score"), yellow);
                        }

                     }

                     else if(strcmp(buffer, "/list_parties") == 0)
                     {
                        printf("The user %s listed the parties\n", clients[i].name);

                        // TODO: Display the owner, visibility, players and spectators of each party
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
                        if(clients[i].room_id != -1)
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
                                       party->game_started = 1;
                                       clients[i].room_id = party->id;
                                       printf("The user %s joined the party %d as a player\n", clients[i].name, party->id);
                                       broadcast_message(clients, clients_size, 0, clients[i].room_id, display(*game, party->player_one->name, party->player_two->name), blue);
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
                                          // TODO: Realloc the list of spectators
                                          strncpy(buffer, "The party is full\n", BUF_SIZE - 1);
                                          send_message_to_client(clients, clients_size, 0, clients[i].sock, buffer, red);
                                          continue;
                                       }
                                       party->spectators[party->spectators_size] = clients[i].sock;
                                       party->spectators_size++;
                                       clients[i].room_id = party->id;
                                       printf("User %s joined the party %d as a spectator\n", clients[i].name, clients[i].room_id);
                                       broadcast_message(clients, clients_size, 0, clients[i].room_id, display(*game, party->player_one->name, party->player_two->name), blue);
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
                        printf("The user %s left the party %d\n", clients[i].name, clients[i].room_id);
                        // Check if the user is in a party
                        if(clients[i].room_id == -1)
                        {
                           strncpy(buffer, "You are not in a party\n", BUF_SIZE - 1);
                           send_message_to_client(clients, clients_size, 0, clients[i].sock, buffer, red);
                        }

                        // TODO: If the user is in the middle of a game, stop it and send the message to everyone in the party
                        // TODO: If the user is the owner of the party, kick everyone, delete the party and send the message to everyone in the party
                        // Or maybe just transfer the ownership to the player2 and delete the party only if there is not player1 nor player2
                        else
                        {
                           // Save the lobby id to the client room attribute
                           clients[i].room_id = -1;

                           // Send a message to the user
                           strncpy(buffer, "You left the party\n", BUF_SIZE - 1);
                           send_message_to_client(clients, clients_size, 0, clients[i].sock, buffer, yellow);
                        }
                     }

                     else if(sscanf(buffer, "%s %[^\n]", command, receiver_username) == 2 && strncmp(command, "/add_friend", strlen("/add_friend")) == 0)
                     {
                        printf("The user %s sent a friend request to %s\n", clients[i].name, receiver_username);

                        // The user can not add himself as a friend
                        if(strcmp(clients[i].name, receiver_username) == 0){
                           strncpy(buffer, "You can not add yourself as a friend\n", BUF_SIZE - 1);
                           send_message_to_client(clients, clients_size, 0, clients[i].sock, buffer, red);
                        }

                        // Try to send the friend request
                        else
                        {
                           Client* receiver = get_client_by_username(clients, clients_size, receiver_username);
                           
                           // If the receiver does not exist, display an error
                           if(receiver == NULL) 
                           {
                              strncpy(buffer, "User not found\n", BUF_SIZE - 1);
                              send_message_to_client(clients, clients_size, 0, clients[i].sock, buffer, red);
                           }

                           // If the receiver exists, send the friend request
                           else
                           {
                              int can_add = 1;

                              // If the receiver is already a friend, display an error
                              for(int f = 0; f < clients[i].friends_size; f++)
                              {
                                 if(clients[i].friends[f] == receiver->sock)
                                 {
                                    strncpy(buffer, "User is already a friend\n", BUF_SIZE - 1);
                                    send_message_to_client(clients, clients_size, 0, clients[i].sock, buffer, red);
                                    can_add = 0;
                                 }
                              }

                              // If the receiver has already a pending friend request from the sender, display an error
                              for(int f = 0; f < receiver->friend_requests_size; f++)
                              {
                                 if(receiver->friend_requests[f] == clients[i].sock)
                                 {
                                    strncpy(buffer, "Friend request already sent\n", BUF_SIZE - 1);
                                    send_message_to_client(clients, clients_size, 0, clients[i].sock, buffer, red);
                                    can_add = 0;
                                 }
                              }

                              if(can_add == 1)
                              {
                                 // Add the client id into the receiver friend requests
                                 receiver->friend_requests[receiver->friend_requests_size] = clients[i].sock;
                                 receiver->friend_requests_size++;

                                 // Add the receiver id into the client friends to test
                                 clients[i].friends[clients[i].friends_size] = receiver->sock;
                                 clients[i].friends_size++;

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

                     else if(strcmp(buffer, "/help") == 0)
                     {
                        strncpy(buffer, "Available commands\n",                                                                  BUF_SIZE - 1);
                        strncat(buffer, "/list_users: list all the users connected\n",                                           BUF_SIZE - strlen(buffer) - 1);
                        strncat(buffer, "/chat <<message>>: send a message to all users in the lobby or party\n",                BUF_SIZE - strlen(buffer) - 1);
                        strncat(buffer, "/send <<username>> <<message>>: send a private message to an user\n",                   BUF_SIZE - strlen(buffer) - 1);
                        strncat(buffer, "/create_party <<visibility>>: create a party. Visibility: 0 = public, 1 = private\n",   BUF_SIZE - strlen(buffer) - 1);
                        strncat(buffer, "/list_parties: list all the parties\n",                                                 BUF_SIZE - strlen(buffer) - 1);
                        strncat(buffer, "/join_party <<party_id>> <<mode>>: join a party. Mode: 0 = player, 1 = spectator\n",    BUF_SIZE - strlen(buffer) - 1);
                        strncat(buffer, "/leave_party: leave the current party\n",                                               BUF_SIZE - strlen(buffer) - 1);
                        strncat(buffer, "/add_friend <<username>>: send a friend request to an user\n",                          BUF_SIZE - strlen(buffer) - 1);
                        
                        send_message_to_client(clients, clients_size, 0, clients[i].sock, buffer, yellow);
                     }

                     else
                     {
                        strncpy(buffer, "Unknown command\n",                               BUF_SIZE - 1);
                        strncat(buffer, "Type /help for a list of available commands\n",   BUF_SIZE - strlen(buffer) - 1);
                        send_message_to_client(clients, clients_size, 0, clients[i].sock, buffer, red);
                     }
                  }

                  // If the buffer does not start with "/" then it's a move or a chat in the lobby
                  else
                  {
                     // if the user is in the lobby, he can only chat
                     if(clients[i].room_id == -1)
                        broadcast_message(clients, clients_size, clients[i].sock, clients[i].room_id, buffer, blue);
                     else
                     {
                        party = get_party_by_id(parties, parties_size, clients[i].room_id);
                        game = party->game;

                        // Waite for game to start
                        if ( !party->game_started ) {
                           strncpy(buffer, "Waiting for game to start\n", BUF_SIZE - 1);
                           write_client(clients[i].sock, buffer);
                        }
                        else if ( !game->finished && clients[i].sock == party->turn) {
                           
                           move = atoi(buffer);
                           if (check_move(game, game->turn, move, message)) {
                                 write_client(clients[i].sock, message);
                                 continue;
                           }

                           broadcast_message(clients, clients_size, 0, clients[i].room_id, display(*game, party->player_one->name, party->player_two->name), blue);
                           
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
                           broadcast_message(clients, clients_size, 0, clients[i].room_id, display_winner(*game, party->player_one->name, party->player_two->name), blue);
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

Client* get_client_by_id(Client *clients, int clients_size, int client_id)
{
   for(int i = 0; i < clients_size; i++)
   {
      if(clients[i].sock == client_id)
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

void send_message_to_client(Client *clients, int clients_size, int sender_id, int receiver_id, char *buffer, char *color)
{
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

   write_client(receiver_id, message);
}

void broadcast_message(Client *clients, int clients_size, int sender_id, int room_id, char *buffer, char *color)
{
   for(int i = 0; i < clients_size; i++)
   {
      if(clients[i].room_id == room_id && clients[i].sock != sender_id)
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

int getRandomValue(int min, int max) {
    // Seed the random number generator with the current time
    srand((unsigned int)time(NULL));

    // Generate a random number 0 or 1
    int randomBit = rand() % 2;

    // Return either min or max based on the random bit
    return (randomBit == 0) ? min : max;
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
