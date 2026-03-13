#ifndef _M_SERVER_H_
#define _M_SERVER_H_
#define BUFFER_DEFAULT_SIZE 1024  //数据超过1024就会扩容
#include<iostream>
#include<cassert>
#include<vector>
#include<cstring>
#include<ctime>
#include<string>
#include<unordered_map>
#include<sys/socket.h>  //套接字头文件 
#include<netinet/in.h>  //地址结构头文件
#include<arpa/inet.h>  //字节序转换头文件
#include<unistd.h>
#include<fcntl.h>  
#include<errno.h>
#include<thread>
#include<mutex>
#include<memory>
#include<sys/eventfd.h>
#include<sys/timerfd.h>
#include<functional>
#include<sys/epoll.h>
#include<condition_variable> 
#include<typeinfo>
#include<signal.h>

//日志宏的实现--__FILE__:表示文件名，__LINE__表示行号,format:格式,...:不定参数量配合__VA_ARGS__使用
#define INF 0
#define DBG 1
#define ERR 2
#define LOG_LEVEL INF
#define LOG(level,format,...) do{\
    if(level < LOG_LEVEL) {}\
    else {\
        time_t t = time(NULL);\
        struct tm *ltm = localtime(&t);\
        char tmp[32] = {0};\
        strftime(tmp,31, "%H:%M:%S",ltm);\
        fprintf(stdout,"[%p %s %s:%d]" format "\n",(void*)pthread_self(),tmp,__FILE__,__LINE__,##__VA_ARGS__);\
    }\
}while(0)
#define INF_LOG(format,...) LOG(INF,format,##__VA_ARGS__)
#define DBG_LOG(format,...) LOG(DBG,format,##__VA_ARGS__)
#define ERR_LOG(format,...) LOG(ERR,format,##__VA_ARGS__)


class Buffer{
    private:
        std::vector<char> _buffer;  //使用vector进行空间管理
        uint64_t _reader_idx;  //读偏移
        uint64_t _writer_idx;  //写偏移
    public:
        //构造函数
        Buffer():_reader_idx(0),_writer_idx(0),_buffer(BUFFER_DEFAULT_SIZE){}
        void *Begin(){ return &*_buffer.begin(); }
        const void *Begin() const { return &*_buffer.begin(); }
       //获取当前写入起始地址-从buffer的空间起始处,_buffer的空间起始地址+偏移量
        char* Write_pos(){return (char*)Begin()+_writer_idx;}
        //获取当前读取起始地址
        char* Read_pos(){return (char*)Begin()+_reader_idx;}
        const char* Read_pos() const {return (const char*)Begin()+_reader_idx;}
        //获取缓冲区末尾空闲空间大小-写偏移之后的空间大小,总体空间大小减去写偏移
        uint64_t TailIdleSize() const { return _buffer.size()-_writer_idx;}
        //获取后沿（缓冲区起始）空闲空间大小-读偏移之前的空闲大小,
        uint64_t HeadIdleSize() const {return _reader_idx;}
        //获取可读数据大小--写偏移-读偏移
        uint64_t ReadAbleSize() const {return _writer_idx-_reader_idx;}
        //读偏移向后移动-可读范围不能超过可写范围
        void MoveReadOffset(uint64_t len){
            if(len == 0){return;}
            //向后移动的大小必须小于等于写偏移
            assert( _reader_idx+len <= _writer_idx);
            _reader_idx += len;
        }
        //写偏移向后移动
        void MoveWriteOffset(uint64_t len){
            assert(len <= TailIdleSize()+HeadIdleSize());
            _writer_idx+=len;
        }
        //确保可写空间足够（整体空闲空间足够==数据迁移|扩容）
        void EnsureWriteSpace(uint64_t len){
            if(TailIdleSize()>=len){return ;}//末尾空间不足
            //末尾空间不够,则判断加上起始位置的空闲空间大小是否足够.足够则把数据移动到起始位置
            if(len<=TailIdleSize()+HeadIdleSize()){
                uint64_t rsz = ReadAbleSize();  //保存当前可读数据大小
                std::copy(Read_pos(),Read_pos()+rsz,(char*)Begin());  //把可读数据拷贝到起始位置
                _reader_idx = 0;  //读偏移归零
                _writer_idx = rsz;  //写位置置为可读数据大小，因为当前的可读数据大小就是写偏移量
            }
            else{  //扩容的情况-总体空间不够，不移动数据，直接扩容到足够空间就行，每次只扩容到足够的空间
                _buffer.resize(_writer_idx+len);
            }
        }
        void WriteAndPush(const void *data,uint64_t len){
            Write(data,len);
            MoveWriteOffset(len);
        }
        //写入数据-保证有足够空间，之后拷贝数据进去
        void Write(const void *data,uint64_t len){
            EnsureWriteSpace(len);
            const char *d = (const char*)data;
            std::copy(d,d+len,Write_pos());
        }
        void WriteString(const std::string &data){
            return Write(data.c_str(),data.size());
        }
        void WriteStringAndPush(const std::string &data){
            WriteString(data);
            MoveWriteOffset(data.size());
        }
        void WriteBuffer(Buffer &data){
            Write(data.Read_pos(),data.ReadAbleSize());
        }
        void WriteBufferAndPush(Buffer &data){
            WriteBuffer(data);
            MoveWriteOffset(data.ReadAbleSize());
        }
        //读取数据
        void Read(void *buf,uint64_t len){
            //获取的数据大小<可读数据大小
            assert(len<=ReadAbleSize());
            std::copy(Read_pos(),Read_pos()+len,(char*)buf);
        }
        void ReadAndPop(void *buf,uint64_t len){
            Read(buf,len);
            MoveReadOffset(len);
        }
        //缓冲区中的数据当成string读取出去
        std::string ReadAsString(uint64_t len){
            assert(len<=ReadAbleSize());
            std::string str;
            str.resize(len);
            Read(&str[0],len);  //获取空间起始地址
            return str;
        }
        std::string ReadAsStringAndPop(uint64_t len){
            assert(len<=ReadAbleSize());
            std::string str = ReadAsString(len);
            MoveReadOffset(len);
            return str;
        }
        //http中获取一行数据函数实现
        char *FindCRLF(){  //寻找换行字符
            void *res = memchr(Read_pos(),'\n',ReadAbleSize());  //memchr返回的是void*类型
            return (char*)res;
        }
        std::string GetLine(){  //获取一行数据通常针对的是明文字符串
            char *pos = FindCRLF();
            if(pos == NULL){
                return "";  //返回空字符串，这样有一个换行符
            }
            //+1是为了取出换行字符
            uint64_t len = pos - Read_pos() + 1;
            assert(len <= ReadAbleSize());
            return ReadAsString(len);
        }
        std::string GetLineAndPop(){
            std::string str = GetLine();
            MoveReadOffset(str.size());
            return str;
        }
        //清空缓冲区
        void Clear(){
            //只需要把偏移量归零就行，新数据会覆盖元数据
            _reader_idx = 0;
            _writer_idx = 0;
        }
};


