#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/gantt.h"
#include "../include/escalonador.h"

/*
 * -----------------------------------------------------------------------------
 * Diagrama de Gantt em SVG
 * -----------------------------------------------------------------------------
 * O grafico desenhado aqui segue o seguinte layout:
 *
 *   - eixo horizontal (X): relogio global da simulacao, medido em ticks;
 *   - eixo vertical   (Y): as tarefas do sistema, ordenadas do ID mais baixo
 *     (mais proximo do eixo X, na parte de baixo) para o ID mais alto (acima);
 *   - uma faixa extra no topo mostra, para cada CPU, os periodos em que ela
 *     esteve desligada por ociosidade (requisito de "mostrar o tempo em que o
 *     processador ficou desligado");
 *   - eventos importantes (chegada, termino, prazo perdido, sorteio e
 *     desligamento de CPU) sao indicados com elementos graficos distintos,
 *     explicados em uma legenda.
 *
 * As cores/estados de cada tique sao recuperados dos snapshots armazenados em
 * sim->historico (um por tick). Cada snapshot guarda o estado completo de todas
 * as tarefas e das CPUs no inicio daquele tick, permitindo reconstruir o
 * diagrama final e, no modo debug, o diagrama parcial ate o tick atual.
 * -----------------------------------------------------------------------------
 */

/* Converte uma cor "RRGGBB" (hexadecimal de 6 digitos) nos canais RGB. */
static void hex_para_rgb(const char* cor6, int* r, int* g, int* b) {
    unsigned int v = (unsigned int)strtoul(cor6, NULL, 16);
    *r = (int)((v >> 16) & 0xFF);
    *g = (int)((v >> 8) & 0xFF);
    *b = (int)(v & 0xFF);
}

/* Monta "#RRGGBB" a partir do campo cor da tarefa. */
static void cor_css(const char* cor6, char* saida) {
    saida[0] = '#';
    memcpy(saida + 1, cor6, 6);
    saida[7] = '\0';
}

/*
 * Escolhe a cor do texto desenhado sobre um retangulo de execucao (o numero da
 * CPU). Usa a luminancia do fundo para garantir contraste (texto preto sobre
 * cores claras e branco sobre cores escuras).
 */
static const char* cor_texto(const char* cor6) {
    int r, g, b;
    hex_para_rgb(cor6, &r, &g, &b);
    double lum = 0.299 * r + 0.587 * g + 0.114 * b;
    return lum > 150.0 ? "#000000" : "#ffffff";
}

/*
 * Define quantos pixels de largura cada tick ocupa. Para simulacoes longas a
 * largura por tick e reduzida para que o arquivo continue navegavel, sem
 * impor um limite maximo de tempo de simulacao.
 */
static double largura_tick(int max_tick) {
    if (max_tick <= 50)   return 24.0;
    if (max_tick <= 200)  return 12.0;
    if (max_tick <= 1000) return 6.0;
    if (max_tick <= 5000) return 3.0;
    return 2.0;
}

/* Posicao X do inicio do tick `t`. */
static double x_tick(int t, double x0, double cw) {
    return x0 + t * cw;
}

/*
 * Posicao X de um evento no tick `t`. Os eventos de chegada/deadline/sorteio/
 * desligamento ocorrem no inicio do tick; ja o termino (FIM) e registrado ao
 * final do ultimo tick de execucao da tarefa, portanto e desenhado no fim do
 * intervalo [t, t+1).
 */
static double x_evento(int t, TipoEvento tipo, double x0, double cw) {
    double base = x0 + t * cw;
    if (tipo == EVT_FIM) base += cw;
    return base;
}

/*
 * Monta um vetor `linha` que mapeia o indice do vetor de tarefas para a linha
 * ocupada no grafico (0 = linha de baixo, mais proxima do eixo X). A linha 0
 * (mais baixa) pertence a tarefa de menor ID, e assim por diante, conforme o
 * requisito de ordenacao descrescente por ID de baixo para cima.
 */
