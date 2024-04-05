#include <stdio.h>
#include <stdlib.h>

#include "ppos.h"
#include "ppos_data.h"
#include "queue.h"

#define STACKSIZE 64 * 1024 /* tamanho de pilha das threads */

task_t **tcb, taskMain; /* TCB, aponta para o HEAD ou melhor, a tarefa corrente */

int gerador_id = 1;

// Inicializa o sistema operacional; deve ser chamada no inicio do main()
void ppos_init() {
	/* desativa o buffer da saida padrao (stdout), usado pela função printf */
	setvbuf(stdout, 0, _IONBF, 0);
	tcb = (task_t **)malloc(sizeof(task_t *));
	*tcb = NULL;
	taskMain.next = NULL;
	taskMain.prev = NULL;
#ifdef DEBUG
	printf("ppos_init: alocado espaco para main \n");
#endif

	queue_append((queue_t **)tcb, (queue_t *)&taskMain);

	taskMain.id = 0;

#ifdef DEBUG
	printf("ppos_init: tamanho da tcb: %d \n", queue_size((queue_t *)*tcb));
	printf("ppos_init: main na fila \n");
#endif
}

// Inicializa uma nova tarefa. Retorna um ID> 0 ou erro.
int task_init(task_t *task,                // descritor da nova tarefa
              void (*start_func)(void *),  // funcao corpo da tarefa
              void *arg)                   // argumentos para a tarefa
{
	char *stack;

	// atualiza o id, incrementando para o proximo
	task->id = gerador_id++;

	// salva o contexto atual
	getcontext(&(task->context));

	// salva algumas informacoes extras sobre a pilha da tarefa
	stack = malloc(STACKSIZE);
	if(stack) {
		task->context.uc_stack.ss_sp = stack;
		task->context.uc_stack.ss_size = STACKSIZE;
		task->context.uc_stack.ss_flags = 0;
		task->context.uc_link = 0;
	} else {
		perror("Erro na criação da pilha: ");
		exit(-1);
	}

	// quando comecar trocar o contexto para essa task, iniciara na funcao start_func
	makecontext(&(task->context), (void *)(start_func), 1, arg);

#ifdef DEBUG
	printf("task_init: inserindo na fila task %d...\n", task->id);
#endif

	// insere na fila a task.
	queue_append((queue_t **)tcb, (queue_t *)task);

#ifdef DEBUG
	printf("task_init: tarefa %d na tcb\n", task->id);
#endif
	return 1;
}

// Termina a tarefa corrente com um status de encerramento
void task_exit(int exit_code) {
#ifdef DEBUG
	printf("task_exit: terminando... \n");
#endif

	// free(tcb->context.uc_stack.ss_sp);
#ifdef DEBUG
	printf("task_exit: id da task atual: %d\n", (*tcb)->id);
#endif
	if((*tcb)->id != taskMain.id) {
		task_switch(&taskMain);
	}

#ifdef DEBUG
	printf("task_exit: saindo da main...\n");
#endif
	// se estivermos saindo da main, removo todos as tarefas da tcb, tirando a main
	while(queue_size((queue_t *)*tcb) != 1) {
		if((*tcb)->id == 0)
			*tcb = (*tcb)->next;
#ifdef DEBUG
		printf("task_exit: tamanho da tcb: %d \n", queue_size((queue_t *)*tcb));
		int id = (*tcb)->id;
#endif
		free((*tcb)->context.uc_stack.ss_sp);

		queue_remove((queue_t **)tcb, (queue_t *)*tcb);
#ifdef DEBUG
		printf("task_exit: removido task [%d]\n", id);
#endif
	}
	free(tcb);
}

// alterna a execução para a tarefa indicada
int task_switch(task_t *task) {
#ifdef DEBUG
	printf("task_switch: trocando da task [%d] para a task [%d]\n", (*tcb)->id, task->id);
#endif
	// aux agora tem a tarefa atual
	task_t *aux = *tcb;

	*tcb = task;

	//  retorno: valor negativo se houver erro, ou zero
	if(swapcontext(&(aux->context), &(task->context)) == -1) {
		perror("Erro na troca de contexto: ");
		return -1;
	}

	return 0;
}

// retorna o identificador da tarefa corrente (main deve ser 0)
int task_id() {
	return (*tcb)->id;
}
