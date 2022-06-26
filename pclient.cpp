#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>             //加锁、清除键盘缓冲、延时、不清除buf
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/shm.h>
#include <sys/stat.h>

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <termio.h>

#define RESET_CURSOR()printf("\033[H")

#define MYPORT  6665
#define BUFFER_SIZE 1024

#define MAXLINE 1024    //缓冲数组大小
#define CLIENT 3    //客户端最大数目
#define MAXB 20   //子弹最大数目
#define MAXE 100   //事件最大数目
#define SERV_PORT 6665  //服务器端口号
#define HEIGHT 20
#define WIDTH 40
#define NUMO 2         //障碍物量参数
#define v_bul 5        //子弹速度

//UI函数
void ShowMenu();
void Quit();
void Waitting();
void clean(int );
void judge(int );
void Fail();
void Victory();

int client_num = 0;
pthread_mutex_t mute;
int game_status;
int x[CLIENT];  //坦克方位
int y[CLIENT];
int turn[CLIENT];
int map[WIDTH][HEIGHT];
struct Bullets{
    int x;
    int y;
    int turn;       //方向
    bool status;    //false代表子弹不存在，true代表子弹存在
}bullets[CLIENT];
int id;
void depackage(char buf[])
{
    int k = 0;
    id = (int)buf[k];
    k++;//";"
    k++;
    for(int i = 0;i < WIDTH;i++)
    {
        for(int j = 0;j < HEIGHT;j++)
        {
            map[i][j]=(int)buf[k]; 
            k++;
        }
    }
    //output[k] = ';';
    k++;
    for(int i = 0; i < CLIENT;i++)
    {
        if(buf[k]!='0')
        {
            x[i]=(int)buf[k];
            k++;
        }
        else
        {
            x[i]=0;
            k++;
        }
    }
    //output[k] = ';';
    k++;
    for(int i = 0; i < CLIENT;i++)
    {
        if(buf[k]!='0')
        {
            y[i]=(int)buf[k];
            k++;
        }
        else
        {
            y[i]=0;
            k++;
        }
    }
    //output[k] = ';';
    k++;
    for(int i = 0; i < CLIENT;i++)
    {
        turn[i]=(int)buf[k];
        k++;
    }
    //output[k] = ';';
    k++;
    for(int i = 0;i < CLIENT;i++)
    {
        bullets[i].x=(int)buf[k];
        k++;
    }
    //output[k] = ';';
    k++;
    for(int i = 0;i<CLIENT;i++)
    {
        bullets[i].y=(int)buf[k];
        k++;
    }
    //output[k] = ';';
    k++;
    for(int i = 0;i<CLIENT;i++)
    {
        bullets[i].turn=(int)buf[k];
        k++;
    }
    //output[k] = ';';
    k++;
    for(int i = 0;i<CLIENT;i++)
    {
        //output[k] = (char)bullets[i].status;
        if(buf[k]=='1')bullets[i].status=true;
        else bullets[i].status=false;
        k++;
    }
    //output[k] = ';';
    k++;
    game_status = (int)buf[k];
    k++;
}


