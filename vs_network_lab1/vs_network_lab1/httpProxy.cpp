// #include "stdafx.h"
#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <Windows.h>
#include <process.h>
#include <string.h>

#pragma comment(lib,"Ws2_32.lib")

#define MAXSIZE 65507 //发送数据报文的最大长度
#define HTTP_PORT 80 //http 服务器端口

//Http 重要头部数据
struct HttpHeader {
	char method[4];//POST 或者 GET,注意有些为CONTENT,本实验暂不考虑
	char url[1024]; //请求的url
	char host[1024]; //目标主机
	char cookie[1024 * 10]; //cookie
	HttpHeader() {
		ZeroMemory(this, sizeof(HttpHeader));//内存置零
	}
};

bool InitSocket();
void ParseHttpHead(char *buffer, HttpHeader *httpHead);
bool ConnectToServer(SOCKET *serverSocket, char *host);
unsigned int __stdcall ProxyThread(LPVOID lpParameter);
void ErrorProcess(LPVOID lpParameter);

//代理相关参数
SOCKET ProxyServer;//代理服务器socket
sockaddr_in ProxyServerAddr;//代理地址信息
const int ProxyPort = 12333;//代理端口

//由于新的链接都是用新线程进行处理，对线程的频繁的创建和销毁特别浪费资源可以使用线程池技术提高服务器效率
//已添加线程池来提升效率
const int ProxyThreadMaxNum = 20;
HANDLE ProxyThreadhandle[ProxyThreadMaxNum] = { 0 };
DWORD ProxyThreadDW[ProxyThreadMaxNum] = { 0 };

struct ProxyParam {
	SOCKET clientSocket;
	SOCKET serverSocket;
};

int main(int argc, char* argv[]) {
	printf("sever is starting up\n");
	printf("init...\n");
	if (!InitSocket()) {//初始化socket
		printf("socket initialization failed\n");
		return -1;
	}
	printf("sever is running, listen port: %d\n", ProxyPort);
	SOCKET acceptSocket = INVALID_SOCKET;
	ProxyParam *lpProxyParam;
	HANDLE hThread;
	DWORD dwThreadID;
	//代理服务器不断监听
	while (true) {
		acceptSocket = accept(ProxyServer, NULL, NULL);//监听到socket
		lpProxyParam = new ProxyParam;
		if (lpProxyParam == NULL) {
			continue;
		}
		lpProxyParam->clientSocket = acceptSocket;
		hThread = (HANDLE)_beginthreadex(NULL, 0, &ProxyThread, (LPVOID)lpProxyParam, 0, 0);
		CloseHandle(hThread);
		//QueueUserWorkItem((LPTHREAD_START_ROUTINE)ProxyThread, (LPVOID)lpProxyParam, WT_EXECUTEINLONGTHREAD);//启用线程池响应socket

		Sleep(200);
	}
	closesocket(ProxyServer);
	WSACleanup();
	return 0;
}

bool InitSocket() {//初始化socket
				   //加载套接字库（必须）
	WORD wVersionRequested;
	WSADATA wsaData;
	//套接字加载时错误提示
	int err;
	//版本2.2
	wVersionRequested = MAKEWORD(2, 2);
	//加载dll文件Scoket库
	err = WSAStartup(wVersionRequested, &wsaData);
	if (err != 0) {
		//找不到winsock.dll
		printf("loading winsock failed, error code：%d\n", WSAGetLastError());
		return false;
	}
	if (LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 2) {
		printf("can't find correct winsock version\n");
		WSACleanup();
		return false;
	}
	ProxyServer = socket(AF_INET, SOCK_STREAM, 0);//创建套接字
	if (INVALID_SOCKET == ProxyServer) {
		printf("create socket failed, error code：%d\n", WSAGetLastError());
		return false;
	}
	ProxyServerAddr.sin_family = AF_INET;
	ProxyServerAddr.sin_port = htons(ProxyPort);//将主机字节顺序转化为网络字节顺序，大端
	ProxyServerAddr.sin_addr.S_un.S_addr = INADDR_ANY;
	if (bind(ProxyServer, (SOCKADDR*)&ProxyServerAddr, sizeof(SOCKADDR)) == SOCKET_ERROR) {//代理与socket绑定
		printf("binding socked failed\n");
		return false;
	}
	if (listen(ProxyServer, SOMAXCONN) == SOCKET_ERROR) {
		printf("listen port %d failed", ProxyPort);
		return false;
	}
	return true;
}