//这个接口有点难度
#define MAX_LISTEN 1024
class Socket{
    private:
        int _sockfd;
    public:
        Socket():_sockfd(-1){}
        Socket(int fd):_sockfd(fd){}
        ~Socket(){Close();}
        int Fd(){return _sockfd;}
        //创建套接字
        bool Create(){
            _sockfd = socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
            if(_sockfd<0){
                ERR_LOG("创建套接字失败");
                return false;
            }
            return true;
        }
        //绑定地址信息
        bool Bind(const std::string &ip,uint16_t port){
            struct sockaddr_in addr;
            addr.sin_family = AF_INET;
            addr.sin_port = htons(port);
            addr.sin_addr.s_addr = inet_addr(ip.c_str());
            socklen_t len  = sizeof(struct sockaddr_in);
            int ret = bind(_sockfd,(struct sockaddr*)&addr,len);
            if(ret < 0){
                ERR_LOG("绑定失败");
                return false;
            }
            return true;
        }
        //开始监听
        int Listen(int backlog = MAX_LISTEN){
            int ret = listen(_sockfd,backlog);
            if(ret < 0){
                ERR_LOG("套接字监听失败");
                return false;
            }
            return true;
        }
        //向服务器发起连接 
        bool Connect(const std::string &ip,uint16_t port){
            struct sockaddr_in addr;
            addr.sin_family = AF_INET;
            addr.sin_port = htons(port);
            addr.sin_addr.s_addr = inet_addr(ip.c_str());
            socklen_t len  = sizeof(struct sockaddr_in);
            int ret = connect(_sockfd,(struct sockaddr*)&addr,len);
            if(ret < 0){
                ERR_LOG("向服务器发起连接失败");
                return false;
            }
            return true;  
        } 
        //获取新连接--返回描述符-1表示失败
        int Accept(){
            int newfd = accept(_sockfd,NULL,NULL);
            if(newfd < 0){
                ERR_LOG("获取新连接失败");
                return -1;
            }
            return newfd;
        }
        //接收数据,ssize_t表示有符号长整型,flag默认为阻塞
        ssize_t Recv(void *buf,size_t len,int flag = 0){
            ssize_t ret = recv(_sockfd,buf,len,flag);
            if(ret <= 0){
                //EAGAIN表示当前socket的接收缓冲区没有数据，只有在非阻塞的情况下才会有这个错误
                //EINTR表示当前socket的阻塞等待被信号打断了，但是没有出错。
                if(errno == EAGAIN||errno == EINTR){
                    return 0;  //表示当前没有接收到数据
                }
                ERR_LOG("套接字接收数据错误");
                return -1;
            }
            return ret;  //实际接受的数据长度
        }
        //非阻塞接受
        ssize_t NonBlockRecv(void *buf, size_t len){
            return Recv(buf, len, MSG_DONTWAIT);  //MSG_DONTWAIT表示当前接受为非阻塞
        }
        //发送数据
        ssize_t Send(const void *buf, size_t len, int flag = 0) {
            // ssize_t send(int sockfd, void *data, size_t len, int flag);
            ssize_t ret = send(_sockfd, buf, len, flag);
            if (ret < 0) {
                if (errno == EAGAIN || errno == EINTR) {
                    return 0;
                }
                ERR_LOG("SOCKET SEND FAILED!!");
                return -1;
            }
            return ret;//实际发送的数据长度
        }
        ssize_t NonBlockSend(const void *buf, size_t len){
            return Send(buf, len, MSG_DONTWAIT);  //MSG_DONTWAIT表示当前发送为非阻塞
        }

        //关闭套接字
        void Close(){
            if(_sockfd!=-1){
                close(_sockfd);
                _sockfd = -1;
            }
        }
        //创建一个服务端连接
        bool CreateServer(uint16_t port,const std::string &ip = "0.0.0.0",bool block_flag = false){
            //1.创建套接字
            //2.启动地址复用
            //3.绑定地址
            //4.开始监听
            //5.设置非阻塞
            if(Create()==false) return false;
            ReuseAddress();  //地址复用必须在绑定之前调用
            if(Bind(ip,port) == false) return false;
            if(Listen() < 0) return false;
            if(block_flag) Nonblock();  
            return true;
        }
        //创建一个客户端连接
        bool CreateClient(uint16_t port,const std::string &ip){
            //1.创建套接字
            //2.连接服务器
            if(Create()==false) return false;
            if(Connect(ip,port) == false) return false;
            return true;
        }
        //设置套接字选项---开启地址端口重用
        void ReuseAddress(){
            int val = 1;
            setsockopt(_sockfd,SOL_SOCKET,SO_REUSEADDR,(const void*)&val,sizeof(int));  //地址重用
            val = 1;
            setsockopt(_sockfd,SOL_SOCKET,SO_REUSEPORT,(const void*)&val,sizeof(int));  //端口重用

        }
        //设置套接字阻塞属性--设置为非阻塞
        void Nonblock(){
            int flag = fcntl(_sockfd,F_GETFL,0);
            fcntl(_sockfd,F_SETFL,flag | O_NONBLOCK);
        }
};



//前向声明Poller类
class Poller;
class EventLoop;
//channel模块实现-是Cnnoction模块的子模块，用于描述符事件管理
//大部分都是设置类型的函数
class Channel{
    private:
        int _fd;
        uint32_t _events;  //当前需要监控的对象
        uint32_t _revents;  //当前链接触发的事件
        EventLoop *_loop;
        using EventCallback = std::function<void()>;
        EventCallback _read_callback;  //可读事件被触发的回调函数
        EventCallback _write_callback;  //可写事件被触发的回调函数
        EventCallback _error_callback;  //错误事件被触发的回调函数
        EventCallback _close_callback;  //链接断开事件被触发的回调函数
        EventCallback _event_callback;  //任意事件被触发的回调函数
    public:
        Channel(EventLoop *loop,int fd):_fd(fd),_events(0),_revents(0),_loop(loop){}
        int Fd(){return _fd;}
        void SetRevents(uint32_t events){_revents = events;}
        uint32_t GetEvents(){return _events;}  //获取想要监控的事件
        void SetReadCallback(const EventCallback &cb){_read_callback = cb;}  //设置实际就绪的事件
        void SetWriteCallback(const EventCallback &cb){_write_callback = cb;}
        void SetErrorCallback(const EventCallback &cb){_error_callback = cb;}
        void SetCloseCallback(const EventCallback &cb){_close_callback = cb;}
        void SetEventCallback(const EventCallback &cb){_event_callback = cb;}
         