int main(int argc, char *argv[])
 {
    system("stty -icanon");
    ShowMenu();//调用菜单
    char buf[MAXLINE];
    int sockfd;
    fd_set rfds;
    struct timeval tv;
    int retval, maxfd;
    ///定义sockfd
    sockfd = socket(AF_INET,SOCK_STREAM, 0);
    ///定义sockaddr_in
    struct sockaddr_in servaddr;
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(MYPORT);  ///服务器端口
    servaddr.sin_addr.s_addr = inet_addr("127.0.0.1");  ///服务器ip
    
    //连接服务器，成功返回0，错误返回-1
    //服务端IP地址和端口号
    //p表达(presentation):ASCII字符串       n数值(numeric):存放到套接字地址结构的二进制值
    //将 点分十进制的ip地址格式string to 用于网络传输的数值格式
 	//inet_pton(AF_INET, "127.0.0.1", &servaddr.sin_addr);//环回地址(lookback),为了本地测试连接

    //服务端：127.0.0.1.6666  server和client的地址不能一样，但只有一台电脑只能本机测试,就会把本机作为server
    //但要给一个不同于本机的ip，这就是127.0.0.1；把本机作为服务器，同时也把本机作为客户机
     if (connect(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0)
    {
        perror("connect");
        exit(1);
    }
    // 渲染所需数据格式
    //人物位置数组，第三个元素表示朝向（上下左右炸：01234）
    int location_palyer1[3];
    int location_palyer2[3];
    //子弹位置数组，第三个元素表示朝向（上下左右炸：01234）
    int location_bullet1[3];
    int location_bullet2[3];
    void trans(int *x,int *y,int *turn,struct Bullets *bullets,int* location_palyer1, int* location_palyer2, int* location_bullet1, int* location_bullet2);
    void show(int (*pre_map)[HEIGHT],int weight,int height, int* location_palyer1, int* location_palyer2, int* location_bullet1, int* location_bullet2);

    while(1){
        /*把可读文件描述符的集合清空*/
        FD_ZERO(&rfds);
        /*把标准输入的文件描述符加入到集合中*/
        FD_SET(0, &rfds);
        maxfd = 0;
        /*把当前连接的文件描述符加入到集合中*/
        FD_SET(sockfd, &rfds);
        /*找出文件描述符集合中最大的文件描述符*/
        if(maxfd < sockfd)
            maxfd = sockfd;
        /*设置超时时间*/
        tv.tv_sec = 1;
        tv.tv_usec = 0;
        /*等待聊天*/
        retval = select(maxfd+1, &rfds, NULL, NULL, &tv);
        if(retval == -1){
            printf("select出错,客户端程序退出\n");
            break;
        }else if(retval == 0){
            continue;
        }
        else{
            /*服务器发来了消息*/
            if(FD_ISSET(sockfd,&rfds)){
                char recvbuf[BUFFER_SIZE];
                int len;
                
                len = recv(sockfd, recvbuf, sizeof(recvbuf),0);
                if(len > 0)
                {
                    depackage(recvbuf);
                    printf("\033[2J");
                    printf("receive:\n");
			        for(int i=0;i<CLIENT;i++)printf("players %d:(%d,%d),turn:%d\n",i,x[i],y[i],turn[i]);
                    trans(x,y,turn,bullets,location_palyer1,location_palyer2,location_bullet1,location_bullet2);
                    show(map,WIDTH,HEIGHT,location_palyer1,location_palyer2,location_bullet1,location_bullet2);
                }
                
                memset(recvbuf, 0, sizeof(recvbuf));
            }
            /*用户输入信息了,开始处理信息并发送*/
            if(FD_ISSET(0, &rfds)){
                char sendbuf[BUFFER_SIZE];
                // printf("\noperation:");
                pthread_mutex_lock(&mute);
                // fgets(sendbuf, sizeof(sendbuf), stdin);
                sendbuf[0] = getchar();
                setbuf(stdin, NULL);                    // 清空缓冲区
                send(sockfd, sendbuf, strlen(sendbuf),0); //发送
                memset(sendbuf, 0, sizeof(sendbuf));
                pthread_mutex_unlock(&mute);
		        // printf("send success!\n");
			    //Write(STDOUT_FILENO, buf, n);
            }
        }
    }

 	close(sockfd);
 	return 0;
}

 //map中{-1：爆炸，0：无，1：障碍，2-10：玩家1及子弹1信息(红色），22-30：玩家2及子弹2信息（绿色）}
void show(int (*pre_map)[HEIGHT],int weight,int height, int* location_palyer1, int* location_palyer2, int* location_bullet1, int* location_bullet2){
    int i, j;
    //复制map数组
    int map[WIDTH][HEIGHT] = {0};

    //在map数组中添加元素信息
    // 1.玩家一的位置信息（2-6：▲▼@<>）
    i = location_palyer1[0];
    j = location_palyer1[1];
    switch(location_palyer1[2]){
        case 0:
            map[i][j] = 2;
            break;
        case 1:
            map[i][j] = 3;
            break;
        case 2:
            map[i][j] = 4; //head @
            map[i-1][j] = 5;
            break;
        case 3:
            map[i][j] = 4;
            map[i+1][j] = 6;
            break;
        case 4:
            map[i][j] = -1;
            break;
    }
    // 2.子弹一的位置信息（7-10:⇡⇣⇠⇢）
    i = location_bullet1[0];
    j = location_bullet1[1];
    switch(location_bullet1[2]){
        case 0:
            map[i][j] = 7;
            break;
        case 1:
            map[i][j] = 8;
            break;
        case 2:
            map[i][j] = 9;
            break;
        case 3:
            map[i][j] = 10;
            break;
        case 4:
            map[i][j] = -1;
            break;
    }
    // 3.玩家二的位置信息
    i = location_palyer2[0];
    j = location_palyer2[1];
    switch(location_palyer2[2]){
        case 0:
            map[i][j] = 22;
            break;
        case 1:
            map[i][j] = 23;
            break;
        case 2:
            map[i][j] = 24; //head @
            map[i-1][j] = 25;
            break;
        case 3:
            map[i][j] = 24;
            map[i+1][j] = 26;
            break;
        case 4:
            map[i][j] = -1;
            break;
    }
    // 4.子弹2的位置信息
    i = location_bullet2[0];
    j = location_bullet2[1];
    switch(location_bullet2[2]){
        case 0:
            map[i][j] = 27;
            break;
        case 1:
            map[i][j] = 28;
            break;
        case 2:
            map[i][j] = 29;
            break;
        case 3:
            map[i][j] = 30;
            break;
        case 4:
            map[i][j] = -1;
            break;
    }
    // 最后渲染初始地图
    for (j = 0; j < height; j++){
        for (i = 0; i < weight; i++){
            if(pre_map[i][j]==1)
                map[i][j] = pre_map[i][j];
        }
    }
    // 渲染修改后的map数组
    for (j = 0; j < height; j++){
        for (i = 0; i < weight; i++){ 
            switch(map[i][j]){
                case -1:
                    printf("\033[0;47;30m☯\033[0m");  //后面的\033[0m指令是为了让终端恢复默认值
                    break;
                case 0:
                    printf("\033[0;47;37m \033[0m");  //后面的\033[0m指令是为了让终端恢复默认值
                    break;
                case 1:
                    printf("█");
                    break;
                case 2:
                    printf("\033[0;47;31m▲\033[0m");
                    break;
                case 3:
                    printf("\033[0;47;31m▼\033[0m");
                    break;
                case 4:
                    printf("\033[0;47;31m@\033[0m");
                    break;
                case 5:
                    printf("\033[0;47;31m<\033[0m");
                    break;
                case 6:
                    printf("\033[0;47;31m>\033[0m");
                    break;
                case 7:
                    printf("\033[0;47;31m⇡\033[0m");
                    break;
                case 8:
                    printf("\033[0;47;31m⇣\033[0m");
                    break;
                case 9:
                    printf("\033[0;47;31m⇠\033[0m");
                    break;
                case 10:
                    printf("\033[0;47;31m⇢\033[0m");
                    break;  
                case 22:
                    printf("\033[0;47;32m▲\033[0m");
                    break;
                case 23:
                    printf("\033[0;47;32m▼\033[0m");
                    break;
                case 24:
                    printf("\033[0;47;32m@\033[0m");
                    break;
                case 25:
                    printf("\033[0;47;32m<\033[0m");
                    break;
                case 26:
                    printf("\033[0;47;32m>\033[0m");
                    break;
                case 27:
                    printf("\033[0;47;32m⇡\033[0m");
                    break;
                case 28:
                    printf("\033[0;47;32m⇣\033[0m");
                    break;
                case 29:
                    printf("\033[0;47;32m⇠\033[0m");
                    break;
                case 30:
                    printf("\033[0;47;32m⇢\033[0m");
                    break;
            }
        }
        printf("\n");
    }
                    if(game_status > 0 && game_status == id + 1)
                    {
                        printf("You win!\n");
                        exit(1);
                    }
                    else if(game_status > 0 && game_status != id + 1)
                    {
                        printf("You lose!\n");
                        printf("%d %d\n",id,game_status);
                        exit(1);
                    }
}

//将服务端传来的数据格式转化为渲染所需格式
void trans(int *x,int *y,int *turn,struct Bullets *bullets,int* location_palyer1, int* location_palyer2, int* location_bullet1, int* location_bullet2){
    location_palyer1[0] = x[0];
    location_palyer1[1] = y[0];
    location_palyer1[2] = turn[0];

    location_palyer2[0] = x[1];
    location_palyer2[1] = y[1];
    location_palyer2[2] = turn[1];

    //子弹不存在则坐标归零
    for(int i=0;i<2;i++){
        if(bullets[i].status==false && bullets[i].turn!=4){
            bullets[i].x = 0;
            bullets[i].y = 0;
        }
    }
    location_bullet1[0] = bullets[0].x;
    location_bullet1[1] = bullets[0].y;
    location_bullet1[2] = bullets[0].turn;

    location_bullet2[0] = bullets[1].x;
    location_bullet2[1] = bullets[1].y;
    location_bullet2[2] = bullets[1].turn;
}
void ShowMenu()
{
    //system("cls");
    printf ("\33[2J");//清屏
    printf("\33[?25l");//隐藏光标
    //system("color 7D");
    printf("\33[1;0H");//移动到（0，0）
    char user_order;
    char line_1[] = "                                  #################################\n";
    char title[] = "                                  ###########小小抢战##############\n";
    char start[] = "                                  #          开始游戏             #\n";
    char over[] = "                                  #          退出游戏             #\n";
    char order[] = "                                             按J确定             ";
    printf("%s",line_1);
    printf("%s",title);
    printf("%s",start);
    printf("%s",over);
    printf("%s",order);
    printf("\33[3;31H");//回到第一行，打印箭头
    printf("→");
    int n = 2;
    //clean(0);
    while(1)
    {
        int tmp = n;
        printf("\33[6;31H");//来到命令区
        user_order = getchar();
        if (user_order == 'w')
        {
            tmp = n - 1;
            if (tmp < 2)
            {
                tmp = 2;
            }
            clean(n);//清理上一个箭头
            for(int i = 5 - tmp;i>0;i--)//把光标移动到需要打印箭头的位置
            {
                printf("\33[1A");
            }
            printf("→");
            printf("\33[6;31H");//返回命令区
            clean(5);//清理命令区
            n = tmp;//改变当前箭头位置的坐标         
        }
        if (user_order == 's')
        {
            tmp = n + 1;
            if(tmp>=3){
                tmp=3;
            }
            clean(n);
            for (int i = 5-tmp;i>0;i--)//把光标移动到需要打印箭头的位置
            {
                printf("\33[1A");
            }
            printf("→");
            printf("\33[6;31H");//返回命令区
            clean(5);//清理命令区
            n = tmp;//改变当前箭头位置的坐标
        }
        if(user_order == 'j')
        {
            if(n==2){
                printf ("\33[2J");
                printf("\33[5;1H");//返回命令区
                break;
            }
            Quit();
        }  
    }
}
void Quit()
{
    printf("\33[1;0H");//移动到（0，0）
    printf ("\33[2J");
    int i;
    printf("                                  ###########已退出游戏############\n");
    exit(1);
}
void clean(int y)
{
    for (int i = 5-y;i>0;i--)
    {
        printf("\33[1A");
    }
    printf("\r");
    printf("                                 ");
    printf("\33[6;31H");
}