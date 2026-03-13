#include<stdio.h>
#include<stdint.h>
#include<fcntl.h>
#include<sys/eventfd.h>
#include<unistd.h>


//在这里看不到有什么事件通知
int main(){
    int efd = eventfd(0, EFD_CLOEXEC|EFD_NONBLOCK);
    if(efd<0){
        perror("eventfd failed!");
        return -1;
    }
    uint64_t val = 1;
    //只能以8字节为单位
    write(efd,&val,sizeof(val));
    write(efd,&val,sizeof(val));
    write(efd,&val,sizeof(val));
    write(efd,&val,sizeof(val));
    write(efd,&val,sizeof(val));
    write(efd,&val,sizeof(val));
    write(efd,&val,sizeof(val));
    write(efd,&val,sizeof(val));

    uint64_t res = 0;
    read(efd,&res,sizeof(res));
    printf("%ld\n",res);
    close(efd);
    return 0;
}