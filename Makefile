CFLAGS=-Wall -Wextra -Werror $(EXTRAFLAGS)
BINS=unbased client based

all: $(BINS)
mock_db: loong.db

unbased: tp.o perf.o config.h db.h
based: tp.o perf.o config.h db.h

client: config.h db.h tp.o

create_db: db.h

loong.db: create_db
	./create_db

clean:
	rm -rf $(BINS) loong.db *.o

.PHONY: clean all bins mock_db
