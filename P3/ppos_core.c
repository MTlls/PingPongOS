// GRR20211774 Matheus Telles Batista
// OBS: codigo de status das tarefas
/*
    0: terminado
    1: pronto
    2: rodando
    3: suspensa
*/
#include <stdio.h>
#include <stdlib.h>

#include "ppos.h"
#include "ppos_data.h"
#include "queue.h"

#define STACKSIZE 64 * 1024 /* tamanho de pilha das threads */
#define STTS_TERMINADA 0
#define STTS_PRONTA 1
#define STTS_RODANDO 2
#define STTS_SUSPENSA 3

task_t **tcb; /* TCB, aponta para o HEAD ou melhor, a tarefa corrente */

task_t task_main, task_dispatcher, *running_task = NULL, *last_task = NULL;

void print_elem(void *ptr);

int gerador_id = 0, user_tasks = 0;

// Inicializa o sistema operacional; deve ser chamada no inicio do main()
void ppos_init() {
	/* desativa o buffer da saida padrao (stdout), usado pela função printf */
	setvbuf(stdout, 0, _IONBF, 0);
	char *stack;
	tcb = (task_t **)malloc(sizeof(task_t *));
	*tcb = NULL;

	task_main.next = NULL;
	task_main.prev = NULL;

	// // adiciona a tarefa main na TCB
	// queue_append((queue_t **)tcb, (queue_t *)&task_main);

	// ID da main = 0
	task_main.id = gerador_id++;

	getcontext(&task_main.context);

#ifdef DEBUG
	printf("ppos_init: main iniciado \n");
	printf("ppos_init: iniciando task dispatcher...\n");
#endif

	if(task_init(&task_dispatcher, dispatcher, 0) < 0) {
		perror("ERRO: ao iniciar task\n");
	}

#ifdef DEBUG
	printf("ppos_init: dispatcher criado e na fila \n");
#endif

	running_task = &task_main;

	return;
}

// Inicializa uma nova tarefa. Retorna um ID> 0 ou erro.
int task_init(task_t *task,                // descritor da nova tarefa
              void (*start_func)(void *),  // funcao corpo da tarefa
              void *arg)                   // argumentos para a tarefa
{
	char *stack;

	// atualiza o id, incrementando para o proximo
	task->id = gerador_id;

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

	// deixa a tarefa pronta para o dispatcher
	task->status = STTS_PRONTA;

	// quando comecar trocar o contexto para essa task, iniciara na funcao start_func
	makecontext(&(task->context), (void *)(start_func), 1, arg);

#ifdef DEBUG
	printf("task_init: inserindo na fila task [%d]...\n", task->id);
#endif

	// insere na fila a task.
	queue_append((queue_t **)tcb, (queue_t *)task);

#ifdef DEBUG
	printf("task_init: tarefa [%d] na tcb\n", task->id);
	queue_print("TCB", (queue_t *)*tcb, print_elem);
#endif

	// incrementa o numero de tarefas de usuario
	user_tasks++;

	return gerador_id++;
}

// Termina a tarefa corrente com um status de encerramento
void task_exit(int exit_code) {
	// reconhecendo a chamada do dispatcher
	if(running_task == &task_dispatcher) {
#ifdef DEBUG
		printf("saindo...\n");
#endif
		exit(0);
	} else if(running_task != &task_dispatcher)
		(*tcb)->status = STTS_TERMINADA;

	task_switch(&task_dispatcher);
}

// alterna a execução para a tarefa indicada
int task_switch(task_t *task) {
#ifdef DEBUG
	printf("task_switch: trocando da task [%d] para a task [%d]\n", running_task->id, task->id);

#endif

	if(task == NULL || running_task == NULL) {
		perror("ERRO: ponteiro vazio");
		return -1;
	}

	// trocando a task atual...
	last_task = running_task;
	running_task = task;
#ifdef DEBUG
	printf("ptr last_task    = %d\n", last_task->id);
	printf("ptr running_task = %d\n", running_task->id);
#endif

	// atualizando status..
	task->status = STTS_RODANDO;
	//  retorno: valor negativo se houver erro, ou zero
	if(swapcontext(&(last_task->context), &(running_task->context)) == -1) {
		perror("Erro na troca de contexto: ");
		return -1;
	}
#ifdef DEBUG
	printf("ptr last_task    = %p\n", last_task->context.uc_stack.ss_sp);
	printf("ptr running_task = %p\n", running_task->context.uc_stack.ss_sp);
#endif
	return 0;
}

