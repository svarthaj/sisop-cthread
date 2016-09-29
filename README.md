# cthreads.h

## Primeiro trabalho de INF01142 - Sistemas Operacionais I

O objetivo deste trabalho é a aplicação dos conceitos de sistemas operacionais relacionados ao escalonamento e ao contexto de execução, o que inclui a criação, chaveamento e destruição de contextos. Esses conceitos serão empregados no desenvolvimento de uma biblioteca de threads em nível de usuário. Essa biblioteca de threads, denominada de compact thread (ou apenas cthread), deverá oferecer capacidades básicas para programação com threads como criação, execução, sincronização, término e trocas de contexto.

```
                                                        end of
                               dispatcher              function
+----------+          +-------+          +-------------+     +-------------+
|          | ccreate  |       +---------->             |     |             |
|  create  +---------->  apt  |          |  executing  +----->  terminate  |
|          |          |       <----------+             |     |             |
+----------+          +---^---+  cyield  +------+------+     +-------------+
                          |                     |
                          |                     |
                          |                     |
            unblock cjoin |                     | cjoin
                  csignal |                     | cwait
                          |                     |
                          |    +-----------+    |
                          |    |           |    |
                          +----+  blocked  <----+
                               |           |
                               +-----------+
```

## Perguntas

- loop:
	getcontext
	executa_dispatcher
	
	variável de retorno não escalável [RESOLVIDO: ret_code é salvo na pilha de cada TCB]

## Uma breve história das coisas

### ccreate

1. verifica se existe a thread main
	- se não, cria thread main
	- TCB(0).context.uc_link = NULL /* se a main terminar a execução, a thread do SO encerra. */
1. cria uma nova thread _N_
1. faz _N_.context = início da função `start()`
1. faz _N_.context.uclink = makecontext(dispatcher) /* se uma thread de usuário terminar corretamente, o dispatcher é chamado */
1. salva _N_ na árvore de threads /* decidir o tipo de árvore - tenho a implementação da BTree e AVL */
1. adicionar _N_ à fila de aptos

### dispatcher

1. gerar um número _r_
1. busca na fila de aptos o tid _c_ mais próximo a _r_. Em caso de empate, seleciona o menor tid.
1. remove TCB(_c_) da fila de aptos
1. executando passa a ser _c_
1. muda contexto para TCB(_c_).context

### cyield

1. ret_code = 0
1. salva o contexto da thread atual
1. if (ret_code == 0)
	1. ret_code = 1
	1. muda o estado da thread atual para apta
	1. colocar a thread atual no fim da fila das aptas
	1. chama o dispatcher

### cjoin

1. verifica se TCBtarget(_tid_) está na árvore de threads e se TCBtarget(status!=4) /* a thread precisa existir e não ter terminado */
	- se não, retorna código de erro
1. busca na fila de threads esperadas por TCBtarget(_tid_)
	- se TCBtarget(_tid_) já é esperada por outra thread, retorna código de erro
1. move TCBcaller para lista de bloqueados
1. altera TCBcaller(status=3) /* status bloqueado */
1. aguarda TCBtarget(status=4) /* raise flags, pooling.. como fazer? */
1. altera TCBcaller(status=1)
1. move TCBcaller para lista de aptos
1. retorna código de sucesso 

### csem_init

1. csem_t.count = K /* K é o numero de recursos disponíveis incialmente */
1. cria fila de bloqueados para aquele recurso
1. csem_t.fila = * (FilaBloqueados)  /* cada recurso aponta para um fila de bloqueados diferentes, que são diferentes da fila de bloqueados pela primitiva cjoin */ 

### cwait

1. verifica o valor de sem->count
1. se sem->count <= 0 faz
	1. altera o valor de TCB(status=3) 
	1. adiciona TBC a lista de bloqueados sem->fila
1. decrementa sem->count

### csignal

1. incrementa sem->count
1. altera o status da primeira TCB(status=1)
1. move a primeira TCB para lista de aptos
