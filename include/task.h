#ifndef TASK_H
#define TASK_H


typedef struct {
    int id, ingresso, duracao, periodo, prazo;
    char cor[6]; // hexadecimal
    short int estado; // 0 = pronto, 1 = executando, 2 = bloqueado, 3 = finalizado
    // lista_eventos;
} TaskControlBlock;


#endif 