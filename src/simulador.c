#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "../include/simulador.h"
#include "../include/escalonador.h"
#include "../include/gantt.h"

int ehHex(char* str) {
    if (strlen(str) != 6) return 0;
    for (int i = 0; i < 6; i++) {
        char c = str[i];
        if (!((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f'))) {
            return 0;
        }
    }
    return 1;
}

static void registrar_evento(Simulador* sim, int tick, int id_tarefa, int cpu,
                             TipoEvento tipo) {
    if (sim->qtde_eventos >= sim->cap_eventos) {
        sim->cap_eventos = sim->cap_eventos ? sim->cap_eventos * 2 : 128;
        sim->eventos = (EventoGantt*)realloc(sim->eventos,
                                             sim->cap_eventos * sizeof(EventoGantt));
    }
    EventoGantt* e = &sim->eventos[sim->qtde_eventos++];
    e->tick = tick;
    e->id_tarefa = id_tarefa;
    e->cpu = cpu;
    e->tipo = tipo;
}

static void ativar_tarefa(Simulador* sim, int i) {
    TaskControlBlock* t = &sim->tarefas[i];

    t->ativacoes++;
    t->ativacao_abs = sim->tick;
    t->restante = t->duracao;
    t->corrido = 0;
    t->deadline_abs = sim->tick + t->prazo;
    t->perdeu_prazo = 0;
    t->quantum_cont = 0;
    t->tempo_fila = sim->tick;
    t->estado = TASK_PRONTO;
    t->cpu = -1;
    t->proxima_ativ = sim->tick + t->periodo;

    registrar_evento(sim, sim->tick, i, -1, EVT_CHEGADA);
}

static void tratar_ativacoes(Simulador* sim) {
    for (int i = 0; i < sim->qtde_tarefas; i++) {
        TaskControlBlock* t = &sim->tarefas[i];
        //ignorar aperiodicas
        if (t->periodo <= 0) continue; 

        //ignorar tarefas ja completas (fez 10 execucoes)
        if (t->execucoes >= MAX_EXECUCOES) continue;

        if (t->estado == TASK_INATIVO && sim->tick >= t->proxima_ativ) {
            ativar_tarefa(sim, i);
        }
    }
}

static void tratar_deadline(Simulador* sim) {
    for (int i = 0; i < sim->qtde_tarefas; i++) {
        TaskControlBlock* t = &sim->tarefas[i];
        if (t->estado != TASK_FINALIZADO && 
            t->ativacoes > t->execucoes &&
            t->restante > 0 && 
            !t->perdeu_prazo && 
            sim->tick > t->deadline_abs) {

            registrar_evento(sim, sim->tick, i, t->cpu, EVT_DEADLINE);
            t->perdeu_prazo = 1;

        }
    }
}

static void escalonar(Simulador* sim) {
    int ncpu = sim->cfg->geral.qtde_cpus;

    int prontas = 0;
    for (int i = 0; i < sim->qtde_tarefas; i++)
        if (sim->tarefas[i].estado == TASK_PRONTO) prontas++;

    // CPUs ociosas e com trabalho disponivel: ligar e escalonar 
    for (int c = 0; c < ncpu; c++) {
        if (sim->cpu_tarefa[c] != -1) continue; //ocupada

        int sorteio = 0;
        int escolhida = escalona(sim->operacoes, sim->tarefas, sim->qtde_tarefas,
                                 sim->tick, sim->cpu_tarefa, ncpu, &sorteio);
        if (escolhida == -1) {
            // sem tarefa pronta para esta cpu: desligar
            if (sim->cpu_ligada[c] && sim->cpu_desligada_desde[c] == -1) {
                sim->cpu_desligada_desde[c] = sim->tick;
                registrar_evento(sim, sim->tick, -1, c, EVT_DESLIGA);
            }
            sim->cpu_ligada[c] = 0;
            continue;
        }

        // ha trabalho: ligar a cpu (se tava desligada) 
        if (!sim->cpu_ligada[c]) {
            sim->cpu_ligada[c] = 1;
            sim->cpu_desligada_desde[c] = -1;
        }

        // atribui a tarefa a cpu
        sim->tarefas[escolhida].estado = TASK_EXECUTANDO;
        sim->tarefas[escolhida].cpu = c;
        sim->tarefas[escolhida].quantum_cont = 0;
        sim->cpu_tarefa[c] = escolhida;
        if (sorteio) {
            sim->sorteio_ultimo_tick = 1;
            registrar_evento(sim, sim->tick, escolhida, c, EVT_SORTEIO);
        }
    }
}

static void preemptar(Simulador* sim) {
    int sorteio = 0;

    for (int c = 0; c < sim->cfg->geral.qtde_cpus; c++) {
        if (!sim->cpu_ligada[c]) continue;

        //indice da tarefa executando agora na cpu c
        int atual = sim->cpu_tarefa[c];
        if (atual == -1) continue;

        int melhor = escalona(sim->operacoes, sim->tarefas, sim->qtde_tarefas,
                              sim->tick, sim->cpu_tarefa,
                              sim->cfg->geral.qtde_cpus, &sorteio);
        //so preempta se tiver tarefa pronta mais prioritarias que a atual
        if (melhor == -1 || melhor == atual) continue;

        int cmp = sim->operacoes.algoritmo_compara_prioridade(&sim->tarefas[melhor], &sim->tarefas[atual]);
        if (cmp && cmp != -1) {
            //desaloja atual e coloca melhor
            sim->tarefas[atual].estado = TASK_PRONTO;
            sim->tarefas[atual].cpu = -1;
            sim->tarefas[atual].quantum_cont = 0;
            sim->tarefas[atual].tempo_fila = sim->tick;
            sim->cpu_tarefa[c] = melhor;
            sim->tarefas[melhor].estado = TASK_EXECUTANDO;
            sim->tarefas[melhor].cpu = c;
            sim->tarefas[melhor].quantum_cont = 0;
            if (sorteio) {
                sim->sorteio_ultimo_tick = 1;
                registrar_evento(sim, sim->tick, melhor, c, EVT_SORTEIO);
            }
        }
    }
}

static void executar_um_tick(Simulador* sim) {
    int quantum = sim->cfg->geral.quantum;

    for (int c = 0; c < sim->cfg->geral.qtde_cpus; c++) {
        if (!sim->cpu_ligada[c]) continue;
        int i = sim->cpu_tarefa[c];
        if (i == -1) continue;

        TaskControlBlock* t = &sim->tarefas[i];
        t->restante--;
        t->corrido++;
        t->quantum_cont++;

        // a tarefa terminou sua duracao neste tick?
        if (t->restante <= 0) {
            t->execucoes++;
            registrar_evento(sim, sim->tick, i, c, EVT_FIM);
            sim->cpu_tarefa[c] = -1;
            t->cpu = -1;
            t->quantum_cont = 0;

            if (t->execucoes >= MAX_EXECUCOES) {
                t->estado = TASK_FINALIZADO;
            } else {
                t->estado = TASK_INATIVO; 
            }
            continue;
        }

        // preempcao por tempo (quantum): o quantum expirou e a tarefa ainda
        // tem trabalho a fazer, entao ela volta para a fila de prontos. Isso
        // vale para QUALQUER algoritmo: e o "tempo compartilhado" do sistema.
        // (p.ex. FCFS + quantum passa a se comportar como Round-Robin)
        if (t->quantum_cont >= quantum) {
            t->estado = TASK_PRONTO;
            t->cpu = -1;
            t->quantum_cont = 0;
            t->tempo_fila = sim->tick; // fim da fila (importante p/ FCFS/RR)
            sim->cpu_tarefa[c] = -1;
            registrar_evento(sim, sim->tick, i, c, EVT_QUANTUM);
        }
    }
}

static void salvar_snapshot(Simulador* sim) {
    if (sim->qtde_historico >= sim->cap_historico) {
        sim->cap_historico = sim->cap_historico ? sim->cap_historico * 2 : 64;
        sim->historico = (SnapshotSistema**)realloc(sim->historico,
            sim->cap_historico * sizeof(SnapshotSistema*));
    }

    SnapshotSistema* s = (SnapshotSistema*)malloc(sizeof(SnapshotSistema));
    s->tick = sim->tick;
    s->qtde_tarefas = sim->qtde_tarefas;
    s->snapshots = (SnapshotTarefa*)malloc(
        sim->qtde_tarefas * sizeof(SnapshotTarefa));
    s->cpu_tarefa = (int*)malloc(sim->cfg->geral.qtde_cpus * sizeof(int));
    s->cpu_ligada = (int*)malloc(sim->cfg->geral.qtde_cpus * sizeof(int));

    for (int i = 0; i < sim->qtde_tarefas; i++)
        s->snapshots[i].tcb = sim->tarefas[i];
    for (int c = 0; c < sim->cfg->geral.qtde_cpus; c++) {
        s->cpu_tarefa[c] = sim->cpu_tarefa[c];
        s->cpu_ligada[c] = sim->cpu_ligada[c];
    }

    sim->historico[sim->qtde_historico++] = s;
}

static int acabou(Simulador* sim) {
    for (int i = 0; i < sim->qtde_tarefas; i++)
        if (/*sim->tarefas[i].periodo > 0 &&*/ sim->tarefas[i].execucoes < MAX_EXECUCOES)
            return 0;
    return 1;
}

static void restaurar_snapshot(Simulador* sim, SnapshotSistema* s) {
    sim->tick = s->tick;
    for (int i = 0; i < sim->qtde_tarefas; i++)
        sim->tarefas[i] = s->snapshots[i].tcb;
    for (int c = 0; c < sim->cfg->geral.qtde_cpus; c++) {
        sim->cpu_tarefa[c] = s->cpu_tarefa[c];
        sim->cpu_ligada[c] = s->cpu_ligada[c];
    }
}

/*---------- exports----------*/
int simulador_iniciar(Simulador* sim, Configuracao* cfg) {
    //zerando struct
    memset(sim, 0, sizeof(Simulador));

    sim->cfg = cfg;
    sim->qtde_tarefas = cfg->qtde_tarefas;

    //inicializa tarefas
    sim->tarefas = (TaskControlBlock*)malloc(
        sim->qtde_tarefas * sizeof(TaskControlBlock));
    for (int i = 0; i < sim->qtde_tarefas; i++) {
        sim->tarefas[i] = cfg->tarefas[i];
        sim->tarefas[i].estado = TASK_INATIVO; 
        sim->tarefas[i].cpu = -1;
        sim->tarefas[i].execucoes = 0;
        sim->tarefas[i].ativacoes = 0;
        sim->tarefas[i].proxima_ativ = cfg->tarefas[i].ingresso;
        sim->tarefas[i].restante = 0;
        sim->tarefas[i].deadline_abs = 0;
        sim->tarefas[i].ativacao_abs = 0;
        sim->tarefas[i].corrido = 0;
        sim->tarefas[i].perdeu_prazo = 0;
        sim->tarefas[i].quantum_cont = 0;
        sim->tarefas[i].tempo_fila = cfg->tarefas[i].ingresso;
    }

    // inicializa cpus
    int cpus = cfg->geral.qtde_cpus;
    sim->cpu_tarefa = (int*)malloc(cpus * sizeof(int));
    sim->cpu_ligada = (int*)malloc(cpus * sizeof(int));
    sim->cpu_desligada_desde = (int*)malloc(cpus * sizeof(int));
    for (int c = 0; c < cpus; c++) {
        sim->cpu_tarefa[c] = -1;
        sim->cpu_ligada[c] = 1;
        sim->cpu_desligada_desde[c] = -1;
    }

    sim->prontos = NULL;
    sim->historico = NULL;
    sim->qtde_historico = 0;
    sim->cap_historico = 0;

    if(strcmp(cfg->geral.algoritmo, "RM") == 0){
        sim->operacoes.nome = "RM";
        sim->operacoes.algoritmo_compara_prioridade = compara_prioridadeRM;
    }else if(strcmp(cfg->geral.algoritmo, "EDF") == 0){
        sim->operacoes.nome = "EDF";
        sim->operacoes.algoritmo_compara_prioridade = compara_prioridadeEDF;
    } else if (strcmp(cfg->geral.algoritmo, "FCFS") == 0){
        sim->operacoes.nome = "FCFS";
        sim->operacoes.algoritmo_compara_prioridade = compara_prioridadeFCFS;
    }

    srand(time(NULL)); 
    return 0;
}

static void avancar_tick(Simulador* sim) {
    sim->sorteio_ultimo_tick = 0;

    tratar_ativacoes(sim);

    tratar_deadline(sim);

    escalonar(sim);

    preemptar(sim);

    salvar_snapshot(sim);

    executar_um_tick(sim);
    sim->tick++;

    if (acabou(sim)) sim->acabou = 1;
}

static void relatar_cpus_desligadas(const Simulador* sim) {
    int ncpu = sim->cfg->geral.qtde_cpus;
    for (int c = 0; c < ncpu; c++) {
        int inicio = -1;
        int total = 0;
        printf("CPU %d ficou desligada nos ticks: ", c);
        int tem_periodo = 0;
        for (int h = 0; h < sim->qtde_historico; h++) {
            int ligada = sim->historico[h]->cpu_ligada[c];
            if (ligada) {
                if (inicio != -1) {
                    int fim = sim->historico[h]->tick;
                    printf("%s[%d..%d]", tem_periodo ? " " : "", inicio, fim - 1);
                    total += fim - inicio;
                    inicio = -1;
                    tem_periodo = 1;
                }
            } else {
                if (inicio == -1) inicio = sim->historico[h]->tick;
            }
        }
        if (inicio != -1) {
            int fim = sim->tick;
            printf("%s[%d..%d]", tem_periodo ? " " : "", inicio, fim - 1);
            total += fim - inicio;
        }
        if (total == 0) printf("nunca desligada");
        else printf(" (%d ticks)", total);
        printf("\n");
    }
}

void simulador_executar_completo(Simulador* sim) {
    while (!sim->acabou) {
        avancar_tick(sim);
        if (sim->tick > MAX_TICKS) break;
    }
    printf("\n=== Resultado final da simulacao (modo completo) ===\n");
    relatar_cpus_desligadas(sim);
    
    gantt_exportar_svg(sim, "gantt.svg");
    printf("Grafico de Gantt exportado para 'gantt.svg'.\n");
}

void simulador_executar_debug(Simulador* sim) {
    char cmd[512];

    printf("=== Modo passo-a-passo (debugger) ===\n");
    printf("Comandos: (n)ext, (p)rev, (i)nfo [id], (s)tatus, (m)odificar id campo valor,\n");
    printf("          (g)antt, (q)uit, (h)elp\n");

    for (;;) {
        printf("\n[tick %d] > ", sim->tick);
        if (!fgets(cmd, sizeof(cmd), stdin)) break;
        cmd[strcspn(cmd, "\r\n")] = '\0';

        char* p = strtok(cmd, " ");
        if (!p) continue;

        if(strcmp(p, "n") == 0 || strcmp(p, "next") == 0) {
            if (sim->acabou) {
                printf("Simulacao ja terminou.\n");
                continue;
            }
            avancar_tick(sim);

        } else if (strcmp(p, "p") == 0 || strcmp(p, "prev") == 0) {
            if (sim->qtde_historico > 1) {
                sim->qtde_historico--;
                restaurar_snapshot(sim, sim->historico[sim->qtde_historico]);
                int alvo = sim->historico[sim->qtde_historico]->tick;
                int k = 0;
                while (k < sim->qtde_eventos) {
                    if (sim->eventos[k].tick > alvo) {
                        memmove(&sim->eventos[k], &sim->eventos[k + 1],
                                (sim->qtde_eventos - k - 1) * sizeof(EventoGantt));
                        sim->qtde_eventos--;
                    } else {
                        k++;
                    }
                }
                sim->acabou = 0;
            } else {
                printf("Ja esta no inicio da simulacao.\n");
            }
        } else if (strcmp(p, "s") == 0 || strcmp(p, "status") == 0) {
            for (int i = 0; i < sim->qtde_tarefas; i++) {
                TaskControlBlock* t = &sim->tarefas[i];
                const char* e = (t->estado == TASK_PRONTO) ? "PRONTO" :
                    (t->estado == TASK_EXECUTANDO) ? "EXECUTANDO" :
                    (t->estado == TASK_BLOQUEADO) ? "BLOQUEADO" :
                    (t->estado == TASK_FINALIZADO) ? "FINALIZADO" : "INATIVO";
                
                

                printf("Tarefa id=%d [%s] cpu=%d execucoes=%d restante=%d prazo_absoluto=%d ativacoes=%d\n",
                       t->id, e, t->cpu, t->execucoes, t->restante,
                       t->deadline_abs, t->ativacoes);
            }
        } else if (strcmp(p, "i") == 0 || strcmp(p, "info") == 0) {
            char* arg = strtok(NULL, " ");
            if (!arg) { printf("Use: info <id>\n"); continue; }
            int id = atoi(arg);
            int achou = 0;
            for (int i = 0; i < sim->qtde_tarefas; i++) {
                if (sim->tarefas[i].id == id) {
                    TaskControlBlock* t = &sim->tarefas[i];
                    if (!ehHex(t->cor)) {
                        printf("Cor da tarefa %d invalida.\n", id);
                        continue;
                    }
                    printf("Tarefa id=%d cor=%s ingresso=%d duracao=%d periodo=%d prazo=%d\n",
                           t->id, t->cor, t->ingresso, t->duracao, t->periodo, t->prazo);
                    printf("  execucoes=%d ativacoes=%d restante=%d prazo_absoluto=%d\n",
                           t->execucoes, t->ativacoes, t->restante, t->deadline_abs);
                    achou = 1;
                    break;
                }
            }
            if (!achou) printf("Tarefa %d nao encontrada.\n", id);
        } else if (strcmp(p, "m") == 0 || strcmp(p, "modificar") == 0) {
            char* sid = strtok(NULL, " ");
            char* campo = strtok(NULL, " ");
            char* valor = strtok(NULL, " ");
            if (!sid || !campo || !valor) {
                printf("Use: modificar <id> <campo> <valor>\n");
                continue;
            }
            int id = atoi(sid);
            int achou = 0;
            for (int i = 0; i < sim->qtde_tarefas; i++) {
                if (sim->tarefas[i].id != id) continue;
                TaskControlBlock* t = &sim->tarefas[i];
                int v = atoi(valor);
                if (strcmp(campo, "duracao") == 0) {
                    if (v <= 0) {printf("Erro: duracao deve ser > 0.\n"); continue;}
                    else if(t->corrido > v) printf("Warning: duracao menor que tempo corrido da ativacao atual. Na proxima ativacao o tempo restante sera ajustado.\n");
                    
                    t->duracao = v;
                    t->restante = t->duracao - t->corrido;
                } else if (strcmp(campo, "periodo") == 0) {
                    
                    t->proxima_ativ -= t->periodo; //remove o periodo antigo 
                    t->periodo = v >= 0 ? v : t->periodo;
                    t->proxima_ativ += t->periodo; //adiciona o novo periodo

                } else if (strcmp(campo, "prazo") == 0) {

                    t->deadline_abs -= t->prazo; //remove o prazo antigo
                    t->prazo = v >= 0 ? v : t->prazo;
                    t->deadline_abs += t->prazo; //adiciona o novo prazo

                } else if (strcmp(campo, "estado") == 0) {
                    
                    if (v < 0 || v > 4) {printf("Erro: estado invalido (0=inativo,1=pronto,2=exec,3=bloq,4=fim).\n"); continue;}
                    
                    if(v != 2){ 
                        int tarefa_ind = -1;
                        for(int i = 0; i < sim->cfg->qtde_tarefas; i++){
                            if(sim->tarefas[i].id == t->id){
                                tarefa_ind = i;
                                break;
                            }
                        }
                        if(tarefa_ind == -1){
                            printf("Erro: tarefa nao encontrada.\n");
                            continue;
                        }
                        for(int i = 0; i < sim->cfg->geral.qtde_cpus; i++){
                            if(sim->cpu_tarefa[i] == tarefa_ind){
                                sim->cpu_tarefa[i] = -1;
                                sim->tarefas[tarefa_ind].cpu = -1;
                                break;
                            }
                        }
                    }
                    
                    t->estado = v;

                } else if (strcmp(campo, "id") == 0){
                    printf("Nao eh permitido alterar o id da tarefa.\n");
                } else if (strcmp(campo, "cor") == 0) {
                    if (!ehHex(valor)) {
                        printf("Cor da tarefa %d invalida.\n", id);
                        continue;
                    }
                    strcpy(t->cor, valor);
                }
                else {
                    printf("Campo '%s' nao reconhecido.\n", campo);
                }
                achou = 1;
                break;
            }
            if (!achou) printf("Tarefa %d nao encontrada.\n", id);
        } else if (strcmp(p, "g") == 0 || strcmp(p, "gantt") == 0) {
            if (gantt_exportar_svg(sim, "gantt.svg") == 0)
                printf("Grafico de Gantt exportado para 'gantt.svg'.\n");
            else
                printf("Erro ao gerar o grafico de Gantt.\n");
        } else if (strcmp(p, "h") == 0 || strcmp(p, "help") == 0) {
            printf("n/next: avancar 1 tick | p/prev: retroceder 1 tick\n");
            printf("status: estado de todas | info id: detalhes de uma tarefa\n");
            printf("modificar id campo valor | gantt: desenhar | q: sair\n");
        } else if (strcmp(p, "q") == 0 || strcmp(p, "quit") == 0) {
            gantt_exportar_svg(sim, "gantt.svg");
            printf("Gantt exportado para 'gantt.svg'. Saindo.\n");
            break;
        } else {
            printf("Comando desconhecido '%s'. Digite 'h' para ajuda.\n", p);
        }
    }
}


void simulador_liberar(Simulador* sim) {
    free(sim->tarefas);
    free(sim->cpu_tarefa);
    free(sim->cpu_ligada);
    free(sim->cpu_desligada_desde);
    for (int i = 0; i < sim->qtde_historico; i++) {
        free(sim->historico[i]->snapshots);
        free(sim->historico[i]->cpu_tarefa);
        free(sim->historico[i]->cpu_ligada);
        free(sim->historico[i]);
    }
    free(sim->historico);
    free(sim->eventos);
}