unsigned int __stdcall ProxyThread(LPVOID lpParameter) {//socket响应线程
	char Buffer[MAXSIZE];
	char *CacheBuffer;
	ZeroMemory(Buffer, MAXSIZE);
	SOCKADDR_IN clientAddr;
	int length = sizeof(SOCKADDR_IN);
	INT recvSize;
	int ret;
	recvSize = recv(((ProxyParam*)lpParameter)->clientSocket, Buffer, MAXSIZE, 0);//收取数据
	if (recvSize <= 0) {
		ErrorProcess(lpParameter);
		return 0;
	}
	HttpHeader *httpHeader = new HttpHeader();
	CacheBuffer = new char[recvSize + 1];
	ZeroMemory(CacheBuffer, recvSize + 1);
	memcpy(CacheBuffer, Buffer, recvSize);
	ParseHttpHead(CacheBuffer, httpHeader);//转换头
	delete CacheBuffer;
	if (!ConnectToServer(&((ProxyParam*)lpParameter)->serverSocket, httpHeader->host)) {//代理访问
		printf("connect to server failed\n");
		ErrorProcess(lpParameter);
		return 0;
	}
	printf("proxy connect to sever %s success\n", httpHeader->host);
	//将客户端发送的HTTP数据报文直接转发给目标服务器
	ret = send(((ProxyParam*)lpParameter)->serverSocket, Buffer, strlen(Buffer) + 1, 0);
	//等待目标服务器返回数据
	recvSize = recv(((ProxyParam*)lpParameter)->serverSocket, Buffer, MAXSIZE, 0);
	if (recvSize <= 0) {
		ErrorProcess(lpParameter);
		return 0;
	}
	//将目标服务器返回的数据直接转发给客户端
	ret = send(((ProxyParam*)lpParameter)->clientSocket, Buffer, sizeof(Buffer), 0);
	//错误处理
error:
	printf("close socket\n");
	Sleep(200);
	closesocket(((ProxyParam*)lpParameter)->clientSocket);
	closesocket(((ProxyParam*)lpParameter)->serverSocket);
	delete lpParameter;
	//_endthreadex(0);
	return 0;
}

void ParseHttpHead(char *buffer, HttpHeader *httpHeader) {//转换头
	char *p;
	char *ptr;
	const char *delim = "\r\n";//换行符
	p = strtok_s(buffer, delim, &ptr);//提取第一行
	printf("%s\n", p);
	if (p[0] == 'G') {	//GET
		memcpy(httpHeader->method, "GET", 3);
		memcpy(httpHeader->url, &p[4], strlen(p) - 13);
	}
	else if (p[0] == 'P') {	//POST
		memcpy(httpHeader->method, "POST", 4);
		memcpy(httpHeader->url, &p[5], strlen(p) - 14);
	}
	printf("%s\n", httpHeader->url);//url
	p = strtok_s(NULL, delim, &ptr);//使用换行符分割，提取当前行
	while (p) {
		switch (p[0]) {
		case 'H'://HOST
			memcpy(httpHeader->host, &p[6], strlen(p) - 6);
			break;
		case 'C'://Cookie
			if (strlen(p) > 8) {
				char header[8];
				ZeroMemory(header, sizeof(header));
				memcpy(header, p, 6);
				if (!strcmp(header, "Cookie")) {
					memcpy(httpHeader->cookie, &p[8], strlen(p) - 8);
				}
			}
			break;
		default:
			break;
		}
		p = strtok_s(NULL, delim, &ptr);//使用换行符分割，提取当前行
	}
}

bool ConnectToServer(SOCKET *serverSocket, char *host) {//代理访问server
	sockaddr_in serverAddr;
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(HTTP_PORT);
	/*
	if (strcmp("software.hit.edu.cn", host) == 0)
	{
		printf("BLOCK http://software.hit.edu.cn/\n");
		return false;
	}
	if (strcmp("www.hao123.com", host) == 0)
		strcpy(host, "jwts.hit.edu.cn");
	*/
	HOSTENT *hostent = gethostbyname(host);//获取主机名字、地址信息
	printf("host:%s\n", host);
	if (!hostent) {
		for (int ss = 0; ss < 10; ss++) {
			hostent = gethostbyname(host);
			if (!hostent)
				printf("%d\n", ss);
		}
		return false;
	}
	in_addr Inaddr = *((in_addr*)*hostent->h_addr_list);
	serverAddr.sin_addr.s_addr = inet_addr(inet_ntoa(Inaddr));
	*serverSocket = socket(AF_INET, SOCK_STREAM, 0);
	if (*serverSocket == INVALID_SOCKET) {
		return false;
	}
	if (connect(*serverSocket, (SOCKADDR*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {//尝试连接
		closesocket(*serverSocket);
		return false;
	}
	return true;
}

void ErrorProcess(LPVOID lpParameter) {
	printf("close socket\n");
	Sleep(200);
	closesocket(((ProxyParam*)lpParameter)->clientSocket);
	closesocket(((ProxyParam*)lpParameter)->serverSocket);
	delete lpParameter;
	//_endthreadex(0);
}
