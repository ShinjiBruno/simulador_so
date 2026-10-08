#include <stdio.h>
#include <unistd.h>


int main(int argc, char *argv[]) {
    /*--- validacoes iniciais ---*/
    int opt;
    while ((opt = getopt(argc, argv, "dc:")) != -1) {
        switch (opt) {
            case 'd': 
                break;
            case 'c': 
                break;
            default:
                fprintf(stderr, "Uso: %s [-v] [-o saida] arquivo\n", argv[0]);
                return 1;
        }
    }


    if(argc < 3){
        printf("Execute no formato: ./main %s <arquivo txt>", argv[0]);
        return 1;
    }




    return 0;
}