// GRR20211774 Matheus Telles Batista
// OBS: codigo de status das tarefas
/*
    0: terminado
    1: pronto
    2: rodando
    3: suspensa
*/
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>

#include "ppos.h"
#include "ppos_data.h"
#include "queue.h"

#define STACKSIZE 64 * 1024 /* tamanho de pilha das threads */
#define STTS_TERMINADA 0
#define STTS_PRONTA 1
#define STTS_RODANDO 2
#define STTS_SUSPENSA 3

#define TICKS 20
void tratador();

task_t *scheduler();
void dispatcher();
task_t *procura_task(queue_t *queue);
void enter_cs (int *lock);
void leave_cs (int *lock);

int lock = 0;
/**
 * Relogio interno do sistema
 */
uint clock = 0;

/**
 * Função que seta os valores do temporizador e registra a ação para o sinal de timer.
 */
void configTimer();

// estrutura que define um tratador de sinal (deve ser global ou static)
struct sigaction action;

// estrutura de inicialização do timer
struct itimerval timer;

/**
 * Flag global que é TRUE quando uma tarefa de usuário estiver executando seu próprio código e FALSE quando a execução estiver dentro de uma função do sistema */
short ehUserTask = 1;

int quantum = TICKS;

/* fila de tasks */
queue_t *queue_prontas, *queue_adormecidas;

task_t task_main, task_dispatcher, *running_task;

void print_elem(void *ptr);

int gerador_id = 0, user_tasks = 0;

