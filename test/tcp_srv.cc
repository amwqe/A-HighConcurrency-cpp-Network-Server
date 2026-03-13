#include"../source/server.hpp"
EventLoop loop;


// void HandleClose(Channel *channel){
//     //std::cout<<"close: "<<channel->Fd()<<std::endl;
//     DBG_LOG("close fd:%d",channel->Fd()); 
//     channel->Remove();  //移除监控
//     delete channel;
// }
// void HandleWrite(Channel *channel){
//     int fd =  channel->Fd();
//     const char *data = "你好吗,神人？";
//     int ret = send(fd,data,strlen(data),0);
//     if(ret<0){return HandleClose(channel);}
//     channel->DisableWrite();  //关闭写监控
// }
//关闭释放
//void HandleError(Channel *channel){HandleClose(channel);  }
// void HandleEvent(EventLoop *loop,Channel *channel,uint64_t timerid){
//     loop->TimerRefresh(timerid);
// }

// 管理所有的连接
std::unordered_map<uint64_t,PtrConnection> _conns;
uint64_t _conn_id = 0;
EventLoop base_loop;
// std::vector<LoopThread> thread(2);
int next_loop = 0;
LoopThreadPool *loop_pool;

void ConnectionDestroy(const PtrConnection &conn){
    _conns.erase(conn->Id());
}
void OnConnected(const PtrConnection &conn){
    DBG_LOG("NEW CONNECTION:%p",conn.get()); //打印出来地址看一看
}
void OnMessage(const PtrConnection &conn,Buffer *buff){
    DBG_LOG("%s",buff->Read_pos());
    buff->MoveReadOffset(buff->ReadAbleSize());
    std::string str = "Hello World!";
    conn->Send(str.c_str(),str.size());
    //conn->Shutdown();  //一次通信直接释放
}
//Channel *lst_channel:监听套接字
void Accept(EventLoop *loop,Channel *lst_channel){
    int fd = lst_channel->Fd();
    int newfd = accept(fd,NULL,NULL);
    if(newfd<0){return ;}
    
    _conn_id++;
    PtrConnection conn (new Connection(loop,_conn_id,newfd));
    conn->SetMessageCallback(std::bind(OnMessage,std::placeholders::_1,std::placeholders::_2));  
    conn->SetSrvClosedCallback(std::bind(ConnectionDestroy,conn)); 
    conn->SetConnectedCallback(std::bind(OnConnected,conn));  
    conn->EnableInactiveRelease(10);  //启动非活跃超时销毁
    conn->Established(); //就绪初始化
    _conns.insert(std::make_pair(_conn_id,conn));
    //Connection写完了之后我们就不是直接对Channel进行操作了
    //uint64_t timerid = rand() % 10000;
    // Channel *channel = new Channel(loop,newfd);
    // channel->SetReadCallback(std::bind(HandleRead,channel));  //为通信套接字设置可读事件的回调函数
    // channel->SetWriteCallback(std::bind(HandleWrite,channel));  //可写
    // channel->SetCloseCallback(std::bind(HandleClose,channel));  //关闭
    // channel->SetErrorCallback(std::bind(HandleError,channel));  //错误
    // channel->SetEventCallback(std::bind(HandleEvent,loop,channel,timerid));  //任意----这样写会报错
    //显式指定static_cast要绑定的函数
    //channel->SetEventCallback(std::bind(static_cast<void(*)(EventLoop*, Channel*, uint64_t)>(HandleEvent),loop,channel,timerid));  //任意
    //channel->EnableRead();
    //非活跃连接的超时释放操作,10s后关闭连接
    //注意：定时销毁任务必须在启动读事件之前，因为可能启动了事件监控之后，立即就有了事件，但这时候还没有任务
    //loop->TimerAdd(timerid,10,std::bind(HandleClose,channel)); 
}


void NewConnection(int fd){
    _conn_id++;
    next_loop = (next_loop +1 )%2;
    PtrConnection conn (new Connection(loop_pool->NextLoop(),_conn_id,fd));
    conn->SetMessageCallback(std::bind(OnMessage,std::placeholders::_1,std::placeholders::_2));  
    conn->SetSrvClosedCallback(std::bind(ConnectionDestroy,conn)); 
    conn->SetConnectedCallback(std::bind(OnConnected,conn));  
    conn->EnableInactiveRelease(10);  //启动非活跃超时销毁
    conn->Established(); //就绪初始化
    _conns.insert(std::make_pair(_conn_id,conn));
    DBG_LOG("NEW----------------------");
}

int main(){
    //srand(time(NULL));
    loop_pool = new LoopThreadPool(&base_loop);
    loop_pool->SetThreadCount(2);
    loop_pool->Create();
    Acceptor acceptor(&base_loop,8500); //主线程
    acceptor.SetAcceptCallback(std::bind(NewConnection,std::placeholders::_1));
    acceptor.Listen();
    //Socket lst_sok;
    //bool ret = lst_sok.CreateServer(8500);
    //为监听套接字创建一个Channel进行事件的管理以及事件的处理
    //Channel channel(&loop,lst_sok.Fd());
    // channel.SetReadCallback(std::bind(Acceptor,&loop,&channel));  //回调中获取新连接，为新连接创建Channel
    // channel.EnableRead();  //启动可读事件监控
    // while(1){
    //     int newfd = lst_sok.Accept();
    //     if(newfd < 0) continue;
    //     Socket cli_sock(newfd);
    //     char buff[1024] = {0};
    //     int ret = cli_sock.Recv(buff,1023);
    //     if(ret < 0){
    //         cli_sock.Close(); 
    //         continue;
    //     }
    //     cli_sock.Send(buff,ret);
    //     cli_sock.Close();
    // }
    while(1){
        base_loop.Start();
    }
    //lst_sok.Close();
    
    return 0;
}