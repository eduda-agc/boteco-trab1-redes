#ifndef BOTECO_H
#define BOTECO_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>       // close
#include <arpa/inet.h>    //sockaddr_in, htons, ntohs, inet_ntop 
#include <sys/socket.h>   // socket, setsockopt, bind, listen, accept, recv, send

#include <pthread.h>      // threads e mutex, para pthread_create, pthread_detach

#define MAX_CLIENTES  30              // conexoes simultaneas suportadas
#define TAM_APELIDO   32              // tamanho maximo do apelido
#define TAM_MENSAGEM  (TAM_BUFFER + TAM_APELIDO + 32)

#define PORTA       8080  

#define TAM_BUFFER  1024  //tam maximo de uma mensagem recebida
#define MAX_FILA    10    //conexoes pendentes na fila do listen

#define NOME_APP "Boteco" 

#endif 

