CC=gcc -Wall
LIB_DIR=../lib
INC_DIR=../include
BIN_DIR=../bin
SRC_DIR=../src

.PHONY: all
all: $(LIB_DIR)/libcthread.a

$(LIB_DIR)/libcthread.a: $(BIN_DIR)/cthread.o
	ar -cvq $(LIB_DIR)/libcthread.a $(BIN_DIR)/cthread.o $(BIN_DIR)/support.o

no_block.out: $(LIB_DIR)/libcthread.a no_block.c
	$(CC) -I $(INC_DIR) -o no_block.out no_block.c -L$(LIB_DIR) -lcthread

cwait.out: $(LIB_DIR)/libcthread.a cwait.c
	$(CC) -I $(INC_DIR) -o cwait.out cwait.c -L$(LIB_DIR) -lcthread

cjoin.out: $(LIB_DIR)/libcthread.a cjoin.c
	$(CC) -I $(INC_DIR) -o cjoin.out cjoin.c -L$(LIB_DIR) -lcthread

$(BIN_DIR)/cthread.o: $(SRC_DIR)/cthread.c
	$(CC) -I $(INC_DIR) -c $(SRC_DIR)/cthread.c -o $(BIN_DIR)/cthread.o

.PHONY: clean
clean:
	rm -rf $(LIB_DIR)/libcthread.a no_block.out cwait.out cjoin.out $(BIN_DIR)/cthread.o
