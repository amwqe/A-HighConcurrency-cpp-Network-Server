//二维数组作为时间轮，对定时器任务进行封装
#include <functional>   //函数指针
#include <vector>
#include <unordered_map>    //哈希表
#include <memory>   //智能指针
#include <cstdint>  //C风格
#include <iostream>
#include <unistd.h> //sleep头文件

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
class Timerwheel
{
    private:
        using WeakTask = std::weak_ptr<TimerTask>;
        using PtrTask = std::shared_ptr<TimerTask>;  //时间轮使用二维数组实现
        int _tick;  //当前的秒针，走到哪里释放哪里，释放哪里，就相当于执行哪里的任务
        int _capacity;  //时间轮最大容量----最大延迟时间
        std::vector<std::vector<PtrTask>> _wheel;
        std::unordered_map<uint64_t,WeakTask>_timers;
    private:
        void RemoveTimer(uint64_t _id){
            auto it = _timers.find(_id);
            if(it!=_timers.end()){
                _timers.erase(it);
            }
        }
    public:
        Timerwheel(): _capacity(60), _tick(0){
            _wheel.resize(_capacity);
        }

        //这里传入定时器的基本信息--添加定时任务
        void TimerAdd(uint64_t _id,uint32_t delay,const TaskFunc &cb){
            PtrTask pt(new TimerTask(_id,delay,cb));
            pt->SetRelease(std::bind(&Timerwheel::RemoveTimer, this, _id));
            _timers[_id] = std::weak_ptr<TimerTask>(pt);
            int pos = (_tick+delay)%_capacity;
            _wheel[pos].push_back(pt);
        }

        //刷新定时任务
        void TimerRefresh(uint64_t _id){
            //通过保存的定时器对象weak_ptr构造一个shared_ptr出来，添加到轮子中
            auto it = _timers.find(_id);
            if(it == _timers.end()){
                return ;    //没找到定时任务，无法刷新，没法延迟
            }
            PtrTask pt = it->second.lock(); //lock获取weak_ptr管理的对象队对应的shared_ptr
            int delay = pt->DelayTime();
            int pos = (_tick+delay)%_capacity;
            _wheel[pos].push_back(pt);
        }

        //执行定时任务，每秒钟执行一次，相当于指针向后走了一步
        void RunTimerTask(){
            _tick = (_tick+1)%_capacity;
            _wheel[_tick].clear();  //清空指定位置的数组，就会把数组中保存的所有管理的定时器对象的shared_ptr释放掉
        }

        //取消定时任务
        void TimerCancel(uint64_t _id){
            auto it = _timers.find(_id);
            if(it == _timers.end()){
                return ;    //没找到定时任务，无法刷新，没法延迟
            }
            PtrTask pt = it->second.lock(); //lock获取weak_ptr管理的对象队对应的shared_ptr
            if(pt) pt->Cancel();      
        }
};

//测试类
class Test{
    public:
        Test(){std::cout<<"构造"<<std::endl;}
        ~Test(){std::cout<<"析构"<<std::endl;}
};

void DelTest(Test*t){
    delete t;
}

//测试函数
int main(){
    Timerwheel tw;
    Test *t = new Test();
    tw.TimerAdd(888,5,std::bind(DelTest,t));
    for(int i=0;i<5;i++){
        sleep(1);
        tw.TimerRefresh(888);  //刷新定时任务
        tw.RunTimerTask();  //向后移动秒针
        std::cout<<"刷新了一下定时任务,重新需要5s之后才会销毁\n";
    }
    tw.TimerCancel(888);
    while(1){
        sleep(1);
        std::cout<<"---------------------\n";
        tw.RunTimerTask();  //向后移动秒针
    }
    return 0;
}