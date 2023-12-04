all: serverfile clientfile

serverfile: ./Serveur/server2.c ./Serveur/awale.c
	gcc ./Serveur/server2.c ./Serveur/awale.c -o serverfile

clientfile: ./Client/client2.c
	gcc ./Client/client2.c -o clientfile

.PHONY: clean
clean:
	rm -f serverfile clientfile
