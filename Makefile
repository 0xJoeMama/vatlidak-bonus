CFLAGS=-Wall -Wextra -Werror -std=c11
BINS=unbased

all: $(BINS) loong.db

unbased: tp.o

loong.db: create_db
	./create_db

clean:
	rm -rf $(BINS) loong.db *.o