// Inicializa o sistema operacional; deve ser chamada no inicio do main()
void ppos_init() {
	/* desativa o buffer da saida padrao (stdout), usado pela função printf */
	setvbuf(stdout, 0, _IONBF, 0);

	configTimer();

	task_main.next = NULL;
	task_main.prev = NULL;

	// ID da main = 0
	user_tasks++;
	task_main.activations = 1;
	task_main.id = gerador_id++;
	task_main.status = STTS_RODANDO;
	task_main.prioridade_e = 0;
	task_main.prioridade_d = task_main.prioridade_e;

	getcontext(&task_main.context);

#ifdef DEBUG
	printf("ppos_init: main iniciado \n");
	printf("ppos_init: iniciando task dispatcher...\n");
#endif

	// cria a fila de tarefas prontas
	if(task_init(&task_dispatcher, dispatcher, 0) < 0)
		perror("ERRO: ao iniciar task\n");

	// insere a main na fila de prontas
	if(queue_append(&queue_prontas, (queue_t *)&task_main) < 0) {
		perror("ERRO: ao inserir task na fila de prontas\n");
		exit(-1);
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
	task->execution_time = systime();
	task->processor_time = 0;
	task->activations = 0;

	char *stack;

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
	task->prioridade_e = 0;
	task->prioridade_d = task->prioridade_e;
	// quando comecar trocar o contexto para essa task, iniciara na funcao start_func
	makecontext(&(task->context), (void *)(start_func), 1, arg);

	// insere na fila a task.
	if(queue_append(&queue_prontas, (queue_t *)task) < 0) {
		perror("Erro ao inserir task na fila de prontas: ");
		return -1;
	}

	// incrementa o numero de tarefas de usuario
	user_tasks++;
	// atualiza o id
	task->id = gerador_id;

#ifdef DEBUG
	printf("task_init: task [%d] criada\n", task->id);
	// printa todos os campos da task
	printf("task_init: task [%d] prioridade estatica = %d\n", task->id, task->prioridade_e);
	printf("task_init: task [%d] prioridade dinamica = %d\n", task->id, task->prioridade_d);
	printf("task_init: task [%d] status = %d\n", task->id, task->status);

	printf("task_init: tarefa [%d] na tcb\n", task->id);
	queue_print("TCB", queue_prontas, print_elem);
#endif

	// retorna o id atual e o incrementa para o proximo
	return gerador_id++;
}

// Termina a tarefa corrente com um status de encerramento
void task_exit(int exit_code) {
	running_task->exit_code = exit_code;

#ifdef DEBUG
	printf("task_exit: fila de suspensas da task [%d]\n", running_task->id);
#endif

	// reconhecendo a chamada do dispatcher
	if(running_task->id == task_dispatcher.id) {
#ifdef DEBUG
		printf("saindo...\n");
#endif
		// contabilizado o uso do processador pela tarefa
		printf("Task %d exit: execution time %d ms, processor time %d ms, %d activations\n", running_task->id, (systime() - running_task->execution_time), running_task->processor_time, running_task->activations);

		free(task_dispatcher.context.uc_stack.ss_sp);
		exit(0);
	} else if(running_task->id != task_dispatcher.id) {
		// muda o estado da tarefa atual para TERMINADA
		running_task->status = STTS_TERMINADA;
		// atualiza os tempos de execução
		running_task->execution_time = systime() - running_task->execution_time;
		running_task->processor_time += TICKS - quantum;

		// contabilizado o uso do processador pela tarefa
		printf("Task %d exit: execution time %d ms, processor time %d ms, %d activations\n", running_task->id, running_task->execution_time, running_task->processor_time, running_task->activations);
	}

	if(queue_size((queue_t *)running_task->fila_suspensas) != 0) {
		// para cada tarefa que estava suspensa, volta para a fila de prontas
		task_t *aux = running_task->fila_suspensas;
		task_t *prox = aux->next;
		while(queue_size((queue_t *)running_task->fila_suspensas) != 0) {
			// imprime a fila de suspensas
			// queue_print("fila_suspensas", (queue_t *)running_task->fila_suspensas, print_elem);
			task_awake(aux, &running_task->fila_suspensas);

			aux = prox;
			prox = aux->next;
		}
	}
	task_switch(&task_dispatcher);
}

// alterna a execução para a tarefa indicada
int task_switch(task_t *task) {
#ifdef DEBUG
	printf("task_switch: trocando da task [%d] para a task [%d]\n", running_task->id, task->id);

#endif

	if(task == NULL) {
		perror("ERRO: ponteiro vazio");
		return -1;
	}

	// trocando a task atual...
	task_t *last_task = running_task;
	running_task = task;
#ifdef DEBUG
	printf("id last_task    = %d\n", last_task->id);
	printf("id running_task = %d\n", running_task->id);
#endif

	if(running_task == &task_dispatcher) {
		ehUserTask = 0;
	} else {
		ehUserTask = 1;
		quantum = TICKS;
	}

	task->activations++;

	// atualizando status..
	task->status = STTS_RODANDO;
	//  retorno: valor negativo se houver erro, ou zero
	if(swapcontext(&last_task->context, &running_task->context) == -1) {
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
	running_task->status = STTS_PRONTA;

	// volta para o dispatcher
	task_switch(&task_dispatcher);
}

void dispatcher() {
	task_t *proxima = NULL;
// retira o dispatcher da fila de prontas, para evitar que ele ative a si próprio
#ifdef DEBUG
	printf("dispatcher: removendo dispatcher da fila\n");
#endif
	queue_remove(&queue_prontas, (queue_t *)&task_dispatcher);
	user_tasks--;

#ifdef DEBUG
	queue_print("TCB", queue_prontas, print_elem);

	printf("dispatcher: removido da fila\n");
#endif
	// enquanto houverem tarefas de usuário
	while(user_tasks > 0) {	
		// verifica se alguma dorminhoca acordou
		if(queue_size(queue_adormecidas) != 0) {
#ifdef DEBUG
			printf("dispatcher: verificando se alguma task acordou\n");
#endif

			task_t *aux = (task_t *)queue_adormecidas;
			task_t *prox = aux->next;
			// para cada tarefa que estava adormecida, verifica se deve acordar
			for(int i = 0; queue_size(queue_adormecidas) > i; i++) {
				if(aux->momento_acordar <= systime()) {
					// acorda a tarefa
					task_awake(aux, (task_t **)&queue_adormecidas);

				}
				aux = prox;
				prox = aux->next;
			}
		}
		// queue_print("prontas", queue_prontas, print_elem);
// escolhe a próxima tarefa a executar

		proxima = scheduler();
#ifdef DEBUG
		printf("dispatcher: achou agendador! Mudando para proxima task...\n");
#endif
		if(proxima != NULL) {
			// remove a task encontrada da fila de prontas
			queue_remove(&queue_prontas, (queue_t *)proxima);
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
				if(proxima->id != 0)
					free(proxima->context.uc_stack.ss_sp);
#ifdef DEBUG
				printf("dispatcher: tentando remover a task [%d]...\n", proxima->id);
#endif
				user_tasks--;
				break;
			case STTS_PRONTA:
				// adiciona novamente a fila de prontos
				queue_append(&queue_prontas, (queue_t *)proxima);
				break;
			// POR ENQUANTO NAO ESTA INDICADO O QUE FAZER QUANDO ESTIVER NOS OUTROS STATUS
			default:
				break;
			}
		}
		
	}

	task_exit(0);
}

task_t *scheduler() {
	task_t *primeiro = (task_t *)queue_prontas,
	       *aux = primeiro,
	       *menor = primeiro;

#ifdef DEBUG
	printf("agendador: procurando ...\n");
#endif

	// verifica se ha tasks
	if(queue_size(queue_prontas) == 0)
		return NULL;
	else if(queue_size(queue_prontas) == 1 && primeiro->status == STTS_PRONTA) {
		primeiro->prioridade_d = primeiro->prioridade_e;
		return primeiro;
	}

	// anda ate o final da fila, aumentando em 1 a prioridade dinamica de cada task, chamado AGING
	while(aux->next != primeiro) {
		if(aux->prioridade_d != -20)
			aux->prioridade_d--;

		aux = aux->next;
	}
	// ajusta o ultimo
	aux->prioridade_d--;

	aux = primeiro;

	// procura a task com a menor prioridade
#ifdef DEBUG
	printf("procurando a task com menor prioridade \n");
#endif
	while(aux->next != primeiro) {
#ifdef DEBUG
		printf("comparando a prioridade da task [%d] com a task [%d]! \nprioridade de [%d]: %d\nprioridade de [%d]: %d\n", menor->id, aux->id, menor->id, menor->prioridade_d, aux->id, aux->prioridade_d);
#endif
		if(menor->prioridade_d > aux->prioridade_d && aux->status == STTS_PRONTA) {
			menor = aux;
		}
		aux = aux->next;
	}
#ifdef DEBUG

	printf("comparando a prioridade da task [%d] com a task [%d]! \nprioridade de [%d]: %d\nprioridade de [%d]: %d\n", menor->id, aux->id, menor->id, menor->prioridade_d, aux->id, aux->prioridade_d);
#endif
	if(menor->prioridade_d > aux->prioridade_d && aux->status == STTS_PRONTA) {
		menor = aux;
	}

	if(menor->status != STTS_PRONTA)
		return NULL;

	menor->prioridade_d = menor->prioridade_e;
	return menor;
}

void print_elem(void *ptr) {
	task_t *elem = ptr;

	if(!elem)
		return;

	elem ? printf("[%d]", elem->id) : printf("*");
}

void task_setprio(task_t *task, int prio) {
	if(!task) {
		running_task->prioridade_e = prio;
		running_task->prioridade_d = running_task->prioridade_e;
	} else {
		task->prioridade_e = prio;
		task->prioridade_d = task->prioridade_e;
	}
}

int task_getprio(task_t *task) {
	if(!task)
		return running_task->prioridade_e;
	else
		return task->prioridade_e;
}

void tratador() {
	clock++;

	// se a tarefa atual for de usuario
	if(ehUserTask) {
		if(quantum == 0) {
			// muda o estado da tarefa atual para PRONTA
			running_task->status = STTS_PRONTA;

			quantum = TICKS;

			ehUserTask = 0;

			// adiciona o tempo de processamento
			running_task->processor_time += TICKS;

			// printa o processor time atual
			// volta para o dispatcher
			task_switch(&task_dispatcher);
		} else {
			// diminui o quantum em 1
			quantum--;
		}
	} else {
		// altera os tempos do dispatcher
		running_task->processor_time++;
	}
}

unsigned int systime() { return clock; }

void configTimer() {
	// registra a ação para o sinal de timer SIGALRM (sinal do timer)
	action.sa_handler = tratador;
	sigemptyset(&action.sa_mask);
	action.sa_flags = 0;
	if(sigaction(SIGALRM, &action, 0) < 0) {
		perror("Erro em sigaction: ");
		exit(1);
	}

	// ajusta valores do temporizador
	timer.it_value.tv_usec = 1000;     // primeiro disparo, em micro-segundos
	timer.it_value.tv_sec = 0;         // primeiro disparo, em segundos
	timer.it_interval.tv_usec = 1000;  // disparos subsequentes, em micro-segundos
	timer.it_interval.tv_sec = 0;      // disparos subsequentes, em segundos

	// arma o temporizador ITIMER_REAL
	if(setitimer(ITIMER_REAL, &timer, 0) < 0) {
		perror("Erro em setitimer: ");
		exit(1);
	}
}

// suspende a tarefa atual,
// transferindo-a da fila de prontas para a fila "queue"
void task_suspend(task_t **queue) {
#ifdef DEBUG
	printf("task_suspend: suspendendo task [%d]\n", running_task->id);

#endif
	// muda o estado da tarefa atual para SUSPENSA
	running_task->status = STTS_SUSPENSA;

#ifdef DEBUG
	printf("task_suspend: alterando status da task [%d] para SUSPENSA\n", running_task->id);
#endif

	// procura a tarefa atual na fila de prontas, que é uma lista circular
	if(procura_task((queue_t *)queue_prontas) != NULL) {
		// se a tarefa atual estiver na fila de prontas, remove-a
		if(queue_remove((queue_t **)&queue_prontas, (queue_t *)running_task) < 0) {
			perror("task_suspend: erro ao remover task da fila de prontas\n");
		}
	}

	// insere a tarefa atual na fila apontada por queue (se essa fila não for nula)
	if(queue_append((queue_t **)queue, (queue_t *)running_task) < 0) {
		perror("task_suspend: erro ao inserir task na fila de suspensas\n");
	}

#ifdef DEBUG
	printf("task_suspend: inserido na fila de suspensas\n");
#endif

	// volta para o dispatcher
	task_switch(&task_dispatcher);
}

void task_awake(task_t *task, task_t **queue) {
#ifdef DEBUG
	printf("task_awake: acordando task [%d]\n", task->id);
#endif

	// se a fila queue não for nula, retira a tarefa apontada por task dessa fila
	// imprime a fila de suspensas e de prontas
	/*queue_print("fila_suspensas", (queue_t *)*queue, print_elem);
	queue_print("fila_prontas", (queue_t *)queue_prontas, print_elem);*/
	if(*queue != NULL) {
		// muda o estado da tarefa para PRONTA
		task->status = STTS_PRONTA;
		if(queue_remove((queue_t **)queue, (queue_t *)task) < 0) {
			perror("task_awake: erro ao remover task da fila de suspensas\n");
		}

		// insere a tarefa na fila de tarefas prontas
		if(queue_append(&queue_prontas, (queue_t *)task) < 0) {
			perror("task_awake: erro ao inserir task na fila de prontas\n");
		}
	}
}

int task_wait(task_t *task) {
#ifdef DEBUG
	printf("task_wait: task [%d] esperando por task [%d]\n", running_task->id, task->id);
#endif

	// Caso a tarefa b não exista ou já tenha encerrado, esta chamada deve retornar imediatamente, sem suspender a tarefa atual
	if(task == NULL)
		return -1;

	else if(task->status == STTS_TERMINADA)
		return task->exit_code;

	// muda o estado da tarefa atual para SUSPENSA
	running_task->status = STTS_SUSPENSA;

	// suspende a tarefa atual e a coloca na fila de tarefas suspensas da tarefa b
	task_suspend(&task->fila_suspensas);

	// retorna o código de retorno da tarefa b
	return task->exit_code;
}

void task_sleep(int t) {
	// muda o estado da tarefa atual para SUSPENSA
	running_task->status = STTS_SUSPENSA;

	// calcula o momento em que a tarefa voltará a ser executada
	running_task->momento_acordar = systime() + t;

	// suspende a tarefa atual e a coloca na fila de tarefas suspensas
	task_suspend((task_t **)&queue_adormecidas);
}

/**
 * Função que procura a task dentro da queue, caso não encontre, retorna NULL
 */
task_t *procura_task(queue_t *queue) {
	task_t *aux = (task_t *)queue;

	// caso a fila não tenha nenhum elemento
	if(aux == NULL)
		return NULL;

	// verifica se há apenas um elemento na fila
	if(aux->next == (task_t *)queue) {
		if(aux->id == running_task->id) {
			return aux;
		}
	}

	// procura a task na fila de prontas
	while(aux->next != (task_t *)queue) {
		if(aux->id == running_task->id) {
			return aux;
		}
		aux = aux->next;
	}

	// se nao achou a task na fila de prontas retorna NULL
	return NULL;
}

int sem_init (semaphore_t *s, int value) {
	if(s == NULL) {
		perror("Erro ao alocar memoria para o semaforo\n");
		return -1;
	}
	

	s->counter = value;
	s->queue = NULL;

#ifdef DEBUG
	printf("sem_init: semaforo inicializado com queue no endereço %p\n", s->queue);
#endif

	// A chamada retorna 0 em caso de sucesso ou -1 em caso de erro.
	return 0;
}

// Se a tarefa for bloqueada, ela será reativada quando uma outra tarefa liberar o semáforo (através da operação sem_up) ou caso o semáforo seja destruído (operação sem_destroy).
int sem_down (semaphore_t *s) {
	// A chamada retorna 0 em caso de sucesso ou -1 em caso de erro (semáforo não existe ou foi destruído).
	if(s == NULL) {
		perror("Erro ao alocar memoria para o semaforo\n");
		return -1;
	}
	
	// decrementa o contador do semaforo, entrando na seção crítica
	enter_cs(&lock);
	s->counter--;
	leave_cs(&lock);



	// se o contador for menor que 0, a tarefa atual deve ser suspensa
	if(s->counter < 0) {
#ifdef DEBUG
		printf("sem_down: task [%d] vai ser suspensa\n", running_task->id);
#endif
		// suspende a tarefa atual e a coloca na fila do semáforo, mandando ao dispatcher depois (está dentro de task_suspend)
		task_suspend((task_t **)&s->queue);
	}

	// retorna 0 em caso de sucesso
	return 0;
}

// Realiza a operação Up no semáforo apontado por s.
int sem_up (semaphore_t *s) {
	// Esta chamada não é bloqueante (a tarefa que a executa não perde o processador).

	// A chamada retorna 0 em caso de sucesso ou -1 em caso de erro (semáforo não existe ou foi destruído).
	if(s == NULL) {
		perror("Erro ao alocar memoria para o semaforo\n");
		return -1;
	}

	// incrementa o contador do semaforo, entrando na seção crítica
	enter_cs(&lock);
	s->counter++;
	leave_cs(&lock);

	// Se houverem tarefas aguardando na fila do semáforo, a primeira da fila deve ser acordada e retornar à fila de tarefas prontas.
	if(queue_size(s->queue) > 0) {
#ifdef DEBUG
		printf("sem_up: acordando task [%d]\n", ((task_t *)s->queue)->id);
#endif
		task_awake((task_t *)s->queue, (task_t **)&s->queue);
	}

	return 0;
}

// 	Destrói o semáforo apontado por s, acordando todas as tarefas que aguardavam por ele.
int sem_destroy (semaphore_t *s) {
	// se nao houver semaforo, retorna erro
	if(s == NULL) {
		perror("Erro ao alocar memoria para o semaforo\n");
		return -1;
	}

#ifdef DEBUG
	printf("sem_destroy: destruindo semaforo\n");
#endif

	s->counter = 0;
	
	// destroi a queue do semaforo
	if(queue_size(s->queue) > 0) {
		task_t *aux = (task_t *)s->queue;
		task_t *prox = aux->next;
		while(queue_size(s->queue) > 0) {
			// imprime a fila de suspensas
			task_awake(aux, (task_t **)&s->queue);

			aux = prox;
			prox = aux->next;
		}
	}

	// libera a memoria alocada para o semaforo
	free(s->queue);

	// A chamada retorna 0 em caso de sucesso ou -1 em caso de erro.
	return 0;
}

// enter critical section
void enter_cs (int *lock) {
  // atomic OR (Intel macro for GCC)
  while (__sync_fetch_and_or (lock, 1));   // busy waiting
}
 
// leave critical section
void leave_cs (int *lock) {
  (*lock) = 0 ;
}