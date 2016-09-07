#include <stdio.h>
#include <stdlib.h>
#include "support.h"

void quits(const char *msg) {
    puts(msg);
    exit(1);
}

int main(int argc, char** argv) {
    FILA2 fila;
    PFILA2 pFila = &fila;

    if (CreateFila2(pFila) != 0) {
        quits("CreateFila2 failed");
    }

    int content[10];
    int i;
    for (i = 0; i < 10; i++) {
        content[i] = 10*i;
        AppendFila2(pFila, content + i);
    }

    if (FirstFila2(pFila) != 0) {
        quits("FirstFila failed");
    }

    int *it;
    while ((it = (int *)GetAtIteratorFila2(pFila)) != NULL) {
        printf("iterador em: %p\n", it);
        printf("conteudo: %d\n\n", *it);
        if (NextFila2(pFila) != 0) {
            quits("NextFila2 failed");
        }
    }

    return 0;
}
