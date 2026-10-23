#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "../include/escalonador.h"


int algoritmo_para_indice(const char* str) {
    if (!str) return -1;
    char buf[16];
    size_t i = 0;
    for (; str[i] && i < sizeof(buf) - 1; i++)
        buf[i] = (char)toupper((unsigned char)str[i]);
    buf[i] = '\0';

    if (strcmp(buf, "RM") == 0) return ALG_RM;
    if(strcmp(buf, "EDF") == 0) return ALG_EDF;
    if(strcmp(buf, "FCFS") == 0) return ALG_FCFS;

    return -1;
}

const char* nome_algoritmo(int indice) {
    switch (indice) {
        case ALG_RM:  return "RM";
        case ALG_EDF: return "EDF";
        case ALG_FCFS: return "FCFS";
        default:      return "?";
    }
}

static int esta_executando(int i, const int* cpu_tarefa, int qtde_cpus) {
    for (int c = 0; c < qtde_cpus; c++)
        if (cpu_tarefa[c] == i) return 1;
    return 0;
}

static int compara_prioridade(int alg, const TaskControlBlock* a,
                              const TaskControlBlock* b) {
    long ka, kb;
    if (alg == ALG_RM) {
        ka = a->periodo;
        kb = b->periodo;
    } 
    else if(alg == ALG_EDF){ ///EDF 
        ka = a->deadline_abs;
        kb = b->deadline_abs;
    }
    else if(alg == ALG_FCFS){
        ka = a->ativacao_abs;
        kb = b->ativacao_abs;
    } 

    if (ka != kb) return (ka < kb) ? -1 : 1;

    if (a->prazo != b->prazo) return (a->prazo < b->prazo) ? -1 : 1;

    if (a->ingresso != b->ingresso)
        return (a->ingresso < b->ingresso) ? -1 : 1;

    if (a->duracao != b->duracao)
        return (a->duracao < b->duracao) ? -1 : 1;

    return 0; 
}

//indice 0 = RM, 1 = EDF
int escalona(int indice, TaskControlBlock* tarefas, int n, int tick,
             const int* cpu_tarefa, int qtde_cpus, int* sorteio) {
    (void)tick;
    *sorteio = 0;

    int melhor = -1;

    for (int i = 0; i < n; i++) {
        if (tarefas[i].estado != TASK_PRONTO) continue;

        if (melhor == -1) {
            melhor = i;
            continue;
        }

        int cmp = compara_prioridade(indice, &tarefas[i], &tarefas[melhor]);

        if (cmp == 0) {
            int i_exec = esta_executando(i, cpu_tarefa, qtde_cpus);
            int m_exec = esta_executando(melhor, cpu_tarefa, qtde_cpus);
            if (i_exec != m_exec) {
                if (i_exec) melhor = i;
                continue;
            }
            if (rand() % 2 == 0) melhor = i;
            *sorteio = 1;
            continue;
        }

        if (cmp < 0) melhor = i;
    }

    return melhor;
}