# Tests du coeur commun du centre de controle (src/controle), le meme code
# dans les trois versions (Android et Pi en ont une copie). gcc de Linux
# (sous Windows : WSL), sans le Cardputer ni PlatformIO :
#
#   make -C tests
#
# Reprises du banc de GC-Explorer3 (abandonne), adaptees a la v4.0.0.

CC     = gcc
CFLAGS = -O1 -Wall -Wextra -Werror -std=gnu11 -I../src/controle
SRC    = ../src/controle

TESTS = test_msg test_ws test_rows test_clock test_net

all: $(TESTS)
	@fail=0; for t in $(TESTS); do ./$$t || fail=1; done; \
	if [ $$fail = 0 ]; then echo "TOUT EST VERT"; else echo "ECHECS"; exit 1; fi

test_msg: test_msg.c check.h $(SRC)/msg.c $(SRC)/msg.h
	$(CC) $(CFLAGS) -o $@ test_msg.c $(SRC)/msg.c

test_ws: test_ws.c check.h $(SRC)/ws.c $(SRC)/ws.h
	$(CC) $(CFLAGS) -o $@ test_ws.c $(SRC)/ws.c

test_rows: test_rows.c check.h $(SRC)/rows.c $(SRC)/rows.h
	$(CC) $(CFLAGS) -o $@ test_rows.c $(SRC)/rows.c

test_clock: test_clock.c check.h $(SRC)/clocksync.c $(SRC)/clocksync.h
	$(CC) $(CFLAGS) -o $@ test_clock.c $(SRC)/clocksync.c

test_net: test_net.c check.h $(SRC)/net.c $(SRC)/net.h $(SRC)/addr.c $(SRC)/addr.h
	$(CC) $(CFLAGS) -o $@ test_net.c $(SRC)/net.c $(SRC)/addr.c -pthread

clean:
	rm -f $(TESTS)

.PHONY: all clean
