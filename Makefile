# Nome do arquivo executável final
TARGET = build/simulador

# Compilador e flags de compilação (g = debug, Wall/Wextra = avisos de erro)
CC = gcc
CFLAGS = -Wall -Wextra -g -Iinclude

# Encontra automaticamente todos os arquivos .c dentro da pasta src/
SOURCES = $(wildcard src/*.c)

# Substitui o caminho src/%.c por build/%.o para gerar os objetos
OBJECTS = $(patsubst src/%.c, build/%.o, $(SOURCES))

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

# Regra para limpar os arquivos gerados (útil para recompilar do zero)
# Comando: make clean
clean:
	rm -rf build/

# Regra para compilar e já executar o programa
# Comando: make run
run: all
	./$(TARGET)