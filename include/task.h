#ifndef TASK_H
#define TASK_H

typedef enum {
    TASK_INATIVO    = 0, //ainda nao criada ou entre ativacoes periodicas 
    TASK_PRONTO     = 1, //ativada e na fila de prontos, aguardando CPU  
    TASK_EXECUTANDO = 2, // ocupando uma CPU                            
    TASK_BLOQUEADO  = 3, // suspensa (evento, I/O, mutex, etc)          
    TASK_FINALIZADO = 4  // concluiu suas 10 execucoes                   
} EstadoTarefa;


typedef struct {
    //parametros de configuracao
    int     id;
    char    cor[7];       //hexadecimal  + '\0' */
    int     ingresso;
    int     duracao;
    int     periodo;
    int     prazo;

    //parametros de simulacao
    EstadoTarefa estado;  
    int     cpu;          // indice da CPU ocupada (-1 se nenhuma)
    int     execucoes;    // quantas ativacoes ja concluiram (max 10)      
    int     ativacoes;    // quantas ativacoes ja ocorreram               
    int     proxima_ativ; // proximo instante de ativacao periodica        
    int     restante;     // tempo de execucao restante da ativacao atual 
    int     deadline_abs; // deadline absoluto da ativacao corrente        
    int     ativacao_abs; // instante absoluto da ativacao corrente        
    int     corrido;      // tempo ja executado na ativacao corrente       
    int     perdeu_prazo; // 1 se a ativacao corrente ja perdeu o prazo   
    int     quantum_cont; // ticks ja consumidos na fatia de tempo atual  
    int     tempo_fila;   // instante em que entrou (por ultimo) na fila de prontos
} TaskControlBlock;

#endif