static void montar_linhas(const Simulador* sim, int* linha) {
    int n = sim->qtde_tarefas;
    int* ordem = (int*)malloc(n * sizeof(int));
    for (int i = 0; i < n; i++) ordem[i] = i;

    /* insertion sort por ID crescente (na pratica o parser ja entrega ordenado,
       mas este passo garante a ordenacao independente da origem dos dados). */
    for (int i = 1; i < n; i++) {
        int chave = ordem[i];
        int j = i - 1;
        while (j >= 0 && sim->tarefas[ordem[j]].id > sim->tarefas[chave].id) {
            ordem[j + 1] = ordem[j];
            j--;
        }
        ordem[j + 1] = chave;
    }
    for (int k = 0; k < n; k++) linha[ordem[k]] = k;
    free(ordem);
}

/* Desenha os padroes de preenchimento (pontilhado e hachurado) no <defs>. */
static void desenhar_defs(FILE* f) {
    fprintf(f,
        "  <defs>\n"
        "    <pattern id=\"patBloqueado\" width=\"8\" height=\"8\" patternUnits=\"userSpaceOnUse\">\n"
        "      <rect width=\"8\" height=\"8\" fill=\"#ffffff\"/>\n"
        "      <circle cx=\"2\" cy=\"2\" r=\"1.4\" fill=\"#000000\"/>\n"
        "      <circle cx=\"6\" cy=\"6\" r=\"1.4\" fill=\"#000000\"/>\n"
        "    </pattern>\n"
        "    <pattern id=\"patDesligado\" width=\"8\" height=\"8\" patternUnits=\"userSpaceOnUse\">\n"
        "      <rect width=\"8\" height=\"8\" fill=\"#e9e9e9\"/>\n"
        "      <line x1=\"0\" y1=\"8\" x2=\"8\" y2=\"0\" stroke=\"#999999\" stroke-width=\"1.2\"/>\n"
        "    </pattern>\n"
        "  </defs>\n");
}

