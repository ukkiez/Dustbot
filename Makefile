CC=clang++
SRC=src
OBJ=obj
CFLAGS=-g -Wall -std=c++20
LIBS=-lsqlite3 -ldpp -lssl -lcrypto

SRCS=$(wildcard $(SRC)/*.cpp)
OBJS=$(patsubst $(SRC)/%.cpp,$(OBJ)/%.o,$(SRCS))

BINDIR=bin
BIN=$(BINDIR)/main

all: $(BIN)

release: CFLAGS=-Wall -O2 -DNDEBUG -std=c++20
release: clean
release: $(BIN)

$(BIN): $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o $@ $(LIBS)

$(OBJ)/%.o: $(SRC)/%.cpp
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	$(RM) -r $(BINDIR)/* $(OBJ)/*
