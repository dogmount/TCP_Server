#ifndef _SER_CONFIG_HPP_
#define _SER_CONFIG_HPP_

//SOCKET
#ifdef _WIN32
#define FD_SETSIZE      256
#define WIN32_LEAN_AND_MEAN
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include<WinSock2.h>

//在工程文件属性-链接器-输入-附加依赖项中添加也可以
#pragma comment(lib,"ws2_32.lib")		//引用winSocket静态库

#else
#include<unistd.h>				// uni std
#include<sys/socket.h>			// socket(), connect()
#include<netinet/in.h>			// sockaddr_in, htons()
#include<arpa/inet.h>			//linux网络编程头文件
#include<string.h>

#define SOCKET int
#define INVALID_SOCKET  (SOCKET)(~0)
#define SOCKET_ERROR            (-1)
#endif

#include<iostream>


#ifndef RECV_BUFF_SIZE
#define RECV_BUFF_SIZE 10240
#define SEND_BUFF_SIZE 10240
#endif // RECV_BUFF_SIZE

#endif