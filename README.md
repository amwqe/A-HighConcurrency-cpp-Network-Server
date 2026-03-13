##
Ayuan/
└── Tcp-Server-MS/
    ├── .vscode/          # VSCode 配置文件
    ├── example/          # 示例代码（可选）
    ├── source/           # 核心源码
    │   ├── echo/         # TCP 回显服务
    │   │   ├── echo.hpp  # 回显服务核心逻辑
    │   │   ├── main.cc   # 入口文件
    │   │   └── Makefile  # 编译脚本
    │   ├── http/         # HTTP 静态资源服务
    │   │   ├── wwwroot/  # 静态资源目录（HTML/TXT等）
    │   │   ├── http.hpp  # HTTP 协议解析与响应
    │   │   ├── main.cc   # 入口文件
    │   │   ├── Makefile  # 编译脚本
    │   │   ├── mime      # MIME 类型映射
    │   │   ├── statu     # HTTP 状态码映射
    │   │   └── server.hpp# 服务器核心封装
    │   └── test/         # 测试用例
    │       ├── client*.cpp # 客户端测试代码
    │       ├── server.cc   # 测试服务器
    │       ├── tcp_*.cc    # 基础 TCP 客户端/服务器
    │       └── Makefile    # 编译脚本
    ├── WebBench-master/  # WebBench 压测工具
    ├── .gitignore        # Git 忽略文件
    └── LICENSE           # MIT 开源协议
    ##
