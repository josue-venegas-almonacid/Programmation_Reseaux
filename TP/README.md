# TP Programmation Reseaux
### Exercise 1
1. Compile the files using:

- *Server:* gcc socket_server.c -o socket_server

- *Client:* gcc socket_client.c -o socket_client

2. Run the files using:

- *Server:* ./socket_server 9999

- *Client:* ./socket_client "server ip address" 9999

2. 1. You can check your IP Address using:

- ip a

### Exercise 2
1. Compile the file using:

- *Client:* gcc socket_client.c -o socket_client

2. Run the file using:

- *Client:* ./socket_client 129.6.15.30 37

### Exercise 3
1. Compile the files using:

- *Server:* gcc socket_server.c -o socket_server

- *Client:* gcc socket_client.c -o socket_client

2. Run the files using:

- *Server:* ./socket_server 9999

- *Client 01, 02, etc.:* ./socket_client "server ip address" 9999

2. 1. You can check your IP Address using:

- ip a

### Exercise 4
1. Compile the files using:

- *Server:* gcc socket_server.c -lcurl -o socket_server

- *Server titles only:* gcc socket_server_titles.c -lcurl -o socket_server_titles

2. Run the files using:

- *Server:* ./socket_server

- *Server titles only:* ./socket_server_titles

### Exercise 5
1. Compile the file using:

- *Client:* gcc socket_client.c -o socket_client

2. Run the files using:

- *Client:* ./socket_client "port to scan" "client ip address"

2. 1. You can check your IP Address using:

- ip a
