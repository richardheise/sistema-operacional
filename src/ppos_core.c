// GRR20191053 Richard Fernando Heise Ferreira

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/time.h>
#include <string.h>
#include "ppos.h"
#include "queue.h"

/* ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ */

#define STACKSIZE 128 * 1024 // Tamanho da pilha
#define ERROR_STATUS -5      // Erro caso o status da tarefa não existe
#define ERROR_QUEUE -6       // Erro genérico da fila
#define ERROR_STACK -7       // Erro genérico da pilha
#define ALPHA -1             // Fator de envelhecimento (linear) {
#define QUANTUM 20           // Quantum padrão de cada tarefa

/* ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ */

// Enum dos status
enum status_t {

    READY = 1,
    RUNNING,
    ASLEEP,
    FINISHED

};

struct sigaction action;              // Estrutura que define um tratador de sinal
struct itimerval timer;               // Estrutura de inicialização to timer

task_t *current_task = NULL;          // Tarefa atual
task_t main_task;                     // Tarefa da main

unsigned int task_id_counter = 0;     // Contador de identificador de tarefas
unsigned int active_tasks = 0;        // Número de tarefas de usuário ativas

task_t *ready_tasks = NULL;           // Fila de tarefas prontas
task_t *sleeping_tasks = NULL;
task_t dispatcher_task;               // Tarefa do despachante

unsigned int ticks = 0;               // Ticks do relógio
unsigned int uptime = 0;              // Tempo ativo de cada tarefa

int lock = 0;

/* ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ */



/* ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ */

static void dispatcher_body();              // Despachante
static task_t *priority_scheduler();       // Agendador de prioridade
static void setup_system_clock();           // Definição do relógio do sistema
static void quantum_controller();           // Controlador de quantum
static void wakeup_sleeping_tasks();        // Acordador de tarefas dormentes
void enter_cs (int *lock);                  // Entra na zona crítica
void leave_cs (int *lock);                  // Sai da zona crítica

/* ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ */
/* +++++++++++++++++++ Início das funções sobre tarefas  ++++++++++++++++++ */
/* ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ */

void ppos_init() {

    // Criando a main task
    task_create(&main_task, NULL, NULL);

    // Atual é a main
    current_task = &main_task;
    
    // Criando o despachante
    task_create(&dispatcher_task, dispatcher_body, NULL);

    // Ajustes pós-criação genérica
    dispatcher_task.status = RUNNING;
    dispatcher_task.preemptable = 0;
    if (queue_remove((queue_t **)&ready_tasks, (queue_t *)&dispatcher_task) < 0) {
    
        fprintf(stderr, "Erro ao remover dispatcher da lista, abortando.\n");
        exit(ERROR_QUEUE);
    }
    --active_tasks;

    /* desativa o buffer da saida padrao (stdout), usado pela função printf */
    setvbuf(stdout, 0, _IONBF, 0);

    // Definindo o relógio
    setup_system_clock();

    // Emtregando o controle ao dispatcher
    task_yield();
}

/* ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ */

static void wakeup_sleeping_tasks() {
    
    // Se temos tasks dormindo
    if (sleeping_tasks) {

        // Olhamos para o primeiro da filta
        task_t *waker = sleeping_tasks; 

        // Pegamos o tamanho da fila
        int sleeping_tasks_count = queue_size( (queue_t *) sleeping_tasks );

        // Varremos a fila acordando as tarefas dormentes
        for (int i = 0; i < sleeping_tasks_count; i++) {

            waker = waker->next;
            if (waker->alarm <= systime()) { // Se é hora de acordar

                // Acorda
                task_resume(waker, &sleeping_tasks);  
                waker = sleeping_tasks;
            } 
        }
    }
}

/* ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ */

static void dispatcher_body () {

    task_t *next_task = NULL;

    // Enquanto houverem tarefas de usuário
    while ( active_tasks ) {

        // O galo acorda tarefas
        wakeup_sleeping_tasks();

        // Escolhe a próxima tarefa a executar
        next_task = priority_scheduler();

        // Escalonador escolheu uma tarefa?      
        if (next_task) {

            // Transfere controle para a próxima tarefa
            task_switch (next_task);

            // Voltando ao dispatcher, trata a tarefa de acordo com seu estado
            switch (next_task->status) {
                case READY:

                    next_task->quantum = QUANTUM;

                    break;
                case RUNNING:
                    break;
                case ASLEEP:
                    break;
                case FINISHED:

                    // Removemos a tarefa terminada da fila e ajustamos o número total de tarefas ativas
                    --active_tasks;
                    free(next_task->context.uc_stack.ss_sp);
                    if ( queue_remove( (queue_t **) &ready_tasks, (queue_t *)next_task ) < 0 ) {

                        fprintf(stderr, "Erro ao remover elemento %d da lista, abortando.\n", next_task->id);
                        exit(ERROR_QUEUE);

                    }

                    break;
                default:
                    fprintf(stderr, "Status da tarefa %d não abarcado, abortando.\n", next_task->id);
                    exit(ERROR_STATUS);
            }         

        }

    }

   // encerra a tarefa dispatcher
   task_exit(0);
}

/* ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ */

static task_t *priority_scheduler() {

    if (!ready_tasks) return NULL;

    // A tarefa com menos drip é a prioritária
    task_t *highest_prio_task = ready_tasks;
    task_t *aux = ready_tasks;

    // Percorremos a lista de tarefas
    do {
    
        aux = aux->next;

        // A tarefa com menor drip é escolhida
        if (aux->dynamic_prio < highest_prio_task->dynamic_prio)
            highest_prio_task = aux;

    } while (aux != ready_tasks);

    // Percorremos a lista de tarefas
    do {
        
        // Aging
        aux->static_prio += ALPHA;
        aux = aux->next;

    } while (aux != ready_tasks);

    // Resetamos o drip dinâmico da tarefa escolhida
    highest_prio_task->dynamic_prio = highest_prio_task->static_prio;

    return highest_prio_task;
}

/* ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ */

static void quantum_controller() {

    ticks++;

    if (!current_task->preemptable) {
        return;
    }

    if (current_task->quantum > 0) {
    
        --current_task->quantum;
    }
    else {
    
        current_task->status = READY;
        task_switch(&dispatcher_task);
    }
}

/* ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ */

static void setup_system_clock() {

    // Registra a ação para o sinal de timer SIGALRM
    action.sa_handler = quantum_controller;
    sigemptyset(&action.sa_mask);
    action.sa_flags = 0;
    if (sigaction(SIGALRM, &action, 0) < 0) {
    
        perror("Erro em sigaction: ");
        exit(1);
    }

    timer.it_value.tv_usec = 1000; // Primeiro disparo, em micro-segundos
    timer.it_value.tv_sec = 0;
    timer.it_interval.tv_usec = 1000; // Disparos subsequentes, em micro-segundos
    timer.it_interval.tv_sec = 0;

    // Arma o temporizador ITIMER_REAL
    if (setitimer(ITIMER_REAL, &timer, 0) < 0) {
    
        perror("Erro em setitimer: ");
        exit(1);
    }
}

/* ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ */

int task_create(task_t *task, void (*start_func)(void *), void *arg) {

    // Definimos as variáveis da tarefa
    task->prev = NULL;
    task->next = NULL;
    task->preemptable = 1;
    task->status = READY;
    task->id = task_id_counter++;
    task->static_prio = 0;
    task->dynamic_prio = 0;
    task->quantum = QUANTUM;
    task->exec_time = systime();
    task->proc_time = 0;
    task->activations = 0;
    task->exit_code = -1;
    task->suspended_tasks = NULL;
    task->alarm = 0;

    // Alocamos uma pilha para a task
    char *stack;

    getcontext(&task->context);

    stack = malloc(STACKSIZE);
    if (stack) {
    
        task->context.uc_stack.ss_sp = stack;
        task->context.uc_stack.ss_size = STACKSIZE;
        task->context.uc_stack.ss_flags = 0;
        task->context.uc_link = 0;

    }
    else {

        perror("Erro na criação da pilha: ");
        exit(ERROR_STACK);
    }

    makecontext(&task->context, (void *)start_func, 1, arg);

    // Adiciona a tarefa na fila
    if (queue_append((queue_t **)&ready_tasks, (queue_t *)task) < 0) {
    
        fprintf(stderr, "Erro ao inserir elemento %d na lista, abortando.\n", task->id);
        exit(ERROR_QUEUE);
    }
    active_tasks += 1;

    return task->id;
}

/* ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ */

int task_switch(task_t *task) {

    if (current_task->status != FINISHED) {
        current_task->proc_time += (systime() - uptime);
    }

    uptime = systime();

    // A task atual precisa ser guardada antes de ser reatribuída
    ucontext_t *old_task_context = &current_task->context;
    current_task = task;
    current_task->status = RUNNING;

    task->activations++;
    swapcontext(old_task_context, &task->context);

    return 0;
}

/* ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ */

void task_exit(int exit_code) {

    // Status da tarefa agora é terminada
    current_task->status = FINISHED;

    current_task->proc_time += (systime() - uptime);
    current_task->exec_time = (systime() - current_task->exec_time);
    current_task->exit_code = exit_code;

    printf("Task %d exit: execution time %d ms, processor time %d ms, %d activations\n", 
    current_task->id, current_task->exec_time, current_task->proc_time, current_task->activations);

    task_t* suspended_task = current_task->suspended_tasks;

    while ( suspended_task ) {
        task_resume(suspended_task, &current_task->suspended_tasks);
        suspended_task = current_task->suspended_tasks;
    }

    // Se a tarefa atual não é o despachante, passa a ser
    if (current_task != &dispatcher_task) {
    
        task_switch(&dispatcher_task);
    }
    else {
        exit(0);
    }
}

/* ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ */

int task_id() { 
    return current_task->id;
}

/* ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ */

void task_yield() { 

    task_switch(&dispatcher_task);
}

/* ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ */

void task_setprio(task_t *task, int prio) { 

    // Quando atribuímos o drip estático
    // precisamos atribuir o dinâmico também
    if (!task) {
    
        current_task->dynamic_prio = prio;
        current_task->static_prio = prio;
        return;
    }

    task->dynamic_prio = prio;
    task->static_prio = prio;
}

/* ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ */

int task_getprio(task_t *task) {

    if (!task) {
        return current_task->static_prio;
    }
    return task->static_prio;
}

/* ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ */

unsigned int systime () { 
    return ticks;
}

/* ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ */

void task_suspend (task_t **queue) {

    if ( queue_remove((queue_t **)&ready_tasks, (queue_t *)current_task) < 0 ) {

        fprintf(stderr, "Erro ao remover tarefa na fila %p, abortando.\n", queue);
        exit(ERROR_QUEUE);
    }

    current_task->status = ASLEEP;
    if (queue_append((queue_t **)queue, (queue_t *)current_task) < 0) {
    
        fprintf(stderr, "Erro ao adicionar tarefa na fila %p, abortando.\n", queue);
        exit(ERROR_QUEUE);
    }

    task_yield();

}

/* ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ */

void task_resume (task_t *task, task_t **queue) {

    if (queue_remove((queue_t **)queue, (queue_t *)task) < 0) {
    
        fprintf(stderr, "Erro ao remover tarefa da fila %p, abortando.\n", queue);
        exit(ERROR_QUEUE);
    }

    task->status = READY;
    task->quantum = QUANTUM;
    
    if (queue_append((queue_t **)&ready_tasks, (queue_t *)task) < 0) {
    
        fprintf(stderr, "Erro ao adicionar tarefa na fila de prontas (%p), abortando.\n", ready_tasks);
        exit(ERROR_QUEUE);
    }

}

/* ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ */

int task_join (task_t *task) {

    if (!task || task->status == FINISHED) return -1;

    task_suspend(&task->suspended_tasks);

    return task->exit_code;
}

/* ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ */

void task_sleep (int t) {
    current_task->alarm = systime() + t;
    task_suspend(&sleeping_tasks);
    task_yield();
}
/* ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ */
/* +++++++++++++++++++ Fim das funções sobre tarefas  +++++++++++++++++++++ */
/* ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ */

/* ------------------------------------------------------------------------ */

/* ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ */
/* +++++++++++++++++++ Início das funções sobre semáforos +++++++++++++++++ */
/* ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ */

int sem_create (semaphore_t *s, int value) {

    s->counter = value;
    s->queue = NULL;
    s->alive = 1;

    return 0;
}

/* ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ */

