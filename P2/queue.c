// GRR20211774 Matheus Telles Batista
#include "queue.h"

#include <stdio.h>

// Insere um elemento no final da fila.
// Condicoes a verificar, gerando msgs de erro:
// - a fila deve existir
// - o elemento deve existir
// - o elemento nao deve estar em outra fila
// Retorno: 0 se sucesso, <0 se ocorreu algum erro
//
int queue_append(queue_t **queue, queue_t *elem) {
    queue_t *aux = NULL;
    int pertence = 0;
    // verificacao dos erros
    if (!queue) {
        perror("ERRO: fila nao existe");
        return -1;
    } else if (!elem) {
        perror("ERRO: elemento nao existe");
        return -1;
    } 
    // verifica se o elemento esta em uma fila (podendo ser nessa ou nao)
    else if (elem->next || elem->prev) {
        // aqui dentro eh verificado se esta nessa fila ou se nao esta, disparando a mensagem de erro adequada
        
        aux = (*queue);

        // laco que verifica se pertence a fila
        do {
            if (aux == elem) {
                pertence = 1;
                break;
            }

            aux = aux->next;
        } while (aux != *queue);

        pertence == 1 ? perror("ERRO: elemento ja esta na fila") : perror("ERRO: elemento pertence a outra fila");
        return -1;
    }

    // se a fila estiver vazia, o first (*queue) aponta para o elem
    if (!(*queue)) {
        elem->next = elem;
        elem->prev = elem;
        (*queue) = elem;
    }
    // se a fila nao estiver vazia, insere no final da fila
    else {
        // o elemento auxiliar aponta para o ultimo
        aux = (*queue)->prev;

        // o anterior ao que sera inserido sera o antigo ultimo
        elem->prev = aux;
        // o proximo ao que sera inserido sera o primeiro
        elem->next = *queue;

        // o proximo do antigo ultimo sera o elemento inserido
        aux->next = elem;

        // o anterior ao primeiro sera o elemento inserido
        (*queue)->prev = elem;
    }

    return 0;
}

//------------------------------------------------------------------------------
// Remove o elemento indicado da fila, sem o destruir.
// Condicoes a verificar, gerando msgs de erro:
// - a fila deve existir
// - a fila nao deve estar vazia
// - o elemento deve existir
// - o elemento deve pertencer a fila indicada
// Retorno: 0 se sucesso, <0 se ocorreu algum erro
int queue_remove(queue_t **queue, queue_t *elem) {
    queue_t *anterior, *proximo;
    queue_t *aux;
    int pertence = 0;

    // verificacao dos erros
    if (!queue) {
        perror("ERRO: fila nao existe");
        return -1;
    } else if (!(*queue)) {
        perror("ERRO: fila vazia");
        return -1;
    } else if (!elem) {
        perror("ERRO: elemento nao existe");
        return -1;
    } else if (!(elem->next) && !(elem->next)) {
        perror("ERRO: elemento nao pertence a nenhuma fila");
        return -1;
    }
    // verificacao se pertence a fila: eh realizado um teste com todos os elementos
    aux = *queue;

    // teste com o primeiro elemento, se nao for, ira para o proximo
    if (elem != aux) {
        aux = aux->next;
        // laco que verifica se pertence a fila
        while (aux != *queue) {
            if (aux == elem) {
                pertence = 1;
                break;
            }

            aux = aux->next;
        }

        if (pertence == 0) {
            perror("ERRO: elemento nao pertence a esta fila");
            return -1;
        }

        // se passou daqui, nao tem apenas um elemento e pertence a fila

        // ponteiros auxiliares.
        anterior = elem->prev;
        proximo = elem->next;

        // nestes comentarios, interprete anterior e proximo literalmente como as variaveis proximo e anterior
        // next do anterior sera o proximo
        anterior->next = proximo;
        // prev do proximo sera o anterior
        proximo->prev = anterior;

    }
    // se for o primeiro elemento que sera removido
    else {
        // caso tenha apenas um elemento
        if (elem->prev == elem) {
            *queue = NULL;
        } else {
            (*queue)->prev->next = elem->next;
            (*queue)->next->prev = elem->prev;
            *queue = elem->next;
        }
    }

    // isola o elemento, tornando nulo prev e next
    elem->next = NULL;
    elem->prev = NULL;

    return 0;
}

// Percorre a fila e imprime na tela seu conteúdo. A impressão de cada
// elemento é feita por uma função externa, definida pelo programa que
// usa a biblioteca. Essa função deve ter o seguinte protótipo:
//
// void print_elem (void *ptr) ; // ptr aponta para o elemento a imprimir
void queue_print(char *name, queue_t *queue, void print_elem(void *)) {
    // esta variavel "primeiro" denota o fim de um ciclo completo na fila circular
    queue_t *primeiro = NULL;
    queue_t *aux = NULL;

    printf("%s: [", name);

    // verificacao dos erros
    if (queue != NULL) {
        primeiro = queue;
        aux = queue->next;

        // printa o primeiro elemento
        print_elem(primeiro);

        // caso tenha apenas um elemento, nao cai nesse loop (pois primeiro->next = primeiro)
        while (primeiro != aux) {
            putchar(' ');
            print_elem(aux);
            aux = aux->next;
        }
    }
    printf("]\n");
}

// Conta o numero de elementos na fila
// Retorno: numero de elementos na fila

int queue_size(queue_t *queue) {
    // esta variavel "primeiro" denota o fim de um ciclo completo na fila circular
    queue_t *primeiro = queue;
    queue_t *aux = NULL;
    int num_elem = 0;

    // se esta vazia
    if (!primeiro) return 0;

    // se nao esta vazia, temos pelo menos um elemento
    num_elem++;

    aux = queue->next;

    // caso tenha apenas um elemento, nao cai nesse loop (pois primeiro->next = primeiro)
    while (primeiro != aux) {
        num_elem++;
        aux = aux->next;
    }

    return num_elem;
}
