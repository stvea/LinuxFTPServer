//server.c
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <dirent.h>
#include <sys/stat.h>
#include <fcntl.h>

#define N 256


int visitor = 0;//总浏览者
int id=0;//浏览者id
int online_visitor = 0;
char online_visitor_list[2*N][N];

char system_dir[N];

void user_mkdir(int,char *,char *);
void user_pwd(int,char *);
void user_ls(int,char *);
void user_cd(int,char *,char *);
void user_rmdir(int,char *,char *);
void user_login(int,char *,char *,int);
void user_mul_get(int,int,char *,char *);
void user_mul_put(int,int,char *,char *);
void system_del_users(char *);
void system_get_visitors();
void system_save_visitors();
void system_progress(int);
void system_del_all_files(char *);
static void system_accept(void * sock_fd);
static void system_commd();



int main(int arg, char *argv[]){
    int ser_sockfd;
    int cli_sockfd;
    int ser_len;
    int cli_len;
    int ser_port;
    struct sockaddr_in ser_addr;
    struct sockaddr_in cli_addr;
    char commd [N];
    system_get_visitors();
    getcwd(system_dir,sizeof(system_dir));

    bzero(commd,N);//将commd所指向的字符串的前N个字节置为0，包括'\0'
    bzero(&ser_addr,sizeof(ser_addr));

    ser_addr.sin_family = AF_INET;
    ser_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    ser_addr.sin_port = htons(8989);
    ser_len = sizeof(ser_addr);

    pthread_t system_thread_id;
    pthread_create(&system_thread_id,NULL,(void *)&system_commd,NULL);
    //创建socketfd
    if((ser_sockfd=socket(AF_INET, SOCK_STREAM, 0) ) < 0){
        printf("Sokcet Error!\n");
        return -1;
    }

    //将ip地址与套接字绑定
    if((bind(ser_sockfd, (struct sockaddr*)&ser_addr, ser_len)) < 0){
        printf("Bind Error!\n");
        return -1;
    }

    //服务器端监听
    if(listen(ser_sockfd, 5) < 0){
        printf("Linsten Error!\n");
        return -1;
    }
    
    bzero(&cli_addr, sizeof(cli_addr));
    ser_len = sizeof(cli_addr);
 
    while(1){
        pthread_t thread_id;

        if((cli_sockfd=accept(ser_sockfd, (struct sockaddr*)&cli_addr, &cli_len)) < 0){
            printf("Accept Error!\n");
            exit(1);
        }
        id++;
        printf("id + 1 :%d\n", id);
        visitor++;
        online_visitor++;
        if(pthread_create(&thread_id,NULL,(void *)&system_accept,(void *)&cli_sockfd)==-1){ 
            printf("pthread_create error!\n");
            exit(1);
        }        
    }
    return 0;
}
// 站点计数与用户管理功能。统计服务器站点的当前活动用户数及自运行以来的访客总数。系统管理员可在服务器软件运行终端窗口执行以下命令：
// A）  count current：当前活动用户数
// B）  count all：显示系统访客总数
// C）  list：显示当前在线的所有用户的用户名
// D）  kill username：强制删除某个用户。
// E）  quit：关闭ftp服务器软件。

