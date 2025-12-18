// PingPongOS - PingPong Operating System
// Prof. Carlos A. Maziero, DINF UFPR
// Versão 1.4 -- Janeiro de 2022

// Teste de filas de mensagens

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include "ppos.h"

task_t producer_tasks[3], summer_task, consumer_tasks[2] ;
mqueue_t values_queue, roots_queue ;

// corpo da thread produtor
void producer_body (void * output)
{
   int value ;

   while (1)
   {
      // sorteia um valor inteiro aleatorio
      value = random () % 1000 ;

      // envia o valor inteiro na fila de saida
      if (mqueue_send (&values_queue, &value) < 0)
      {
         printf ("T%d terminou\n", task_id()) ;
         task_exit (0) ;
      }

      printf ("T%d enviou %d\n", task_id(), value) ;

      // dorme um intervalo aleatorio
      task_sleep (random () % 3000) ;
   }
}

// corpo da thread somador
void summer_body (void * arg)
{
   int v1, v2, v3, i ;
   double sum, root ;

   for (i=0; i<10; i++)
   {
      // recebe tres valores inteiros
      mqueue_recv (&values_queue, &v1) ;
      printf ("               T%d: recebeu %d\n", task_id(), v1) ;
      mqueue_recv (&values_queue, &v2) ;
      printf ("               T%d: recebeu %d\n", task_id(), v2) ;
      mqueue_recv (&values_queue, &v3) ;
      printf ("               T%d: recebeu %d\n", task_id(), v3) ;

      // calcula a soma e sua raiz
      sum = v1 + v2 + v3 ;
      root = sqrt (sum) ;
      printf ("               T%d: %d+%d+%d = %f (raiz %f)\n",
              task_id(), v1, v2, v3, sum, root) ;

      // envia a raiz da soma
      mqueue_send (&roots_queue, &root) ;

      // dorme um intervalo aleatorio
      task_sleep (random () % 3000) ;
   }
   task_exit(0) ;
}

// corpo da thread consumidor
void consumer_body (void * arg)
{
   double value ;

   while(1)
   {
      // recebe um valor (double)
      if (mqueue_recv (&roots_queue, &value) < 0)
      {
         printf ("                                 T%d terminou\n",
                 task_id()) ;
         task_exit (0) ;
      }
      printf ("                                 T%d consumiu %f\n",
              task_id(), value) ;

      // dorme um intervalo aleatorio
      task_sleep (random () % 3000) ;
   }
}

int main (int argc, char *argv[])
{
   printf ("main: inicio\n") ;

   ppos_init () ;

   // cria as filas de mensagens (5 valores cada)
   mqueue_create (&values_queue, 5, sizeof(int)) ;
   mqueue_create (&roots_queue,  5, sizeof(double)) ;

   // cria as threads
   task_create (&summer_task, summer_body, NULL) ;
   task_create (&consumer_tasks[0], consumer_body, NULL) ;
   task_create (&consumer_tasks[1], consumer_body, NULL) ;
   task_create (&producer_tasks[0], producer_body, NULL) ;
   task_create (&producer_tasks[1], producer_body, NULL) ;
   task_create (&producer_tasks[2], producer_body, NULL) ;

   // aguarda o somador encerrar
   task_join (&summer_task) ;

   // destroi as filas de mensagens
   printf ("main: destroi values_queue\n") ;
   mqueue_destroy (&values_queue) ;
   printf ("main: destroi roots_queue\n") ;
   mqueue_destroy (&roots_queue) ;

   // encerra a thread main
   printf ("main: fim\n") ;
   task_exit (0) ;

   exit (0) ;
}