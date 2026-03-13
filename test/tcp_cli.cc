#include"../source/server.hpp"

int main(){
    Socket cli_sok;
    cli_sok.CreateClient(8500,"127.0.0.1");
    while(1){
        std::string str = "你好，神人";
        cli_sok.Send(str.c_str(),str.size());
        char buf[1024] = {0};
        cli_sok.Recv(buf,1023);
        DBG_LOG("%s",buf);
        sleep(1);
    }
    return 0;
}