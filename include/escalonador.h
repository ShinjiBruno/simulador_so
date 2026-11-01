#ifndef ESCALONADOR_H
#define ESCALONADOR_H

#include "task.h"

typedef struct OperacoesEscalonador {
    const char* nome;
    
    //int (*escalona)(struct OperacoesEscalonador* self, TaskControlBlock* tarefas, int n, int tick, const int* cpu_tarefa, int qtde_cpus, int* sorteio);
    
    //'a é mais prioritaria que 'b'?
    int (*algoritmo_compara_prioridade)(const TaskControlBlock* a, const TaskControlBlock* b);
} OperacoesEscalonador;

typedef enum {
    ERROR,
    ALG_RM, 
    ALG_EDF, 
    ALG_FCFS
} Algortimo_Escalonamento;

Algortimo_Escalonamento algoritmo_para_indice(const char* str);

const char* nome_algoritmo(Algortimo_Escalonamento indice);


int escalona(OperacoesEscalonador operacao, TaskControlBlock* tarefas, int n, int tick,
             const int* cpu_tarefa, int qtde_cpus, int* sorteio);

int ccmpara_prioridade_algoritmo(Algortimo_Escalonamento alg, TaskControlBlock* a, TaskControlBlock* b);

int compara_prioridadeRM(const TaskControlBlock* a, const TaskControlBlock* b);

int compara_prioridadeEDF(const TaskControlBlock* a, const TaskControlBlock* b);

int compara_prioridadeFCFS(const TaskControlBlock* a, const TaskControlBlock* b);

#endif