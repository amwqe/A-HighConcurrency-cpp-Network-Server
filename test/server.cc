#include "../source/server.hpp"
// 连接回调
void OnConnected(const PtrConnection &conn) {
    DBG_LOG("NEW CONNECTION:%p", conn.get());
}
// 关闭回调
void OnClosed(const PtrConnection &conn) {
    DBG_LOG("CLOSE CONNECTION:%p", conn.get());
}
// 消息回调 
void OnMessage(const PtrConnection &conn, Buffer *buf) {
    DBG_LOG("%s", buf->Read_pos());
    buf->MoveReadOffset(buf->ReadAbleSize());
    std::string str = "Hello World";
    conn->Send(str.c_str(), str.size());
    //conn->Shutdown();
}    
int main()
{
    
    TcpServer server(8500);
    server.SetThreadCount(2);
    server.EnableInactiveRelease(10);
    server.SetClosedCallback(OnClosed);
    server.SetConnectedCallback(OnConnected);
    server.SetMessageCallback(OnMessage);
    server.Start();
    return 0;
}