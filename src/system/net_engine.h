#ifndef NET_ENGINE_H_
#define NET_ENGINE_H_


#ifdef __cplusplus
extern "C" {
#endif

int RecvFully(void * buffer,int size);
int SendFully(const void *buffer,int size);
int net_send(unsigned char mode, unsigned int data);
int net_init(int port);
void net_close();

#ifdef __cplusplus
}
#endif
#endif /* GPU_GPU_INTERFACE_H_ */
