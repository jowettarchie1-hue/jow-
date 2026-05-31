CC     = gcc
CFLAGS = -O2 -Wall

all: jow

jow: jow.c
	$(CC) $(CFLAGS) -o jow jow.c

install: jow
	cp jow $(HOME)/.local/bin/jow
	@echo "installed — type 'jow' anywhere"

uninstall:
	rm -f $(HOME)/.local/bin/jow

clean:
	rm -f jow
