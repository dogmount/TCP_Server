tcp聊天服务器，改进中
本个人项目使用select I/O模型，TCP网络协议实现
头文件内容简述：
1、通用类：cellThread（线程类）、cellTime（系统时间类）、cellSemaphore（信号量类）
2、数据库类：sqlCommand（数据库操作定义）、sqlHeartbeat（数据库连接检测）
3、网络框架类：serConfig（网络基础定义）、TcpServer（服务器主线程）、cellServer（服务器I/O线程类）、cellclient（客户端信息类）【clientEvent（客户端请求接口类）已弃置，改为消息事件队列】
4、消息处理类：itemMsg（消息队列接口）、msgHeader（数据包头定义）、msgPack（数据包体定义）、msgQueue（客户端请求处理队列）、sendQueue（服务器响应发送队列）
