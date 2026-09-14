 
CC      = gcc
CFLAGS  = -Wall -Wextra -pthread
EXECS   = servidor cliente
HEADER  = boteco.h
ZIP     = boteco-trab1-redes.zip
 
all: $(EXECS)
 
servidor: servidor.c $(HEADER)
	$(CC) $(CFLAGS) -o servidor servidor.c
 
cliente: cliente.c $(HEADER)
	$(CC) $(CFLAGS) -o cliente cliente.c
 
# Execucao: servidor e cliente rodam em terminais separados.
run-servidor: servidor
	./servidor
 
run-cliente: cliente
	./cliente
 
# Remove tudo que e gerado pela compilacao.
clean:
	rm -f $(EXECS) $(ZIP)
 
# Pacote de entrega. Depende de clean para nao empacotar binarios:
# o executavel deve ser compilado na maquina de quem corrige.
zip: clean
	zip $(ZIP) *.c *.h Makefile README.md
 
# Alvos que nao geram arquivo com esse nome.
.PHONY: all run-servidor run-cliente clean zip

