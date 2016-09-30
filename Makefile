#
# Makefile ESQUELETO
#
# OBRIGATÓRIO ter uma regra "all" para geração da biblioteca e de uma
# regra "clean" para remover todos os objetos gerados.
#
# NECESSARIO adaptar este esqueleto de makefile para suas necessidades.
#  1. Cuidado com a regra "clean" para não apagar o "fila2.o"
#
# OBSERVAR que as variáveis de ambiente consideram que o Makefile está no diretótio "cthread"
#

CC=gcc -Wall
LIB_DIR=./lib
INC_DIR=./include
BIN_DIR=./bin
SRC_DIR=./src
TST_DIR=./teste

.PHONY: all
all: $(LIB_DIR)/libcthread.a

.PHONY: run_tests
run_tests: $(BIN_DIR)/no_block.out $(BIN_DIR)/cjoin.out $(BIN_DIR)/cwait.out
	@echo ">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> no_block.out"
	$(BIN_DIR)/no_block.out
	@echo ">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> cjoin.out"
	$(BIN_DIR)/cjoin.out
	@echo ">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> cwait.out"
	$(BIN_DIR)/cwait.out

$(LIB_DIR)/libcthread.a: $(BIN_DIR)/cthread.o
	ar -cvq $(LIB_DIR)/libcthread.a $(BIN_DIR)/cthread.o $(BIN_DIR)/support.o

$(BIN_DIR)/no_block.out: $(LIB_DIR)/libcthread.a
	$(CC) -I $(INC_DIR) -o $(BIN_DIR)/no_block.out $(TST_DIR)/no_block.c -L$(LIB_DIR) -lcthread

$(BIN_DIR)/cwait.out: $(LIB_DIR)/libcthread.a
	$(CC) -I $(INC_DIR) -o $(BIN_DIR)/cwait.out $(TST_DIR)/cwait.c -L$(LIB_DIR) -lcthread

$(BIN_DIR)/cjoin.out: $(LIB_DIR)/libcthread.a
	$(CC) -I $(INC_DIR) -o $(BIN_DIR)/cjoin.out $(TST_DIR)/cjoin.c -L$(LIB_DIR) -lcthread

$(BIN_DIR)/cthread.o: $(SRC_DIR)/cthread.c
	$(CC) -I $(INC_DIR) -c $(SRC_DIR)/cthread.c -o $(BIN_DIR)/cthread.o

cthread.tar.gz: clean
	tar -zcvf cthread.tar.gz bin include lib src exemplos teste Makefile relatorio.pdf

.PHONY: clean
clean:
	rm -rf $(BIN_DIR)/no_block.out $(BIN_DIR)/cwait.out $(BIN_DIR)/cjoin.out $(BIN_DIR)/cthread.o $(LIB_DIR)/libcthread.a cthread.tar.gz
