# Boteco — chat multiusuário em sockets TCP

Trabalho 1 — Redes de Computadores — ICMC/USP São Carlos

**Autora:** Eduarda Almeida Garrett de Carvalho — nº USP 14566794

---

## Descrição

Aplicação cliente/servidor de bate-papo em sala única. O servidor aceita até 30
conexões simultâneas, cada uma atendida por uma thread própria, e retransmite as
mensagens recebidas para todos os demais participantes. O cliente permite enviar
e receber ao mesmo tempo, sem travar à espera do teclado.

Implementado em C puro, sem bibliotecas externas: apenas a API de sockets POSIX
(`sys/socket.h`, `arpa/inet.h`) e a biblioteca de threads POSIX (`pthread.h`).

## Ambiente de desenvolvimento

| Item | Versão |
|---|---|
| Sistema operacional | Ubuntu 24.04.1 LTS |
| Compilador | gcc 13.3.0 (Ubuntu 13.3.0-6ubuntu2~24.04.1) |
| Protocolo de transporte | TCP (IPv4) |
| Porta padrão | 8080 |

## Estrutura dos arquivos

| Arquivo | Conteúdo |
|---|---|
| `boteco.h` | Constantes e includes comuns a todos os módulos |
| `clientes.h` / `clientes.c` | Lista de clientes conectados, mutex e broadcast |
| `comandos.h` / `comandos.c` | Interpretação dos comandos iniciados por `/` |
| `servidor.c` | Socket de escuta, laço de `accept`, thread de cada cliente |
| `cliente.c` | Conexão ao servidor e envio/recebimento simultâneos |
| `Makefile` | Compilação, execução e empacotamento |

O array de clientes e seu mutex são declarados `static` dentro de `clientes.c`.
Nenhum outro módulo acessa a estrutura diretamente — todo acesso passa pelas
funções públicas declaradas em `clientes.h`, que já cuidam do travamento.

## Compilação

```bash
make
```

Gera os executáveis `servidor` e `cliente`. Compila sem avisos com
`-Wall -Wextra`.

Outros alvos:

```bash
make run-servidor   # compila (se necessário) e executa o servidor
make run-cliente    # compila (se necessário) e executa o cliente
make clean          # remove objetos, executáveis e o zip
make zip            # gera o pacote de entrega apenas com os fontes
```

## Execução

Em um terminal:

```bash
./servidor
```

Em outros terminais, um por participante:

```bash
./cliente
```

Por padrão o cliente conecta em `127.0.0.1`. Para usar o servidor em outra
máquina da rede, altere a constante `IP_SERVIDOR` no topo de `cliente.c` e
recompile.

Encerre o servidor com `Ctrl+C`.

## Comandos disponíveis

| Comando | Efeito |
|---|---|
| `/ajuda` | Lista os comandos |
| `/apelido <nome>` | Troca o nome exibido; recusa nomes já em uso |
| `/quem` | Lista os participantes conectados |
| `/sussurro <apelido> <msg>` | Mensagem privada a um participante |
| `/sair` | Encerra a conexão do cliente |

Qualquer linha que não comece com `/` é transmitida aos demais participantes.

## Arquitetura de concorrência

A thread principal do servidor permanece no laço de `accept` e não conversa com
nenhum cliente. Cada conexão aceita recebe uma thread criada com
`pthread_create` e desanexada com `pthread_detach`, de modo que libera seus
próprios recursos ao terminar, sem necessidade de `pthread_join`.

O índice do cliente é passado à thread em memória alocada com `malloc`, liberada
pela própria thread assim que copia o valor. Passar o endereço de uma variável
local da `main` seria incorreto: o valor mudaria no `accept` seguinte, antes que
a thread o lesse.

Todas as threads compartilham o array de clientes, portanto qualquer leitura ou
escrita nele ocorre com `pthread_mutex_lock`. O `broadcast` mantém o mutex
travado durante toda a varredura: se travasse a cada envio isolado, outra thread
poderia remover um cliente no meio do laço e o `send` seguinte usaria um
descritor já fechado.

Como o mutex padrão do pthread não é recursivo, nenhuma função travada chama
outra que também trave — travar duas vezes na mesma thread causaria *deadlock*.

## Tratamento de falhas de conexão e transmissão

Toda chamada à API de sockets tem o retorno verificado. Falhas são reportadas
com `perror`, que imprime a causa informada pelo sistema operacional.

**Falhas na abertura da conexão**

| Situação | Comportamento |
|---|---|
| `socket`, `bind` ou `listen` falham no servidor | Mensagem via `perror`, sockets já abertos são fechados e o processo encerra com `EXIT_FAILURE` |
| Porta ainda em `TIME_WAIT` de uma execução anterior | Evitado com `SO_REUSEADDR`, aplicado antes do `bind` |
| `connect` falha no cliente (servidor fora do ar) | `perror` reporta *Connection refused*, o socket é fechado e o cliente encerra |
| IP de destino inválido no cliente | `inet_pton` retorna ≤ 0, mensagem de erro e encerramento |
| `accept` falha no servidor | Erro é reportado e o laço continua: uma conexão malsucedida não derruba o servidor |
| Limite de 30 clientes atingido | O servidor avisa o cliente, fecha aquele socket e segue aceitando os demais |
| `pthread_create` ou `malloc` falham | O slot reservado é liberado, o socket é fechado e o servidor continua operando |

**Falhas durante a transmissão**

| Situação | Comportamento |
|---|---|
| `recv` retorna 0 | Desconexão ordenada do outro lado: o slot é liberado e os demais participantes são avisados |
| `recv` retorna < 0 | Erro reportado com `perror`; a thread encerra e libera o slot |
| `send` retorna < 0 | Erro reportado; no `broadcast` o envio prossegue para os demais destinatários |
| Escrita em socket fechado pelo outro lado | `SIGPIPE` é ignorado com `signal(SIGPIPE, SIG_IGN)`; sem isso o sinal encerraria o processo do servidor. O `send` apenas retorna −1 e o erro é tratado |
| Queda abrupta do cliente (`Ctrl+C`, sem `/sair`) | O `recv` do servidor retorna 0 e o cliente é removido normalmente |
| Queda do servidor com clientes conectados | A thread de recebimento do cliente detecta o fim da conexão, avisa o usuário e encerra o processo |

**Encerramento do servidor**

`Ctrl+C` é capturado por um tratador de `SIGINT` que faz apenas duas operações
seguras em contexto de sinal: escreve em uma variável `volatile sig_atomic_t` e
fecha o socket de escuta com `close`. Fechar o descritor faz o `accept` retornar
−1, o laço principal termina e toda a limpeza ocorre em contexto normal, fora do
tratador — os clientes são avisados e seus sockets fechados antes de o processo
terminar.

## Limitações conhecidas

- Sala única: não há separação por canais ou salas.
- Apelidos não são persistentes entre execuções.
- Mensagens acima de 1023 bytes são entregues fragmentadas, comportamento
  esperado de TCP, que é um fluxo de bytes e não de mensagens delimitadas.
- Comunicação em texto puro, sem qualquer cifragem.