 //client.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <fcntl.h>
 
#define N 256

char recv_data[N];
typedef struct sockaddr SA;

void user_con_commd(int,char *);
void user_mul_get(int,char *);
void user_mul_put(int,char *);
/*
1、  登录管理。客户端在shell中输入
myftp username:passwd@ftp-server-ip:ftp-server-port 登录ftpserver。
若通过身份验证，服务器将向客户回显"you are client #."(#为第几位用户)
并显示ftp命令输入提示符myftp>；否则显示"username doesn't exist or password is error!"。
*/

int main(int argc, char *argv[]){
    char commd[N];
    struct sockaddr_in addr;
    int len;
    int sockfd;
    int ser_port=0;
    char userinfo[N],ser_ip[N];


    strcpy(userinfo,strtok(argv[1],"@"));
    strcpy(ser_ip,strtok(NULL,":"));
    ser_port = atoi(strtok(NULL,":"));

    bzero(&addr, sizeof(addr));     //将＆addr中的前sizeof（addr）字节置为0，包括'\0'

    addr.sin_family = AF_INET;      //AF_INET代表TCP／IP协议
    addr.sin_addr.s_addr = inet_addr("127.0.0.1"); //将点间隔地址转换为网络字节顺序
    addr.sin_port = htons(8989);    //转换为网络字节顺序
    len = sizeof(addr);

    if((sockfd=socket(AF_INET, SOCK_STREAM, 0)) < 0){
        printf("Socket Error!\n");
        exit(1);
    }

    if(connect(sockfd, (SA *)&addr, sizeof(addr)) < 0){
        printf("Connect Error!\n");
        exit(1);
    }

    // //select
    // fd_set client_fd_set;
    // struct timeval tv;
    // tv.tv_sec = 20;
    // tv.tv_usec = 0;


    //发送 login user:pwd 的请求
    strcpy(commd,"login");
    strcat(commd," ");
    strcat(commd,userinfo);
    write(sockfd,commd,N);

    int b=1;
    read(sockfd,&b,N);

    if(!b){
        printf("username doesn't exist or password is error!\n");
        exit(1);
    }
    printf("you are client %d.\n", b);

    while(1){
        bzero(commd,sizeof(commd));//清零
        bzero(recv_data,sizeof(recv_data));
        printf("myftp>%s",commd);
        fgets(commd,N,stdin);
        commd[strlen(commd)-1]='\0';    //fgets函数读取的最后一个字符为换行符，此处将其替换为'\0'
        
        if((!strncmp(commd,"mkdir",5))||(!strncmp(commd,"pwd",3))||(!strncmp(commd,"cd",2))||(!strncmp(commd,"rmdir",5))||(!strncmp(commd,"ls",2))){
            user_con_commd(sockfd,commd);
        }else if((!strncmp(commd,"get",3))||(!strncmp(commd,"mget",4))){
            user_mul_get(sockfd,commd);
        }else if((!strncmp(commd,"put",3))||(!strncmp(commd,"mput",4))){
            user_mul_put(sockfd,commd);
        }else if(!strncmp(commd,"exit",4)){
            write(sockfd,"exit",N);
            exit(0);
        }else{
            printf("myftp>commd error\n");
        }
    }
    return 0;
}

void user_con_commd(int sockfd,char * commd){
    write(sockfd,commd,N);
    read(sockfd,recv_data,N);
    printf("%s\n", recv_data);
    return ;
}

void user_mul_get(int sockfd,char *commd){
    int fd;
    int file_nums=0;
    int nbytes;
    char buffer[N];
    char file_mul_name[N];
    char file_name[N];

    write(sockfd,commd,N);

    strcpy(file_mul_name,commd);
    //获得文件数量
    strtok(commd," ");
    while(strtok(NULL," "))
        file_nums++;
    strtok(file_mul_name," ");
    while(file_nums){
        strcpy(file_name,strtok(NULL," "));
        printf("down %s\n", file_name);
        read(sockfd,recv_data,N);
        //用于检测服务器端文件是否打开成功
        if(recv_data[0] =='N'){
            printf("Can't Open The File!\n");
            return ;
        }
        //open函数创建一个文件，文件地址为(commd+4)，该地址从命令行输入获取
        if((fd=open(file_name, O_WRONLY|O_CREAT|O_TRUNC, 0644)) < 0){
            printf("Open Error!\n");
            return ;
        }
        //read函数从套接字中获取N字节数据放入buffer中，返回值为读取的字节数
        nbytes=read(sockfd, buffer, N);
        while(1){
            //write函数将buffer中的内容读取出来写入fd所指向的文件，返回值为实际写入的字节数
            if(write(fd, buffer, nbytes) < 0)
                printf("Write Error!At commd_get 2");
            if(nbytes<N)
                break;
            else
                nbytes = read(sockfd,buffer,N);
        }
        write(sockfd,"Y",N);//结束的标志
        bzero(buffer,sizeof(buffer));
        bzero(recv_data,sizeof(recv_data));
        file_nums--;
        close(fd); 
    }
    return ;
}

void user_mul_put(int sockfd,char *commd){

    int file_nums=0;
    char file_name[N];
    char file_mul_name[N];

    write(sockfd, commd, N);
    read(sockfd,recv_data,N);
    if(recv_data[0]=='N'){
        printf("No Enough Rights!\n");
        return ;
    }else{
        printf("success!\n");
    }
    do{
        strcpy(file_mul_name,commd);
        //文件数
        strtok(commd," ");
        while(strtok(NULL," "))
            file_nums++;
        //去除put/mput
        strtok(file_mul_name," ");

        while(file_nums){
            int fd;
            int nbytes;
            char buffer[N];
            bzero(buffer,sizeof(buffer));

            strcpy(file_name,strtok(NULL," "));
            printf("%s\n", file_name);
            if((fd=open(file_name, O_RDONLY)) < 0){
                printf("Open Error!%s\n",file_name);
                break;
            }
            while((nbytes=read(fd, buffer, N)) > 0){
                if(write(sockfd, buffer, nbytes) < 0){
                    printf("Write Error!At commd_put 2");
                }
            }
            read(sockfd,commd,N);
            file_nums--;
            close(fd);
        }
    }while(0);

    return ;
}
//void user_file_commd(int sockfd,)

