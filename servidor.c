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
    pthread_t tid; //identificador da thread do cliente
    
    for (i=0; i < MAX_CLIENTES; i++) {//inicializa todos os slots como livres
        clientes[i].ativo = 0;
        clientes[i].fd = -1;
    }

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

    // associa o socket a porta e IP local. falha aqui normalmente = porta ocupada
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

    printf("[%s] Servidor escutando na porta %d (ate %d clientes)...\n", NOME_APP, PORTA, MAX_CLIENTES);


    // laço infinito: aceita conexoes e cria threads para cada cliente
    while (1) {
        tam_cliente = sizeof(cliente);
        fd_cliente = accept(fd_servidor, (struct sockaddr *)&cliente, &tam_cliente); // bloqueia até um cliente entrar
        if (fd_cliente < 0) {
            perror("Erro no accept");
            close(fd_servidor);
            exit(EXIT_FAILURE);
        }

        inet_ntop(AF_INET, &cliente.sin_addr, ip_cliente, sizeof(ip_cliente));
        printf("Conexao recebida de %s:%d\n", ip_cliente, ntohs(cliente.sin_port));

        // adiciona o cliente na lista de clientes conectados
        indice = adicionar_cliente(fd_cliente);
        if (indice < 0) {
            const char *aviso = "*** Boteco lotado, tente mais tarde ***\n";
            send(fd_cliente, aviso, strlen(aviso), 0);
            close(fd_cliente);
            printf("Conexao recusada: limite de %d clientes atingido\n", MAX_CLIENTES);
            continue;
        }
 
        // cria a thread para lidar com o cliente
        arg_thread = malloc(sizeof(int));
        if (arg_thread == NULL) {
            perror("Erro ao alocar argumento da thread");
            remover_cliente(indice);
            continue;
        }
        *arg_thread = indice;
 
        erro = pthread_create(&tid, NULL, thread_cliente, arg_thread);
        if (erro != 0) {
            fprintf(stderr, "Erro ao criar thread do cliente: %s\n", strerror(erro));
            free(arg_thread);
            remover_cliente(indice);
            continue;
        }
 
        // libera os recursos da thread automaticamente no fim
        pthread_detach(tid);
    }

    close(fd_servidor);
    return 0;
}

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
 
    return indice;
}

// remove um cliente da lista de clientes conectados
static void remover_cliente(int indice) {
        pthread_mutex_lock(&mutex_clientes);
    if (clientes[indice].ativo) {
        close(clientes[indice].fd);
        clientes[indice].fd    = -1;
        clientes[indice].ativo = 0;
    }
    pthread_mutex_unlock(&mutex_clientes);

}

// envia uma mensagem para todos os clientes conectados, exceto o remetente
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

// thread que lida com a comunicação de um cliente específico
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




