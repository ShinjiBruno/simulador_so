## Dicionario das funcoes de bibliotecas nativas utilizadas nesse projeto

### `snprintf`
> é uma função (definida em `<stdio.h>`) que formata dados e os grava em um buffer de caractere de tamanho limitado, prevenindo transbordamentos perigosos de memória (buffer overflow) comuns à função `sprintf()`
```c
int snprintf(char *str, size_t size, const char *format, ...);

// exemplo
#include <stdio.h>

int main(void) {
    char buffer[50];
    int idade = 25;
    
    int ret = snprintf(buffer, sizeof(buffer), "Olá, tenho %d anos.", idade);
    
    printf("String: %s\n", buffer);
    printf("Caracteres necessários: %d\n", ret);
    return 0;
}
```
- Terminação Nula: Garante que a string de saída termine sempre com \0, desde que size seja maior que zero.
- Truncamento Seguro: Se o texto formatado for maior do que size - 1, o excedente é descartado e apenas os primeiros caracteres cabíveis são gravados.
- Valor de Retorno: Retorna o número de caracteres que seriam escritos se o buffer tivesse tamanho infinito (sem contar o \0). Se o valor retornado for maior ou igual a size, significa que houve truncamento
---

### `calloc`
> é uma função (`<stdlib.h>`)utilizada para alocar memória dinâmica na Heap para múltiplos elementos (como um array) e possui o diferencial de inicializar todos os bytes alocados com o valor zero
```c
void* calloc(size_t num_elementos, size_t tamanho_cada_elemento);

//exemplo
#include <stdio.h>
#include <stdlib.h> // Necessário para calloc() e free()

int main() {
    int n = 5;
    int *arr;

    // 1. Aloca memória para um array de 5 inteiros
    // calloc já garante que arr[0] até arr[4] começam valendo 0
    arr = (int*) calloc(n, sizeof(int));

    // 2. Sempre verifique se a alocação foi bem-sucedida
    if (arr == NULL) {
        printf("Erro: Memória insuficiente!\n");
        return 1; 
    }

    // Como usamos calloc, o output será: 0 0 0 0 0
    printf("Valores iniciais no array:\n");
    for (int i = 0; i < n; i++) {
        printf("%d ", arr[i]);
    }
    printf("\n");

    // 3. Modificando os elementos normalmente
    for (int i = 0; i < n; i++) {
        arr[i] = (i + 1) * 10;
    }

    // Output atualizado: 10 20 30 40 50
    printf("Valores atualizados:\n");
    for (int i = 0; i < n; i++) {
        printf("%d ", arr[i]);
    }
    printf("\n");

    // 4. Libera a memória alocada e limpa o ponteiro
    free(arr);
    arr = NULL;

    return 0;
}
```

Diferenças com `malloc`: 
- Parâmetros: malloc receber um único argumento, o total de bytes necessários
- Inicialização : malloc deixa a memória intacta, contendo lixo de memoria (valores aleatorios)
- Desempenho: malloc é ligeiramente mais rápido já que apenas reserva o espaço
---

## Tratamento de strings

### `atoi`
> Biblioteca <stdlib.h>. Converte uma string que representa um número inteiro em um valor `int`. Vem do `ASCII to Integer`. Se for invalido, retorna 0.
```c
int atoi(const char *str)
```
---

### `strcpy`
> função da biblioteca <string.h>. Copia o conteúdo de uma string deorigem para uma string de destino incluindo o caractere nulo de temrinação

```c
char *strcpy(char *dest, const char *src);

//exemplo
#include <stdio.h>
#include <string.h> // Necessário para usar strcpy

int main() {
    char origem[] = "Olá, mundo!";
    char destino[50]; // Deve ter tamanho suficiente para caber a string + '\0'

    strcpy(destino, origem);

    printf("Origem: %s\n", origem);
    printf("Destino: %s\n", destino);

    return 0;
}
```
---

### `strlen`
> Da biblioteca <string.h>. Retorna comprimento de uma string. O caractere '\0' não é incluído .
```c
size_t strlen(const char *str);
```
---

### `strcmp`
> Biblioteca <string.h>. Serve para comparar duas strings caractere por caractere. Retorna 0 se forem identicas. Retorna menor que 0 se str1 < str2. Retorna maior que 0 se str1 > str2
```c
int strcmp(const char *str1, const char *str2);
```
---

### `strchr` e `strrchr`
> `strchr`: Serve para encontrar a primeira ocorrência de um caractere dentro de uma string. Retorna o ponteiro para a posição da primeira ocorrencia. Se não foi encontrado, retorna NULL
```c
char *strchr(const char *str, int ch);
```
Cálculo do Índice: Como a função retorna um ponteiro de memória, é possível descobrir o índice do caractere subtraindo o ponteiro inicial da string do ponteiro retornado (resultado - frase)

> `strrchr`: O mesmo que strchr, porém este serve para encontrar a última ocorrência de um caractere.
---

### `strcspn`
> vem de 'String Complement Span', calcula o comprimento do segmento inicial de uma string que consiste inteiramente de caracteres que não estão presentes em uma segunda string de busca. Ou seja, ela diz quantos caracteres precisa pular até encontrar qualquer um dos caracteres proibidos.
```c
size_t strcspn(const char *dest, const char *src);
```
Onde: 
- dest: A string que será analisada.
- src: A string que contém os caracteres que servem como critério de parada (rejeição).
- Retorno: Retorna o número de caracteres lidos em dest antes de encontrar a primeira ocorrência de qualquer caractere contido em src. Se nenhum caractere coincidir, retorna o tamanho total de dest.

O uso mais clássico de strcspn é remover a quebra de linha (\n) que a função fgets deixa no final da string lida do teclado:
```c
fgets(nome, sizeof(nome), stdin); // supunhetamos que o nome seja "Ana\n"
// encontra a posição do '\n' e substitui pelo terminador '\0'
nome[strcspn(nome, "\n")] = '\0';
```
--- 

### `memset`
> Biblioteca <string.h>. Usada para preencher um bloco de memória com um valor específico byte a byte. Comumente usada para inicializar arrays, zerar structs ou limpar buffers de forma rápida.
```c
//retorna o proprio ponteiro ptr
void *memset(void *ptr, int value, size_t num);
```
---

### `memmove`
> <string.h>. Usada para copiar um bloco de memória de uma origem para um destino, mesmo que essas duas regiões de memória se sobreponham (overlap).
```c
void *memmove(void *dest, const void *src, size_t num);
```

