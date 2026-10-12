#ifndef ESCALONADOR_H
#define ESCALONADOR_H

#include "task.h"

#define ALG_RM  0
#define ALG_EDF 1

int algoritmo_para_indice(const char* str);

const char* nome_algoritmo(int indice);


int escalona(int indice, TaskControlBlock* tarefas, int n, int tick,
             const int* cpu_tarefa, int qtde_cpus, int* sorteio);

#endif