//通用容器的实现
//也可以使用C++提供的，也可以使用C语言提供的
//我的项目是使用的是C++17中提供的any
//下面的实现参考了C++17中文文档

#include<iostream>
#include<typeinfo>
#include<cassert>
#include<unistd.h>
#include <any>
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




//测试
class test{
    public:
        test(){std::cout<<"构造"<<std::endl;}
        test(const test &t){std::cout<<"拷贝构造"<<std::endl;}
        ~test(){std::cout<<"析构"<<std::endl;}
};

int main()
{
    // Any a,b;
    // a = 10;
    // int *pa = a.get<int>();
    // std::cout<<*pa<<std::endl;
    // b = std::string("nihao");
    // std::string *ps = b.get<std::string>();
    // std::cout<<*ps<<std::endl;

    // //检查是否有内存泄漏
    // Any c;
    // {//花括号相当于声明了一个作用域
    //     test t;
    //     c = t;
    // }
    // while(1){
    //     sleep(1);
    // }


    //C++17的any使用
    std::any a;
    a = 10;
    int *p = std::any_cast<int>(&a);
    std::cout<<*p<<std::endl;
    std::any b;
    b=std::string("hello");
    std::string *ps = std::any_cast<std::string>(&b);
    std::cout<<*ps<<std::endl;
    return 0;
}