        bool ReadAble(){return (_events & EPOLLIN);}  //当前是否监控了可读
        bool WriteAble(){return (_events & EPOLLOUT);}//当前是否监控了可写
        void EnableRead(){  //启动读事件监控-挂到eventloop上去添加事件监控
            _events |= EPOLLIN;  //后边会添加到evenrloop的事件监控中
            Update();
        }
        void EnableWrite(){  //启动写事件监控
            _events |= EPOLLOUT;
            Update();
        } 
        void DisableRead(){  //解除读事件监控~0010=1101,后边会修改到evenrloop的事件监控中
            _events &= ~EPOLLIN;  
            Update();
        }
        void DisableWrite(){  //解除写事件监控
            _events &= ~EPOLLOUT;
            Update();
        }
        void DisableAll(){  //关闭所有事件监控
            _events = 0;
            Update();
        }
        //移除监控_poller->UpdateEvent(this);-声明，实现在Poller类下方
        void Remove();
        void Update();
        void HandleEvent(){  //事件处理，一旦连接触发了事件，就调用这个函数，自己触发了什么事件，自己决定该如何处理
            // 先处理错误和关闭事件，因为这些会释放连接
            if(_revents & EPOLLERR){
                if(_error_callback) _error_callback();
                return;
            }
            if(_revents & EPOLLHUP){
                if(_close_callback) _close_callback();
                return;
            }
            // 处理可读事件
            if((_revents & EPOLLIN)||(_revents & EPOLLRDHUP)||(_revents & EPOLLPRI)){
                if(_read_callback)  
                    _read_callback();  //可读事件回调
                return; // 可读事件处理后可能会释放连接，直接返回
            }
            // 处理可写事件
            if((_revents & EPOLLOUT)){
                if(_write_callback) _write_callback();
            }
            // 处理任意事件
            if(_event_callback){  //不管什么事件都要调用的回调函数-放到事件处理完毕之后调用、刷新活跃度
                _event_callback();
            }
        }
};
#define MAX_EPOLLEVENTS 1024
class Poller{
    private:
        int _epfd;  //描述符
        struct epoll_event _evs[MAX_EPOLLEVENTS];
        std::unordered_map<int,Channel*> _channels;
    private:
        //实际对epoll的直接操作
        void Update(Channel *channel,int op){
            // int epoll_ctl(int epfd, int op,  int fd,  struct epoll_event *ev);
            int fd = channel->Fd();
            struct epoll_event ev;
            ev.data.fd = fd;
            ev.events = channel->GetEvents();
            int ret = epoll_ctl(_epfd,op,fd,&ev);
            if(ret < 0){
                ERR_LOG("EPOLLCTL FAILED!");

                //abort();  //退出程序便于调试
            }
            return;
        }
        //判断一个Channel是否已经添加了事件监控
        bool HasChannel(Channel *channel){
            auto it = _channels.find(channel->Fd());
            if(it == _channels.end()){
                return false;
            }
            return true;
        }
    public:
        Poller(){
            _epfd = epoll_create(MAX_EPOLLEVENTS); 
            if(_epfd < 0){
                ERR_LOG("EPOL CREATE FAILED!");
                abort();//_epfd创建失败，程序没有运行的意义了
            }
        }
        //添加或修改监控事件
        void UpdateEvent(Channel *channel){
            bool ret = HasChannel(channel);
            if(ret == false){
                //不存在则添加
                _channels.insert(std::make_pair(channel->Fd(),channel));
                return Update(channel,EPOLL_CTL_ADD);  //添加
            }
            else{
                return Update(channel,EPOLL_CTL_MOD);  //修改
            }
        }
        //移除监控
        void RemoveEvent(Channel *channel){
            auto it = _channels.find(channel->Fd());
            if(it != _channels.end()){
                _channels.erase(it);
            }
            Update(channel,EPOLL_CTL_DEL);
        }       
        //开始监控返回活跃链接
        void Poll(std::vector<Channel*> *active){
            // int epoll_wait(int epfd, struct epoll_event *evs, int maxevents, int timeout)
            int nfds = epoll_wait(_epfd,_evs,MAX_EPOLLEVENTS,-1); //阻塞监控
            if(nfds < 0){
                if(errno == EINTR)
                    return ;
                ERR_LOG("EPOLL WAIT ERROR:%s\n",strerror(errno));
                abort(); //直接退出程序
            }
            for(int i=0;i<nfds;i++){  //小于活跃事件数量
                auto it = _channels.find(_evs[i].data.fd);
                if(it == _channels.end()){
                    // 跳过已经被删除的Channel
                    continue;
                }
                it->second->SetRevents(_evs[i].events);  //设置it实际就绪的事件
                active->push_back(it->second);
            }
            return ;
        }
};

using TaskFunc = std::function<void()>; 
using ReleaseFunc = std::function<void()>;
class TimerTask
{
public:
    //初始化列表：定时器任对象id，延迟时间对象，定时任务
    TimerTask(uint64_t _id,uint32_t delay,const TaskFunc &cb):_id(_id),_timeout(delay),_task_cb(cb),_canceled(false){}
    //在析构中执行定时认任务
    ~TimerTask(){
        if(_canceled == false) _task_cb();
        _release();
    }
    void Cancel(){_canceled = true;}
    void SetRelease(const ReleaseFunc &cb){_release = cb;}
    uint32_t DelayTime(){return _timeout;}
private:
    uint64_t _id;   //定时器任对象id
    uint32_t _timeout;  //定时任务的超时时间
    bool _canceled; //false-表示没有被取消，true表示被取消了
    TaskFunc _task_cb;   //定时器对象要执行的定时任务
    ReleaseFunc _release;   //用于删除timerwheel中保存的定时器对象信息
};
//时间轮类
class Timerwheel{
    private:
        using WeakTask = std::weak_ptr<TimerTask>;
        using PtrTask = std::shared_ptr<TimerTask>;  //时间轮使用二维数组实现
        int _tick;  //当前的秒针，走到哪里释放哪里，释放哪里，就相当于执行哪里的任务
        int _capacity;  //时间轮最大容量----最大延迟时间
        std::vector<std::vector<PtrTask>> _wheel;  //存放所有的定时任务
        std::unordered_map<uint64_t,WeakTask>_timers;  //保存了所有的定时器信息

