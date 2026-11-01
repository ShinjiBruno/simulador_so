# Nome do arquivo executável final
TARGET = build/simulador

# Compilador e flags de compilação (g = debug, Wall/Wextra = avisos de erro).
# -MMD -MP gera arquivos de dependência (.d) para que alterações em headers
# (ex.: task.h) também forcem a recompilação dos .c que os incluem.
CC = gcc
CFLAGS = -Wall -Wextra -g -Iinclude -MMD -MP

# Encontra automaticamente todos os arquivos .c dentro da pasta src/
SOURCES = $(wildcard src/*.c)

# Substitui o caminho src/%.c por build/%.o para gerar os objetos
OBJECTS = $(patsubst src/%.c, build/%.o, $(SOURCES))

# Arquivos .d de dependências (um por .o)
DEPS = $(OBJECTS:.o=.d)

# Regra padrão executada ao digitar apenas 'make'
all: prepare $(TARGET)

# Garante que o diretório build/ existe antes de compilar
prepare:
	@mkdir -p build

# Linkagem: junta todos os arquivos .o para criar o executável
$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) -o $@ $^

# Compilação: transforma cada arquivo .c em um .o correspondente
build/%.o: src/%.c
	$(CC) $(CFLAGS) -c $< -o $@

# Inclui as regras de dependência geradas automaticamente (se existirem)
-include $(DEPS)

# Regra para limpar os arquivos gerados (útil para recompilar do zero)
# Comando: make clean
clean:
	rm -rf build/

# Regra para compilar e já executar o programa (ex.: make run ARGS="-c test/teste1.txt")
# Comando: make run
run: all
	./$(TARGET)

.PHONY: all prepare clean run