// retorna o identificador da tarefa corrente (main deve ser 0)
int task_id() {
	return running_task->id;
}

// a tarefa atual libera o processador para outra tarefa
void task_yield() {
	// muda o estado da tarefa atual para PRONTA
	(*tcb)->status = STTS_PRONTA;

	// a tarefa atual vai ser a ultima da fila
	// aqui mudamos a HEAD para a proxima task
	(*tcb) = (*tcb)->next;

	// volta para o dispatcher
	task_switch(&task_dispatcher);
}

void dispatcher() {
	task_t *proxima = NULL;
// retira o dispatcher da fila de prontas, para evitar que ele ative a si próprio
#ifdef DEBUG
	printf("dispatcher: removendo dispatcher da fila\n");
#endif
	queue_remove((queue_t **)tcb, (queue_t *)&task_dispatcher);
	user_tasks--;

#ifdef DEBUG
	queue_print("TCB", (queue_t *)*tcb, print_elem);

	printf("dispatcher: removido da fila\n");
#endif
	// enquanto houverem tarefas de usuário
	while(user_tasks > 0) {
// escolhe a próxima tarefa a executar
#ifdef DEBUG
		queue_print("TCB", (queue_t *)*tcb, print_elem);
#endif
		if(!(proxima = scheduler()))
			perror("nao ha proxima\n");
#ifdef DEBUG
		printf("dispatcher: achou agendador! Mudando para proxima task...\n");
#endif
		if(proxima != NULL) {
			if(task_switch(proxima) == -1)
				exit(-1);
				/* voltando ao dispatcher, trata a tarefa de acordo com seu estado
				caso o estado da tarefa*/
				// 0: TERMINADA, 1: PRONTA, 2: RODANDO 3: SUSPENSA */
#ifdef DEBUG
			printf("dispatcher: task->status = %d\n", proxima->status);
#endif
			switch(proxima->status) {
			case STTS_TERMINADA:
				// libera as estruturas de dados da task
				free(proxima->context.uc_stack.ss_sp);
#ifdef DEBUG
				printf("dispatcher: tentando remover a task [%d]...\n", proxima->id);
#endif
				queue_remove((queue_t **)tcb, (queue_t *)proxima);
				user_tasks--;
#ifdef DEBUG
				queue_print("TCB", (queue_t *)*tcb, print_elem);
				printf("user_tasks: %d\n", user_tasks);

#endif

				// escolhe a próxima tarefa a executar
				proxima = scheduler();
				break;
			case STTS_PRONTA:
				proxima = scheduler();
				break;
			// POR ENQUANTO NAO ESTA INDICADO O QUE FAZER QUANDO ESTIVER NOS OUTROS STATUS
			default:
				break;
			}
		}
	}

	task_exit(0);
}
/*
início
// retira o dispatcher da fila de prontas, para evitar que ele ative a si próprio
queue_remove(...)

    // enquanto houverem tarefas de usuário
    enquanto(userTasks > 0)

    // escolhe a próxima tarefa a executar
    próxima = scheduler()

    // escalonador escolheu uma tarefa?
    se próxima ≠ NULO então

    // transfere controle para a próxima tarefa
    task_switch(próxima)

// voltando ao dispatcher, trata a tarefa de acordo com seu estado
caso o estado da tarefa "próxima" seja : PRONTA : ... TERMINADA : ... SUSPENSA : ...(etc)
                                                                                     fim caso

                                                                                 fim se

                                                                                 fim enquanto

                                                                                 // encerra a tarefa dispatcher
                                                                                 task_exit(0)
                                                                                     fim * /

                                                                                */

task_t *scheduler() {
	task_t *aux = (*tcb);
	// Como o task_yield() ja coloca a proxima task como primeira da fila, partiremos da primeira.

#ifdef DEBUG
	printf("agendador: procurando ...\n");
#endif

	// verifica caso a tcb esteja vazia
	if(queue_size((queue_t *)aux) == 0)
		return NULL;

	// anda ate o final da fila, procurando alguma tarefa com o status PRONTA
	while(aux->status != STTS_PRONTA)
		aux = aux->next;

#ifdef DEBUG
	printf("agendador: achou! \n");
#endif

	if(aux->status != STTS_PRONTA)
		return NULL;

	return aux;
}

void print_elem(void *ptr) {
	task_t *elem = ptr;

	if(!elem)
		return;

	elem ? printf("[%d]", elem->id) : printf("*");
}