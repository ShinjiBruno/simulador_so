#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/simulador.h"
#include "../include/parser.h"
#include "../include/gantt.h"


static void imprimir_uso(const char* prog) {
    printf("Uso: %s [-c | -d] <arquivo de configuracao>\n", prog);
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        imprimir_uso(argv[0]);
        return 1;
    }

    ModoExecucao modo;
    const char* arquivo;

    if (strcmp(argv[1], "-c") == 0) {
        modo = MODO_COMPLETO;
    } else if (strcmp(argv[1], "-d") == 0) {
        modo = MODO_DEBUG;
    } else {
        imprimir_uso(argv[0]);
        return 1;
    }
    arquivo = argv[2];

    // carrega a configuracao 
    char msg_erro[256];
    Configuracao* cfg = carregarConfiguracao(arquivo, msg_erro);
    if (!cfg) {
        printf("Erro ao carregar o arquivo de configuracao: %s\n", msg_erro);
        return 1;
    }

    int achou_aperiodica = 0;
    for (int i = 0; i < cfg->qtde_tarefas; i++) {
        if (cfg->tarefas[i].periodo == 0) {
            if (!achou_aperiodica) {
                printf("Aviso: tarefas aperiodicas sao ignoradas no Projeto A.\n");
                achou_aperiodica = 1;
            }
            printf("  Tarefa de id %d ignorada (periodo = 0).\n", cfg->tarefas[i].id);
        }
    }

    Simulador sim;
    if (simulador_iniciar(&sim, cfg) != 0) {
        printf("Erro ao inicializar o simulador.\n");
        liberarConfiguracao(cfg);
        return 1;
    }

    if (modo == MODO_COMPLETO) {
        simulador_executar_completo(&sim);
    } else {
        simulador_executar_debug(&sim);
    }

    simulador_liberar(&sim);
    liberarConfiguracao(cfg);
    return 0;
}