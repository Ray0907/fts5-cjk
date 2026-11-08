UNAME := $(shell uname)
CFLAGS ?= -O2 -Wall -fPIC
EXT := $(if $(filter Darwin,$(UNAME)),dylib,so)

cjk.$(EXT): cjk.c
	$(CC) $(CFLAGS) -Ivendor/sqlite -shared -o $@ $<

test: cjk.$(EXT)
	./test.sh

clean:
	rm -f cjk.so cjk.dylib

.PHONY: test clean
