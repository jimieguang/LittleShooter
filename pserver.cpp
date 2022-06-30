#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/shm.h>
#include <iostream>
#include <thread>
#include <pthread.h>
#include <list>
#include <sys/signal.h>
using namespace std;

#define PORT 12222
#define IP "127.0.0.1"
#define CLIENT 3        //客户端最大数目
#define SERV_PORT 12222  //服务器端口号
#define HEIGHT 20
#define WIDTH 40
#define NUMO 10         //障碍物量参数
#define v_bul 1        //子弹速度
int x[CLIENT];          //坦克方位
int y[CLIENT];
int turn[CLIENT];   	//坦克方向：0,1,2,3分别是上下左右，4代表爆炸
int map[WIDTH][HEIGHT]; //地图
int game_status=-1;     //游戏状态:-2（平局），-1（游戏未开始），0（游戏进行中），>0的整数代表胜家（且游戏结束），比如1代表以玩家1获胜结束游戏
char output[1024];
int test = 66;
int client_num = 0;
int mSocket;
struct sockaddr_in servaddr;
socklen_t len;
std::list<int> connList;//用list来存放套接字，没有限制套接字的容量就可以实现一个server跟若干个client通信
pthread_mutex_t mute;
struct s_info{
    struct sockaddr_in cliaddr;
    int connfd;
};
struct Bullets{
    int x;
    int y;
    int turn;         //0,1,2,3分别是上下左右，4代表爆炸
    bool status;    //false代表子弹不存在，true代表子弹存在，用来控制子弹数量
}bullets[CLIENT];
void map_set() {        //地图设置，0-通行，1-障碍
    int i, j;
    int obstacle[NUMO];
    int in = WIDTH / NUMO;      //间隔参数
    srand((unsigned)time(NULL));
    for (i = 0; i < NUMO; i++) {        //随机障碍
        obstacle[i] = i * in + rand() % (in / 2 + 1);
    }
    for (i = 0; i < HEIGHT; i++) {
        //随机障碍
        for (j = 0; j < NUMO && obstacle[j] < WIDTH; j++) {
            if (rand() % 10 > 7)       //障碍爆率参数
                map[obstacle[j]][i] = 1;
        }
        map[WIDTH - 1][i] = 1;      //左右边界
        map[WIDTH - 2][i] = 1;
        map[0][i] = 1;
        map[1][i] = 1;
    }
    for (i = 0; i < WIDTH; i++) {
        map[i][HEIGHT - 1] = 1;       //上下边界
        map[i][0] = 1;
    }
}

void SetupSignal() {
    struct sigaction sa;

    //在linux下写socket的程序的时候，如果尝试send到一个disconnected socket上，就会让底层抛出一个SIGPIPE信号。
    //这个信号的缺省处理方法是退出进程
    //重载这个信号的处理方法,如果接收到一个SIGPIPE信号，忽略该信号
    sa.sa_handler = SIG_IGN;
    sa.sa_flags = 0;
    //sigemptyset()用来将参数set信号集初始化并清空
    if (sigemptyset(&sa.sa_mask) == -1 ||
            sigaction(SIGPIPE, &sa, 0) == -1) {
        exit(-1);
    }
}
void hit() {            //子弹命中逻辑
    for (int i = 0; i < CLIENT; i++) {
        if(bullets[i].status == true){
        if (map[bullets[i].x][bullets[i].y] == 1) {         //子弹命中障碍
            bullets[i].status = false;
        }
        for (int j = 0; j < CLIENT; j++) {
            if(i==j){
                continue;
            }
            if (bullets[i].x == x[j] && bullets[i].y == y[j]) {         //子弹命中坦克
                bullets[i].status = false;
                bullets[i].turn = 4;   //4表示子弹为爆炸状态
                turn[j] = 4;           //同理，坦克为爆炸状态
                //victor();
                if(j < client_num)
                {
                    game_status = i + 1;
                }
            }
        }
        for (int k = i + 1; k < CLIENT; k++) {          //子弹命中子弹
            if(bullets[i].x == bullets[k].x && bullets[i].y == bullets[k].y){
                bullets[i].status = false;
                bullets[k].status = false;
                bullets[i].turn = 4;
                bullets[k].turn = 4;
            }
        }
    }
    }
}
void package(int id)
{
    int k = 0;
    output[k] = (char)id;
    k++;
    output[k] = ';';
    k++;
    for(int i = 0;i < WIDTH;i++)
    {
        for(int j = 0;j < HEIGHT;j++)
        {
            output[k] = (char)map[i][j]; 
            k++;
        }
    }
    output[k] = ';';
    k++;
    for(int i = 0; i < CLIENT;i++)
    {
        if(i < client_num)
        {
            output[k] = (char)x[i];
            k++;
        }
        else
        {
            output[k] = (char)0;
            k++;
        }
    }
    output[k] = ';';
    k++;
    for(int i = 0; i < CLIENT;i++)
    {
        if(i < client_num)
        {
            output[k] = y[i];
            k++;
        }
        else
        {
            output[k] = (char)0;
            k++;
        }
    }
    output[k] = ';';
    k++;
    for(int i = 0; i < CLIENT;i++)
    {
        if(i < client_num)
        {
            output[k] = turn[i];
            k++;
        }
        else
        {
            output[k] = 4;
            k++;
        }
    }
    output[k] = ';';
    k++;
    for(int i = 0;i < CLIENT;i++)
    {
        output[k] = (char)bullets[i].x;
        k++;
    }
    output[k] = ';';
    k++;
    for(int i = 0;i<CLIENT;i++)
    {
        output[k] = (char)bullets[i].y;
        k++;
    }
    output[k] = ';';
    k++;
    for(int i = 0;i<CLIENT;i++)
    {
        output[k] = (char)bullets[i].turn;
        k++;
    }
    output[k] = ';';
    k++;
    for(int i = 0;i<CLIENT;i++)
    {
        //output[k] = (char)bullets[i].status;
        if(bullets[i].status)output[k]='1';
        else output[k]='0';
        k++;
    }
    output[k] = ';';
    k++;
    output[k] = (char)game_status;
    k++;
    output[k] = ';';
    k++;
    output[k] = (char)test;
    k++;
} 

 
void sendMsg(int sender,char* msg,int length)
{
    std::list<int>::iterator it;
    for(it=connList.begin(); it!=connList.end(); ++it)
    {
        //给其他人发送消息
        if (sender == *it)
        {
            send(*it, msg, 1024, 0);
        }
    }
}

bool end()
{
    if(turn[0] == 4 && turn[1] == 4)
    {
        return true;
    }
    else if(turn[0] == 4 && turn[2] == 4)
    {
        return true;
    }
    else if(turn[1] == 4 && turn[2] == 4)
    {
        return true;
    }
    else 
    {
        return false;
    }
} 
void getConn()
{
    int id;
    int id_x=1;
    int id_y=1;
    int action;
    while(1)
    {
        int conn = accept(mSocket, (struct sockaddr*)&servaddr, &len);
        connList.push_back(conn);
        //printf("%d\n", conn);
        pthread_mutex_lock(&mute);
        id = client_num;
        client_num++;
        action = 3;
        srand(time(NULL));
        while(1){
            id_x = rand() % WIDTH;
            id_y = rand() % HEIGHT;
            if(map[id_x][id_y] != 1)break;
        }
        x[id] = id_x;
        y[id] = id_y;
        turn[id] = action;
        pthread_mutex_unlock(&mute);
        printf("玩家 %d 进入房间 \n",conn-3);   
            
        //同步信息
    }
}
//fd_set可以理解为一个集合，这个集合中存放的是文件描述符(file descriptor)，即文件句柄，它用一位来表示一个fd
void get_Datas()
{
    void hit();
    int n;
    int action ;
    int id;
    char input[1024];
    struct timeval tv;//该结构用于描述一段时间长度，如果在这个时间内，需要监视的描述符没有事件发生则函数返回，返回值为0
    tv.tv_sec = 0;//设置倒计时时间
    tv.tv_usec = 10;
    pthread_detach(pthread_self());
    //使子线程处于分离态，保证子线程资源可被回收
    action = 3;
    //人物移动
    void player_act(int mover,int id);
    //子弹移动
    void bullet_move();
    //初始化信息  
    int num = 0;      
    while(1)
    {
        for(int z=0;z<20000000;z++);
        //memset(buf, 0 ,sizeof(buf));
        std::list<int>::iterator it;
        for(it=connList.begin(),id = 0; it!=connList.end(); ++it,++id)
        {
            fd_set rfds;
            FD_ZERO(&rfds);//清空fd_set集合
            int maxfd = 0;//最大号
            int retval = 0;
            FD_SET(*it, &rfds);//将一个给定的文件描述符加入集合之中
            if(maxfd < *it)
            {
                maxfd = *it;
            }//记录下需要监听的最大号文件描述符
            //监视我们需要监视的文件描述符（读或写的文件集中的文件描述符）的状态变化情况。并能通过返回的值告知我们
            //select(maxfdp+1,fd_set *readfds,fd_set *writefds,fd_set *errorfds,struct timeval *timeout);
            retval = select(maxfd+1, &rfds, NULL, NULL, &tv);

            if(retval == -1)
            {
                printf("select error\n");
            }
            else if(retval == 0)//超时
            {
                // printf("not message\n");
                // pthread_mutex_lock(&mute);
                // package(id);
                // sendMsg(*it, output, strlen(output));
                // num ++;
                // pthread_mutex_unlock(&mute);
            }
            else//有可读数据
            {
                pthread_mutex_lock(&mute);
                int len = recv(*it, input, sizeof(input), 0);
                if (len > 0) {
                    player_act(input[0],id);  //人物行动
                }
                pthread_mutex_unlock(&mute);
            }
            pthread_mutex_lock(&mute);
            bullet_move();
            // num ++;
            // printf("move over\n");
            // printf("num:%d\n",num);
            //同步信息
            package(id);
            sendMsg(*it, output, strlen(output));
            memset(output, 0, sizeof(output));
            // printf("deal\n");
            pthread_mutex_unlock(&mute);
            
            
        }
    }
}
int main()
{
    SetupSignal();
    map_set();
    //new socket
    cout<<"1.创建socket"<<endl;
    mSocket = socket(AF_INET, SOCK_STREAM, 0);
    
    
    memset(&servaddr, 0, sizeof(servaddr));
    
    servaddr.sin_family = AF_INET;
    
    //设置服务器监听的端口
    servaddr.sin_port = htons(PORT);
    
    
    //如果是局域网，这里可以使用IP127.0.0.1
    //servaddr.sin_addr.s_addr = inet_addr(IP);
    //如果部署到linux服务器 这里使用htonl(INADDR_ANY)
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    
    
    cout<<"2.绑定socket和端口号"<<endl;
    if(bind(mSocket, (struct sockaddr* ) &servaddr, sizeof(servaddr))==-1)
    {
        perror("bind");
        while(end()){

        }
        exit(1);
    }
    cout<<"3.监听端口号"<<endl;
    
    if(listen(mSocket, 20) == -1)
    {
        perror("listen");
        while(end()){

        }
        exit(1);
    }
    len = sizeof(servaddr);
 
    //thread : while ==>> accpet
    std::thread t(getConn);
    t.detach();//detach的话后面的线程不同等前面的进程完成后才能进行，如果这里改为join则前面的线程无法判断结束，就会
    //一直等待，导致后面的线程无法进行就无法实现操作
    //printf("done\n");
    
    //thread : recv ==>> show
    std::thread t2(get_Datas);
    t2.detach();
    while(1)//做一个死循环使得主线程不会提前退出
    {
        sleep(1);
    }
    return 0;
}

