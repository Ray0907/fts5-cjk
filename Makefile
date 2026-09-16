UNAME := $(shell uname)
SQLITE_INC ?= $(if $(filter Darwin,$(UNAME)),/opt/homebrew/opt/sqlite/include,/usr/include)
CFLAGS ?= -O2 -Wall -fPIC -I$(SQLITE_INC)
EXT := $(if $(filter Darwin,$(UNAME)),dylib,so)

cjk.$(EXT): cjk.c
	$(CC) $(CFLAGS) -shared -o $@ $<

test: cjk.$(EXT)
	./test.sh

clean:
	rm -f cjk.so cjk.dylib

.PHONY: test clean
