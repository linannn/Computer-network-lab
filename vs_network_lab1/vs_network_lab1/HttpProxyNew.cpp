#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <Windows.h>
#include <process.h>
#include <string.h>
#include <tchar.h>
#include <map>
#pragma comment(lib,"Ws2_32.lib")
#define MAXSIZE 65507 //发送数据报文的最大长度
#define HTTP_PORT 80 //http 服务器端口
using namespace std;
//Http 重要头部数据
struct HttpHeader {
	char method[4]; // POST 或者 GET，注意有些为 CONNECT，本实验暂不考虑
	char url[1024]; // 请求的 url
	char host[1024]; // 目标主机
	char cookie[1024 * 10]; //cookie
	char userAgent[1024 * 10]; //user-Agent
	HttpHeader() {
		ZeroMemory(this, sizeof(HttpHeader));
	}
};
BOOL InitSocket();
void ParseHttpHead(char *buffer, HttpHeader * httpHeader);
BOOL ConnectToServer(SOCKET *serverSocket, char *host);
unsigned int __stdcall ProxyThread(LPVOID lpParameter);
//代理相关参数
SOCKET ProxyServer;
sockaddr_in ProxyServerAddr;
char *user_ip;
const int ProxyPort = 10240;
//由于新的连接都使用新线程进行处理，对线程的频繁的创建和销毁特别浪费资源
//可以使用线程池技术提高服务器效率
//const int ProxyThreadMaxNum = 20;
//HANDLE ProxyThreadHandle[ProxyThreadMaxNum] = {0};
//DWORD ProxyThreadDW[ProxyThreadMaxNum] = {0};
struct ProxyParam {
	SOCKET clientSocket;
	SOCKET serverSocket;
};

int _tmain(int argc, _TCHAR* argv[])
{
	printf("Proxy server is starting\n");
	printf("Initializate...\n");
	if (!InitSocket()) {
		printf("socket initializate failed\n");
		return -1;
	}
	printf("Proxy server is running, listen port %d\n", ProxyPort);
	SOCKET acceptSocket = INVALID_SOCKET;
	sockaddr_in user_addr;
	ProxyParam *lpProxyParam;
	HANDLE hThread;
	DWORD dwThreadID;
	int len = sizeof(SOCKADDR);
	//代理服务器不断监听
	while (true) {
		acceptSocket = accept(ProxyServer, (sockaddr*)&user_addr, &len);
		//acceptSocket = accept(ProxyServer, NULL, NULL);
		lpProxyParam = new ProxyParam;
		if (lpProxyParam == NULL) {
			continue;
		}
		user_ip = inet_ntoa(user_addr.sin_addr);
		//if (!strcmp("127.0.0.1", user_ip)) {
			//printf("This user whose IP is %s can't visit the website\n", user_ip);
			//continue;
		//}
		lpProxyParam->clientSocket = acceptSocket;
		hThread = (HANDLE)_beginthreadex(NULL, 0, &ProxyThread, (LPVOID)lpProxyParam, 0, 0);
		CloseHandle(hThread);	
		Sleep(200);
	}
	closesocket(ProxyServer);
	WSACleanup();
	return 0;
}

