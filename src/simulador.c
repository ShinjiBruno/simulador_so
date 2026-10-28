#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/simulador.h"
#include "../include/escalonador.h"
#include "../include/gantt.h"


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
    t->estado = TASK_PRONTO;
    t->cpu = -1;
    t->proxima_ativ = sim->tick + t->periodo;

    registrar_evento(sim, sim->tick, i, -1, EVT_CHEGADA);
}

static void tratar_ativacoes(Simulador* sim) {
    for (int i = 0; i < sim->qtde_tarefas; i++) {
        TaskControlBlock* t = &sim->tarefas[i];
        if (t->periodo <= 0) continue; 

        if (t->execucoes >= 10) continue;

        if (t->estado == TASK_INATIVO && sim->tick >= t->proxima_ativ) {
            ativar_tarefa(sim, i);
        }
    }
}

static void tratar_deadline(Simulador* sim) {
    for (int i = 0; i < sim->qtde_tarefas; i++) {
        TaskControlBlock* t = &sim->tarefas[i];
        if (t->estado != TASK_FINALIZADO && t->ativacoes > t->execucoes &&
            t->restante > 0 && !t->perdeu_prazo && sim->tick > t->deadline_abs) {
            registrar_evento(sim, sim->tick, i, t->cpu, EVT_DEADLINE);
            t->perdeu_prazo = 1;
        }
    }
}

static void escalonar(Simulador* sim) {
    int alg = algoritmo_para_indice(sim->cfg->geral.algoritmo);
    int ncpu = sim->cfg->geral.qtde_cpus;

    int prontas = 0;
    for (int i = 0; i < sim->qtde_tarefas; i++)
        if (sim->tarefas[i].estado == TASK_PRONTO) prontas++;

    // CPUs ociosas e com trabalho disponivel: ligar e escalonar 
    for (int c = 0; c < ncpu; c++) {
        if (sim->cpu_tarefa[c] != -1) continue; //ocupada

        int sorteio = 0;
        int escolhida = escalona(alg, sim->tarefas, sim->qtde_tarefas,
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
        sim->cpu_tarefa[c] = escolhida;
        if (sorteio) {
            sim->sorteio_ultimo_tick = 1;
            registrar_evento(sim, sim->tick, escolhida, c, EVT_SORTEIO);
        }
    }
}

static void preemptar(Simulador* sim) {
    int alg = algoritmo_para_indice(sim->cfg->geral.algoritmo);
    int sorteio = 0;

    for (int c = 0; c < sim->cfg->geral.qtde_cpus; c++) {
        if (!sim->cpu_ligada[c]) continue;

        //indice da tarefa executando agora na cpu c
        int atual = sim->cpu_tarefa[c];
        if (atual == -1) continue;

        int melhor = escalona(alg, sim->tarefas, sim->qtde_tarefas,
                              sim->tick, sim->cpu_tarefa,
                              sim->cfg->geral.qtde_cpus, &sorteio);
        //so preempta se tiver tarefa pronta mais prioritarias que a atual
        if (melhor == -1 || melhor == atual) continue;

        int cmp = ccmpara_prioridade_algoritmo(alg, &sim->tarefas[melhor], &sim->tarefas[atual]);

        if (cmp) {
            //desaloja atual e coloca melhor
            sim->tarefas[atual].estado = TASK_PRONTO;
            sim->tarefas[atual].cpu = -1;
            sim->cpu_tarefa[c] = melhor;
            sim->tarefas[melhor].estado = TASK_EXECUTANDO;
            sim->tarefas[melhor].cpu = c;
            if (sorteio) {
                sim->sorteio_ultimo_tick = 1;
                registrar_evento(sim, sim->tick, melhor, c, EVT_SORTEIO);
            }
        }
    }
}

static void executar_um_tick(Simulador* sim) {
    for (int c = 0; c < sim->cfg->geral.qtde_cpus; c++) {
        if (!sim->cpu_ligada[c]) continue;
        int i = sim->cpu_tarefa[c];
        if (i == -1) continue;

        TaskControlBlock* t = &sim->tarefas[i];
        t->restante--;
        t->corrido++;

        if (t->restante <= 0) {
            t->execucoes++;
            registrar_evento(sim, sim->tick, i, c, EVT_FIM);
            sim->cpu_tarefa[c] = -1;
            t->cpu = -1;

            if (t->execucoes >= 10) {
                t->estado = TASK_FINALIZADO;
            } else {
                t->estado = TASK_INATIVO; 
            }
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
        if (/*sim->tarefas[i].periodo > 0 &&*/ sim->tarefas[i].execucoes < 10)
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
        if (sim->tick > 100000) break;
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

        if (strcmp(p, "n") == 0 || strcmp(p, "next") == 0) {
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
                    if (v <= 0) printf("Erro: duracao deve ser > 0.\n");
                    else t->duracao = v;
                } else if (strcmp(campo, "periodo") == 0) {
                    t->periodo = v >= 0 ? v : t->periodo;
                } else if (strcmp(campo, "prazo") == 0) {
                    t->prazo = v >= 0 ? v : t->prazo;
                } else if (strcmp(campo, "estado") == 0) {
                    if (v < 0 || v > 4) printf("Erro: estado invalido (0=inativo,1=pronto,2=exec,3=bloq,4=fim).\n");
                    else t->estado = v;
                } else {
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