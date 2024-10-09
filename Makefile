CC=clang++
SRC=src
OBJ=obj
CFLAGS=-g -Wall -std=c++20
LIBS=-lsqlite3 -ldpp -lssl -lcrypto

MAIN_OBJS=obj/main.o
TEST_OBJS=obj/test.o

BINDIR=bin
BIN_MAIN=$(BINDIR)/main
BIN_TEST=$(BINDIR)/test

all: $(BIN_MAIN) $(BIN_TEST)

release: CFLAGS=-Wall -O2 -DNDEBUG -std=c++20
release: clean
release: $(BIN)

$(BIN_MAIN): $(MAIN_OBJS)
	$(CC) $(CFLAGS) $(MAIN_OBJS) -o $@ $(LIBS)

$(BIN_TEST): $(TEST_OBJS)
	$(CC) $(CFLAGS) $(TEST_OBJS) -o $@ $(LIBS)

$(OBJ)/%.o: $(SRC)/%.cpp
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	$(RM) -r $(BINDIR)/* $(OBJ)/*