        EventLoop *_loop;
        int _timerfd;  //定时器描述符-
        std::unique_ptr<Channel> _timer_channel;

    private:
        void RemoveTimer(uint64_t _id){
            auto it = _timers.find(_id);
            if(it!=_timers.end()){
                _timers.erase(it);
            }
        }
        int CreateTimerfd(){
            int timerfd = timerfd_create(CLOCK_MONOTONIC,0);
            if(timerfd < 0){
                ERR_LOG("TIMERFD CREATE FAILED!");
                abort();
            }
            struct itimerspec itime;
            itime.it_value.tv_sec = 1;
            itime.it_value.tv_nsec = 0; //第一次超时时间为1s后
            itime.it_interval.tv_sec = 1;
            itime.it_interval.tv_nsec = 0;  //第一次超时后，每次超时的间隔时间
            timerfd_settime(timerfd,0,&itime,NULL);
            return timerfd;
        }
        void ReadTimerfd(){
            uint64_t times;
            int ret = read(_timerfd,&times,sizeof(times));
            if(ret < 0){
                ERR_LOG("READ TIMEFD FAILED!");
                abort();
            }
            return ;
        }
        //执行定时任务，每秒钟执行一次，相当于指针向后走了一步
        void RunTimerTask(){
            _tick = (_tick+1)%_capacity;
            if(_tick < _wheel.size()){
                _wheel[_tick].clear();  //清空指定位置的数组，就会把数组中保存的所有管理的定时器对象的shared_ptr释放掉
            }
        }
        void Ontime(){
            ReadTimerfd();
            RunTimerTask();
        }
        //这里传入定时器的基本信息--添加定时任务--任何一个线程都有可能添加定时任务
        void TimerAddInLoop(uint64_t _id,uint32_t delay,const TaskFunc &cb){
            PtrTask pt(new TimerTask(_id,delay,cb));
            pt->SetRelease(std::bind(&Timerwheel::RemoveTimer, this, _id));
            _timers[_id] = std::weak_ptr<TimerTask>(pt);
            int pos = (_tick+delay)%_capacity;
            _wheel[pos].push_back(pt);
        }
        //刷新定时任务
        void TimerRefreshInLoop(uint64_t _id){
            //通过保存的定时器对象weak_ptr构造一个shared_ptr出来，添加到轮子中
            auto it = _timers.find(_id);
            if(it == _timers.end()){
                return ;    //没找到定时任务，无法刷新，没法延迟
            }
            PtrTask pt = it->second.lock(); //lock获取weak_ptr管理的对象队对应的shared_ptr
            if(!pt){
                return ;    //定时器对象已经被释放，无法刷新
            }
            int delay = pt->DelayTime();
            int pos = (_tick+delay)%_capacity;
            _wheel[pos].push_back(pt);
        }
        //取消定时任务
        void TimerCancelInLoop(uint64_t _id){
            auto it = _timers.find(_id);
            if(it == _timers.end()){
                return ;    //没找到定时任务，无法刷新，没法延迟
            }
            PtrTask pt = it->second.lock(); //lock获取weak_ptr管理的对象队对应的shared_ptr
            if(pt) pt->Cancel();      
        }
    public:
        Timerwheel(EventLoop *loop)
            :_capacity(60)
            , _tick(0)
            ,_loop(loop)
            ,_timerfd(CreateTimerfd())
            ,_wheel(_capacity)
            ,_timer_channel(new Channel(_loop, _timerfd)){
           _timer_channel.reset(new Channel(loop, _timerfd));  //-------------------------
            _timer_channel->SetReadCallback(std::bind(&Timerwheel::Ontime,this));
            _timer_channel->EnableRead();  //启动可读事件监控
        }
        //因为很多定时任务都涉及到对连接的操作，需要考虑线程安全
        /*定时器中有个_timers成员，定时器信息的操作有可能在多线程中进行，因此需要考虑线程安全问题
        如果不想加锁，那就把对定期的所有操作，都放到一个线程中进行*/
        //定时任务的添加
        void TimerAdd(uint64_t id,uint32_t delay,const TaskFunc &cb);
        //刷新/延迟定时任务
        void TimerRefresh(uint64_t id);
        //定时任务的取消
        void TimerCancel(uint64_t id);
        //这个接口也存在线程安全问题，这个接口在调用的时候就不能在其他线程中调用，只能在EventLoop模块的线程内调用
        bool HasTimer(uint64_t id){
            auto it = _timers.find(id);
            if(it == _timers.end()){
                return false;    //没找到定时任务，无法刷新，没法延迟
            }
            return true;
        }
};
//EventLoop类
class EventLoop{
    private:
        using Functor = std::function<void()>;
        std::thread::id _thread_id; // 线程id
        int _event_fd;  //eventfd唤醒IO事件监控有可能导致的阻塞
        std::unique_ptr<Channel> _event_channel;  //使用智能指针进行管理，ptr释放的时候连带event一同释放
        Poller _poller;  //进行所有描述符的事件监控
        std::vector<Functor> _tasks;  //任务池
        std::mutex _mutex;  //实现任务池操作的线程安全
        Timerwheel _timer_wheel; //定时器模块
    public:
        //执行任务池中的所有任务-需要加锁保护起来
        void RunAllTask(){
            std::vector<Functor> functor;
            //把任务池中的任务交换出来，同时为任务池枷锁
            {
                std::unique_lock<std::mutex> _lock(_mutex);
                _tasks.swap(functor);
            }//声明周期一旦结束锁会自动解开
            for(auto &f:functor){
                f(); //一一执行任务池中的任务
            }
            return ;
        }
        static int CreateEventFd(){
            int efd = eventfd(0, EFD_CLOEXEC|EFD_NONBLOCK);
            if(efd<0){
                ERR_LOG("CREATE EVENTFD FAILED!");
                abort(); //程序异常退出
            }
            return efd;
        }
        void ReadEventfd(){
            uint64_t res = 0;
            int ret = read(_event_fd,&res,sizeof(res));
            if(ret < 0){
                if(errno == EINTR||errno == EAGAIN){ //EINTR表示被信号打断，EAGAIN表示无数据可读，都可以原谅
                    return ;
                }
                ERR_LOG("READ EVENTFD FAILED!"); //表示读取 eventfd 失败，不可原谅
                abort();
            }
            return ;
        }
        void WeakUpEventFd(){
            uint64_t val = 1;
            int ret = write(_event_fd,&val,sizeof(val));
            if(ret < 0){
                if(errno == EINTR){ //表示被中断的系统调用，可以原谅
                    return ;
                }
                ERR_LOG("READ EVENTFD FAILED!"); //表示读取 eventfd 失败，不可原谅
                abort();
            }
            return ;
        }
    public:
        EventLoop()
            :_thread_id(std::this_thread::get_id())
            ,_event_fd(CreateEventFd())
            ,_event_channel(new Channel(this,_event_fd))
            ,_timer_wheel(this){
                //给eventfd添加可读事件回调函数，读取eventfd时间通知次数
                _event_channel->SetReadCallback(std::bind(&EventLoop::ReadEventfd,this));
                //启动eventfd的读事件监控
                _event_channel->EnableRead();
            }
        //三步走：事件监控->就绪事件处理->执行任务
        void Start(){
            while(1){
                //1.事件监控
                std::vector<Channel*> actives;  //用于接受活跃连接
                _poller.Poll(&actives);
                //2.事件处理
                for(auto &s:actives){
                    s->HandleEvent();
                }
                //3.执行任务
                RunAllTask();
            }
        }
        //判断当前操作是否是EventLoop对应的线程
        bool IsInLoop(){
            return (_thread_id == std::this_thread::get_id());
        }
        void AssertInLoop(){
            assert(_thread_id == std::this_thread::get_id());
        }
        //判断要执行的任务是否处于当前线程中，如果是则执行，不是的话则压入队列
        void RunInLoop(const Functor &cb){
            if(IsInLoop()){
                return cb();
            }
            return QueueInLoop(cb);
        }
        //将操作压入任务池
        void QueueInLoop(const Functor &cb){
            {
                std::unique_lock<std::mutex> _lock(_mutex);
                _tasks.push_back(cb);
            }
            //唤醒有可能因为没有事件就绪，而导致的epoll阻塞；
            //其实就是给eventfd写入一个数据，eventfd就会触发可读事件
            WeakUpEventFd();
        }
        //定时器的操作
        //添加或修改描述符的事件监控
        void UpdateEvent(Channel *channel){return _poller.UpdateEvent(channel);}
        //移除描述符的事件监控
        void RemoveEvent(Channel *channel){  return _poller.RemoveEvent(channel);}
        //添加一个定时器任务
        void TimerAdd(uint64_t id,uint32_t delay,const TaskFunc &cb){return _timer_wheel.TimerAdd(id,delay,cb);}
        //刷新定时任务
        void TimerRefresh(uint64_t id){return _timer_wheel.TimerRefresh(id);}
        //取消定时任务
        void TimerCancle(uint64_t id){return _timer_wheel.TimerCancel(id);}
        //是否存在某个定时任务
        bool TimerHas(uint64_t id){return _timer_wheel.HasTimer(id);}
}; 
class LoopThread {
    private:
        //用于实现_loop获取的同步关系，避免线程创建了，但是_loop还没有实例化之前去获取_loop
        std::mutex _mutex;     //互斥锁
        std::condition_variable _cond;  //条件变量用于实现线程间的等待-通知机制
        EventLoop *_loop;       // EventLoop指针变量，这个对象需要在线程内实例化
        std::thread _thread;    //由thread创建的EventLoop对应的线程
    private:
        //实例化 EventLoop 对象，唤醒_cond上有可能阻塞的线程，并且开始运行EventLoop模块的功能
        void ThreadEntry() { //线程入口函数
            EventLoop loop;  //实例化对象
            {//加锁 
                std::unique_lock<std::mutex> lock(_mutex);//加锁
                _loop = &loop;
                _cond.notify_all();  //唤醒有可能阻塞的线程
            }
            loop.Start();
        }
    public:
        //创建线程，设定线程入口函数（传入口函数就行）
        LoopThread():_loop(NULL), _thread(std::thread(&LoopThread::ThreadEntry, this)) {}
        //返回当前线程关联的EventLoop对象指针
        EventLoop *GetLoop() {
            EventLoop *loop = NULL;
            {
                std::unique_lock<std::mutex> lock(_mutex);//加锁
                _cond.wait(lock, [&](){ return _loop != NULL; });//loop为NULL就一直阻塞，第二个参数是函数返回值的bool值
                loop = _loop;
            }
            return loop;
        }
};
//线程池类，用于管理多个线程，每个线程都关联一个EventLoop对象
class LoopThreadPool {
    private:
        int _thread_count;  //线程池中的线程数量
        int _next_idx;      //下一个要分配的线程索引-进行R轮转
        EventLoop *_baseloop;  //基础的EventLoop对象指针，用于处理非线程池中的事件
        std::vector<LoopThread*> _threads;  //线程池中的线程对象指针数组
        std::vector<EventLoop *> _loops;  //线程池中的EventLoop对象指针数组 
    public:
        LoopThreadPool(EventLoop *baseloop):_thread_count(0), _next_idx(0), _baseloop(baseloop) {}
        void SetThreadCount(int count) { _thread_count = count; }
        void Create() {
            if (_thread_count > 0) {
                _threads.resize(_thread_count); //
                _loops.resize(_thread_count);
                for (int i = 0; i < _thread_count; i++) {
                    _threads[i] = new LoopThread();
                    _loops[i] = _threads[i]->GetLoop();
                }
            }
            return ;
        }
        EventLoop *NextLoop() {
            if (_thread_count == 0) { //如果线程池中的线程数量为0，直接返回基础的EventLoop对象指针
                return _baseloop;
            }
            _next_idx = (_next_idx + 1) % _thread_count; //进行R轮转逐个去遍历
            return _loops[_next_idx];
        }
};
class Any{
    private:
        class holder{
            public:
                virtual ~holder(){}
                //获取数据类型
                virtual const std::type_info& type() =0;
                virtual holder*clone() = 0;
        };
        template<class T>
        class placeholder:public holder{
            public:
                //初始化
                placeholder(const T&val):_val(val){}
                //析构函数
                virtual ~placeholder(){}
                //获取子类对象保存的数据类型
                virtual const std::type_info& type() {return typeid(T);}
                //针对当前对象自身，克隆出一个新的子类对象
                virtual holder*clone(){return new placeholder(_val);}
            public:
                T _val;
        };
        holder*_content;