//************************************
// Method: InitSocket
// FullName: InitSocket
// Access: public
// Returns: BOOL
// Qualifier: 初始化套接字
//************************************
BOOL InitSocket() {
	//加载套接字库（必须）
	WORD wVersionRequested;
	WSADATA wsaData;
	//套接字加载时错误提示
	int err;
	//版本 2.2
	wVersionRequested = MAKEWORD(2, 2);
	//加载 dll 文件 Scoket 库
	err = WSAStartup(wVersionRequested, &wsaData);
	if (err != 0) {
		//找不到 winsock.dll
		printf("loading winsock fail，error code: %d\n", WSAGetLastError());
		return FALSE;
	}
	if (LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 2)
	{
		printf("can't find right winsock version\n");
		WSACleanup();
		return FALSE;
	}
	ProxyServer = socket(AF_INET, SOCK_STREAM, 0);
	if (INVALID_SOCKET == ProxyServer) {
		printf("create socket failed, error code: %d\n", WSAGetLastError());
		return FALSE;
	}
	ProxyServerAddr.sin_family = AF_INET;
	ProxyServerAddr.sin_port = htons(ProxyPort);
	ProxyServerAddr.sin_addr.S_un.S_addr = INADDR_ANY;
	if (bind(ProxyServer, (SOCKADDR*)&ProxyServerAddr, sizeof(SOCKADDR)) == SOCKET_ERROR) {
		printf("bind socked failed\n");
		return FALSE;
	}
	if (listen(ProxyServer, SOMAXCONN) == SOCKET_ERROR) {
		printf("listen port %d failed", ProxyPort);
		return FALSE;
	}
	return TRUE;
}
//************************************
// Method: ProxyThread
// FullName: ProxyThread
// Access: public
// Returns: unsigned int __stdcall
// Qualifier: 线程执行函数
// Parameter: LPVOID lpParameter
//************************************
unsigned int __stdcall ProxyThread(LPVOID lpParameter) {
	char Buffer[MAXSIZE];
	char *CacheBuffer;
	ZeroMemory(Buffer, MAXSIZE);
	SOCKADDR_IN clientAddr;
	int length = sizeof(SOCKADDR_IN);
	int recvSize;
	int ret;
	recvSize = recv(((ProxyParam*)lpParameter)->clientSocket, Buffer, MAXSIZE, 0);
	if (recvSize <= 0) {
		printf("close socket\n");
		Sleep(200);
		closesocket(((ProxyParam*)lpParameter)->clientSocket);
		closesocket(((ProxyParam*)lpParameter)->serverSocket);
		delete lpParameter;
		_endthreadex(0);
		return 0;
	}
	HttpHeader* httpHeader = new HttpHeader();
	CacheBuffer = new char[recvSize + 1];
	ZeroMemory(CacheBuffer, recvSize + 1);
	memcpy(CacheBuffer, Buffer, recvSize);
	ParseHttpHead(CacheBuffer, httpHeader);
	delete CacheBuffer;
	/*
	char urlUserNotVisit[5][30] = { "ms.hit.edu.cn", NULL, NULL, NULL, NULL };
	const char *userAgent = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/65.0.3325.181 Safari/537.36";
	for (int i = 0; i < 5; i++) {
		if (strcmp(urlUserNotVisit[i], httpHeader->host) == 0 &&
			strcmp(userAgent, httpHeader->userAgent) == 0) {
			printf("```````````````````````` you can't visit this website, user");
			goto error;
		}
	}
	*/
	if (!ConnectToServer(&((ProxyParam*)lpParameter)->serverSocket, httpHeader->host)) {
		goto error;
	}
	printf("connect to proxy server %s success\n", httpHeader->host);
	//将客户端发送的 HTTP 数据报文直接转发给目标服务器
	ret = send(((ProxyParam *)lpParameter)->serverSocket, Buffer, strlen(Buffer) + 1, 0);
	// 等待目标服务器返回数据
	recvSize = recv(((ProxyParam*)lpParameter)->serverSocket, Buffer, MAXSIZE, 0);
	if (recvSize <= 0) {
		goto error;
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
	_endthreadex(0);
	return 0;
}
//************************************
// Method: ParseHttpHead
// FullName: ParseHttpHead
// Access: public
// Returns: void
// Qualifier: 解析 TCP 报文中的 HTTP 头部
// Parameter: char * buffer
// Parameter: HttpHeader * httpHeader
//************************************
void ParseHttpHead(char *buffer, HttpHeader * httpHeader) {
	char *p;
	char *ptr;
	const char * delim = "\r\n";
	p = strtok_s(buffer, delim, &ptr);//提取第一行
	printf("%s\n", p);
	if (p[0] == 'G') {//GET 方式
		memcpy(httpHeader->method, "GET", 3);
		memcpy(httpHeader->url, &p[4], strlen(p) - 13);
	}
	else if (p[0] == 'P') {//POST 方式
		memcpy(httpHeader->method, "POST", 4);
		memcpy(httpHeader->url, &p[5], strlen(p) - 14);
	}
	printf("%s\n", httpHeader->url);
	p = strtok_s(NULL, delim, &ptr);
	while (p) {
		switch (p[0]) {
		case 'H'://Host
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
		//添加IMS头行信息
		case 'U':
			//printf("%d	yigong	%s\n", strlen(p), p);
			memcpy(httpHeader->userAgent, &p[12], strlen(p));
			//printf("hahahahah :%s hahahahahahahaha\n", httpHeader->userAgent);
			break;
		default:
			break;
		}
		p = strtok_s(NULL, delim, &ptr);
	}
}
//************************************
// Method: ConnectToServer
// FullName: ConnectToServer
// Access: public
// Returns: BOOL
// Qualifier: 根据主机创建目标服务器套接字，并连接
// Parameter: SOCKET * serverSocket
// Parameter: char * host
//************************************
BOOL ConnectToServer(SOCKET *serverSocket, char *host) {
	sockaddr_in serverAddr;
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(HTTP_PORT);
	
	char urlNotVisit[5][30] = { "today.hit.edu.cn", NULL, NULL, NULL, NULL };
	char urlFishing[5][30] = { "jwes.hit.edu.cn", NULL, NULL, NULL, NULL };
	//网站过滤
	for (int i = 0; i < 5; i++) {
		if (strcmp(urlNotVisit[i], host) == 0) {
			printf("*****************************************************");
			printf(" you should not visit that website\n");
			return FALSE;
		}
	}
	//网站引导
	for (int i = 0; i < 5; i++) {
		if (strcmp(urlFishing[i], host) == 0) {
			printf("+++++++++++++++++++++++++++++++++++++++++++++++++++++");
			printf(" you are in a fishing website\n");
			strcpy(host, "jwts.hit.edu.cn");
		}
	}
	//用戶过滤
	if (!strcmp("127.0.0.1", user_ip) && strcmp("ms.hit.edu.cn", host) == 0) {
		printf("This user whose IP is %s can't visit the website\n", user_ip);
		return FALSE;
	}
	HOSTENT *hostent = gethostbyname(host);
	if (!hostent) {
		return FALSE;
	}
	in_addr Inaddr = *((in_addr*)*hostent->h_addr_list);
	serverAddr.sin_addr.s_addr = inet_addr(inet_ntoa(Inaddr));
	*serverSocket = socket(AF_INET, SOCK_STREAM, 0);
	if (*serverSocket == INVALID_SOCKET) {
		return FALSE;
	}
	if (connect(*serverSocket, (SOCKADDR *)&serverAddr, sizeof(serverAddr))
		== SOCKET_ERROR) {
		closesocket(*serverSocket);
		return FALSE;
	}
	return TRUE;
}