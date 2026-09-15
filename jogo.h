#ifndef JOGO_H
#define JOGO_H

#include "boteco.h"

#define JOGO_PRIVADO  0
#define JOGO_PUBLICO  1


// sorteia tema e numeros, inscreve os clientes conectados e os bots.
int jogo_iniciar(const char *quem, char *saida, size_t tam);

// informa a quem pediu o proprio numero secreto. 
int jogo_meu_numero(int indice, char *saida, size_t tam);

// registra a dica do jogador sem revelar o conteudo aos demais. Informa se todos ja deram a sua.
int jogo_dica(int indice, const char *texto, char *saida, size_t tam);

// mostra todas as dicas, desde que todos ja tenham dado a sua
int jogo_revelar(char *saida, size_t tam);

// confere a ordem proposta, revela os numeros e mostra o chute dos bots
int jogo_ordem(const char *quem, const char *lista, char *saida, size_t tam);

// situacao atual da rodada
int jogo_status(char *saida, size_t tam);

#endif 