        //通用容器类的功能实现
    public:
        //三个构造函数
        //空构造
        Any():_content(NULL){}
        //通用构造
        template<class T>
        Any(const T &_val):_content(new placeholder<T>(_val)){}
        //针对其他容器构造自己的容器
        Any(const Any &other):_content(other._content ? other._content->clone() : NULL){}
        //析构函数
        ~Any(){delete _content;}
        //
        Any& swap(Any &other){
            std::swap(_content,other._content);
            return *this;
        }
        template<class T>
        T *get(){   //返回子类对象保存的数据的指针 
            //获取的数据类型必须和保存的数据类型一致 
            assert(typeid(T) == _content->type());
            if(typeid(T)!=_content->type()) 
                return NULL;
            return &((placeholder<T>*)_content)->_val;
        }
        template <class T>
        Any& operator=(const T &val){   //赋值运算符重载
            //为val构造一个临时的通用容器，然后与当前容器自身进行指针交换，临时对象释放的时候，原先保存的数据也会被释放掉
            Any(val).swap(*this);
            return *this;
        }
        Any& operator=(const Any &other){
            Any(other).swap(*this);
            return *this;
        }
};

//Connection模块的实现 -- 烧脑
//DISCONECTED -- 连接关闭状态；   CONNECTING -- 连接建立成功-待处理状态
//CONNECTED -- 连接建立完成，各种设置已完成，可以通信的状态；  DISCONNECTING -- 待关闭状态
typedef enum { DISCONNECTED, CONNECTING, CONNECTED, DISCONNECTING}ConnStatu;
class Connection;  //前向声明
using PtrConnection = std::shared_ptr<Connection>;  //智能指针
class Connection:public std::enable_shared_from_this<Connection>{  //模板类
    private:
        uint64_t _conn_id; //链接的唯一id，便于连接的管理和查找
        //uint64_t _timer_id;   //定时器ID，必须是唯一的，这块为了简化操作使用conn_id作为定时器ID
        int _sockfd;        // 连接关联的文件描述符
        bool _enable_inactive_release; //连接是否启动非活跃销毁的判断标志，默认为false
        EventLoop *_loop;   // 连接所关联的一个EventLoop,COnnection关联到EventLoop,EventLoop关联到一个线程中，确保线程安全
        ConnStatu _statu;   // 连接状态，不同的状态有不同的处理方式
        Socket _socket;     // 套接字操作管理
        Channel _channel;   // 连接的事件管理
        Buffer _in_buffer;  // 输入缓冲区---存放从socket中读取到的数据
        Buffer _out_buffer; // 输出缓冲区---存放要发送给对端的数据
        Any _context;       // 请求的接收处理上下文