static void system_commd(){
    char commd[N];
    while(1){
        bzero(commd,N);
        printf("ftpserver>");
        fgets(commd,N,stdin);
        if(!strncmp(commd,"count current",13)){
            printf("%d\n",online_visitor);
        }else if(!strncmp(commd,"count all",9)){
            printf("%d\n",visitor);
        }else if(!strncmp(commd,"list",4)){
            int i=0;
            for(;i<=id;i++){
                if(strlen(online_visitor_list[i])!=0)
                    printf("---id:%d username:%s\n", i,online_visitor_list[i]);
            }
        }else if(!strncmp(commd,"kill",4)){
            system_del_users(commd+5);
        }else if(!strncmp(commd,"quit",4)){
            system_save_visitors();
            exit(0);
            pthread_exit(NULL);
        }
    }
}
static void system_accept(void * sockfd){
    int fd = *((int *)sockfd);
    system_progress(fd);
    close(fd);
    close(sockfd);
    //printf("线程退出\n");
    pthread_exit(NULL);
}
/*
创建/删除目录（mkdir/rmdir）、显示当前路径（pwd）、
切换目录（cd）、查看当前目录下的所有文件（ls）、
上传单个/多个文件（put/mput）、下载单个/多个文件（get/mget）。
*/ 
void system_progress(int sockfd){
    char sys_commd[N];
    char recv_commod[N];
    char user_rights[N];
    char user_add[N];
    int user_id = id;
    getcwd(user_add,sizeof(user_add));
    while(1){
        while(read(sockfd, recv_commod, N)>0){
            printf("id:%d send msg for u %s\n", id,recv_commod);
            printf("ftpserver>");
            if(!strncmp(recv_commod,"login", 5)){
                user_login(sockfd,recv_commod+6,user_rights,user_id);
            }else if(!strncmp(recv_commod,"mkdir", 5)){
                if(strstr(user_rights,"W"))
                    user_mkdir(sockfd,recv_commod+6,user_add);
                else
                    write(sockfd,"No Enough Rights!",N);
            }else if(!strncmp(recv_commod,"rmdir", 5)){
                if(strstr(user_rights,"W"))
                    user_rmdir(sockfd,recv_commod+6,user_add);
                else
                    write(sockfd,"No Enough Rights!",N);
            }else if(!strncmp(recv_commod,"pwd",3)){
                user_pwd(sockfd,user_add);
            }else if(!strncmp(recv_commod,"cd",2)){
                user_cd(sockfd,recv_commod+3,user_add);
            }else if(!strncmp(recv_commod,"ls",2)){
                user_ls(sockfd,user_add);
            }else if((!strncmp(recv_commod,"get",3))||(!strncmp(recv_commod,"mget",4))){
                    int file_nums = 0;
                    char commds[N];
                    strcpy(commds,recv_commod);
                    strtok(recv_commod," ");
                    while(strtok(NULL," "))
                        file_nums++;
                    user_mul_get(sockfd,file_nums,commds,user_add);
            }else if((!strncmp(recv_commod,"put",3))||(!strncmp(recv_commod,"mput",4))){
                if(!strstr(user_rights,"W")){
                    write(sockfd,"N",N);
                }else{
                    write(sockfd,"Y",N);
                    int file_nums = 0;
                    char commds[N];
                    strcpy(commds,recv_commod);
                    strtok(recv_commod," ");
                    while(strtok(NULL," "))
                        file_nums++;
                    user_mul_put(sockfd,file_nums,commds,user_add);
                }
            }else if(!strncmp(recv_commod,"exit",4)){
                close(sockfd);
                bzero(online_visitor_list[user_id],N);
                online_visitor--;
                return ;
            }
            bzero(recv_commod,N);
        }

    }
    return ;
}
// 登陆
void user_login(int sockfd, char *user_info,char *user_rights,int user_id){
    int fd, nbytes,error=0;
    char buffer[30];
    if((fd = open("user.txt",O_RDONLY,30))<0){
        printf("Open user File Error\n");
        printf("ftpserver>");
        perror(open);
        write(sockfd,&error,N);
        return ;
    }
    while(read(fd,buffer,30)){
        //printf("%s\n", buffer);
        if(strstr(buffer,user_info)){
            char info[N],name[N];
            strcpy(info,strtok(buffer," "));
            strcpy(user_rights,strtok(NULL," "));
            printf("A new user login user_rights:%s\n",user_rights);
            printf("ftpserver>");
            strcpy(name,strtok(info,":"));
            //printf("%s\n", name);
            strcpy(online_visitor_list[user_id],name);
            //printf("%s\n", online_visitor_list[online_visitor]);
            write(sockfd,&visitor,N);
            return ;
        }
    }
    write(sockfd,&error,N);
    return ;
}
/*
创建/删除目录（mkdir/rmdir）、显示当前路径（pwd）、
切换目录（cd）、查看当前目录下的所有文件（ls）、
上传单个/多个文件（put/mput）、下载单个/多个文件（get/mget）。
*/
void user_mkdir(int sockfd,char * path_name,char * user_add){
    chdir(user_add);
    char dir[N];
    getcwd(dir, sizeof(dir));
    //将路径完整
    if(!strncmp(path_name,"/",1)){
        strcat(dir,path_name);
    }else{
        strcat(dir,"/");
        strcat(dir,path_name);
    }
    if(!(dir[sizeof(dir)-1]=='/'))
        strcat(dir,"/");
    //printf("%s\n",dir );
    int i,len=strlen(dir);
    for(i=1;i<len;i++){
        if(dir[i]=='/'){
            dir[i]=0;
            if(access(dir,NULL)!=0){//查看文件夹是否存在
                if(mkdir(dir,0755)==-1){
                    perror("mkdir error");
                    write(sockfd,"mkdir error",N);
                    return ;
                }
            }
            dir[i] = '/';
        }
    }
    write(sockfd,"mkdir success!",N);
    //printf("%s\n", dir);
    return ;
}
void user_rmdir(int sockfd,char *path_name,char * user_add){
    chdir(user_add);
    if(strlen(path_name)==0){
        write(sockfd,"Wrong path name!",N);
        return ;
    }
    char dir[N];
    getcwd(dir, sizeof(dir));
    //将路径完整
    if(!strncmp(path_name,"/",1)){
        strcat(dir,path_name);
    }else{
        strcat(dir,"/");
        strcat(dir,path_name);
    }
    system_del_all_files(dir);
    rmdir(dir);
    write(sockfd,"mkdir success!",N);
    return ;
}
void user_pwd(int sockfd,char * user_add){
    chdir(user_add);
    char dir[N];
    strcpy(dir,user_add);
    int i = 0;
    for(i=0;i<sizeof(system_dir);i++){
        if(system_dir[i]!=dir[i])
            break;
    }
    if(i==(strlen(system_dir)+1))
        write(sockfd,"/",N);
    else
        write(sockfd,dir+(i),N);
    return ;
}
void user_cd(int sockfd,char * path_name,char * user_add){
    chdir(user_add);
    char dir[N];
    strcpy(dir,system_dir);
    //将路径完整
    if(path_name[0]!='/'){
        strcat(dir,"/");
    }
    strcat(dir,path_name);
    if(path_name[strlen(path_name)-1]!='/'){
        strcat(dir,"/");
    }
    printf("%s\n", dir);
    //printf("%s\n", path_name);

    if(chdir(dir)==-1){
        perror("chdir");
        printf("%s\n", dir);
        write(sockfd,"No such dir",N);
        return;
    }
    getcwd(user_add,sizeof(user_add));
    strcpy(user_add,dir);
    printf("%s\n", user_add);
    write(sockfd,"cd success",N);
    return ;
}
//用于显示当前目录下所有文件
void user_ls(int sockfd,char * user_add){
    chdir(user_add);
    DIR *dp;
    char commd[N] ;
    char dir[N];
    struct dirent *entry;

    bzero(commd, N);
    strcpy(dir,user_add);

    if((dp=opendir(dir))==NULL){
        write(sockfd,"open dir error",N);
        return ;
    }
    while((entry = readdir(dp))!=NULL){
        strcat(commd,entry->d_name);
        strcat(commd,"    ");
    }
    write(sockfd, commd, N);
    getcwd(user_add, sizeof(user_add));
    return ;
}
//用于下载单个多个文件
void user_mul_get(int sockfd,int file_nums,char *commd,char *user_add){
    chdir(user_add);
    int fd;
    int nbytes;
    char buffer[N];
    char file_name[N];
    //printf("%s\n", commd);

    strtok(commd," ");
    while(file_nums){
        strcpy(file_name,strtok(NULL," "));
        printf("down %s\n", file_name);
        if((fd=open(file_name, O_RDONLY)) < 0){
            printf("Open file Error!\n");
            printf("ftpserver>");
            buffer[0]='N';
            write(sockfd, buffer, N);
            return ;
        }
        buffer[0] = 'Y';
        if(write(sockfd, buffer, N) <0){
            printf("Write Error! At commd_get 2!\n");
            close(fd);
            return ;
        }
        while((nbytes=read(fd, buffer, N)) > 0){
            if(write(sockfd, buffer, nbytes) < 0){
                printf("Write Error! At commd_get 3!\n");
                close(fd);
                return ;
            }
        }
        read(sockfd,buffer,N);
        close(fd);
        bzero(buffer,sizeof(buffer));
        file_nums--;
    }
    return ;
}
//用于上传文件
void user_mul_put(int sockfd,int file_nums,char *commd,char *user_add){
    chdir(user_add);

    char file_name[N];

    strtok(commd," ");
    while(file_nums){
            int fd;
            int nbytes;
            char buffer[N];
            bzero(buffer, N);

        strcpy(file_name,strtok(NULL," "));

        printf("%s\n", file_name);
        if((fd=open(file_name, O_WRONLY|O_CREAT|O_TRUNC, 0644)) < 0){
            printf("Open file Error!\n");
            return ;
        }
        nbytes=read(sockfd, buffer, N);
     
        while(1){
            if(write(fd, buffer, nbytes) < 0){
                printf("Write Error! At commd_put 1!\n");
                close(fd);
                return ;
            }
            //printf("向%s写入%s\n", file_name,buffer);
            if(nbytes<N)
                break;
            else{
                nbytes = read(sockfd,buffer,N);
                printf("%d\n", nbytes);
            }
        }
        write(sockfd,"Y",N);
        close(fd);

        file_nums--;

    }
    return ;
}
//用于删除传入目录下所有文件
void system_del_all_files(char *dir){
    DIR *dp; //声明一个句柄
    struct dirent *entry;
    struct stat statbuf;

    if((dp=opendir(dir))==NULL){
        perror("opendir");
        fprintf(stderr, "cannot open directory: %s\n", dir);
        return ;
    }
    chdir(dir);
    while((entry = readdir(dp))!=NULL){
        lstat(entry->d_name,&statbuf);
        if(S_ISDIR(statbuf.st_mode)){
            if(strcmp(".",entry->d_name)==0||strcmp("..",entry->d_name)==0){
                continue;
            }
            //printf("%*s%s/\n", depth," ",entry->d_name);
            system_del_all_files(entry->d_name);
            remove(entry->d_name);  
        }else{
            //printf("%*s%s\n",depth," ",entry->d_name );
            remove(entry->d_name);
        }
    }
    chdir("..");
    closedir(dp);
    return ;
}

