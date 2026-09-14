
#include "boteco.h"   

#include <pthread.h> // para pthread_create, pthread_detach 

#define IP_SERVIDOR  "127.0.0.1" //loopback: servidor na mesma maquina 

//global, pois precisa ser visto pela thread de recebimento e pela main. 
static int fd_servidor;
 
//thread de recebimento: le do socket ate a conexao cair e imprime na tela.
void *thread_recebimento(void *arg) {
    char buffer[TAM_BUFFER];
    ssize_t bytes;
 
    (void)arg;   //evita warning do -Wextra 
 
    while ((bytes = recv(fd_servidor, buffer, sizeof(buffer) - 1, 0)) > 0) {
        buffer[bytes] = '\0';
        printf("%s", buffer);
        fflush(stdout);   //forca a impressao imediata, sem esperar o \n do buffer
    }
 
    if (bytes == 0) {
        printf("\n[%s] Conexao encerrada pelo servidor.\n", NOME_APP);
    } else {
        perror("Erro no recv");
    }
 
    // encerra o processo inteiro: a thread principal esta bloqueada no fgetse nao tem como perceber sozinha que o servidor caiu.
    close(fd_servidor);
    exit(EXIT_SUCCESS);
 
    return NULL;   //nunca alcancado
}
 
int main(void) {
    struct sockaddr_in endereco;   //endereco do servidor
    pthread_t tid;                 //identificador da thread de recebimento 
    char linha[TAM_BUFFER];        //frase digitada pelo usuario 
    int erro;
 
    //cria o socket (tcp)
    fd_servidor = socket(AF_INET, SOCK_STREAM, 0);
    if (fd_servidor < 0) {
        perror("Erro ao criar socket");
        exit(EXIT_FAILURE);
    }
 
    // monta o endereco de destino
    memset(&endereco, 0, sizeof(endereco));
    endereco.sin_family = AF_INET;
    endereco.sin_port   = htons(PORTA);
    if (inet_pton(AF_INET, IP_SERVIDOR, &endereco.sin_addr) <= 0) {
        fprintf(stderr, "Endereco IP invalido: %s\n", IP_SERVIDOR);
        close(fd_servidor);
        exit(EXIT_FAILURE);
    }
 
    // abre a conexao. falha aqui normalmente = a servidor fora do ar
    if (connect(fd_servidor, (struct sockaddr *)&endereco, sizeof(endereco)) < 0) {
        perror("Erro ao conectar no servidor");
        close(fd_servidor);
        exit(EXIT_FAILURE);
    }
 
    printf("[%s] Conectado a %s:%d. Digite /quit para sair.\n",
           NOME_APP, IP_SERVIDOR, PORTA);
 
    // cria a thread de recebimento, que vai imprimir na tela tudo que o servidor enviar
    erro = pthread_create(&tid, NULL, thread_recebimento, NULL);
    if (erro != 0) {
        fprintf(stderr, "Erro ao criar thread de recebimento: %s\n", strerror(erro));
        close(fd_servidor);
        exit(EXIT_FAILURE);
    }
    pthread_detach(tid);   
 
    // looping principal: le do teclado e envia para o servidor.
    while (fgets(linha, sizeof(linha), stdin) != NULL) {
        if (send(fd_servidor, linha, strlen(linha), 0) < 0) {
            perror("Erro no send");
            break;
        }
        if (strncmp(linha, "/quit", 5) == 0) {
            break;
        }
    }
 
    // limpa e encerra
    close(fd_servidor);
    printf("[%s] Cliente encerrado.\n", NOME_APP);
 
    return 0;
}