int sem_down (semaphore_t *s) {
    enter_cs(&lock);
    if (!s->alive) {
        leave_cs(&lock);
        return -1;
    }

    s->counter = s->counter - 1;

    if (s->counter < 0) {
        leave_cs(&lock);
        task_suspend(&s->queue);
        return 0;
    }
    leave_cs(&lock);

    return 0;
}

/* ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ */

int sem_up (semaphore_t *s) {
    enter_cs(&lock);
    if (!s->alive) {
        leave_cs(&lock);
        return -1;
    }

    s->counter = s->counter + 1;

    if (s->counter <= 0) {
        task_t* task = s->queue;
        task_resume(task, &s->queue);
        leave_cs(&lock);
        return 0;
    }   
    leave_cs(&lock);

    return 0;
}

/* ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ */

int sem_destroy (semaphore_t *s) {
    enter_cs(&lock);
    if (!s->alive) {
        return -1;
    }

    while (s->queue) {
        task_resume(s->queue, &s->queue);
    }
    
    s->alive = 0;
    s = NULL;

    leave_cs(&lock);

    return 0;
}

/* ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ */

void enter_cs (int *lock) {
  // atomic OR (Intel macro for GCC)
  while (__sync_fetch_and_or (lock, 1)) ;   // busy waiting
}
 
/* ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ */

void leave_cs (int *lock) {
  (*lock) = 0 ;
}

/* ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ */
/* +++++++++++++++++++++ Fim das funções sobre semáforos ++++++++++++++++++ */
/* ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ */

/* ------------------------------------------------------------------------ */

/* ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ */
/* +++++++++++ Início das funções sobre tfilas de mensagens +++++++++++++++ */
/* ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ */

int mqueue_create (mqueue_t *queue, int max_msgs, int msg_size) {
    if (!queue) return -1;

    sem_create(&queue->buff_sem, 1);
    sem_create(&queue->recv_sem, 0);
    sem_create(&queue->send_sem, max_msgs);

    queue->size = msg_size;
    queue->buffer = NULL;

    return 0;
}

/* ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ */

int mqueue_send (mqueue_t *queue, void *msg) {
    if (!queue || !msg) return -1;

    if(sem_down(&queue->send_sem) < 0)
        return -1;
    if(sem_down(&queue->buff_sem) < 0)
        return -1;

    buffer_t* elem = malloc(sizeof(buffer_t));
    elem->next = NULL;
    elem->prev = NULL;
    
    bcopy(msg, &elem->value, queue->size);

    if ( queue_append((queue_t **) &queue->buffer, (queue_t *) elem) ) {
        fprintf(stderr, "Erro ao adicionar item ao buffer.\n");
        exit(ERROR_QUEUE);
    }

    if (sem_up(&queue->buff_sem) < 0)
        return -1;
    if (sem_up(&queue->recv_sem) < 0)
        return -1;

    return 0;
}

/* ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ */

int mqueue_recv (mqueue_t *queue, void *msg) {
    if (!queue || !msg) return -1;

    if(sem_down(&queue->recv_sem)<0)
        return -1;
    if(sem_down(&queue->buff_sem)<0)
        return -1;

    bcopy(&queue->buffer->value, msg, queue->size);
    if ( queue_remove((queue_t **) &queue->buffer, (queue_t *) queue->buffer) ) {
        fprintf(stderr, "Erro ao remover item do buffer.\n");
        exit(ERROR_QUEUE);
    }

    if (sem_up(&queue->buff_sem) < 0)
        return -1;
    if (sem_up(&queue->send_sem) < 0)
        return -1;

    return 0;
}

/* ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ */

int mqueue_destroy (mqueue_t *queue) {
    if (!queue) return -1;

    while(queue->buffer) {
        queue_remove((queue_t **) &queue->buffer, (queue_t *) queue->buffer);
    }
    
    if (sem_destroy(&queue->buff_sem) < 0)
        return -1;
    if (sem_destroy(&queue->recv_sem) < 0)
        return -1;
    if (sem_destroy(&queue->send_sem) < 0)
        return -1;

    free(queue->buffer);

    return 0;
}

/* ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ */

int mqueue_msgs (mqueue_t *queue) {

    if (!queue) return -1;
    
    return queue->recv_sem.counter;
}

/* ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ */
/* +++++++++++++ Fim das funções sobre tfilas de mensagens ++++++++++++++++ */
/* ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ */