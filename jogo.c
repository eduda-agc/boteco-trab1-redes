
#include "jogo.h"
#include "clientes.h"

/// uma faixa de valores e a frase que os bots usam para descreve-la
typedef struct {
    int         min;
    int         max;
    const char *frase;
} Faixa;

typedef struct {
    const char *nome;
    Faixa       faixas[NUM_FAIXAS];
} Tema;

// tabela de temas e faixas, usada pelos bots para gerar dicas
static const Tema TEMAS[] = {
    {
        "Quao CARA e a coisa (1 = de graca, 100 = impagavel)",
        {
            {  1,  20, "bala de menta do balcao" },
            { 21,  40, "coxinha da cantina" },
            { 41,  60, "tenis usado da feira" },
            { 61,  80, "notebook de segunda mao" },
            { 81, 100, "apartamento na avenida" }
        }
    },
    {
        "Quao ASSUSTADOR e o bicho (1 = fofo, 100 = pavoroso)",
        {
            {  1,  20, "coelhinho dormindo" },
            { 21,  40, "gato de vizinho mal-humorado" },
            { 41,  60, "barata voando na cozinha" },
            { 61,  80, "cachorro bravo sem coleira" },
            { 81, 100, "aranha do tamanho da mao" }
        }
    },
    {
        "Quao CONSTRANGEDOR e o vexame (1 = nada, 100 = mudar de cidade)",
        {
            {  1,  20, "tropecar sozinho na rua" },
            { 21,  40, "chamar o professor de mae" },
            { 41,  60, "mandar mensagem para a pessoa errada" },
            { 61,  80, "cair no meio da apresentacao" },
            { 81, 100, "curtir foto antiga da crush sem querer" }
        }
    }
};

#define NUM_TEMAS ((int)(sizeof(TEMAS) / sizeof(TEMAS[0])))

static const char *NOMES_BOTS[NUM_BOTS] = { "Zeca-bot", "Marisa-bot" };

// um participante da rodada: cliente conectado ou bot
typedef struct {
    int  indice; // indice do cliente na lista de ativos, ou -1 se for bot
    char apelido[TAM_APELIDO];
    int  numero; // numero secreto, de 1 a 100
    char dica[TAM_DICA];
    int  tem_dica;
    int  eh_bot;
} Jogador;

// estado da rodada, privado deste modulo
static Jogador jogadores[MAX_JOGADORES];
static int n_jogadores  = 0;
static int tema_atual   = 0;
static int rodada_ativa = 0;
static pthread_mutex_t mutex_jogo = PTHREAD_MUTEX_INITIALIZER;

//devolve a frase da faixa em que o numero cai, dentro do tema
static const char *frase_da_faixa(int tema, int numero) {
    int i;

    for (i = 0; i < NUM_FAIXAS; i++) {
        if (numero >= TEMAS[tema].faixas[i].min &&
            numero <= TEMAS[tema].faixas[i].max) {
            return TEMAS[tema].faixas[i].frase;
        }
    }

    return "sei la, passo";
}

// procura um jogador pelo indice do cliente. Retorna -1 se nao acha
static int posicao_por_indice(int indice) {
    int i;

    for (i = 0; i < n_jogadores; i++) {
        if (!jogadores[i].eh_bot && jogadores[i].indice == indice) {
            return i;
        }
    }

    return -1;
}

// procura um jogador pelo apelido. Retorna -1 se nao achar
static int posicao_por_apelido(const char *apelido) {
    int i;

    for (i = 0; i < n_jogadores; i++) {
        if (strcmp(jogadores[i].apelido, apelido) == 0) {
            return i;
        }
    }

    return -1;
}

// ordena o vetor de posicoes pelo criterio em chave (insertion sort)
static void ordenar_por_chave(int *posicoes, int *chave, int n) {
    int i, j, pos_tmp, chave_tmp;

    for (i = 1; i < n; i++) {
        pos_tmp   = posicoes[i];
        chave_tmp = chave[i];
        j = i - 1;
        while (j >= 0 && chave[j] > chave_tmp) {
            posicoes[j + 1] = posicoes[j];
            chave[j + 1]    = chave[j];
            j--;
        }
        posicoes[j + 1] = pos_tmp;
        chave[j + 1]    = chave_tmp;
    }
}

// faz o bot chutar a ordem dos jogadores, sem saber os numeros secretos
static void chute_do_bot(int pos_bot, char *saida, size_t tam) {
    int posicoes[MAX_JOGADORES];
    int chave[MAX_JOGADORES];
    int i;
    size_t usado;

    for (i = 0; i < n_jogadores; i++) {
        posicoes[i] = i;
        if (jogadores[i].eh_bot) {
            chave[i] = jogadores[i].numero; // bot sabe o proprio numero secreto
        } else {
            chave[i] = (rand() % 100) + 1; // chute aleatorio para os outros
        }
    }

    ordenar_por_chave(posicoes, chave, n_jogadores);

    usado = (size_t)snprintf(saida, tam, "    %s chutou: ",
                             jogadores[pos_bot].apelido);
    for (i = 0; i < n_jogadores && usado < tam; i++) {
        usado += (size_t)snprintf(saida + usado, tam - usado, "%s%s",
                                  jogadores[posicoes[i]].apelido,
                                  (i < n_jogadores - 1) ? " < " : "\n");
    }
}


int jogo_iniciar(const char *quem, char *saida, size_t tam) {
    int indices[MAX_CLIENTES];
    char apelidos[MAX_CLIENTES][TAM_APELIDO];
    int total, i, j, repetido;
    size_t usado;

    // lista os clientes conectados, para inscrever na rodada
    total = listar_ativos(indices, apelidos, MAX_CLIENTES);

    pthread_mutex_lock(&mutex_jogo);

    if (rodada_ativa) {
        snprintf(saida, tam,
                 "*** Ja tem rodada rolando. Use /ordem para fechar ou /placar.\n");
        pthread_mutex_unlock(&mutex_jogo);
        return JOGO_PRIVADO;
    }

    n_jogadores = 0;

    // inscreve os clientes conectados, ate o limite de MAX_JOGADORES.
    for (i = 0; i < total && n_jogadores < MAX_JOGADORES; i++) {
        jogadores[n_jogadores].indice   = indices[i];
        jogadores[n_jogadores].eh_bot   = 0;
        jogadores[n_jogadores].tem_dica = 0;
        jogadores[n_jogadores].dica[0]  = '\0';
        snprintf(jogadores[n_jogadores].apelido, TAM_APELIDO, "%s", apelidos[i]);
        n_jogadores++;
    }

    // inscreve os bots
    for (i = 0; i < NUM_BOTS && n_jogadores < MAX_JOGADORES; i++) {
        jogadores[n_jogadores].indice   = -1;
        jogadores[n_jogadores].eh_bot   = 1;
        jogadores[n_jogadores].tem_dica = 1;   /* bot ja chega com a dica */
        snprintf(jogadores[n_jogadores].apelido, TAM_APELIDO, "%s", NOMES_BOTS[i]);
        n_jogadores++;
    }

    tema_atual = rand() % NUM_TEMAS;

    // sorteia numeros distintos: repete o sorteio enquanto houver colisao
    for (i = 0; i < n_jogadores; i++) {
        do {
            repetido = 0;
            jogadores[i].numero = (rand() % 100) + 1;
            for (j = 0; j < i; j++) {
                if (jogadores[j].numero == jogadores[i].numero) {
                    repetido = 1;
                    break;
                }
            }
        } while (repetido);
    }

    // agora que os numeros existem, os bots escolhem a frase deles
    for (i = 0; i < n_jogadores; i++) {
        if (jogadores[i].eh_bot) {
            snprintf(jogadores[i].dica, TAM_DICA, "%s",
                     frase_da_faixa(tema_atual, jogadores[i].numero));
        }
    }

    rodada_ativa = 1;

    usado = (size_t)snprintf(saida, tam,
        "\n=== RODADA ABERTA por %s ===\n"
        "Tema: %s\n"
        "Cada um recebeu um numero secreto de 1 a 100.\n"
        "Use /meunumero para ver o seu e /dica <frase> para descreve-lo.\n"
        "Na mesa: ", quem, TEMAS[tema_atual].nome);

    for (i = 0; i < n_jogadores && usado < tam; i++) {
        usado += (size_t)snprintf(saida + usado, tam - usado, "%s%s",
                                  jogadores[i].apelido,
                                  (i < n_jogadores - 1) ? ", " : "\n");
    }

    pthread_mutex_unlock(&mutex_jogo);

    return JOGO_PUBLICO;
}