/* Desenha a legenda explicando todos os elementos graficos usados no grafico. */
static void desenhar_legenda(FILE* f, double x0) {
    double x = x0;
    double y1 = 42.0;   /* linha 1 da legenda */
    double y2 = 62.0;   /* linha 2 da legenda */
    double y3 = 82.0;   /* linha 3 da legenda */
    double y4 = 102.0;  /* linha 4 da legenda */
    double s = 12.0;    /* tamanho do quadrado de amostra */
    double ty = 0.0;    /* deslocamento do texto para alinhar com a amostra */

    /* linha 1 ------------------------------------------------------------- */
    /* execucao (cor solida da tarefa) */
    fprintf(f,
        "  <rect x=\"%.1f\" y=\"%.1f\" width=\"%.1f\" height=\"%.1f\" fill=\"#3f51b5\" stroke=\"#000000\" stroke-width=\"1\"/>\n",
        x, y1 - s + 3, s, s);
    ty = y1 + 1;
    fprintf(f, "  <text x=\"%.1f\" y=\"%.1f\" font-size=\"12\">Executando (cor da tarefa + CPU)</text>\n", x + s + 6, ty);
    x += 230;

    /* pronta (ausencia de cor) */
    fprintf(f,
        "  <rect x=\"%.1f\" y=\"%.1f\" width=\"%.1f\" height=\"%.1f\" fill=\"none\" stroke=\"#999999\" stroke-width=\"1\"/>\n",
        x, y1 - s + 3, s, s);
    fprintf(f, "  <text x=\"%.1f\" y=\"%.1f\" font-size=\"12\">Pronta (na fila)</text>\n", x + s + 6, ty);
    
    /* linha 2 ------------------------------------------------------------- */
    x = x0;//x += 150;

    /* suspensa / bloqueada */
    fprintf(f,
        "  <rect x=\"%.1f\" y=\"%.1f\" width=\"%.1f\" height=\"%.1f\" fill=\"url(#patBloqueado)\" stroke=\"#000000\" stroke-width=\"1\"/>\n",
        x, y2 - s + 3, s, s);
    ty = y2 + 1;
    fprintf(f, "  <text x=\"%.1f\" y=\"%.1f\" font-size=\"12\">Suspensa (bloqueada)</text>\n", x + s + 6, ty);
    x += 230;

    /* CPU desligada */
    fprintf(f,
        "  <rect x=\"%.1f\" y=\"%.1f\" width=\"%.1f\" height=\"%.1f\" fill=\"url(#patDesligado)\" stroke=\"#666666\" stroke-width=\"1\"/>\n",
        x, y2 - s + 3, s, s);
    fprintf(f, "  <text x=\"%.1f\" y=\"%.1f\" font-size=\"12\">CPU desligada</text>\n", x + s + 6, ty);

    /* linha 3 ------------------------------------------------------------- */
    x = x0;
    /* chegada: triangulo verde apontando para baixo */
    fprintf(f,
        "  <polygon points=\"%.1f,%.1f %.1f,%.1f %.1f,%.1f\" fill=\"#2e7d32\" stroke=\"#ffffff\" stroke-width=\"1\"/>\n",
        x + 2, y3 - s + 3, x + s + 2, y3 - s + 3, x + 7, y3 + 3);
    ty = y3 + 1;
    fprintf(f, "  <text x=\"%.1f\" y=\"%.1f\" font-size=\"12\">Chegada da tarefa</text>\n", x + s + 10, ty);
    x += 230;

    /* termino: triangulo preto apontando para cima */
    fprintf(f,
        "  <polygon points=\"%.1f,%.1f %.1f,%.1f %.1f,%.1f\" fill=\"#000000\" stroke=\"#ffffff\" stroke-width=\"1\"/>\n",
        x + 2, y3 + 3, x + s + 2, y3 + 3, x + 7, y3 - s + 3);
    fprintf(f, "  <text x=\"%.1f\" y=\"%.1f\" font-size=\"12\">Termino</text>\n", x + s + 10, ty);
    
    /* linha 4 ------------------------------------------------------------- */
    //x += 110;
    x = x0;

    /* prazo perdido: losango vermelho */
    fprintf(f,
        "  <polygon points=\"%.1f,%.1f %.1f,%.1f %.1f,%.1f %.1f,%.1f\" fill=\"#d32f2f\" stroke=\"#ffffff\" stroke-width=\"1\"/>\n",
        x + 7, y4 - s + 3, x + 7 + 5, y4 - 2, x + 7, y4 + 3, x + 7 - 5, y4 - 2);
    ty = y4 + 1;
    fprintf(f, "  <text x=\"%.1f\" y=\"%.1f\" font-size=\"12\">Prazo perdido</text>\n", x + s + 12, ty);
    x += 230;

    /* sorteio: circulo ciano */
    fprintf(f, "  <circle cx=\"%.1f\" cy=\"%.1f\" r=\"5\" fill=\"#0288d1\" stroke=\"#ffffff\" stroke-width=\"1\"/>\n",
        x + 7, y4 - 2);
    fprintf(f, "  <text x=\"%.1f\" y=\"%.1f\" font-size=\"12\">Sorteio (desempate)</text>\n", x + s + 12, ty);
    x += 160;

    /* desligamento de CPU: linha vermelha vertical */
    fprintf(f, "  <line x1=\"%.1f\" y1=\"%.1f\" x2=\"%.1f\" y2=\"%.1f\" stroke=\"#d32f2f\" stroke-width=\"2\"/>\n",
        x + 7, y4 - s + 3, x + 7, y4 + 3);
    fprintf(f, "  <text x=\"%.1f\" y=\"%.1f\" font-size=\"12\">CPU desligada (evento)</text>\n", x + s + 12, ty);
}

