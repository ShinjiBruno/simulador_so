#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "../include/simulador.h"
#include "../include/parser.h"
#include "../include/task.h"

char algoritmo_escalonamento[5] = "     ";
int quantum = -1;
int qtde_cpus = -1;

TaskControlBlock criarTarefa(char** parametrosTarefa){
    TaskControlBlock tarefa;
    tarefa.id = atoi(parametrosTarefa[0]);
    strncpy(tarefa.cor, parametrosTarefa[1], sizeof(tarefa.cor));
    tarefa.ingresso = atoi(parametrosTarefa[2]);
    tarefa.duracao = atoi(parametrosTarefa[3]);
    tarefa.periodo = atoi(parametrosTarefa[4]);
    tarefa.prazo = atoi(parametrosTarefa[5]);

    return tarefa;
}

void abrirArquivoSimulacao(char* nomeArquivo){
    //abrir o arquivo, se nao der mostrar mensagem de erro
    FILE* arquivo = fopen(nomeArquivo, "r");
    if(arquivo == NULL){
        printf("Erro ao abrir o arquivo %s\n", nomeArquivo);
        exit(1);
    }
    
    char linha[256];
    //primeira linha sempre eh geral
    if(fgets(linha, sizeof(linha), arquivo) != NULL){

        //chamar funcao de parse dos parametros gerais
        char** parametrosGerais = parseParametrosGerais(linha);
        strncpy(algoritmo_escalonamento, parametrosGerais[0], sizeof(algoritmo_escalonamento));
        quantum = atoi(parametrosGerais[1]);
        qtde_cpus = atoi(parametrosGerais[2]);

        free(parametrosGerais);
    } else {
        printf("Erro ao ler a primeira linha do arquivo %s\n", nomeArquivo);
        exit(1);
    }

    //o resto eh tudo tarefas
    for(; fgets(linha, sizeof(linha), arquivo) != NULL; ){
        char **parametrosTarefa = parseParametrosTarefa(linha);


        free(parametrosTarefa);
    }

    fclose(arquivo);
}