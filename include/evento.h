#ifndef EVENTO_H
#define EVENTO_H


//tipos de evento visualizaveis no gantt 
typedef enum {
    EVT_CHEGADA   = 0, // nova tarefa criada no sistema              
    EVT_FIM       = 1, // tarefa terminou sua execucao               
    EVT_DEADLINE  = 2, // tarefa perdeu o prazo            
    EVT_SORTEIO   = 3, // desempate resolvido por sorteio            
    EVT_DESLIGA   = 4, // cpu desligada por ociosidade
    EVT_QUANTUM   = 5  // preempcao por esgotamento do quantum       
} TipoEvento;

//um ponto de evento registrado na timeline do gantt
typedef struct {
    int         tick;     // instante em que o evento ocorre           
    int         id_tarefa;// tarefa relacionada (-1 se nenhuma)         
    int         cpu;      // CPU relacionada (-1 se nenhuma)           
    TipoEvento  tipo;     // tipo do evento                            
} EventoGantt;

#endif