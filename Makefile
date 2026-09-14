# Nome do executável
EXEC = programa

# Fonte principal do código
SRC = *.c

# Arquivos de cabeçalho
HEADERS = *.h

# Nome do Makefile
MAKEFILE = Makefile

# Diretiva para compilar o código
all:
	gcc -o -pthread $(EXEC) $(SRC)

# Diretiva para executar o código compilado
run: all
	./$(EXEC)

# Limpeza de arquivos compilados
clean:
	rm -f $(EXEC)

# Diretiva para criar um arquivo zip contendo o executável, arquivos de código-fonte, cabeçalhos e o Makefile
zip: all
	zip $(EXEC).zip $(EXEC) $(SRC) $(HEADERS) $(MAKEFILE)


