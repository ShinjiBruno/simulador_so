#ifndef PARSER_H
#define PARSER_H

#include "task.h"

#define MAX_PARAMETROS_GERAIS 3
#define MAX_PARAMETROS_TAREFA 7

#define MAX_LINHA 4096   //tamanho maximo de uma linha do arquivo
#define MAX_TAREFAS 1024 //limite maximo de tarefas suportadas

// Parametros gerais 
typedef struct {
    char algoritmo[8]; // "RM" ou "EDF" 
    int  quantum;
    int  qtde_cpus;
} ParametrosGerais;

typedef struct {
    ParametrosGerais geral;
    TaskControlBlock *tarefas;
    int              qtde_tarefas;
} Configuracao;

Configuracao* carregarConfiguracao(const char* caminho, char* msg_erro);

// libera toda a memoria alocada por carregarConfiguracao() 
void liberarConfiguracao(Configuracao* cfg);

#endif