/*
 * Desenha os marcadores de eventos (chegada, termino, prazo perdido e sorteio)
 * sobre a linha da tarefa correspondente e o marcador de desligamento sobre a
 * faixa da CPU.
 *
 * Observacao: o campo `id_tarefa` dos eventos registrados pelo simulador guarda
 * o INDICE do vetor de tarefas (e nao o id da tarefa), por isso ele e usado
 * diretamente como indice em sim->tarefas.
 */
static void desenhar_eventos(FILE* f, const Simulador* sim, const int* linha,
                             double x0, double cw, double task_top,
                             double row_h, double cpu_top, double cpu_h) {
    int nt = sim->qtde_tarefas;
    for (int e = 0; e < sim->qtde_eventos; e++) {
        EventoGantt ev = sim->eventos[e];

        if (ev.tipo == EVT_DESLIGA) {
            double cx = x_evento(ev.tick, ev.tipo, x0, cw);
            double cy = cpu_top + ev.cpu * cpu_h;
            fprintf(f,
                "  <line x1=\"%.1f\" y1=\"%.1f\" x2=\"%.1f\" y2=\"%.1f\" stroke=\"#d32f2f\" stroke-width=\"2\"/>\n",
                cx, cy, cx, cy + cpu_h);
            continue;
        }
        if (ev.id_tarefa < 0 || ev.id_tarefa >= nt) continue;

        int k = linha[ev.id_tarefa];
        double y = task_top + (nt - 1 - k) * row_h;
        double ytop = y;
        double ybot = y + row_h;
        double yc = y + row_h / 2.0;
        double cx = x_evento(ev.tick, ev.tipo, x0, cw);

        switch (ev.tipo) {
            case EVT_CHEGADA: /* triangulo verde para baixo (chegada) */
                fprintf(f,
                    "  <polygon points=\"%.1f,%.1f %.1f,%.1f %.1f,%.1f\" fill=\"#2e7d32\" stroke=\"#ffffff\" stroke-width=\"1\"/>\n",
                    cx - 5, ytop, cx + 5, ytop, cx, ytop + 9);
                break;
            case EVT_FIM: /* triangulo preto para cima (termino) */
                fprintf(f,
                    "  <polygon points=\"%.1f,%.1f %.1f,%.1f %.1f,%.1f\" fill=\"#000000\" stroke=\"#ffffff\" stroke-width=\"1\"/>\n",
                    cx - 5, ybot, cx + 5, ybot, cx, ybot - 9);
                break;
            case EVT_DEADLINE: /* losango vermelho (prazo perdido) */
                fprintf(f,
                    "  <polygon points=\"%.1f,%.1f %.1f,%.1f %.1f,%.1f %.1f,%.1f\" fill=\"#d32f2f\" stroke=\"#ffffff\" stroke-width=\"1\"/>\n",
                    cx, yc - 6, cx + 6, yc, cx, yc + 6, cx - 6, yc);
                break;
            case EVT_SORTEIO: /* circulo ciano (sorteio) */
                fprintf(f,
                    "  <circle cx=\"%.1f\" cy=\"%.1f\" r=\"4\" fill=\"#0288d1\" stroke=\"#ffffff\" stroke-width=\"1\"/>\n",
                    cx, yc);
                break;
            default:
                break;
        }
    }
}

