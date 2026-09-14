 
CC      = gcc
CFLAGS  = -Wall -Wextra -pthread
EXECS   = servidor cliente
HEADERS = boteco.h clientes.h comandos.h
ZIP     = boteco-trab1-redes.zip
 
# Objetos de cada programa. O cliente nao usa os modulos do servidor.
OBJ_SERVIDOR = servidor.o clientes.o comandos.o
OBJ_CLIENTE  = cliente.o
 
# Alvo padrao: compila os dois programas.
all: $(EXECS)
 
# Etapa de ligacao: junta os objetos num executavel.
servidor: $(OBJ_SERVIDOR)
	$(CC) $(CFLAGS) -o servidor $(OBJ_SERVIDOR)
 
cliente: $(OBJ_CLIENTE)
	$(CC) $(CFLAGS) -o cliente $(OBJ_CLIENTE)
 
# Regra generica de compilacao: todo .o vem do .c de mesmo nome.
# Se qualquer header mudar, todos os objetos sao recompilados.
%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@
 
# Execucao: servidor e cliente rodam em terminais separados.
run-servidor: servidor
	./servidor
 
run-cliente: cliente
	./cliente
 
# Remove tudo que e gerado pela compilacao.
clean:
	rm -f *.o $(EXECS) $(ZIP)
 
# Pacote de entrega. Depende de clean para nao empacotar binarios:
# o executavel deve ser compilado na maquina de quem corrige.
zip: clean
	zip $(ZIP) *.c *.h Makefile README.md
 
# Alvos que nao geram arquivo com esse nome.
.PHONY: all run-servidor run-cliente clean zip

