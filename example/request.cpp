#include<iostream>
#include<string>
#include<regex>

int main(){
    //HTTP请求行格式： GET/xxxxxx/login?user=xiaoming&pass=123 HTTP/1.1\r\n
    std::string str = "get /baidu/login?user=xiaoming&pass=123 HTTP/1.1\r\n";
    std::smatch matchs;
    //请求方法的匹配，GET，HEAD,POST,PUT,DELETE
    //.匹配并提取除了反斜杠之外的所有的，
    //[^?]*  [^?]匹配非问号字符，*表示一次或多次4
    //(HTTP/1\\.[01]) 匹配表示以HTTP/1开始，后面有一个0或1的字符串
    //(?:\n|\r\n)?  (?.....)表示匹配但是不提取，最后的问号表示匹配前面的表达式0次或1次
    std::regex e("(GET|HEAD|POST|PUT|DELETE) ([^?]*)(?:\\?(.*))? (HTTP/1\\.[01])(?:\n|\r\n)?",std::regex::icase);  //std::regex::icase忽略大小写
    bool ret = std::regex_match(str,matchs,e);
    if(ret == false){
        std::cout<<"匹配失败"<<std::endl;
        return -1;
    }
    std::string method = matchs[1];
    std::transform(method.begin(),method.end(),method.begin(),::toupper);  //将method转换为大写
    std::cout<<"method:"<<method<<std::endl;
    int i=0;
    for(auto &s:matchs){
        std::cout<<i<<":";
        std::cout<<s<<std::endl;
        i++;
    }
}
