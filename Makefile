CFLAGS=-Wall -Wextra -Werror
BINS=unbased client based

all: $(BINS) loong.db

unbased: tp.o perf.o config.h db.h
based: tp.o perf.o config.h db.h

client: config.h db.h tp.o

create_db: db.h

loong.db: create_db
	./create_db

clean:
	rm -rf $(BINS) loong.db *.o

