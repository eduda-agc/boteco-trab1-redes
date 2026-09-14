#ifndef CLIENTES_H
#define CLIENTES_H
 
#include "boteco.h"
 
//marca todos os slots como livres. Chamar uma vez, antes do laco de accept. 
void inicializar_clientes(void);
 
//ocupa o primeiro slot livre. Retorna o indice usado, ou -1 se lotado. 
int adicionar_cliente(int fd);
 
// fecha o socket do cliente e libera o slot
void remover_cliente(int indice);
 
// copia o descritor e o apelido do slot. Retorna 1 em sucesso, 0 se inativo
int obter_dados_cliente(int indice, int *fd, char *apelido, size_t tam);
 
// envia uma mensagem a um unico socket, reportando falha de transmissao
void enviar_para(int fd, const char *mensagem);
 
// envia a todos os clientes ativos, exceto o remetente
// passe remetente = -1 para enviar realmente a todos
void broadcast(const char *mensagem, int remetente);
 
// Grava o novo apelido no slot. Retorna 0 se o nome ja estiver em uso, 1 se conseguiu trocar
int trocar_apelido(int indice, const char *novo);
 
// devolve o socket de quem usa esse apelido, ou -1 se ninguem usa
int fd_por_apelido(const char *apelido);
 
// monta o texto da lista de conectados no buffer recebido.
void montar_lista(char *destino, size_t tam);
 
// avisa e desconecta todos os clientes. Usado no encerramento do servidor
void encerrar_clientes(void);
 
#endif 
