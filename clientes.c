
#include "clientes.h"   

// estrutura para armazenar informações de cada cliente conectado
typedef struct {
    int  fd; //socket de comunicacao com esse cliente 
    char apelido[TAM_APELIDO]; //nome exibido nas mensagens
    int  ativo;// 1 = slot ocupado, 0 = slot livre
} Cliente;

// array de clientes conectados, protegido por mutex
static Cliente clientes[MAX_CLIENTES];
// mutex para proteger o array de clientes
static pthread_mutex_t mutex_clientes = PTHREAD_MUTEX_INITIALIZER;

// inicializa todos os slots como livres
void inicializar_clientes(void) {
    int i;
 
    pthread_mutex_lock(&mutex_clientes);
    for (i = 0; i < MAX_CLIENTES; i++) {
        clientes[i].ativo = 0;
        clientes[i].fd    = -1;
    }
    pthread_mutex_unlock(&mutex_clientes);
}
 // adiciona um cliente ao array de clientes conectados
int adicionar_cliente(int fd) {
    int i;
    int indice = -1;
 
    pthread_mutex_lock(&mutex_clientes);
    for (i = 0; i < MAX_CLIENTES; i++) {
        if (!clientes[i].ativo) {
            clientes[i].fd    = fd;
            clientes[i].ativo = 1;
            snprintf(clientes[i].apelido, TAM_APELIDO, "Convidado-%d", i + 1);
            indice = i;
            break;
        }
    }
    pthread_mutex_unlock(&mutex_clientes);
 
    return indice;
}
// remove um cliente do array de clientes conectados
void remover_cliente(int indice) {
    pthread_mutex_lock(&mutex_clientes);
    if (clientes[indice].ativo) {
        close(clientes[indice].fd);
        clientes[indice].fd    = -1;
        clientes[indice].ativo = 0;
    }
    pthread_mutex_unlock(&mutex_clientes);
}
 // copia o descritor e o apelido do slot. Retorna 1 em sucesso, 0 se inativo
int obter_dados_cliente(int indice, int *fd, char *apelido, size_t tam) {
    int ok = 0;
 
    pthread_mutex_lock(&mutex_clientes);
    if (clientes[indice].ativo) {
        *fd = clientes[indice].fd;
        snprintf(apelido, tam, "%s", clientes[indice].apelido);
        ok = 1;
    }
    pthread_mutex_unlock(&mutex_clientes);
 
    return ok;
}
// envia uma mensagem a um unico socket, reportando falha de transmissao
void enviar_para(int fd, const char *mensagem) {
    if (send(fd, mensagem, strlen(mensagem), 0) < 0) {
        perror("Erro no send");
    }
}
 
// envia uma mensagem para todos os clientes conectados, exceto o remetente
void broadcast(const char *mensagem, int remetente) {
    int i;
 
    pthread_mutex_lock(&mutex_clientes);
    for (i = 0; i < MAX_CLIENTES; i++) {
        if (clientes[i].ativo && i != remetente) {
            if (send(clientes[i].fd, mensagem, strlen(mensagem), 0) < 0) {
                perror("Erro ao enviar no broadcast");
            }
        }
    }
    pthread_mutex_unlock(&mutex_clientes);
}
// grava o novo apelido no slot. Retorna 0 se o nome ja estiver em uso, 1 se conseguiu trocar
int trocar_apelido(int indice, const char *novo) {
    int i;
    int ok = 1;
 
    pthread_mutex_lock(&mutex_clientes);
    for (i = 0; i < MAX_CLIENTES; i++) {
        if (clientes[i].ativo && i != indice &&
            strcmp(clientes[i].apelido, novo) == 0) {
            ok = 0;
            break;
        }
    }
    if (ok) {
        snprintf(clientes[indice].apelido, TAM_APELIDO, "%s", novo);
    }
    pthread_mutex_unlock(&mutex_clientes);
 
    return ok;
}
// devolve o socket de quem usa esse apelido, ou -1 se ninguem usa
int fd_por_apelido(const char *apelido) {
    int i;
    int fd = -1;
 
    pthread_mutex_lock(&mutex_clientes);
    for (i = 0; i < MAX_CLIENTES; i++) {
        if (clientes[i].ativo && strcmp(clientes[i].apelido, apelido) == 0) {
            fd = clientes[i].fd;
            break;
        }
    }
    pthread_mutex_unlock(&mutex_clientes);
 
    return fd;
}
// monta o texto da lista de conectados no buffer recebido
void montar_lista(char *destino, size_t tam) {
    size_t usado;
    int i;
 
    usado = (size_t)snprintf(destino, tam, "*** No boteco agora:\n");
 
    pthread_mutex_lock(&mutex_clientes);
    for (i = 0; i < MAX_CLIENTES && usado < tam; i++) {
        if (clientes[i].ativo) {
            usado += (size_t)snprintf(destino + usado, tam - usado,
                                      "    - %s\n", clientes[i].apelido);
        }
    }
    pthread_mutex_unlock(&mutex_clientes);
}
//  avisa e desconecta todos os clientes. Usado no encerramento do servidor
void encerrar_clientes(void) {
    int i;
 
    //aviso antes de travar: broadcast tambem usa o mutex. 
    broadcast("*** O boteco vai fechar. Ate amanha! ***\n", -1);
 
    pthread_mutex_lock(&mutex_clientes);
    for (i = 0; i < MAX_CLIENTES; i++) {
        if (clientes[i].ativo) {
            close(clientes[i].fd);
            clientes[i].fd    = -1;
            clientes[i].ativo = 0;
        }
    }
    pthread_mutex_unlock(&mutex_clientes);
}
