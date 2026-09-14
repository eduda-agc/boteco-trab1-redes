
#include "boteco.h"

#define IP_SERVIDOR  "127.0.0.1"  //loopback: servidor na mesma maquina

// socket do servidor, usado para enviar e receber mensagens. A thread de recebimento precisa dele, por isso é global.
static int fd_servidor;
// thread que so recebe mensagens do servidor e imprime na tela. Termina quando o servidor fecha a conexao.
void *thread_recebimento(void *arg) {
    char buffer[TAM_BUFFER];
    ssize_t bytes;

    (void)arg; //parametro nao utilizado; evita warning do -Wextra

    while ((bytes = recv(fd_servidor, buffer, sizeof(buffer) - 1, 0)) > 0) {
        buffer[bytes] = '\0';
        printf("%s", buffer);
        fflush(stdout);// forca a impressao imediata, sem esperar o \n do buffer 
    }

    if (bytes == 0) {
        printf("\n[%s] Conexao encerrada pelo servidor.\n", NOME_APP);
    } else {
        perror("Erro no recv");
    }

    // encerra o socket e termina o programa. A thread principal vai terminar logo em seguida.
    close(fd_servidor);
    exit(EXIT_SUCCESS);

    return NULL;  // nunca chega aqui, mas evita warning do -Wextra
}

int main(void) {
    struct sockaddr_in endereco;   
    pthread_t tid;                 
    char linha[TAM_BUFFER]; //linha digitada
    int erro;

    //cria o socket TCP
    fd_servidor = socket(AF_INET, SOCK_STREAM, 0);
    if (fd_servidor < 0) {
        perror("Erro ao criar socket");
        exit(EXIT_FAILURE);
    }

    //monta o endereco do servidor
    memset(&endereco, 0, sizeof(endereco));
    endereco.sin_family = AF_INET;
    endereco.sin_port   = htons(PORTA);
    if (inet_pton(AF_INET, IP_SERVIDOR, &endereco.sin_addr) <= 0) {
        fprintf(stderr, "Endereco IP invalido: %s\n", IP_SERVIDOR);
        close(fd_servidor);
        exit(EXIT_FAILURE);
    }

    //conecta no servidor
    if (connect(fd_servidor, (struct sockaddr *)&endereco, sizeof(endereco)) < 0) {
        perror("Erro ao conectar no servidor");
        close(fd_servidor);
        exit(EXIT_FAILURE);
    }

    printf("[%s] Conectado a %s:%d. Digite /ajuda para ver os comandos.\n",
           NOME_APP, IP_SERVIDOR, PORTA);

    // cria a thread que so recebe mensagens do servidor e imprime na tela
    erro = pthread_create(&tid, NULL, thread_recebimento, NULL);
    if (erro != 0) {
        fprintf(stderr, "Erro ao criar thread de recebimento: %s\n", strerror(erro));
        close(fd_servidor);
        exit(EXIT_FAILURE);
    }
    pthread_detach(tid);

    // loop principal: le linhas do teclado e envia para o servidor
    while (fgets(linha, sizeof(linha), stdin) != NULL) {
        if (send(fd_servidor, linha, strlen(linha), 0) < 0) {
            perror("Erro no send");
            break;
        }
        if (strncmp(linha, "/sair", 5) == 0) {
            break;
        }
    }

    //limpa e encerra
    close(fd_servidor);
    printf("[%s] Cliente encerrado.\n", NOME_APP);

    return 0;
}