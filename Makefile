UNAME := $(shell uname)
CFLAGS ?= -O2 -Wall -fPIC
EXT := $(if $(filter Darwin,$(UNAME)),dylib,so)
SAN := -fsanitize=address,undefined -fno-sanitize-recover=undefined -fno-omit-frame-pointer -g -O1
SQLITE_LIB ?= $(if $(filter Darwin,$(UNAME)),-L/opt/homebrew/opt/sqlite/lib,) -lsqlite3

cjk.$(EXT): cjk.c
	$(CC) $(CFLAGS) -Iinclude -shared -o $@ $<

test: cjk.$(EXT)
	./test.sh

cjk-asan.$(EXT): cjk.c
	$(CC) -Wall -fPIC $(SAN) -Iinclude -shared -o $@ $<

asan-test: cjk-asan.$(EXT) test/asan.c
	$(CC) -Wall $(SAN) -Iinclude -o $@ test/asan.c $(SQLITE_LIB)
	./$@ ./cjk-asan.$(EXT)

static-test: cjk.c test/static.c
	$(CC) -Wall -DSQLITE_CORE -Iinclude -o $@ test/static.c cjk.c $(SQLITE_LIB)
	./$@

clean:
	rm -rf cjk.so cjk.dylib cjk-asan.so cjk-asan.dylib cjk-asan.dylib.dSYM asan-test asan-test.dSYM static-test

.PHONY: test asan-test static-test clean
