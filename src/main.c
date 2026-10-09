#include <stdio.h>
#include <unistd.h>
#include "../include/simulador.h"

enum modo_execucao{ modo_debug, modo_completo};

int main(int argc, char *argv[]) {
    /*--- validacoes iniciais ---*/
    enum modo_execucao modo = modo_completo;

    if(argc < 2 || argc > 3){
        printf("Execute no formato: ./main %s <arquivo txt>", argv[0]);
        return 1;
    }
    int opt;
    int flag_c = 0, flag_d = 0;

    while ((opt = getopt(argc, argv, "cd")) != -1) {
        switch (opt) {
            case 'c': flag_c = 1; break;
            case 'd': flag_d = 1; break;
            default:
                printf( "Uso: %s [-c | -d]\n", argv[0]);
                return 1;
        }
    }

    if (flag_c && flag_d) {
        printf("Uso: %s [-c | -d]\n", argv[0]);
        return 1;
    }

    

    if (flag_d) modo = modo_debug;

    abrirArquivoSimulacao(argv[2]);

    return 0;
}