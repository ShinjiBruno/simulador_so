#ifndef SIMULADOR_H
#define SIMULADOR_H
#include "task.h"
#include "parser.h"
#include "evento.h"
#include "../lib/queue/queue.h"

typedef enum {
    MODO_COMPLETO = 0, 
    MODO_DEBUG    = 1  
} ModoExecucao;

typedef struct {
    TaskControlBlock tcb;
} SnapshotTarefa;

// snapshot do estado global em um tick 
typedef struct {
    int             tick;
    int             qtde_tarefas;
    QueueNode*      prontos;     
    SnapshotTarefa *snapshots;   // vetor com qtde_tarefas entradas 
    int            *cpu_tarefa;  // tarefa em cada CPU no instante  
    int            *cpu_ligada;  // 1 se a CPU esta ligada          
} SnapshotSistema;

// estado completo do simulador
typedef struct {
    Configuracao   *cfg;

    // tarefas em execucao de simulacao
    TaskControlBlock *tarefas;
    int               qtde_tarefas;
    //fila de prontos
    QueueNode*      prontos;    

    int *cpu_tarefa;  // indice da tarefa em cada CPU (-1 = livre)
    int *cpu_ligada;  // 1 = ligada, 0 = desligada
    int *cpu_desligada_desde; // tick em que foi desligada (-1)

    // relogio global 
    int tick;

    int acabou;

    SnapshotSistema **historico;
    int                qtde_historico;
    int                cap_historico;

    EventoGantt *eventos;
    int          qtde_eventos;
    int          cap_eventos;

    int sorteio_ultimo_tick;

} Simulador;

int simulador_iniciar(Simulador* sim, Configuracao* cfg);

// Libera toda a memoria do simulador
void simulador_liberar(Simulador* sim);


//executa em modo completo
void simulador_executar_completo(Simulador* sim);

// roda o loop interativo do modo debug
void simulador_executar_debug(Simulador* sim);

#endif