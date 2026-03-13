# Ayuan
基于 Reactor 模式的高性能并发网络服务器，灵感源自 muduo 库。采用 C++11 实现「One Thread One Loop」主从 Reactor 架构，支持 TCP 回显服务与 HTTP/1.1 静态资源服务，专为低延迟、高并发场景设计。

---

## 核心特性
- **高性能架构**：主从 Reactor + One Thread One Loop 线程模型，充分利用多核 CPU，支持万级并发连接
- **高效 IO 模型**：基于 Linux epoll 边缘触发（ET）实现 IO 多路复用，避免无效轮询
- **连接管理**：智能指针管理连接生命周期，时间轮定时器自动清理非活跃连接（O(1) 操作复杂度）
- **协议支持**：
  - TCP 回显服务：验证基础数据传输能力
  - HTTP/1.1 协议解析：支持 GET/HEAD 请求，可提供静态资源（HTML/TXT/文件等）服务
- **工程化设计**：模块化封装（Server/Channel/Buffer/Connection），代码可复用、易扩展
- **测试工具**：内置客户端测试用例与 WebBench 性能压测工具

---

## 技术栈
- **语言**：C++11
- **平台**：Linux（依赖 epoll 机制）
- **核心组件**：
  - Reactor 事件驱动模型
  - epoll IO 多路复用
  - 时间轮定时器
  - 自动扩容缓冲区（解决 TCP 粘包/拆包）
- **构建工具**：Makefile

---
