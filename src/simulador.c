#include <stdio.h>
#include <string.h>
#include "../include/simulador.h"
#include "../include/parser.h"

char algoritmo_escalonamento[5] = "     ";
int quantum = -1;
int qtde_cpus = -1;

void abrirArquivoSimulacao(char* nomeArquivo){
    //abrir o arquivo, se nao der mostrar mensagem de erro
    FILE* arquivo = fopen(nomeArquivo, "r");
    if(arquivo == NULL){
        fprintf(stderr, "Erro ao abrir o arquivo %s\n", nomeArquivo);
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
        fprintf(stderr, "Erro ao ler a primeira linha do arquivo %s\n", nomeArquivo);
        exit(1);
    }

    //o resto eh tudo tarefas
    //for loop pq eu vi no livro de maziero e achei legal
    for(; fgets(linha, sizeof(linha), arquivo) != NULL; ){
        
    }

    fclose(arquivo);
}