#include "boteco.h"

int main(void) {
    int fd_servidor; //socket de escuta 
    int fd_cliente; //socket de comunicacao com o cliente
    int opt = 1; //flag para o SO_REUSEADDR 
    struct sockaddr_in endereco; //endereco do servidor
    struct sockaddr_in cliente; //endereco preenchido pelo accept 
    
    socklen_t tam_cliente; //tamanho da struct do cliente 
    
    char buffer[TAM_BUFFER]; //area de recepcao das mensagens 
    char ip_cliente[INET_ADDRSTRLEN]; //IP do cliente em formato texto 
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