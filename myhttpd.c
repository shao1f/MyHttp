#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <sys/sendfile.h>

#define MAX 1024

static int startup(int port){
    int sock = socket(AF_INET,SOCK_STREAM,0);
    if(sock < 0 ){
        perror("socket");
        exit(2);
    }

    int opt = 1;
    setsockopt(sock,SOL_SOCKET,SO_REUSEADDR,&opt,sizeof(opt));

    struct sockaddr_in server;
    server.sin_family = AF_INET;
    server.sin_port = htons(port);
    server.sin_addr.s_addr = htonl(INADDR_ANY);

    if(bind(sock,(struct sockaddr*)&server,sizeof(server)) < 0){
        perror("bind");
        exit(3);
    }

    if(listen(sock,5) < 0){
        perror("listen");
        exit(4);
    }
    return sock;
}

static void usage(const char* msg){
    printf("Usage: %s [port]",msg);
}

static int get_line(int sock,char len[],int size){
    if(len == NULL || size <= 0){
        return -1;
    }
    int c = 'a';
    int i = 0;
    int n = 0;
    while(i<size-1 && (c != '\n')){
         n = recv(sock,&c,1,0);
         if(n > 0){
             if(c == '\r'){
                recv(sock,&c,1,MSG_PEEK);
                if(c == '\n'){
                    recv(sock,&c,1,0);
                }else {
                    c = '\n';
                }
             }
             len[i++] = c;
         }else {
             c = '\n';
         }
    }
    len[i] = '\0';
    return i;
}

static void echo_eror(int sock){

}

static void clear_header(int sock){
    int ret = -1;
    char buf[MAX];
    do{
        ret = get_line(sock,buf,sizeof(buf));
    //    printf("%s",buf);
    }while(ret > 0 && strcmp(buf,"\n") != 0);
}

void echo_www(int sock,char path[],int size){
    int fd = open(path,O_RDONLY);
    if(fd < 0){
        //TODO
        //错误码,稍后再说
    }
    char buf[MAX];
    sprintf(buf,"HTTP/1.0 200 OK\r\n");
    send(sock,buf,strlen(buf),0);
    sprintf(buf,"Content-Type: text/html; image/jpeg;charset=utf-8\r\n");
    send(sock,buf,strlen(buf),0);
    sprintf(buf,"\r\n");
    send(sock,buf,strlen(buf),0);

    if(sendfile(sock,fd,0,size) < 0){
        //TODO
        //错误处理
        close(fd);
    }
    close(fd);

}

void exe_cgi(int sock,char path[],char method[],char* query_string){

}

void* http_responce(void* arg){
    int sock = (int)arg;
    int ret = -1;
    char buf[MAX];

    int errCode = 200;
    int cgi = 0;
    char method[MAX/32];
    char url[MAX];
    char path[MAX];
    char *query_string = NULL;
#if 0
    do{
        ret = get_line(sock,buf,sizeof(buf));
        printf("%s",buf);
    }while(ret > 0 && strcmp(buf,"\n") != 0);
#endif
    ret = get_line(sock,buf,sizeof(buf));
    if(ret <= 0){
        errCode = 404;
    }

    size_t i = 0;//method下标
    size_t j = 0;//buf下标
    while((i<sizeof(method)-1) && (j<sizeof(buf))  && !(isspace(buf[j]))){
        method[i] = buf[j];
        ++i;
        ++j;
    }
    method[i] = '\0';
    if(strcasecmp(method,"GET")!=0 && strcasecmp(method,"POST")!=0){
        errCode = 404;
        goto end;
    }
    
    if(strcasecmp(method,"POST") == 0){
        cgi = 1;
    }
    i = 0;//获取url
    while((i<sizeof(url)-1) && j<sizeof(buf) && !(isspace(buf[j]))){
        url[i] = buf[j];
        ++i;
        ++j;
    }
//    url[i] = '\0';
    if(strcasecmp(method,"GET") == 0){
        query_string = url;
        while(*query_string != '\0' && *query_string != '?'){
            ++query_string;
        }
        if(*query_string == '?'){
            cgi = 1;
            *query_string++ = '\0';
        }
        sprintf(path,"wwwroot%s",url);
        if(path[strlen(path)-1] == '/'){
            strcat(path,"index.html");
        }
    }
    
    struct stat st;
    if(stat(path,&st) < 0){
        errCode = 404;
        goto end;
    }else {
        if(S_ISDIR(st.st_mode)){
            strcpy(path,"wwwroot/index.html");
        }else if((st.st_mode & S_IXOTH) || (st.st_mode & S_IXGRP) || (st.st_mode & S_IXUSR)){
            cgi = 1;
        }else {
            //TODO
            //是普通文件
        }
        if(cgi){
            exe_cgi(sock,path,method,query_string);
        }else {
            clear_header(sock);
            echo_www(sock,path,st.st_size);
        }
    }

end:
    if(errCode == 404){
        echo_eror(sock);
    }
    close(sock);
    return (void*)0;
}

int main(int argc,char* argv[]){
    if(argc != 2){
        usage(argv[0]);
        return 1;
    }
    
    int listen_sock = startup(atoi(argv[1]));
    for(;;){
        struct sockaddr_in client;
        socklen_t len = sizeof(client);

        int new_sock = accept(listen_sock,(struct sockaddr*)&client,&len);
        if(new_sock < 0){
            perror("accept");
            continue;
        }
        pthread_t pid;
        pthread_create(&pid,NULL,http_responce,(void*)new_sock);
        pthread_detach(pid);
    }

    return 0;
}