void player_act(int mover,int id){
    int action=5;
    switch(mover)
        {
            case 'w':action=0;break;
            case 'a':action=2;break;
            case 's':action=1;break;
            case 'd':action=3;break;
            case 'j':action=4;break;
        }
        if(turn[id] == 4){
            //死者不准动
            return ;
        }
        
        {                //坦克移动逻辑
            if(action == 0){
                if (map[x[id]][y[id] - 1] == 0){
                    y[id] -= 1;      //同向移动（且没有障碍），不同向只改变方向
                }turn[id]=0;
            }
            else if (action == 1) {
                if (map[x[id]][y[id] + 1] == 0) {
                    y[id] += 1;      //同向移动（且没有障碍），不同向只改变方向
                }turn[id] = 1;
            }
            else if (action == 2) {
                if (map[x[id]-1][y[id]] == 0) {
                    x[id] -= 1;     //同向移动（且没有障碍），不同向只改变方向
                }turn[id] = 2;
            }
            else if (action == 3) {
                if (map[x[id] + 1][y[id]] == 0) {
                    x[id] += 1;     //同向移动（且没有障碍），不同向只改变方向
                }turn[id] = 3;
            }
            else if (action == 4) {        //发射子弹
                if( bullets[id].status == false ){
                    bullets[id].x = x[id];
                    bullets[id].y = y[id];
                    bullets[id].turn=turn[id];	//子弹方向调整为坦克方向
                    bullets[id].status = true;
                }
            }
        }
}

void bullet_move(){
    for (int i = 0; i < CLIENT; i++) {      //子弹运动逻辑
        if (bullets[i].status == true) {
            if (bullets[i].turn == 0) {	//确定子弹方向
                for (int j = 0; j < v_bul; j++) {	//子弹位移
                    bullets[i].y -= 1;
                    hit();      //子弹反馈判断
                }
            }
            else if (bullets[i].turn == 1) {
                for (int j = 0; j < v_bul; j++) {
                    bullets[i].y += 1;
                    hit();
                }
            }
            else if (bullets[i].turn == 2) {
                for (int j = 0; j < v_bul; j++) {
                    bullets[i].x -= 1;
                    hit();
                }
            }
            else if (bullets[i].turn == 3) {
                for (int j = 0; j < v_bul; j++) {
                    bullets[i].x += 1; 
                    hit();
                }
            }
        }
        else{
            bullets[i].x = 0;
            bullets[i].y = 0;
        }
    }
}