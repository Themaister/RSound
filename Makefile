
include config.mk

SERVER = rsoundserv

CLIENT = rsound
CLIENTOBJECTS = client.o endian.o

PREFIX = .

all : $(SERVER) $(CLIENT)

$(SERVER) : $(SERVOBJECTS)
	$(CC) -o $(SERVER) $(SERVOBJECTS) $(LIBS)

$(CLIENT) : $(CLIENTOBJECTS)
	$(CC) -o $(CLIENT) $(CLIENTOBJECTS)

rsound.o : rsound.c
	$(CC) $(CFLAGS) -c rsound.c

%.o : %.c
	$(CC) $(CFLAGS) $(EXTRAFLAGS) -c $< 


clean: clean-obj clean-bin

clean-all: clean
	rm config.mk

clean-obj:
	rm *.o

clean-bin:
	rm $(SERVER)
	rm $(CLIENT)

install: all
	mkdir -p $(PREFIX)/bin
	cp -f  $(SERVER) $(PREFIX)/bin/
	cp -f  $(CLIENT) $(PREFIX)/bin/
	chmod 755 $(PREFIX)/bin/$(SERVER)
	chmod 755 $(PREFIX)/bin/$(CLIENT)

install-server: $(SERVER)
	mkdir -p $(PREFIX)/bin
	cp -f  $(SERVER) $(PREFIX)/bin/
	chmod 755 $(PREFIX)/bin/$(SERVER)

install-client: $(CLIENT)
	mkdir -p $(PREFIX)/bin
	cp -f  $(CLIENT) $(PREFIX)/bin/
	chmod 755 $(PREFIX)/bin/$(CLIENT)

server : $(SERVER)

client : $(CLIENT)
