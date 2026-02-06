CFLAGS=-Wall -Wextra -Werror
BINS=unbased client

all: $(BINS) loong.db

unbased: tp.o perf.o config.h

client: config.h

loong.db: create_db
	./create_db

clean:
	rm -rf $(BINS) loong.db *.o

