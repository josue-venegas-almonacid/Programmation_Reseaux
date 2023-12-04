#ifndef CLIENT_H
#define CLIENT_H

#include "server2.h"

typedef struct
{
   // Socket for the connection between the client and the server
   SOCKET sock;

   // Client's ID
   int id;

   // Id of the room where the client is
   // -1 if the client is in the lobby
   // party_id if the client is in a party
   // -2 if the client is in the replay room
   int party_id;

   // Client's name
   char name[BUF_SIZE];

   // Client's bio
   char bio[BUF_SIZE];

   // List of clients who challenged this client
   int challengers[MAX_CLIENTS];
   int challengers_size;

   // List of clients who want to be friends with this client
   int friend_requests[MAX_CLIENTS];
   int friend_requests_size;

   // List of friends
   int friends[MAX_CLIENTS];
   int friends_size;

   // Replay data
   int replay_party_id;
   int replay_position;

   // Client ranking
   int ranking;
}Client;

#endif /* guard */