int jogo_meu_numero(int indice, char *saida, size_t tam) {
    int pos;

    pthread_mutex_lock(&mutex_jogo);

    if (!rodada_ativa) {
        snprintf(saida, tam, "*** Nao tem rodada aberta. Use /jogar.\n");
    } else {
        pos = posicao_por_indice(indice);
        if (pos < 0) {
            snprintf(saida, tam,
                     "*** Voce entrou depois que a rodada comecou. Espere a proxima.\n");
        } else {
            snprintf(saida, tam,
                     "*** Seu numero secreto e %d. Tema: %s\n",
                     jogadores[pos].numero, TEMAS[tema_atual].nome);
        }
    }

    pthread_mutex_unlock(&mutex_jogo);

    return JOGO_PRIVADO;
}

int jogo_dica(int indice, const char *texto, char *saida, size_t tam) {
    int pos, i, faltam = 0;
    int destino = JOGO_PRIVADO;

    pthread_mutex_lock(&mutex_jogo);

    if (!rodada_ativa) {
        snprintf(saida, tam, "*** Nao tem rodada aberta. Use /jogar.\n");
    } else {
        pos = posicao_por_indice(indice);
        if (pos < 0) {
            snprintf(saida, tam, "*** Voce nao esta nesta rodada.\n");
        } else if (*texto == '\0') {
            snprintf(saida, tam, "*** Use: /dica <sua frase>\n");
        } else {
            snprintf(jogadores[pos].dica, TAM_DICA, "%s", texto);
            jogadores[pos].tem_dica = 1;

            for (i = 0; i < n_jogadores; i++) {
                if (!jogadores[i].tem_dica) {
                    faltam++;
                }
            }

            if (faltam > 0) {
                snprintf(saida, tam,
                         "*** %s ja deu a dica. Faltam %d.\n",
                         jogadores[pos].apelido, faltam);
            } else {
                snprintf(saida, tam,
                         "*** %s ja deu a dica. Todos prontos! Use /revelar.\n",
                         jogadores[pos].apelido);
            }
            destino = JOGO_PUBLICO;
        }
    }

    pthread_mutex_unlock(&mutex_jogo);

    return destino;
}

int jogo_revelar(char *saida, size_t tam) {
    int i;
    size_t usado;
    int faltam = 0;

    pthread_mutex_lock(&mutex_jogo);

    if (!rodada_ativa) {
        snprintf(saida, tam, "*** Nao tem rodada aberta. Use /jogar.\n");
        pthread_mutex_unlock(&mutex_jogo);
        return JOGO_PRIVADO;
    }

    for (i = 0; i < n_jogadores; i++) {
        if (!jogadores[i].tem_dica) {
            faltam++;
        }
    }

    if (faltam > 0) {
        usado = (size_t)snprintf(saida, tam, "*** Ainda faltam dicas de: ");
        for (i = 0; i < n_jogadores && usado < tam; i++) {
            if (!jogadores[i].tem_dica) {
                usado += (size_t)snprintf(saida + usado, tam - usado, "%s ",
                                          jogadores[i].apelido);
            }
        }
        snprintf(saida + usado, tam - usado, "\n");
        pthread_mutex_unlock(&mutex_jogo);
        return JOGO_PRIVADO;
    }

    usado = (size_t)snprintf(saida, tam,
        "\n=== DICAS DA MESA ===\nTema: %s\n", TEMAS[tema_atual].nome);

    for (i = 0; i < n_jogadores && usado < tam; i++) {
        usado += (size_t)snprintf(saida + usado, tam - usado,
                                  "    %-16s %s\n",
                                  jogadores[i].apelido, jogadores[i].dica);
    }

    snprintf(saida + usado, tam - usado,
             "Agora combinem a ordem crescente e mandem:\n"
             "    /ordem <apelido1> <apelido2> ...\n");

    pthread_mutex_unlock(&mutex_jogo);

    return JOGO_PUBLICO;
}

