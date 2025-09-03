#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <time.h>
#include <pthread.h>
#include "vivado_common.h"
#include "a818_error.h"
#include "net_engine.h"
#include "fifo_engine.h"

#define NET_ANALYSIS
#define NET_RECV_BUF_SIZE_MAX (5*1024*1024)

#pragma pack(1)
typedef struct{
    u_int16_t   cmd_head;
    u_int32_t   cmd_length;
    u_int8_t    cmd_type;
}packeg_head_t;
#pragma pack()
static unsigned char local_buffer[NET_RECV_BUF_SIZE_MAX];
static pthread_t pht_net;
static int sockfd,client_fd = -1;
static int netrecv_interrupted = 0;

static int Send(const void *buffer,int size){
	return send(client_fd, buffer, size, 0);
}
static int Recv(void * buffer,int size){
	return recv(client_fd, buffer, size, 0);
}

int RecvFully(void * buffer,int size){
    int recv = 0;
    while( recv < size ) {
        int ret = Recv((char *)buffer+recv,size-recv);
        if(ret<=0) {
            return -1;
        }
        recv += ret;
    }
    return 0;
}
int SendFully(const void *buffer,int size){
    const char * p = (const char *)buffer;
    int send = 0;
    while( send < size ) {
        int ret = Send(p,size-send);
        if(ret<0) {
            return -1;
        }
        p+=ret;
        send += ret;
    }
    return 0;
}

int net_send(unsigned char mode, unsigned int data)
{
#if 1
    int ret;
    unsigned int send_size;
    uartlist_send_info_t send_info;

    send_info.head = htons(0xec91);
    send_info.len = htonl(2);
    send_info.mode = mode;
    send_info.data = data;
    send_info.sum = check_sum((unsigned char*)&send_info.mode, sizeof(send_info.data)+1);

    // 发送响应到客户端
    printf("net_send!!!!!\n");
    ret = SendFully(&send_info,sizeof(send_info));
#endif
    return ret;
}

void* net_recv(void *arg)
{
    int ret,count = 0;
    struct timeval timev,tvl;
    timev.tv_sec = 600;
    timev.tv_usec = 0;
    tvl.tv_sec = 1;
    tvl.tv_usec = 0;
    u_int32_t cmd_val;
    u_int8_t* frame_buffer = local_buffer+6;

    while (!netrecv_interrupted) {
        // 接受连接
        if(client_fd<=0)
        {
            client_fd = accept(sockfd, (struct sockaddr *)NULL, NULL);
            if (client_fd < 0) {
                printf("accept err\n");
                break;
            }
            printf("client accept\n");
            ret = setsockopt(client_fd, SOL_SOCKET, SO_RCVTIMEO, (void*)&timev, sizeof(timev));
            if(ret != 0)
            {
                perror("setsockopt SO_RCVTIMEO");
                close(client_fd);
                client_fd = -1;
                break;
            }
            ret = setsockopt(client_fd, SOL_SOCKET, SO_SNDTIMEO, (void*)&timev, sizeof(timev));
            if(ret != 0)
            {
                perror("setsockopt SO_SNDTIMEO");
                close(client_fd);
                client_fd = -1;
                break;
            }

            // 启用TCP保活机制
            int keepalive = 1;
            if (setsockopt(client_fd, SOL_SOCKET, SO_KEEPALIVE, &keepalive, sizeof(keepalive)) < 0) {
                perror("setsockopt SO_KEEPALIVE failed");
                break;
            }

            // 设置多久后开始发送保活包
            int keepidle = 3;
            if (setsockopt(client_fd, IPPROTO_TCP, TCP_KEEPIDLE, &keepidle, sizeof(keepidle)) < 0) {
                perror("setsockopt TCP_KEEPIDLE failed");
                break;
            }

            // 设置保活包发送间隔
            int keepintvl = 3;
            if (setsockopt(client_fd, IPPROTO_TCP, TCP_KEEPINTVL, &keepintvl, sizeof(keepintvl)) < 0) {
                perror("setsockopt TCP_KEEPINTVL failed");
                break;
            }

            // 设置最大探测次数
            int keepcnt = 4;
            if (setsockopt(client_fd, IPPROTO_TCP, TCP_KEEPCNT, &keepcnt, sizeof(keepcnt)) < 0) {
                perror("setsockopt TCP_KEEPCNT failed");
                break;
            }
        }
 
        // 接收客户端消息
        ret = RecvFully(local_buffer, 7);
        if(!ret)
        {
            u_int16_t cmd_head = ntohs(*((u_int16_t*)local_buffer));
            u_int32_t cmd_length = ntohl(*((u_int32_t*)(local_buffer+2)));
            u_int8_t cmd_type = *(local_buffer+6);
            printf("head:%x,length:%d,type:%d\n",cmd_head,cmd_length,cmd_type);
            if( cmd_head == 0xec91)
            {
                ret = RecvFully(local_buffer+sizeof(packeg_head_t), cmd_length);
                if(!ret)
                {
                    // for(int i = 0;i<cmd_length;i++)
                    // {
                    //     printf("%x ",frame_buffer[i]);
                    // }
                    // printf("\n");
                    axififo_frame_data_analyse(frame_buffer, cmd_length,1);
                }
                else
                {
                    printf("client lost while recv data!!!\n");
                    cmd_val = RECV_PAK_ERROR;
                    if(client_fd){
                        shutdown(client_fd,SHUT_RDWR);close(client_fd);client_fd=-1;
                    }
                }
            }
            else
            {
                printf("cmd_head err!!!\n");
                cmd_val = RECV_PAK_ERROR;
                net_send(frame_buffer[0], cmd_val);
            }
        }
        else
        {
            printf("client lost while recv head !!!\n");
            cmd_val = RECV_PAK_ERROR;
            if(client_fd){
                shutdown(client_fd,SHUT_RDWR);close(client_fd);client_fd=-1;
            }
        }
    }
}

void net_close()
{
    // netrecv_interrupted = 1;
    pthread_cancel(pht_net);
    pthread_join(pht_net, NULL);
    // 关闭监听套接字
    shutdown(client_fd,SHUT_RDWR);close(client_fd);client_fd=-1;
    shutdown(sockfd,SHUT_RDWR);close(sockfd);sockfd=-1;
}

int net_init(int port) {
    sockfd,client_fd = -1;
    netrecv_interrupted = 0;
    struct sockaddr_in servaddr;
    int ret = 1;
 
    // 创建套接字
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("socket creation failed");
        return -1;
    }
 
    // 定义服务器地址和端口
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY); // 监听本地所有接口
    servaddr.sin_port = htons(port); // 监听9009端口
 
    int opt = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))){
        perror("setsockopt");
        return -1;
    }

    // 绑定套接字到地址和端口
    if ((bind(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr))) < 0) {
        perror("bind failed");
        return -1;
    }
 
    // 监听连接请求，最多可以有5个等待的连接
    if ((listen(sockfd, 1)) < 0) {
        perror("listen");
        return -1;
    }

    ret = pthread_create(&pht_net,NULL,net_recv,NULL);
 
    return 0;
}