#include<iostream>
#include<functional>
#include<string>
#include<vector>
using namespace std;

void print(const string& str,int num)
{
    cout<<str<<num<<endl;
}
int main()
{
    using Task = std::function<void()>;
    std::vector<Task> tasks;//任务队列

    tasks.push_back(std::bind(print,"hello",10));//为任务池中添加任务
    tasks.push_back(std::bind(print,"hello",20));
    tasks.push_back(std::bind(print,"hello",30));

    for(auto& task:tasks)
    {
        task();//执行任务
    }

    //print("hello!");
    auto func=bind(print,"hellonihao",std::placeholders::_1);   //通过参数绑定预留参数
    // func(10,11);
    // func(20,22);
    func(100);
    return 0;
}