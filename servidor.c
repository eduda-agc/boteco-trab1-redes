#include<signal.h>   

#include "boteco.h"
#include "clientes.h"
#include "comandos.h"

static int fd_servidor = -1; // socket do servidor, usado para fechar o socket quando o servidor for encerrado
static volatile sig_atomic_t servidor_rodando = 1; // flag para indicar se o servidor está rodando


// DESENVOLVIMENTO ABAIXO DA MAIN -------------------------------------------------------------

//trata o sinal SIGINT (Ctrl+C) para encerrar o servidor de forma limpa
static void tratar_sigint(int sinal);
// encerra todos os clientes conectados, usado no encerramento do servidor
void *thread_cliente(void *arg); 



int main(void) {
    int fd_cliente; //socket de escuta 
    int opt = 1; //flag para o SO_REUSEADDR 
    int indice;
    int erro;
    int *arg_thread;
    struct sockaddr_in endereco; //endereco do servidor
    struct sockaddr_in cliente; //endereco preenchido pelo accept 
    
    socklen_t tam_cliente; //tamanho da struct do cliente

    char ip_cliente[INET_ADDRSTRLEN]; //IP do cliente em formato texto 
    pthread_t tid; //identificador da thread do cliente

    // antes de abrir o socket, registra o tratador de SIGINT para encerrar o servidor com Ctrl+C   
    if (signal(SIGINT, tratar_sigint) == SIG_ERR) {
        perror("Erro ao registrar tratador de SIGINT");
        exit(EXIT_FAILURE);
    }

    // ignora SIGPIPE, que ocorre quando tentamos enviar para um socket fechado pelo cliente
    if (signal(SIGPIPE, SIG_IGN) == SIG_ERR) {
        perror("Erro ao ignorar SIGPIPE");
        exit(EXIT_FAILURE);
    }

    // cria o socket TCP
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
    endereco.sin_family = AF_INET;
    endereco.sin_addr.s_addr = INADDR_ANY;   // aceita conexao de qualquer interface 
    endereco.sin_port = htons(PORTA);

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
    while (servidor_rodando) {
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

    printf("\n[%s] Encerrando o servidor...\n", NOME_APP);
    encerrar_clientes();
    printf("[%s] Servidor encerrado.\n", NOME_APP);

    return 0;
}
// -------------------------------------------------------------------------------------------------
static void tratar_sigint(int sinal) {
    (void)sinal;
    servidor_rodando = 0;
    if (fd_servidor >= 0) {
        close(fd_servidor);
    }
}
 
void *thread_cliente(void *arg) {
    int  indice = *((int *)arg);
    int  fd;
    int  encerrar = 0;
    char apelido[TAM_APELIDO];
    char buffer[TAM_BUFFER];
    char mensagem[TAM_MENSAGEM];
    ssize_t bytes;
 
    free(arg);
 
    if (!obter_dados_cliente(indice, &fd, apelido, sizeof(apelido))) {
        return NULL;   //cliente ja foi removido antes da thread iniciar
    }
 
    enviar_para(fd, TEXTO_AJUDA);
 
    snprintf(mensagem, sizeof(mensagem), "*** %s entrou no boteco ***\n", apelido);
    printf("%s", mensagem);
    broadcast(mensagem, indice);
 
    while (!encerrar && (bytes = recv(fd, buffer, sizeof(buffer) - 1, 0)) > 0) {
        buffer[bytes] = '\0';
        remover_quebra_linha(buffer);
 
        if (buffer[0] == '\0') {
            continue;   //linha vazia: ignora
        }
 
        if (buffer[0] == '/') {
            encerrar = processar_comando(indice, fd, apelido, buffer);
        } else {
            snprintf(mensagem, sizeof(mensagem), "[%s] %s\n", apelido, buffer);
            printf("%s", mensagem);
            broadcast(mensagem, indice);
        }
    }
 
    if (!encerrar && bytes < 0) {
        perror("Erro no recv");
    }
 
    remover_cliente(indice);
 
    snprintf(mensagem, sizeof(mensagem), "*** %s saiu do boteco ***\n", apelido);
    printf("%s", mensagem);
    broadcast(mensagem, -1);
 
    return NULL;
}

