#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/parser.h"

//retorna array de strings com os parametros gerais serparados por ;
char** parseParametrosGerais(char* linha){
    //array de strings
    char** parametros = (char**)malloc(MAX_PARAMETROS_GERAIS * sizeof(char*));
    if(parametros == NULL){
        fprintf(stderr, "Erro ao alocar memoria para os parametros gerais\n");
        exit(1);
    }

    //primeira string separada por ;
    char* token = strtok(linha, ";");
    int i = 0;
    while(token != NULL && i < MAX_PARAMETROS_GERAIS){
        parametros[i] = (char*)malloc(strlen(token) + 1);
        if(parametros[i] == NULL){
            fprintf(stderr, "Erro ao alocar memoria para o parametro geral %d\n", i);
            exit(1);
        }

        //copia para o parametro[i]
        strncpy(parametros[i], token, strlen(token) + 1);
        i++;

        //le a proxima string separada por ;
        token = strtok(NULL, ";");
    }
    parametros[i] = NULL;
    return parametros;

}

//retorna array de strings com os parametros da tarefa serparados por ;
char** parseParametrosTarefa(char* linha){
    char** parametros = (char**)malloc(MAX_PARAMETROS_TAREFA * sizeof(char*));
    if(parametros == NULL){
        fprintf(stderr, "Erro ao alocar memoria para os parametros da tarefa\n");
        exit(1);
    }

    char* token = strtok(linha, ";");
    int i = 0;
    while(token != NULL && i < MAX_PARAMETROS_TAREFA){
        parametros[i] = (char*)malloc(strlen(token) + 1);
        if(parametros[i] == NULL){
            fprintf(stderr, "Erro ao alocar memoria para o parametro da tarefa %d\n", i);
            exit(1);
        }
        strncpy(parametros[i], token, strlen(token) + 1);
        i++;
        token = strtok(NULL, ";");
    }
    parametros[i] = NULL;
    return parametros;
}
