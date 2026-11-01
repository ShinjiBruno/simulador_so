#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include "../include/escalonador.h"


static int esta_executando(int i, const int* cpu_tarefa, int qtde_cpus) {
    for (int c = 0; c < qtde_cpus; c++)
        if (cpu_tarefa[c] == i) return 1;
    return 0;
}

static int desempate_prioridade(const TaskControlBlock* a,
                              const TaskControlBlock* b) {
    //criterios de desempate em ordme
    if (a->prazo != b->prazo) return (a->prazo < b->prazo);

    if (a->ingresso != b->ingresso)
        return (a->ingresso < b->ingresso);

    if (a->duracao != b->duracao)
        return (a->duracao < b->duracao);

    return -1; 
}

//retorna o indice da tarefa mais prioritaria entre as prontas. -1 se nao tiger nenhuma
int escalona(OperacoesEscalonador operacao, TaskControlBlock* tarefas, int n, int tick,
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

        int prioridade_algoritmo = operacao.algoritmo_compara_prioridade(&tarefas[i], &tarefas[melhor]);
        if(prioridade_algoritmo != -1){
            if(prioridade_algoritmo) melhor = i;
            continue;
        } 

        //se uma tarefa ta executando imediatamente antes do escalonamento ser acionado
        int i_exec = esta_executando(i, cpu_tarefa, qtde_cpus);
        int m_exec = esta_executando(melhor, cpu_tarefa, qtde_cpus);
        if (i_exec != m_exec) {
            if (i_exec) melhor = i;
            continue;
        }

        int desempate = desempate_prioridade(&tarefas[i], &tarefas[melhor]);

        if(desempate == -1){
            if (rand() % 2 == 0) melhor = i;
            *sorteio = 1;
            continue;
        }

        if (desempate) melhor = i;
    }

    return melhor;
}

int compara_prioridadeRM(const TaskControlBlock* a, const TaskControlBlock* b){
    if(a->periodo == b->periodo) return -1;
    return (a->periodo < b->periodo) ;
}

int compara_prioridadeEDF(const TaskControlBlock* a, const TaskControlBlock* b){
    if(a->deadline_abs == b->deadline_abs) return -1;
    return (a->deadline_abs < b->deadline_abs);
}

int compara_prioridadeFCFS(const TaskControlBlock* a, const TaskControlBlock* b){
    // FCFS ordena por quem entrou primeiro na fila de prontos. O campo
    // tempo_fila e atualizado a cada (re)colocacao na fila, inclusive na
    // preempcao por quantum, o que transforma o FCFS em Round-Robin quando o
    // quantum expira: a tarefa preemptada vai para o fim da fila.
    if(a->tempo_fila == b->tempo_fila) return -1;
    return (a->tempo_fila < b->tempo_fila);
}
