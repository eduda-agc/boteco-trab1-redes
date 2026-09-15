 
#include "comandos.h"
#include "clientes.h"
#include "jogo.h"
 
const char *TEXTO_AJUDA =
    "*** Comandos do Boteco:\n"
    "    /ajuda                     mostra esta lista\n"
    "    /apelido <nome>            troca seu nome\n"
    "    /quem                      quem esta no boteco\n"
    "    /sussurro <apelido> <msg>  mensagem privada\n"
    "    /sair                      vai embora\n"
    "*** Jogo do Boteco:\n"
    "    /jogar                     abre uma rodada\n"
    "    /meunumero                 ve seu numero secreto\n"
    "    /dica <frase>              descreve seu numero\n"
    "    /revelar                   mostra todas as dicas\n"
    "    /ordem <ap1> <ap2> ...     fecha a rodada e confere\n"
    "    /placar                    situacao da rodada\n";


// remove \n e \r do fim da linha recebida, para facilitar o parse
void remover_quebra_linha(char *texto) {
    size_t n = strlen(texto);
 
    while (n > 0 && (texto[n - 1] == '\n' || texto[n - 1] == '\r')) {
        texto[--n] = '\0';
    }
}
 
//trata /apelido <nome>. Retorna sempre 0 (nao desconecta)
static int comando_apelido(int indice, int fd, char *apelido, char *linha) {
    char mensagem[TAM_MENSAGEM];
    char *argumento = linha + 9; // pula "/apelido "
 
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
 
    //atualiza a copia local da thread chamadora
    snprintf(apelido, TAM_APELIDO, "%s", argumento);
 
    return 0;
}
 
//trata /sussurro <apelido> <mensagem>. Retorna sempre 0
static int comando_sussurro(int fd, const char *apelido, char *linha) {
    char mensagem[TAM_MENSAGEM];
    char destinatario[TAM_APELIDO];
    char *argumento = linha + 10;   /* pula "/sussurro " */
    char *espaco;
    int fd_destino;
 
    espaco = strchr(argumento, ' ');
    if (espaco == NULL || (size_t)(espaco - argumento) >= TAM_APELIDO) {
        enviar_para(fd, "*** Use: /sussurro <apelido> <mensagem>\n");
        return 0;
    }
 
    // snprintf com tamanho = numero de caracteres + 1 recorta o apelido
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

// envia a resposta do jogo para o cliente ou para todos, dependendo do destino
static void responder_jogo(int destino, int fd, const char *texto) {
    if (destino == JOGO_PUBLICO) {
        printf("%s", texto);
        broadcast(texto, -1);
    } else {
        enviar_para(fd, texto);
    }
}

//processa os comandos do jogo. Retorna 1 se o comando era do jogo, 0 caso contrario
static int comando_de_jogo(int indice, int fd, const char *apelido, char *linha) {
    char texto[TAM_TEXTO_JOGO];
    int destino;
 
    if (strcmp(linha, "/jogar") == 0) {
        destino = jogo_iniciar(apelido, texto, sizeof(texto));
    } else if (strcmp(linha, "/meunumero") == 0) {
        destino = jogo_meu_numero(indice, texto, sizeof(texto));
    } else if (strcmp(linha, "/revelar") == 0) {
        destino = jogo_revelar(texto, sizeof(texto));
    } else if (strcmp(linha, "/placar") == 0) {
        destino = jogo_status(texto, sizeof(texto));
    } else if (strncmp(linha, "/dica ", 6) == 0) {
        destino = jogo_dica(indice, linha + 6, texto, sizeof(texto));
    } else if (strncmp(linha, "/ordem ", 7) == 0) {
        destino = jogo_ordem(apelido, linha + 7, texto, sizeof(texto));
    } else {
        return 0;   /* nao e comando do jogo */
    }
 
    responder_jogo(destino, fd, texto);
    return 1;
}



//processa os comandos recebidos de um cliente. Retorna 1 se o cliente deve ser desconectado, 0 caso contrario
int processar_comando(int indice, int fd, char *apelido, char *linha) {
    char mensagem[TAM_MENSAGEM];
 
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
        return comando_apelido(indice, fd, apelido, linha);
    }
 
    if (strncmp(linha, "/sussurro ", 10) == 0) {
        return comando_sussurro(fd, apelido, linha);
    }
 
    enviar_para(fd, "*** Comando desconhecido. Tente /ajuda\n");
    return 0;
}