        //这四个回调函数，是让服务器模块来设置的（其实服务器模块的处理回调也是组件使用者设置的）
        //换句话说，这几个回调都是组件使用者使用的
        using ConnectedCallback = std::function<void(const PtrConnection&)>;
        using MessageCallback = std::function<void(const PtrConnection&, Buffer *)>;
        using ClosedCallback = std::function<void(const PtrConnection&)>;
        using AnyEventCallback = std::function<void(const PtrConnection&)>;
        ConnectedCallback _connected_callback;
        MessageCallback _message_callback;
        ClosedCallback _closed_callback;
        AnyEventCallback _event_callback;
        /*组件内的连接关闭回调--组件内设置的，因为服务器组件内会把所有的连接管理起来，一旦某个连接要关闭*/
        /*就应该从管理的地方移除掉自己的信息*/
        ClosedCallback _server_closed_callback;
    private:
        /*五个channel的事件回调函数*/
        //描述符可读事件触发后调用的函数，接收socket数据放到接收缓冲区中，然后调用_message_callback
        void HandleRead() {
            //1. 接收socket的数据，放到缓冲区
            char buf[65536];
            ssize_t ret = _socket.NonBlockRecv(buf, 65535);  //非阻塞接受数据
            if (ret < 0) {
                //出错了,不能直接关闭连接--看一下发送给缓冲区有没数据待发送，接收缓冲区有没有数据要处理
                return ShutdownInLoop();
            } 
            //这里的等于0表示的是没有读取到数据，而并不是连接断开了，连接断开返回的是-1
            //将数据放入输入缓冲区,写入之后顺便将写偏移向后移动
            _in_buffer.WriteAndPush(buf, ret);
            //2. 调用message_callback进行业务处理
            if (_in_buffer.ReadAbleSize() > 0) {
                //shared_from_this--从当前对象自身获取自身的shared_ptr管理对象
                return _message_callback(shared_from_this(), &_in_buffer);  //传入输入缓冲区
            }
        }
        //描述符可写事件触发后调用的函数，将发送缓冲区中的数据进行发送
        void HandleWrite() {
            //_out_buffer中保存的数据就是要发送的数据
            ssize_t ret = _socket.NonBlockSend(_out_buffer.Read_pos(), _out_buffer.ReadAbleSize());
            if (ret < 0) {
                //发送错误就该关闭连接了，
                if (_in_buffer.ReadAbleSize() > 0) {
                    _message_callback(shared_from_this(), &_in_buffer);
                }
                return Release();//这时候就是实际的关闭释放操作了。
            }
            _out_buffer.MoveReadOffset(ret);//千万不要忘了，将读偏移向后移动
            if (_out_buffer.ReadAbleSize() == 0) {
                _channel.DisableWrite();// 没有数据待发送了，关闭写事件监控
                //如果当前是连接待关闭状态，则有数据，发送完数据释放连接，没有数据则直接释放
                if (_statu == DISCONNECTING) {
                    return Release();
                }
            }
            return;
        }
        //描述符触发挂断事件
        void HandleClose() {
            /*一旦连接挂断了，套接字就什么都干不了了，因此有数据待处理就处理一下，完毕关闭连接*/
            if (_in_buffer.ReadAbleSize() > 0) {
                _message_callback(shared_from_this(), &_in_buffer);
            }
            return Release();
        }
        //描述符触发出错事件
        void HandleError() {
            return HandleClose();
        }
        //描述符触发任意事件: 1. 刷新连接的活跃度--延迟定时销毁任务；  2. 调用组件使用者的任意事件回调
        void HandleEvent() {
            if (_enable_inactive_release == true)  {  _loop->TimerRefresh(_conn_id); }
            if (_event_callback)  {  _event_callback(shared_from_this()); }
        }
        //连接获取之后，所处的状态下要进行各种设置（给channel设置事件回调,启动读监控）
        void EstablishedInLoop() {
            // 1. 修改连接状态；  2. 启动读事件监控；  3. 调用回调函数
            assert(_statu == CONNECTING);//当前的状态必须一定是上层的半连接状态
            _statu = CONNECTED;//当前函数执行完毕，则连接进入已完成连接状态
            // 一旦启动读事件监控就有可能会立即触发读事件，如果这时候启动了非活跃连接销毁
            _channel.EnableRead();
            if (_connected_callback) _connected_callback(shared_from_this());
        }
        //这个接口才是实际的释放接口
        void ReleaseInLoop() {
            //1. 修改连接状态，将其置为DISCONNECTED
            _statu = DISCONNECTED;
            //2. 移除连接的事件监控
            _channel.Remove();
            //3. 关闭描述符
            _socket.Close();
            //4. 如果当前定时器队列中还有定时销毁任务，则取消任务
            if (_loop->TimerHas(_conn_id)) CancelInactiveReleaseInLoop();
            //5. 调用关闭回调函数，避免先移除服务器管理的连接信息导致Connection被释放，再去处理会出错，因此先调用用户的回调函数
            if (_closed_callback) _closed_callback(shared_from_this());
            //移除服务器内部管理的连接信息
            if (_server_closed_callback) _server_closed_callback(shared_from_this());
        }
        //这个接口并不是实际的发送接口，而只是把数据放到了发送缓冲区，启动了可写事件监控
        void SendInLoop(Buffer &buf) {
            if (_statu == DISCONNECTED) return ;
            _out_buffer.WriteBufferAndPush(buf);
            if (_channel.WriteAble() == false) {
                _channel.EnableWrite();
            }
        }
        //这个关闭操作并非实际的连接释放操作，需要判断还有没有数据待处理，待发送
        void ShutdownInLoop() {
            _statu = DISCONNECTING;// 设置连接为半关闭状态
            if (_in_buffer.ReadAbleSize() > 0) {
                if (_message_callback) _message_callback(shared_from_this(), &_in_buffer);
            }
            //要么就是写入数据的时候出错关闭，要么就是没有待发送数据，直接关闭
            if (_out_buffer.ReadAbleSize() > 0) {
                if (_channel.WriteAble() == false) {
                    _channel.EnableWrite();
                }
            }
            if (_out_buffer.ReadAbleSize() == 0) {
                Release();
            }
        }
        //启动非活跃连接超时释放规则
        void EnableInactiveReleaseInLoop(int sec) {
            //1. 将判断标志 _enable_inactive_release 置为true
            _enable_inactive_release = true;
            //2. 如果当前定时销毁任务已经存在，那就刷新延迟一下即可
            if (_loop->TimerHas(_conn_id)) {
                return _loop->TimerRefresh(_conn_id);
            }
            //3. 如果不存在定时销毁任务，则新增
            _loop->TimerAdd(_conn_id, sec, std::bind(&Connection::Release, this));
        }
        void CancelInactiveReleaseInLoop() {
            _enable_inactive_release = false;
            if (_loop->TimerHas(_conn_id)) { 
                _loop->TimerCancle(_conn_id); 
            }
        }
        void UpgradeInLoop(const Any &context, 
                    const ConnectedCallback &conn, 
                    const MessageCallback &msg, 
                    const ClosedCallback &closed, 
                    const AnyEventCallback &event) {
            _context = context; 
            _connected_callback = conn;
            _message_callback = msg;
            _closed_callback = closed;
            _event_callback = event;
        }
    public:
        Connection(EventLoop *loop, uint64_t conn_id, int sockfd):_conn_id(conn_id), _sockfd(sockfd),
            _enable_inactive_release(false), _loop(loop), _statu(CONNECTING), _socket(_sockfd),
            _channel(loop, _sockfd) {
            _channel.SetCloseCallback(std::bind(&Connection::HandleClose, this));
            _channel.SetEventCallback(std::bind(&Connection::HandleEvent, this));
            _channel.SetReadCallback(std::bind(&Connection::HandleRead, this));
            _channel.SetWriteCallback(std::bind(&Connection::HandleWrite, this));
            _channel.SetErrorCallback(std::bind(&Connection::HandleError, this));
        }
        ~Connection() { DBG_LOG("RELEASE CONNECTION:%p", this); }
        //获取管理的文件描述符
        int Fd() { return _sockfd; }
        //获取连接ID
        int Id() { return _conn_id; }
        //是否处于CONNECTED状态--连接完成的状态
        bool Connected() { return (_statu == CONNECTED); }
        //设置上下文--连接建立完成时进行调用
        void SetContext(const Any &context) { _context = context; }
        //获取上下文，返回的是指针
        Any *GetContext() { return &_context; }
        void SetConnectedCallback(const ConnectedCallback&cb) { _connected_callback = cb; }
        void SetMessageCallback(const MessageCallback&cb) { _message_callback = cb; }
        void SetClosedCallback(const ClosedCallback&cb) { _closed_callback = cb; }
        void SetAnyEventCallback(const AnyEventCallback&cb) { _event_callback = cb; }
        void SetSrvClosedCallback(const ClosedCallback&cb) { _server_closed_callback = cb; }
        //连接建立就绪后，进行channel回调设置，启动读监控，调用_connected_callback
        void Established() {
            _loop->RunInLoop(std::bind(&Connection::EstablishedInLoop, this));
        }
        //发送数据，将数据放到发送缓冲区，启动写事件监控    
        void Send(const char *data, size_t len) {
            //外界传入的data，可能是个临时的空间，我们现在只是把发送操作压入了任务池，有可能并没有被立即执行
            //因此有可能执行的时候，data指向的空间有可能已经被释放了。
            Buffer buf;
            buf.WriteAndPush(data, len);
            _loop->RunInLoop(std::bind(&Connection::SendInLoop, this, std::move(buf)));
        }
        //提供给组件使用者的关闭接口--并不实际关闭，需要判断有没有数据待处理
        void Shutdown() {
            _loop->RunInLoop(std::bind(&Connection::ShutdownInLoop, this));
        }
        void Release() {
            _loop->QueueInLoop(std::bind(&Connection::ReleaseInLoop, this));
        }
        //启动非活跃销毁，并定义多长时间无通信就是非活跃，添加定时任务
        void EnableInactiveRelease(int sec) {
            _loop->RunInLoop(std::bind(&Connection::EnableInactiveReleaseInLoop, this, sec));
        }
        //取消非活跃销毁
        void CancelInactiveRelease() {
            _loop->RunInLoop(std::bind(&Connection::CancelInactiveReleaseInLoop, this));
        }
        //切换协议---重置上下文以及阶段性回调处理函数 -- 而是这个接口必须在EventLoop线程中立即执行
        //防备新的事件触发后，处理的时候，切换任务还没有被执行--会导致数据使用原协议处理了。
        void Upgrade(const Any &context, const ConnectedCallback &conn, const MessageCallback &msg, 
                     const ClosedCallback &closed, const AnyEventCallback &event) {
            _loop->AssertInLoop();
            _loop->RunInLoop(std::bind(&Connection::UpgradeInLoop, this, context, conn, msg, closed, event));
        }
};


