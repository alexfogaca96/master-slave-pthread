APPS  = ./apps
SRC   = ./src
FLAGS = -lpthread

SERVER_APP = ServerApplication

all:
	g++ $(SRC)/$(SERVER_APP).cpp -o $(APPS)/$(SERVER_APP) $(FLAGS)

run:
	$(APPS)/$(SERVER_APP)


