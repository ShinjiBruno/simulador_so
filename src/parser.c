#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "../include/parser.h"


static void toUpperCase(char* s) {
    for (; *s; s++) *s = (char)toupper((unsigned char)*s);
}

static char* trim(char* s) {
    char* fim;
    while (*s && isspace((unsigned char)*s)) s++;
    if (*s == '\0') return s;
    fim = s + strlen(s) - 1;
    while (fim > s && isspace((unsigned char)*fim)) *fim-- = '\0';
    return s;
}

static int separar_campos(char* linha, char** campos, int max) {
    int n = 0;
    char* p = linha;

    while (n < max) {
        char* token = strchr(p, ';');
        if (token) {
            *token = '\0';
            campos[n++] = trim(p);
            p = token + 1;
        } else {
            // ultimo campo; pode ser vazio (somente se ';' terminou antes) 
            char* f = trim(p);
            if (*f != '\0') campos[n++] = f;
            break;
        }
    }
    return n;
}

static int eh_inteiro(const char* s) {
    if (s == NULL || *s == '\0') return 0;
    if (*s == '-' || *s == '+') s++;
    if (*s == '\0') return 0;
    for (; *s; s++)
        if (!isdigit((unsigned char)*s)) return 0;
    return 1;
}

static int eh_cor_valida(const char* s, char* saida) {
    if (s == NULL || strlen(s) != 6) return 0;
    for (int i = 0; i < 6; i++)
        if (!isxdigit((unsigned char)s[i])) return 0;
    if (saida) {
        strncpy(saida, s, 6);
        saida[6] = '\0';
    }
    return 1;
}