class Acceptor {
    private:
        Socket _socket;//用于创建监听套接字
        EventLoop *_loop; //用于对监听套接字进行事件监控
        Channel _channel; //用于对监听套接字进行事件管理
        using AcceptCallback = std::function<void(int)>; //获取新连接之后的回调
        AcceptCallback _accept_callback;
    private:
        /*监听套接字的读事件回调处理函数---获取新连接，调用_accept_callback函数进行新连接处理*/
        void HandleRead() {
            int newfd = _socket.Accept();
            if (newfd < 0) {
                return ;
            }
            if (_accept_callback) _accept_callback(newfd);
        }
        int CreateServer(int port) { //接口用于创建新连接
            bool ret = _socket.CreateServer(port);
            assert(ret == true);
            return _socket.Fd();
        }
    public:
        /*不能将启动读事件监控，放到构造函数中，必须在设置回调函数后，再去启动*/
        /*否则有可能造成启动监控后，立即有事件，处理的时候，回调函数还没设置：新连接得不到处理，且资源泄漏*/
        Acceptor(EventLoop *loop, int port): _socket(CreateServer(port)), _loop(loop), 
            _channel(loop, _socket.Fd()) {
            _channel.SetReadCallback(std::bind(&Acceptor::HandleRead, this));
        }
        void SetAcceptCallback(const AcceptCallback &cb) { _accept_callback = cb; }
        void Listen() { _channel.EnableRead(); }
};
class TcpServer {
    private:
        uint64_t _next_id;      //这是一个自动增长的连接ID，
        int _port;
        int _timeout;           //这是非活跃连接的统计时间---多长时间无通信就是非活跃连接
        bool _enable_inactive_release;//是否启动了非活跃连接超时销毁的判断标志
        EventLoop _baseloop;    //这是主线程的EventLoop对象，负责监听事件的处理
        Acceptor _acceptor;    //这是监听套接字的管理对象
        LoopThreadPool _pool;   //这是从属EventLoop线程池
        std::unordered_map<uint64_t, PtrConnection> _conns;//保存管理所有连接对应的shared_ptr对象

