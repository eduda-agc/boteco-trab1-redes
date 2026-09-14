#ifndef BOTECO_H
#define BOTECO_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>       // close
#include <arpa/inet.h>    //sockaddr_in, htons, ntohs, inet_ntop 
#include <sys/socket.h>   // socket, setsockopt, bind, listen, accept, recv, send

#define PORTA       8080  

#define TAM_BUFFER  1024  //tam maximo de uma mensagem recebida
#define MAX_FILA    10    //conexoes pendentes na fila do listen

#define NOME_APP "Boteco" 

#endif 

