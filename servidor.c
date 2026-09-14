#include "boteco.h"

typedef struct 
{
    int fd; // socket do cliente
    char apelido[TAM_APELIDO]; // apelido do cliente
    int ativo; // flag para indicar se o cliente está ativo
} Cliente;

static Cliente clientes[MAX_CLIENTES]; // array de clientes conectados
static pthread_mutex_t mutex_clientes = PTHREAD_MUTEX_INITIALIZER; // mutex para proteger o array de clientes

// O DESENVOLVIMENTO DAS FUNÇÕES AUXILIARES (adicionar_cliente, remover_cliente, broadcast...) ESTÃO NO FINAL DO ARQUIVO, APÓS O MAIN, PARA MANTER A ORGANIZAÇÃO DO CÓDIGO...

int main(void) {
    int fd_servidor; //socket de escuta 
    int fd_cliente; //socket de comunicacao com o cliente
    int opt = 1; //flag para o SO_REUSEADDR 
    int i;
    int indice;
    int erro;
    int *arg_thread;
    struct sockaddr_in endereco; //endereco do servidor
    struct sockaddr_in cliente; //endereco preenchido pelo accept 
    
    socklen_t tam_cliente; //tamanho da struct do cliente

    char ip_cliente[INET_ADDRSTRLEN]; //IP do cliente em formato texto 
    pthreard_t tid; //identificador da thread do cliente
    
    ssize_t bytes; //bytes lidos pelo recv

    //cria socket (TCP)
    fd_servidor = socket(AF_INET, SOCK_STREAM, 0);
    if (fd_servidor < 0) {
        perror("Erro ao criar socket");
        exit(EXIT_FAILURE);
    }

    // evita address already in use no futuro
    if (setsockopt(fd_servidor, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("Erro em setsockopt");
        close(fd_servidor);
        exit(EXIT_FAILURE);
    }

    // montagem endereço local
    memset(&endereco, 0, sizeof(endereco));
    endereco.sin_family      = AF_INET;
    endereco.sin_addr.s_addr = INADDR_ANY;   // aceita conexao de qualquer interface 
    endereco.sin_port        = htons(PORTA);

    if (bind(fd_servidor, (struct sockaddr *)&endereco, sizeof(endereco)) < 0) {
        perror("Erro no bind");
        close(fd_servidor);
        exit(EXIT_FAILURE);
    }

    // socket pronto para receber novas conexões 
    if (listen(fd_servidor, MAX_FILA) < 0) {
        perror("Erro no listen");
        close(fd_servidor);
        exit(EXIT_FAILURE);
    }

    printf("Servidor escutando na porta %d...\n", PORTA);

    
    tam_cliente = sizeof(cliente);
    fd_cliente = accept(fd_servidor, (struct sockaddr *)&cliente, &tam_cliente); // bloqueia até um cliente entrar
    if (fd_cliente < 0) {
        perror("Erro no accept");
        close(fd_servidor);
        exit(EXIT_FAILURE);
    }

    inet_ntop(AF_INET, &cliente.sin_addr, ip_cliente, sizeof(ip_cliente));
    printf("Cliente conectado: %s:%d\n", ip_cliente, ntohs(cliente.sin_port));

    // laço de eco
    while ((bytes = recv(fd_cliente, buffer, sizeof(buffer) - 1, 0)) > 0) {
        buffer[bytes] = '\0';   //garante terminacao da string antes de imprimir 
        printf("Recebido (%zd bytes): %s", bytes, buffer);

        if (send(fd_cliente, buffer, (size_t)bytes, 0) < 0) {
            perror("Erro no send");
            break;
        }
    }

    if (bytes == 0) {
        printf("Cliente encerrou a conexao.\n");
    } else if (bytes < 0) {
        perror("Erro no recv");
    }

    // libera os descritores do cliente e do de escuta
    close(fd_cliente);
    close(fd_servidor);
    printf("Servidor encerrado.\n");

    return 0;
}
// adiciona um cliente à lista de clientes conectados
static int adicionar_cliente(int fd) {
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
 
    return indice; // retorna o índice do cliente adicionado ou -1 se não houver espaço

}

// Remove um cliente da lista de clientes conectados
static void remover_cliente(int indice) {
        pthread_mutex_lock(&mutex_clientes);
    if (clientes[indice].ativo) {
        close(clientes[indice].fd);
        clientes[indice].fd    = -1;
        clientes[indice].ativo = 0;
    }
    pthread_mutex_unlock(&mutex_clientes);

}

// Envia uma mensagem para todos os clientes conectados, exceto o remetente
static void broadcast(const char *mensagem, int remetente) {
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

// Thread que lida com a comunicação de um cliente específico
void *thread_cliente(void *arg) {
    int  indice = *((int *)arg);
    int  fd;
    char apelido[TAM_APELIDO];
    char buffer[TAM_BUFFER];
    char mensagem[TAM_MENSAGEM];
    ssize_t bytes;
 
    free(arg);
 
    //copia os dados do slot uma unica vez, sob protecao do mutex. 
    pthread_mutex_lock(&mutex_clientes);
    fd = clientes[indice].fd;
    snprintf(apelido, TAM_APELIDO, "%s", clientes[indice].apelido);
    pthread_mutex_unlock(&mutex_clientes);
 
    // avisa os demais que alguem chegou. 
    snprintf(mensagem, sizeof(mensagem), "*** %s entrou no boteco ***\n", apelido);
    printf("%s", mensagem);
    broadcast(mensagem, indice);
 
    // laço de recepcao: identico ao do bloco 2, mas agora rodando em paralelo com as threads dos outros clientes.
    while ((bytes = recv(fd, buffer, sizeof(buffer) - 1, 0)) > 0) {
        buffer[bytes] = '\0';
 
        snprintf(mensagem, sizeof(mensagem), "[%s] %s", apelido, buffer);
        printf("%s", mensagem);
        broadcast(mensagem, indice);
    }
 
    if (bytes < 0) {
        perror("Erro no recv");
    }
 
    //encerramento: libera o slot antes de anunciar a saida, para que o proprio cliente que saiu nao receba a mensagem. 
    remover_cliente(indice);
 
    snprintf(mensagem, sizeof(mensagem), "*** %s saiu do boteco ***\n", apelido);
    printf("%s", mensagem);
    broadcast(mensagem, -1);
 
    return NULL;
}




