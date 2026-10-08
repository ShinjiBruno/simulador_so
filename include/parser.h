#ifndef PARSER_H
#define PARSER_H

#define MAX_PARAMETROS_GERAIS 3
#define MAX_PARAMETROS_TAREFA 7


char** parseParametrosGerais(char* linha);

char** parseParametrosTarefa(char* linha);

#endif