int jogo_ordem(const char *quem, const char *lista, char *saida, size_t tam) {
    char copia[TAM_BUFFER];
    char *token;
    char *contexto;
    int  proposta[MAX_JOGADORES];
    int  n_proposta = 0;
    int  posicoes[MAX_JOGADORES];
    int  chave[MAX_JOGADORES];
    int  i, pos, acertou;
    size_t usado;

    pthread_mutex_lock(&mutex_jogo);

    if (!rodada_ativa) {
        snprintf(saida, tam, "*** Nao tem rodada aberta. Use /jogar.\n");
        pthread_mutex_unlock(&mutex_jogo);
        return JOGO_PRIVADO;
    }

    //strtok_r modifica a string, por isso trabalhamos sobre uma copia
    snprintf(copia, sizeof(copia), "%s", lista);
    token = strtok_r(copia, " ", &contexto);
    while (token != NULL && n_proposta < MAX_JOGADORES) {
        pos = posicao_por_apelido(token);
        if (pos < 0) {
            snprintf(saida, tam, "*** Nao tem ninguem chamado %s nesta rodada.\n", token);
            pthread_mutex_unlock(&mutex_jogo);
            return JOGO_PRIVADO;
        }
        proposta[n_proposta++] = pos;
        token = strtok_r(NULL, " ", &contexto);
    }

    if (n_proposta != n_jogadores) {
        snprintf(saida, tam,
                 "*** A ordem precisa ter os %d participantes, veio com %d.\n",
                 n_jogadores, n_proposta);
        pthread_mutex_unlock(&mutex_jogo);
        return JOGO_PRIVADO;
    }

    // a proposta esta certa se os numeros ficarem em ordem crescente
    acertou = 1;
    for (i = 1; i < n_proposta; i++) {
        if (jogadores[proposta[i]].numero < jogadores[proposta[i - 1]].numero) {
            acertou = 0;
            break;
        }
    }

    usado = (size_t)snprintf(saida, tam,
        "\n=== FIM DA RODADA (ordem proposta por %s) ===\n", quem);

    usado += (size_t)snprintf(saida + usado, tam - usado,
        acertou ? "RESULTADO: acertaram na mosca!\n"
                : "RESULTADO: errou feio, errou rude.\n");

    // revela os numeros em ordem crescente de verdade
    for (i = 0; i < n_jogadores; i++) {
        posicoes[i] = i;
        chave[i]    = jogadores[i].numero;
    }
    ordenar_por_chave(posicoes, chave, n_jogadores);

    usado += (size_t)snprintf(saida + usado, tam - usado, "Ordem correta:\n");
    for (i = 0; i < n_jogadores && usado < tam; i++) {
        usado += (size_t)snprintf(saida + usado, tam - usado,
                                  "    %3d  %-16s %s\n",
                                  jogadores[posicoes[i]].numero,
                                  jogadores[posicoes[i]].apelido,
                                  jogadores[posicoes[i]].dica);
    }

    // mostra o que cada bot teria chutado
    usado += (size_t)snprintf(saida + usado, tam - usado, "O que os bots achavam:\n");
    for (i = 0; i < n_jogadores && usado < tam; i++) {
        if (jogadores[i].eh_bot) {
            chute_do_bot(i, saida + usado, tam - usado);
            usado += strlen(saida + usado);
        }
    }

    snprintf(saida + usado, tam - usado, "Use /jogar para abrir outra rodada.\n");

    rodada_ativa = 0;

    pthread_mutex_unlock(&mutex_jogo);

    return JOGO_PUBLICO;
}

int jogo_status(char *saida, size_t tam) {
    int i;
    size_t usado;

    pthread_mutex_lock(&mutex_jogo);

    if (!rodada_ativa) {
        snprintf(saida, tam, "*** Nenhuma rodada aberta. Use /jogar.\n");
    } else {
        usado = (size_t)snprintf(saida, tam,
            "*** Rodada em andamento. Tema: %s\n", TEMAS[tema_atual].nome);
        for (i = 0; i < n_jogadores && usado < tam; i++) {
            usado += (size_t)snprintf(saida + usado, tam - usado,
                                      "    %-16s %s\n",
                                      jogadores[i].apelido,
                                      jogadores[i].tem_dica ? "ja deu a dica"
                                                            : "ainda pensando");
        }
    }

    pthread_mutex_unlock(&mutex_jogo);

    return JOGO_PRIVADO;
}