        using ConnectedCallback = std::function<void(const PtrConnection&)>;
        using MessageCallback = std::function<void(const PtrConnection&, Buffer *)>;
        using ClosedCallback = std::function<void(const PtrConnection&)>;
        using AnyEventCallback = std::function<void(const PtrConnection&)>;
        using Functor = std::function<void()>;
        ConnectedCallback _connected_callback;
        MessageCallback _message_callback;
        ClosedCallback _closed_callback;
        AnyEventCallback _event_callback;
    private:
        void RunAfterInLoop(const Functor &task, int delay) {
            _next_id++;
            _baseloop.TimerAdd(_next_id, delay, task);
        }
        //为新连接构造一个Connection进行管理
        void NewConnection(int fd) {
            _next_id++;
            PtrConnection conn(new Connection(_pool.NextLoop(), _next_id, fd));
            conn->SetMessageCallback(_message_callback);
            conn->SetClosedCallback(_closed_callback);
            conn->SetConnectedCallback(_connected_callback);
            conn->SetAnyEventCallback(_event_callback);
            conn->SetSrvClosedCallback(std::bind(&TcpServer::RemoveConnection, this, std::placeholders::_1));
            if (_enable_inactive_release) conn->EnableInactiveRelease(_timeout);//启动非活跃超时销毁
            conn->Established();//就绪初始化
            _conns.insert(std::make_pair(_next_id, conn));
        }
        void RemoveConnectionInLoop(const PtrConnection &conn) {
            int id = conn->Id();
            auto it = _conns.find(id);
            if (it != _conns.end()) {
                _conns.erase(it);
            }
        }
        //从管理Connection的_conns中移除连接信息--真正释放连接
        void RemoveConnection(const PtrConnection &conn) {
            _baseloop.RunInLoop(std::bind(&TcpServer::RemoveConnectionInLoop, this, conn));
        }
    public:
        TcpServer(int port):
            _port(port), 
            _next_id(0), 
            _enable_inactive_release(false), 
            _acceptor(&_baseloop, port),
            _pool(&_baseloop) {
            _acceptor.SetAcceptCallback(std::bind(&TcpServer::NewConnection, this, std::placeholders::_1));
            _acceptor.Listen();//将监听套接字挂到baseloop上
        }
        void SetThreadCount(int count) { return _pool.SetThreadCount(count); }
        void SetConnectedCallback(const ConnectedCallback&cb) { _connected_callback = cb; }
        void SetMessageCallback(const MessageCallback&cb) { _message_callback = cb; }
        void SetClosedCallback(const ClosedCallback&cb) { _closed_callback = cb; }
        void SetAnyEventCallback(const AnyEventCallback&cb) { _event_callback = cb; }
        void EnableInactiveRelease(int timeout) { _timeout = timeout; _enable_inactive_release = true; }
        //用于添加一个定时任务
        void RunAfter(const Functor &task, int delay) {
            _baseloop.RunInLoop(std::bind(&TcpServer::RunAfterInLoop, this, task, delay));
        }
        //启动服务器--创建线程池中的线程
        void Start() { _pool.Create();  _baseloop.Start(); }
};
void Channel::Remove(){_loop->RemoveEvent(this);}
void Channel::Update(){_loop->UpdateEvent(this);}
// --- 实现 Timerwheel 的公共接口 ---
void Timerwheel::TimerAdd(uint64_t id, uint32_t delay, const TaskFunc &cb) {
    // 保证对定时器的操作都在 EventLoop 线程中执行，避免多线程竞争 _timers 成员
    _loop->RunInLoop(std::bind(&Timerwheel::TimerAddInLoop, this, id, delay, cb));
}
void Timerwheel::TimerRefresh(uint64_t id) {
    _loop->RunInLoop(std::bind(&Timerwheel::TimerRefreshInLoop, this, id));
}
void Timerwheel::TimerCancel(uint64_t id) {
    _loop->RunInLoop(std::bind(&Timerwheel::TimerCancelInLoop, this, id));
}
// 忽略SIGPIPE信号--防止服务器因为客户端关闭连接而导致的异常退出
class NetWork {
    public:
        NetWork() {
            DBG_LOG("SIGPIPE INIT");
            signal(SIGPIPE, SIG_IGN);
        }
};
static NetWork nw;
#endif