//como usar fgets, fopen e fscan: https://www.cprogressivo.net/2013/11/Como-ler-arquivos-em-C-As-funcoes-fgetc-fscanf-fgets.html
Configuracao* carregarConfiguracao(const char* caminho, char* msg_erro) {
    FILE* f = fopen(caminho, "r");
    if (!f) {
        snprintf(msg_erro, 256,
                 "nao foi possivel abrir o arquivo '%s'.", caminho);
        return NULL;
    }

    Configuracao* cfg = (Configuracao*)calloc(1, sizeof(Configuracao));
    if (!cfg) {
        snprintf(msg_erro, 256, "erro de alocacao de memoria.");
        fclose(f);
        return NULL;
    }

    cfg->tarefas = (TaskControlBlock*)calloc(MAX_TAREFAS, sizeof(TaskControlBlock));
    if (!cfg->tarefas) {
        snprintf(msg_erro, 256, "erro de alocacao de memoria para tarefas.");
        fclose(f);
        free(cfg);
        return NULL;
    }

    char linha[MAX_LINHA];
    int nr_linha = 0;
    int tem_geral = 0;

    while (fgets(linha, sizeof(linha), f)) {
        nr_linha++;

        linha[strcspn(linha, "\r\n")] = '\0';

        char* l = trim(linha);
        if (*l == '\0') continue; 

        char* campos[MAX_PARAMETROS_TAREFA];
        int n = separar_campos(l, campos, MAX_PARAMETROS_TAREFA);

        if (!tem_geral) {
            //primeira linha: parametros gerai
            if (n < 3) {
                snprintf(msg_erro, 256,
                         "linha %d: parametros gerais devem ter 3 campos "
                         "(algoritmo;quantum;qtde_cpus), mas foram encontrados %d.",
                         nr_linha, n);
                goto erro;
            }

            toUpperCase(campos[0]);
            if (strcmp(campos[0], "RM") == 0) {
                strcpy(cfg->geral.algoritmo, "RM");
            } else if (strcmp(campos[0], "EDF") == 0) {
                strcpy(cfg->geral.algoritmo, "EDF");
            } else {
                snprintf(msg_erro, 256,
                         "linha %d: algoritmo de escalonamento '%s' desconhecido. "
                         "Use RM ou EDF.", nr_linha, campos[0]);
                goto erro;
            }

            if (!eh_inteiro(campos[1])) {
                snprintf(msg_erro, 256,
                         "linha %d: quantum '%s' nao e um numero valido.",
                         nr_linha, campos[1]);
                goto erro;
            }
            cfg->geral.quantum = atoi(campos[1]);
            if (cfg->geral.quantum <= 0) {
                snprintf(msg_erro, 256,
                         "linha %d: quantum deve ser maior que zero (informado %d).",
                         nr_linha, cfg->geral.quantum);
                goto erro;
            }

            if (!eh_inteiro(campos[2])) {
                snprintf(msg_erro, 256,
                         "linha %d: quantidade de CPUs '%s' nao e um numero valido.",
                         nr_linha, campos[2]);
                goto erro;
            }
            cfg->geral.qtde_cpus = atoi(campos[2]);
            if (cfg->geral.qtde_cpus < 1) {
                snprintf(msg_erro, 256,
                         "linha %d: quantidade de CPUs deve ser >= 1 (informado %d).",
                         nr_linha, cfg->geral.qtde_cpus);
                goto erro;
            }

            tem_geral = 1;
            continue;
        }

        // demais linhas: uma tarefa por linha
        if (n < 6) {
            snprintf(msg_erro, 256,
                     "linha %d: tarefa deve ter pelo menos 6 campos "
                     "(id;cor;ingresso;duracao;periodo;prazo), mas foram encontrados %d.",
                     nr_linha, n);
            goto erro;
        }

        TaskControlBlock* t = &cfg->tarefas[cfg->qtde_tarefas];

        if (!eh_inteiro(campos[0])) {
            snprintf(msg_erro, 256,
                     "linha %d: id da tarefa '%s' nao e valido.", nr_linha, campos[0]);
            goto erro;
        }
        t->id = atoi(campos[0]);

        char cor[7];
        if (!eh_cor_valida(campos[1], cor)) {
            snprintf(msg_erro, 256,
                     "linha %d: cor '%s' invalida. Use 6 digitos hexadecimais (ex: F0E0D0).",
                     nr_linha, campos[1]);
            goto erro;
        }
        strcpy(t->cor, cor);

        if (!eh_inteiro(campos[2])) {
            snprintf(msg_erro, 256,
                     "linha %d: ingresso '%s' nao e valido.", nr_linha, campos[2]);
            goto erro;
        }
        t->ingresso = atoi(campos[2]);

        if (!eh_inteiro(campos[3])) {
            snprintf(msg_erro, 256,
                     "linha %d: duracao '%s' nao e valida.", nr_linha, campos[3]);
            goto erro;
        }
        t->duracao = atoi(campos[3]);
        if (t->duracao <= 0) {
            snprintf(msg_erro, 256,
                     "linha %d: duracao deve ser maior que zero.", nr_linha);
            goto erro;
        }

        if (!eh_inteiro(campos[4])) {
            snprintf(msg_erro, 256,
                     "linha %d: periodo '%s' nao e valido.", nr_linha, campos[4]);
            goto erro;
        }
        t->periodo = atoi(campos[4]);
        if (t->periodo < 0) {
            snprintf(msg_erro, 256,
                     "linha %d: periodo negativo (%d) caracteriza erro no arquivo.",
                     nr_linha, t->periodo);
            goto erro;
        }

        if (!eh_inteiro(campos[5])) {
            snprintf(msg_erro, 256,
                     "linha %d: prazo '%s' nao e valido.", nr_linha, campos[5]);
            goto erro;
        }
        t->prazo = atoi(campos[5]);
        if (t->prazo < 0) {
            snprintf(msg_erro, 256,
                     "linha %d: prazo nao pode ser negativo.", nr_linha);
            goto erro;
        }

        // validacao das tarefas somente periodicas
        if (t->periodo == 0) {
            t->periodo = 0;
        }

        cfg->qtde_tarefas++;
        if (cfg->qtde_tarefas >= MAX_TAREFAS) {
            snprintf(msg_erro, 256,
                     "excedeu o limite de %d tarefas suportadas.", MAX_TAREFAS);
            goto erro;
        }
    }

    fclose(f);

    if (!tem_geral) {
        snprintf(msg_erro, 256,
                 "arquivo vazio ou sem nenhuma linha valida de configuracao.");
        liberarConfiguracao(cfg);
        return NULL;
    }

    if (cfg->qtde_tarefas == 0) {
        snprintf(msg_erro, 256, "nenhuma tarefa definida no arquivo.");
        liberarConfiguracao(cfg);
        return NULL;
    }

    for (int i = 1; i < cfg->qtde_tarefas; i++) {
        TaskControlBlock chave = cfg->tarefas[i];
        int j = i - 1;
        while (j >= 0 && cfg->tarefas[j].id > chave.id) {
            cfg->tarefas[j + 1] = cfg->tarefas[j];
            j--;
        }
        cfg->tarefas[j + 1] = chave;
    }

    return cfg;

erro:
    fclose(f);
    liberarConfiguracao(cfg);
    return NULL;
}

void liberarConfiguracao(Configuracao* cfg) {
    if (!cfg) return;
    free(cfg->tarefas);
    free(cfg);
}