void system_get_visitors(){
    int fd;
    char buffer[30];
    if((fd=open("info.txt",O_RDONLY))<0){
        //printf("Open info.txt error\n");
        //printf("ftpserver>");
        //visitor = 0;
        return ;
    }
    read(fd,buffer,30);
    visitor = atoi(buffer);
    printf("have %d\n", visitor);
    return ;
}
/**************************************************/
/*函数功能:保存本次登录的用户数量                     */
/**************************************************/
void system_save_visitors(){
    int fd;
    char buffer[30];
    bzero(buffer,30);
    if((fd=open("info.txt", O_WRONLY|O_CREAT|O_TRUNC, 0644)) < 0){
        printf("Open info.txt fail\n");
        return ;
    }
    write(fd,"asdsda",30);
    close(fd);
    return ;
}
void system_del_users(char *user_info){
    int fd,fd_back_up, nbytes,error=0;
    char buffer[30];
    if((fd = open("user.txt",O_RDWR,30))<0){
        printf("Open user File Error\n");
        printf("ftpserver>");
        perror(open);
        return ;
    }
    if((fd_back_up = open("user_back_up.txt",O_RDWR|O_CREAT,30))<0){
        printf("Open user File Error\n");
        printf("ftpserver>");
        perror(open);
        return ;
    }
        printf("ftpserver>you will kill %s", user_info);
    while(read(fd,buffer,30)){
        if(strncmp(buffer,user_info,strlen(user_info)-1)){
            write(fd_back_up,buffer,30);
        }
    }
//    ftruncate(fd, 0);

    // while(read(fd_back_up,buffer,30)){
    //     printf("%s\n", buffer);
    //     write(fd,buffer,30);
    // }
    //ftruncate(fd_back_up,0);
    close(fd);
    close(fd_back_up);

    return ;
}