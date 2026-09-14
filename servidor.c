#include "boteco.h"

typedef struct 
{
    int fd; // socket do cliente
    char apelido[TAM_APELIDO]; // apelido do cliente
    int ativo; // flag para indicar se o cliente está ativo
} Cliente;

static Cliente clientes[MAX_CLIENTES]; // array de clientes conectados
static pthread_mutex_t mutex_clientes = PTHREAD_MUTEX_INITIALIZER; // mutex para proteger o array de clientes

static int fd_servidor = -1; // socket do servidor, usado para fechar o socket quando o servidor for encerrado
static volatile sig_atomic_t servidor_rodando = 1; // flag para indicar se o servidor está rodando


// O DESENVOLVIMENTO DAS FUNÇÕES AUXILIARES (adicionar_cliente, remover_cliente, broadcast...) ESTÃO NO FINAL DO ARQUIVO, APÓS O MAIN, PARA MANTER A ORGANIZAÇÃO DO CÓDIGO...

int main(void) {
    int fd_cliente; //socket de escuta 
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


// MAIS DE UM CLIENTE AO MESMO TEMPO --------------------------------

// adiciona um cliente na lista de clientes conectados
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

// troca o apelido de um cliente, se o novo apelido não estiver em uso
static int trocar_apelido(int indice, const char *novo) {
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
 
//devolve o socket de quem usa esse apelido, ou -1 se ninguem usa
static int fd_por_apelido(const char *apelido) {
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
 
//monta o texto da lista de conectados no buffer recebido
static void montar_lista(char *destino, size_t tam) {
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

// UTILITÁRIOS ------------------------------------------------------

// envia uma mensagem para um cliente específico
static void enviar_para(int fd, const char *mensagem) {
    if (send(fd, mensagem, strlen(mensagem), 0) < 0) {
        perror("Erro no send");
    }
}

// remove quebras de linha do final de uma string
static void remover_quebra_linha(char *texto) {
    size_t n = strlen(texto);
    while (n > 0 && (texto[n - 1] == '\n' || texto[n - 1] == '\r')) {
        texto[--n] = '\0';
    }
}

// COMANDOS ---------------------------------------------------------

// texto de ajuda para o comando /ajuda
static const char *TEXTO_AJUDA =
    "*** Comandos do Boteco:\n"
    "    /ajuda                     mostra esta lista\n"
    "    /apelido <nome>            troca seu nome\n"
    "    /quem                      quem esta no boteco\n"
    "    /sussurro <apelido> <msg>  mensagem privada\n"
    "    /sair                      vai embora\n";
 
 // processa um comando recebido de um cliente, retornando 1 se o cliente deve sair
static int processar_comando(int indice, int fd, char *apelido, char *linha) {
    char mensagem[TAM_MENSAGEM];
    char *argumento;
 
    if (strcmp(linha, "/sair") == 0) {
        enviar_para(fd, "*** Ate a proxima!\n");
        return 1;
    }
 
    if (strcmp(linha, "/ajuda") == 0) {
        enviar_para(fd, TEXTO_AJUDA);
        return 0;
    }
 
    if (strcmp(linha, "/quem") == 0) {
        montar_lista(mensagem, sizeof(mensagem));
        enviar_para(fd, mensagem);
        return 0;
    }
 
    if (strncmp(linha, "/apelido ", 9) == 0) {
        argumento = linha + 9;
        while (*argumento == ' ') {
            argumento++;
        }
 
        if (*argumento == '\0' || strlen(argumento) >= TAM_APELIDO) {
            enviar_para(fd, "*** Apelido invalido ou muito longo.\n");
            return 0;
        }
 
        if (!trocar_apelido(indice, argumento)) {
            enviar_para(fd, "*** Esse apelido ja esta em uso.\n");
            return 0;
        }
 
        snprintf(mensagem, sizeof(mensagem),
                 "*** %s agora se chama %s ***\n", apelido, argumento);
        printf("%s", mensagem);
        broadcast(mensagem, -1);
 
        //atualiza a copia local da thread.
        snprintf(apelido, TAM_APELIDO, "%s", argumento);
        return 0;
    }
 
    if (strncmp(linha, "/sussurro ", 10) == 0) {
        char destinatario[TAM_APELIDO];
        char *espaco;
        int fd_destino;
 
        argumento = linha + 10;
        espaco = strchr(argumento, ' ');
        if (espaco == NULL || (size_t)(espaco - argumento) >= TAM_APELIDO) {
            enviar_para(fd, "*** Use: /sussurro <apelido> <mensagem>\n");
            return 0;
        }
 
        snprintf(destinatario, (size_t)(espaco - argumento) + 1, "%s", argumento);
 
        fd_destino = fd_por_apelido(destinatario);
        if (fd_destino < 0) {
            enviar_para(fd, "*** Nao tem ninguem com esse apelido por aqui.\n");
            return 0;
        }
 
        snprintf(mensagem, sizeof(mensagem),
                 "(sussurro de %s) %s\n", apelido, espaco + 1);
        enviar_para(fd_destino, mensagem);
        enviar_para(fd, "*** Sussurro entregue.\n");
        return 0;
    }
 
    enviar_para(fd, "*** Comando desconhecido. Tente /ajuda\n");
    return 0;
}


// THREADS DOS CLIENTES -------------------------------------------------
// cada cliente tem sua própria thread, que fica bloqueada no recv() até o cliente enviar algo ou cair a conexão.
void *thread_cliente(void *arg) {
    int  indice = *((int *)arg);
    int  fd;
    int  encerrar = 0;
    char apelido[TAM_APELIDO];
    char buffer[TAM_BUFFER];
    char mensagem[TAM_MENSAGEM];
    ssize_t bytes;
 
    free(arg);
 
    pthread_mutex_lock(&mutex_clientes);
    fd = clientes[indice].fd;
    snprintf(apelido, TAM_APELIDO, "%s", clientes[indice].apelido);
    pthread_mutex_unlock(&mutex_clientes);
 
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

// ENCERRANDO O SERVIDOR -------------------------------------------------

// sinal para encerrar o servidor
static void tratar_sigint(int sinal) {
    (void)sinal;
    servidor_rodando = 0;
    if (fd_servidor >= 0) {
        close(fd_servidor);
    }
}
 
// fecha todos os sockets de clientes ainda conectados.
static void encerrar_clientes(void) {
    int i;
 
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