int gantt_exportar_svg(const Simulador* sim, const char* caminho) {
    if (!sim || !caminho) return 1;

    FILE* f = fopen(caminho, "w");
    if (!f) return 1;

    int nt = sim->qtde_tarefas;
    int ncpu = sim->cfg->geral.qtde_cpus;
    int max_tick = sim->tick;
    if (max_tick < 0) max_tick = 0;
    int nsnap = sim->qtde_historico;
    if (nsnap > max_tick) nsnap = max_tick; /* seguranca: nao ler alem do tick */

    double cw = largura_tick(max_tick);

    /* mapeia indice do vetor de tarefas -> linha no grafico */
    int* linha = (int*)malloc(nt * sizeof(int));
    if (!linha) { fclose(f); return 1; }
    montar_linhas(sim, linha);

    /* geometria do desenho */
    double x0 = 100.0;                 /* margem esquerda (rotulos do eixo Y) */
    double pad_dir = 20.0;
    double chart_w = max_tick * cw;
    double row_h = 26.0;               /* altura de uma linha de tarefa */
    double cpu_h = 14.0;               /* altura de uma linha de CPU */
    double cpu_top = 120.0;             /* topo da faixa de CPUs */
    double task_gap = 10.0;
    double task_top = cpu_top + ncpu * cpu_h + task_gap;
    double task_bottom = task_top + nt * row_h;
    double axis_y = task_bottom + 12.0;
    double svg_h = axis_y + 150.0;

    double chart_fim = x0 + chart_w + pad_dir;
    double svg_w = chart_fim > 760.0 ? chart_fim : 760.0;

    /* cabecalho do SVG */
    fprintf(f,
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"%.1f\" height=\"%.1f\" viewBox=\"0 0 %.1f %.1f\" font-family=\"Arial, Helvetica, sans-serif\">\n",
        svg_w, svg_h, svg_w, svg_h);

    desenhar_defs(f);

    /* fundo */
    fprintf(f, "  <rect x=\"0\" y=\"0\" width=\"%.1f\" height=\"%.1f\" fill=\"#ffffff\"/>\n", svg_w, svg_h);

    /* titulo */
    fprintf(f,
        "  <text x=\"%.1f\" y=\"22\" font-size=\"16\" font-weight=\"bold\">"
        "Diagrama de Gantt — %s | quantum=%d | CPUs=%d | ticks=%d</text>\n",
        x0, sim->operacoes.nome, sim->cfg->geral.quantum, ncpu, max_tick);

    desenhar_legenda(f, x0);

    /* linhas de grade verticais (a cada `passo` ticks) */
    int passo = 1;
    if (max_tick > 20)   passo = 5;
    if (max_tick > 100)  passo = 10;
    if (max_tick > 500)  passo = 50;
    if (max_tick > 5000) passo = 500;
    if (max_tick > 50000) passo = 5000;

    for (int t = 0; t <= max_tick; t += passo) {
        double gx = x_tick(t, x0, cw);
        fprintf(f, "  <line x1=\"%.1f\" y1=\"%.1f\" x2=\"%.1f\" y2=\"%.1f\" stroke=\"#eeeeee\" stroke-width=\"1\"/>\n",
            gx, cpu_top, gx, task_bottom);
    }

    /* faixa das CPUs desligadas */
    for (int c = 0; c < ncpu; c++) {
        double y = cpu_top + c * cpu_h;
        fprintf(f, "  <rect x=\"%.1f\" y=\"%.1f\" width=\"%.1f\" height=\"%.1f\" fill=\"#ffffff\" stroke=\"#cccccc\" stroke-width=\"1\"/>\n",
            x0, y, chart_w, cpu_h);

        if (max_tick > 0) {
            int h = 0;
            while (h < nsnap) {
                int desl = (sim->historico[h]->cpu_ligada[c] == 0);
                int fim = h + 1;
                while (fim < nsnap &&
                       (sim->historico[fim]->cpu_ligada[c] == 0) == desl)
                    fim++;
                if (desl) {
                    double rx = x_tick(h, x0, cw);
                    double rw = (fim - h) * cw;
                    fprintf(f,
                        "  <rect x=\"%.1f\" y=\"%.1f\" width=\"%.1f\" height=\"%.1f\" fill=\"url(#patDesligado)\"/>\n",
                        rx, y, rw, cpu_h);
                }
                h = fim;
            }
        }
        fprintf(f, "  <text x=\"%.1f\" y=\"%.1f\" font-size=\"11\" text-anchor=\"end\">CPU%d</text>\n",
            x0 - 6, y + cpu_h * 0.75, c);
    }

    /* linhas das tarefas */
    char css[8];

    for (int i = 0; i < nt; i++) {
        int k = linha[i];
        double y = task_top + (nt - 1 - k) * row_h;

        /* fundo da linha */
        fprintf(f, "  <rect x=\"%.1f\" y=\"%.1f\" width=\"%.1f\" height=\"%.1f\" fill=\"#ffffff\" stroke=\"#dddddd\" stroke-width=\"1\"/>\n",
            x0, y, chart_w, row_h);

        if (max_tick > 0) {
            int h = 0;
            while (h < nsnap) {
                EstadoTarefa est = sim->historico[h]->snapshots[i].tcb.estado;
                int cpu = sim->historico[h]->snapshots[i].tcb.cpu;
                int fim = h + 1;
                while (fim < nsnap &&
                       sim->historico[fim]->snapshots[i].tcb.estado == est &&
                       sim->historico[fim]->snapshots[i].tcb.cpu == cpu)
                    fim++;

                double rx = x_tick(h, x0, cw);
                double rw = (fim - h) * cw;

                if (est == TASK_EXECUTANDO) {
                    cor_css(sim->tarefas[i].cor, css);
                    fprintf(f,
                        "  <rect x=\"%.1f\" y=\"%.1f\" width=\"%.1f\" height=\"%.1f\" fill=\"%s\" stroke=\"#000000\" stroke-width=\"0.5\"/>\n",
                        rx, y, rw, row_h, css);
                    /* rotulo com o numero da CPU, se houver espaco */
                    if (rw >= cw * 1.5) {
                        fprintf(f,
                            "  <text x=\"%.1f\" y=\"%.1f\" font-size=\"10\" text-anchor=\"middle\" fill=\"%s\">CPU%d</text>\n",
                            rx + rw / 2.0, y + row_h * 0.62, cor_texto(sim->tarefas[i].cor), cpu);
                    }
                } else if (est == TASK_PRONTO) {
                    fprintf(f,
                        "  <rect x=\"%.1f\" y=\"%.1f\" width=\"%.1f\" height=\"%.1f\" fill=\"none\" stroke=\"#999999\" stroke-width=\"1\"/>\n",
                        rx, y, rw, row_h);
                } else if (est == TASK_BLOQUEADO) {
                    fprintf(f,
                        "  <rect x=\"%.1f\" y=\"%.1f\" width=\"%.1f\" height=\"%.1f\" fill=\"url(#patBloqueado)\" stroke=\"#000000\" stroke-width=\"1\"/>\n",
                        rx, y, rw, row_h);
                }
                /* estados INATIVO/FINALIZADO sao deixados em branco */

                h = fim;
            }
        }

        fprintf(f, "  <text x=\"%.1f\" y=\"%.1f\" font-size=\"12\" text-anchor=\"end\">T%d</text>\n",
            x0 - 6, y + row_h * 0.65, sim->tarefas[i].id);
    }

    /* eixo X e rotulos dos ticks */
    fprintf(f, "  <line x1=\"%.1f\" y1=\"%.1f\" x2=\"%.1f\" y2=\"%.1f\" stroke=\"#000000\" stroke-width=\"1\"/>\n",
        x0, axis_y, x0 + chart_w, axis_y);
    for (int t = 0; t <= max_tick; t += passo) {
        double gx = x_tick(t, x0, cw);
        fprintf(f, "  <text x=\"%.1f\" y=\"%.1f\" font-size=\"10\" text-anchor=\"middle\">%d</text>\n",
            gx, axis_y + 14, t);
    }

    /* eventos por cima de tudo */
    desenhar_eventos(f, sim, linha, x0, cw, task_top, row_h, cpu_top, cpu_h);

    fprintf(f, "</svg>\n");
    fclose(f);
    free(linha);
    return 0;
}