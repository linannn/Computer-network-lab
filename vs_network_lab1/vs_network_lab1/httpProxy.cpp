// #include "stdafx.h"
#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <Windows.h>
#include <process.h>
#include <string.h>

#pragma comment(lib,"Ws2_32.lib")

#define MAXSIZE 65507 //�������ݱ��ĵ���󳤶�
#define HTTP_PORT 80 //http �������˿�

//Http ��Ҫͷ������
struct HttpHeader {
	char method[4];//POST ���� GET,ע����ЩΪCONTENT,��ʵ���ݲ�����
	char url[1024]; //�����url
	char host[1024]; //Ŀ������
	char cookie[1024 * 10]; //cookie
	HttpHeader() {
		ZeroMemory(this, sizeof(HttpHeader));//�ڴ�����
	}
};

bool InitSocket();
void ParseHttpHead(char *buffer, HttpHeader *httpHead);
bool ConnectToServer(SOCKET *serverSocket, char *host);
unsigned int __stdcall ProxyThread(LPVOID lpParameter);
void ErrorProcess(LPVOID lpParameter);

//������ز���
SOCKET ProxyServer;//���������socket
sockaddr_in ProxyServerAddr;//�����ַ��Ϣ
const int ProxyPort = 12333;//����˿�

//�����µ����Ӷ��������߳̽��д������̵߳�Ƶ���Ĵ����������ر��˷���Դ����ʹ���̳߳ؼ�����߷�����Ч��
//������̳߳�������Ч��
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
	if (!InitSocket()) {//��ʼ��socket
		printf("socket initialization failed\n");
		return -1;
	}
	printf("sever is running, listen port: %d\n", ProxyPort);
	SOCKET acceptSocket = INVALID_SOCKET;
	ProxyParam *lpProxyParam;
	HANDLE hThread;
	DWORD dwThreadID;
	//������������ϼ���
	while (true) {
		acceptSocket = accept(ProxyServer, NULL, NULL);//������socket
		lpProxyParam = new ProxyParam;
		if (lpProxyParam == NULL) {
			continue;
		}
		lpProxyParam->clientSocket = acceptSocket;
		hThread = (HANDLE)_beginthreadex(NULL, 0, &ProxyThread, (LPVOID)lpProxyParam, 0, 0);
		CloseHandle(hThread);
		//QueueUserWorkItem((LPTHREAD_START_ROUTINE)ProxyThread, (LPVOID)lpProxyParam, WT_EXECUTEINLONGTHREAD);//�����̳߳���Ӧsocket

		Sleep(200);
	}
	closesocket(ProxyServer);
	WSACleanup();
	return 0;
}

bool InitSocket() {//��ʼ��socket
				   //�����׽��ֿ⣨���룩
	WORD wVersionRequested;
	WSADATA wsaData;
	//�׽��ּ���ʱ������ʾ
	int err;
	//�汾2.2
	wVersionRequested = MAKEWORD(2, 2);
	//����dll�ļ�Scoket��
	err = WSAStartup(wVersionRequested, &wsaData);
	if (err != 0) {
		//�Ҳ���winsock.dll
		printf("loading winsock failed, error code��%d\n", WSAGetLastError());
		return false;
	}
	if (LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 2) {
		printf("can't find correct winsock version\n");
		WSACleanup();
		return false;
	}
	ProxyServer = socket(AF_INET, SOCK_STREAM, 0);//�����׽���
	if (INVALID_SOCKET == ProxyServer) {
		printf("create socket failed, error code��%d\n", WSAGetLastError());
		return false;
	}
	ProxyServerAddr.sin_family = AF_INET;
	ProxyServerAddr.sin_port = htons(ProxyPort);//�������ֽ�˳��ת��Ϊ�����ֽ�˳�򣬴��
	ProxyServerAddr.sin_addr.S_un.S_addr = INADDR_ANY;
	if (bind(ProxyServer, (SOCKADDR*)&ProxyServerAddr, sizeof(SOCKADDR)) == SOCKET_ERROR) {//������socket��
		printf("binding socked failed\n");
		return false;
	}
	if (listen(ProxyServer, SOMAXCONN) == SOCKET_ERROR) {
		printf("listen port %d failed", ProxyPort);
		return false;
	}
	return true;
}

unsigned int __stdcall ProxyThread(LPVOID lpParameter) {//socket��Ӧ�߳�
	char Buffer[MAXSIZE];
	char *CacheBuffer;
	ZeroMemory(Buffer, MAXSIZE);
	SOCKADDR_IN clientAddr;
	int length = sizeof(SOCKADDR_IN);
	INT recvSize;
	int ret;
	recvSize = recv(((ProxyParam*)lpParameter)->clientSocket, Buffer, MAXSIZE, 0);//��ȡ����
	if (recvSize <= 0) {
		ErrorProcess(lpParameter);
		return 0;
	}
	HttpHeader *httpHeader = new HttpHeader();
	CacheBuffer = new char[recvSize + 1];
	ZeroMemory(CacheBuffer, recvSize + 1);
	memcpy(CacheBuffer, Buffer, recvSize);
	ParseHttpHead(CacheBuffer, httpHeader);//ת��ͷ
	delete CacheBuffer;
	if (!ConnectToServer(&((ProxyParam*)lpParameter)->serverSocket, httpHeader->host)) {//�������
		printf("connect to server failed\n");
		ErrorProcess(lpParameter);
		return 0;
	}
	printf("proxy connect to sever %s success\n", httpHeader->host);
	//���ͻ��˷��͵�HTTP���ݱ���ֱ��ת����Ŀ�������
	ret = send(((ProxyParam*)lpParameter)->serverSocket, Buffer, strlen(Buffer) + 1, 0);
	//�ȴ�Ŀ���������������
	recvSize = recv(((ProxyParam*)lpParameter)->serverSocket, Buffer, MAXSIZE, 0);
	if (recvSize <= 0) {
		ErrorProcess(lpParameter);
		return 0;
	}
	//��Ŀ����������ص�����ֱ��ת�����ͻ���
	ret = send(((ProxyParam*)lpParameter)->clientSocket, Buffer, sizeof(Buffer), 0);
	//������
error:
	printf("close socket\n");
	Sleep(200);
	closesocket(((ProxyParam*)lpParameter)->clientSocket);
	closesocket(((ProxyParam*)lpParameter)->serverSocket);
	delete lpParameter;
	//_endthreadex(0);
	return 0;
}

void ParseHttpHead(char *buffer, HttpHeader *httpHeader) {//ת��ͷ
	char *p;
	char *ptr;
	const char *delim = "\r\n";//���з�
	p = strtok_s(buffer, delim, &ptr);//��ȡ��һ��
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
	p = strtok_s(NULL, delim, &ptr);//ʹ�û��з��ָ��ȡ��ǰ��
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
		p = strtok_s(NULL, delim, &ptr);//ʹ�û��з��ָ��ȡ��ǰ��
	}
}

bool ConnectToServer(SOCKET *serverSocket, char *host) {//�������server
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
	HOSTENT *hostent = gethostbyname(host);//��ȡ�������֡���ַ��Ϣ
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
	if (connect(*serverSocket, (SOCKADDR*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {//��������
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
