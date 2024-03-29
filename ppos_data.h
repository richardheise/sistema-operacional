// GRR20191053 Richard Fernando Heise Ferreira  

// PingPongOS - PingPong Operating System
// Prof. Carlos A. Maziero, DINF UFPR
// Versão 1.4 -- Janeiro de 2022

// Estruturas de dados internas do sistema operacional

#ifndef __PPOS_DATA__
#define __PPOS_DATA__

#include <ucontext.h>		// biblioteca POSIX de trocas de contexto

// Estrutura que define um Task Control Block (TCB)
typedef struct task_t
{
  struct task_t *prev, *next ;  // ponteiros para usar em filas
  int id ;				              // identificador da tarefa
  ucontext_t context ;			    // contexto armazenado da tarefa
  short status ;			          // pronta, rodando, suspensa, ...
  short preemptable ;	          // pode ser preemptada?
  short st_drip ;	              // equivalente a nice em UNIX, estático
  short di_drip ;               // prioridade dinâmica
  short quantum ;               // quantum de cada tarefa
  unsigned int exeTime ;        // tempo de execução
  unsigned int procTime ;       // tempo de processador
  unsigned int activs ;         // número de ativações
  unsigned int exit_code ;      // código de saida
  struct task_t *sus_tasks ;    // fila de tarefas suspensas por esta tarefa
  unsigned int alarm ;          // tempo que a task deve dormir
  // ... (outros campos serão adicionados mais tarde)
} task_t ;

// estrutura que define um semáforo
typedef struct
{
  int counter;
  task_t* jam; 
  int lit;
} semaphore_t ;

// estrutura que define um mutex
typedef struct
{
  // preencher quando necessário
} mutex_t ;

// estrutura que define uma barreira
typedef struct
{
  // preencher quando necessário
} barrier_t ;

// Buffer para as filas de mensagem
typedef struct buffer_s
{
  struct buffer_t *prev, *next;
  void* value;
} buffer_t ;

// estrutura que define uma fila de mensagens
typedef struct
{
  int size;
  semaphore_t sendSem;
  semaphore_t recvSem;
  semaphore_t buffSem;
  buffer_t* buffer;
} mqueue_t ;

#endif
