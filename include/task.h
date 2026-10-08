#ifndef TASK_H
#define TASK_H


typedef struct {
    int id;
    char cor[7]; // # + hexadecimal
    int ingresso;
    int duracao;
    int periodo;
    int prazo; 
    // lista_eventos;
} TaskControlBlock;


#endif 