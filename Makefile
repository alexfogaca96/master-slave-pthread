APPS    = ./apps
SRC     = ./src
PTHREAD = -lpthread
FLAGS   = -Wall

SERVER_APP = ServerApplication
CLIENT_APP = ClientApplication

all:
	gcc $(SRC)/$(SERVER_APP).c -o $(APPS)/$(SERVER_APP) $(PTHREAD) $(FLAGS)
	gcc $(SRC)/$(CLIENT_APP).c -o $(APPS)/$(CLIENT_APP) $(FLAGS)

server:
	$(APPS)/$(SERVER_APP)

client:
	$(APPS)/$(CLIENT_APP)