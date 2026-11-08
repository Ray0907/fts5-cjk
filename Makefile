UNAME := $(shell uname)
CFLAGS ?= -O2 -Wall -fPIC
EXT := $(if $(filter Darwin,$(UNAME)),dylib,so)
SAN := -fsanitize=address,undefined -fno-sanitize-recover=undefined -fno-omit-frame-pointer -g -O1
SQLITE_LIB ?= $(if $(filter Darwin,$(UNAME)),-L/opt/homebrew/opt/sqlite/lib,) -lsqlite3

cjk.$(EXT): cjk.c
	$(CC) $(CFLAGS) -Ivendor/sqlite -shared -o $@ $<

test: cjk.$(EXT)
	./test.sh

cjk-asan.$(EXT): cjk.c
	$(CC) -Wall -fPIC $(SAN) -Ivendor/sqlite -shared -o $@ $<

asan-test: cjk-asan.$(EXT) test/asan.c
	$(CC) -Wall $(SAN) -Ivendor/sqlite -o $@ test/asan.c $(SQLITE_LIB)
	./$@ ./cjk-asan.$(EXT)

clean:
	rm -rf cjk.so cjk.dylib cjk-asan.so cjk-asan.dylib cjk-asan.dylib.dSYM asan-test asan-test.dSYM

.PHONY: test asan-test clean
