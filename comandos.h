#ifndef COMANDOS_H
#define COMANDOS_H
 
#include "boteco.h"
 
// texto de ajuda enviado na entrada e pelo comando /ajuda. 
extern const char *TEXTO_AJUDA;
 
// remove \n e \r do fim da linha recebida, para facilitar o parse. 
void remover_quebra_linha(char *texto);
 

int processar_comando(int indice, int fd, char *apelido, char *linha);
 
#endif 
