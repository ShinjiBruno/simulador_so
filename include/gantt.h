#ifndef GANTT_H
#define GANTT_H

#include "simulador.h"

/*
 * Gera o diagrama de Gantt da simulacao em um arquivo de imagem.
 *
 * O diagrama e produzido no formato SVG (imagem vetorial), o que permite
 * visualizar toda a simulacao sem se limitar a um tempo maximo: as dimensoes
 * do arquivo sao calculadas dinamicamente a partir do numero de ticks.
 *
 * Parametros:
 *   sim    - simulador cujo historico (snapshots e eventos) sera desenhado;
 *   caminho- caminho do arquivo de saida (ex.: "gantt.svg").
 *
 * Retorno:
 *   0 em caso de sucesso; valor diferente de zero em caso de erro.
 */
int gantt_exportar_svg(const Simulador* sim, const char* caminho);

#endif