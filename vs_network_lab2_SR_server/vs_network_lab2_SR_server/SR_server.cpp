//#include "stdafx.h" //���� VS ��Ŀ������Ԥ����ͷ�ļ�
#include <stdlib.h>
#include <time.h>
#include <WinSock2.h>
#include <fstream>
#include <string>
#include <iostream>
#pragma comment(lib, "ws2_32.lib")
#pragma warning( disable : 4996 )
#define _CRT_SECURE_NO_WARNINGS 1
#define SERVER_PORT 12340		//�˿ں�
#define SERVER_IP "0.0.0.0"		//IP ��ַ
const int BUFFER_LENGTH = 1026; //��������С������̫���� UDP ������֡�а�����ӦС�� 1480 �ֽڣ�
const int SEND_WIND_SIZE = 10;  //���ʹ��ڴ�СΪ 10�� GBN ��Ӧ���� W + 1 <=N��W Ϊ���ʹ��ڴ�С�� N Ϊ���кŸ�����
								//����ȡ���к� 0...19 �� 20 ��
								//��������ڴ�С��Ϊ 1����Ϊͣ-��Э��
const int SEQ_SIZE = 20; //���кŵĸ������� 0~19 ���� 20 ��
						 //���ڷ������ݵ�һ���ֽ����ֵΪ 0�������ݻᷢ��ʧ��
						 //��˽��ն����к�Ϊ 1~20���뷢�Ͷ�һһ��Ӧ
BOOL ack[SEQ_SIZE]; //�յ� ack �������Ӧ 0~19 �� ack
int curSeq;			//��ǰ���ݰ��� seq
int curAck;			//��ǰ�ȴ�ȷ�ϵ� ack
int totalSeq;		//�յ��İ�������
int totalPacket;	//��Ҫ���͵İ�����
					//************************************
					// Method: getCurTime
					// FullName: getCurTime
					// Access: public
					// Returns: void
					// Qualifier: ��ȡ��ǰϵͳʱ�䣬������� ptime ��
					// Parameter: char * ptime
					//************************************
void getCurTime(char *ptime)
{
	char buffer[128];
	memset(buffer, 0, sizeof(buffer));
	time_t c_time;
	struct tm *p;
	time(&c_time);
	p = localtime(&c_time);
	sprintf_s(buffer, "%d/%d/%d %d:%d:%d",
		p->tm_year + 1900,
		p->tm_mon,
		p->tm_mday,
		p->tm_hour,
		p->tm_min,
		p->tm_sec);
	strcpy_s(ptime, sizeof(buffer), buffer);
}
//************************************
// Method: seqIsAvailable
// FullName: seqIsAvailable
// Access: public
// Returns: bool
// Qualifier: ��ǰ���к� curSeq �Ƿ����
//************************************
bool seqIsAvailable()
{
	int step;
	step = curSeq - curAck;
	step = step >= 0 ? step : step + SEQ_SIZE;
	//���к��Ƿ��ڵ�ǰ���ʹ���֮��
	if (step >= SEND_WIND_SIZE)
	{
		return false;
	}
	if (ack[curSeq])
	{
		return true;
	}
	return false;
}
//������
int main(int argc, char *argv[])
{
	//�����׽��ֿ⣨���룩
	WORD wVersionRequested;
	WSADATA wsaData;
	//�׽��ּ���ʱ������ʾ
	int err;
	//�汾 2.2
	wVersionRequested = MAKEWORD(2, 2);
	//���� dll �ļ� Scoket ��
	err = WSAStartup(wVersionRequested, &wsaData);
	if (err != 0)
	{
		//�Ҳ��� winsock.dll
		printf("WSAStartup failed with error: %d\n", err);
		return -1;
	}
	if (LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 2)
	{
		printf("Could not find a usable version of Winsock.dll\n");
		WSACleanup();
	}
	else
	{
		printf("The Winsock 2.2 dll was found okay\n");
	}
	SOCKET sockServer = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	//�����׽���Ϊ������ģʽ
	int iMode = 1;											//1���������� 0������
	ioctlsocket(sockServer, FIONBIO, (u_long FAR *)&iMode); //����������
	SOCKADDR_IN addrServer;									//��������ַ
															//addrServer.sin_addr.S_un.S_addr = inet_addr(SERVER_IP);
	addrServer.sin_addr.S_un.S_addr = htonl(INADDR_ANY);	//���߾���
	addrServer.sin_family = AF_INET;
	addrServer.sin_port = htons(SERVER_PORT);
	err = bind(sockServer, (SOCKADDR *)&addrServer, sizeof(SOCKADDR));
	if (err)
	{
		err = GetLastError();
		printf("Could not bind the port %d for socket.Error code is %d\n", SERVER_PORT, err);
		WSACleanup();
		return -1;
	}
	SOCKADDR_IN addrClient; //�ͻ��˵�ַ
	int length = sizeof(SOCKADDR);
	char buffer[BUFFER_LENGTH]; //���ݷ��ͽ��ջ�����
	ZeroMemory(buffer, sizeof(buffer));
	//���������ݶ����ڴ�
	std::ifstream icin;
	icin.open("../test.txt");
	char data[1024 * 113];
	ZeroMemory(data, sizeof(data));
	icin.read(data, 1024 * 113);
	icin.close();
	totalPacket = sizeof(data) / 1024;
	int recvSize;
	BOOL ackWindow[SEQ_SIZE];
	for (int i = 0; i < SEQ_SIZE; ++i)
	{
		ack[i] = TRUE;
		ackWindow[i] = FALSE;
	}
	while (true)
	{
		//���������գ���û���յ����ݣ�����ֵΪ-1
		recvSize = recvfrom(sockServer, buffer, BUFFER_LENGTH, 0, ((SOCKADDR *)&addrClient), &length);
		if (recvSize < 0)
		{
			Sleep(200);
			continue;
		}
		printf("recv from client: %s\n", buffer);
		if (strcmp(buffer, "-time") == 0)
		{
			getCurTime(buffer);
		}
		else if (strcmp(buffer, "-quit") == 0)
		{
			strcpy_s(buffer, strlen("Good bye!") + 1, "Good bye!");
		}
		else if (strcmp(buffer, "-testsr") == 0)
		{
			//���� gbn ���Խ׶�
			//���� server��server ���� 0 ״̬���� client ���� 205 ״̬�루server���� 1 ״̬��
			//server �ȴ� client �ظ� 200 ״̬�룬����յ���server ���� 2 ״̬������ʼ�����ļ���������ʱ�ȴ�ֱ����ʱ\
															//���ļ�����׶Σ� server ���ʹ��ڴ�С��Ϊ
			ZeroMemory(buffer, sizeof(buffer));
			int recvSize;
			int waitCount = 0;
			int counter[20];
			BOOL counterStage[20];
			for (int i = 0; i < 20; i++) {
				counterStage[i] = FALSE;
				counter[i] = 0;
			}
			printf("Begain to test GBN protocol,please don't abort the process\n");
			//������һ�����ֽ׶�
			//���ȷ�������ͻ��˷���һ�� 205 ��С��״̬�루���Լ�����ģ���ʾ������׼�����ˣ����Է�������
			//�ͻ����յ� 205 ֮��ظ�һ�� 200 ��С��״̬�룬��ʾ�ͻ���׼�����ˣ����Խ���������
			//�������յ� 200 ״̬��֮�󣬾Ϳ�ʼʹ�� GBN ����������
			printf("Shake hands stage\n");
			int stage = 0;
			bool runFlag = true;
			while (runFlag) {
				switch (stage) {
				case 0://���� 205 �׶�
					buffer[0] = 205;
					sendto(sockServer, buffer, strlen(buffer) + 1, 0,
						(SOCKADDR*)&addrClient, sizeof(SOCKADDR));
					Sleep(100);
					stage = 1;
					break;
				case 1://�ȴ����� 200 �׶Σ�û���յ��������+1����ʱ������˴Ρ����ӡ����ȴ��ӵ�һ����ʼ
					recvSize =
						recvfrom(sockServer, buffer, BUFFER_LENGTH, 0, ((SOCKADDR*)&addrClient), &length);
					if (recvSize < 0) {
						++waitCount;
						if (waitCount > 20) {
							runFlag = false;
							printf("Timeout error\n");
							break;
						}
						Sleep(500);
						continue;
					}
					else {
						if ((unsigned char)buffer[0] == 200) {
							printf("Begin a file transfer\n");
							printf("File size is %dB, each packet is 1024B and packet total num is %d\n", sizeof(data), totalPacket);
							curSeq = 0;
							curAck = 0;
							totalSeq = 0;
							waitCount = 0;
							stage = 2;
						}
					}
					break;
				case 2://���ݴ���׶�
					if (seqIsAvailable()) {
						//���͸��ͻ��˵����кŴ� 1 ��ʼ
						buffer[0] = curSeq + 1;
						ack[curSeq] = FALSE;
						counterStage[curSeq] = TRUE;
						//���ݷ��͵Ĺ�����Ӧ���ж��Ƿ������
						//Ϊ�򻯹��̴˴���δʵ��
						memcpy(&buffer[1], data + 1024 * totalSeq, 1024);
						printf("send a packet with a seq of %d\n", curSeq);
						sendto(sockServer, buffer, BUFFER_LENGTH, 0, (SOCKADDR*)&addrClient, sizeof(SOCKADDR));
						++curSeq;
						curSeq %= SEQ_SIZE;
						++totalSeq;
						Sleep(500);
					}
					//�ȴ� Ack����û���յ����򷵻�ֵΪ-1��������+1
					recvSize = recvfrom(sockServer, buffer, BUFFER_LENGTH, 0, ((SOCKADDR*)&addrClient), &length);
					if (recvSize < 0) {
						for (int i = 0; i < 20; i++) {
							if (counterStage[i]) {
								counter[i]++;
							}
							if (counter[i] > 20) {
								printf("time out error\n");
								buffer[0] = i + 1;
								memcpy(&buffer[1], data + 1024 * i, 1024);
								printf("REsend a packet with a seq of %d\n", i);
								sendto(sockServer, buffer, BUFFER_LENGTH, 0, (SOCKADDR*)&addrClient, sizeof(SOCKADDR));
								counter[i] = 0;
							}
						}
					}
					else
					{
						//�յ� ack
						unsigned char index = (unsigned char)buffer[0] - 1; //���кż�һ
						printf("Recv a ack of %d\n", index);
						counter[index] = 0;
						ackWindow[index] = TRUE;
						if (ack[index]) {
							ackWindow[index] = FALSE;
						}
						ack[index] = TRUE;
						counterStage[index] = FALSE;
						while (ackWindow[curAck]) {
							ackWindow[curAck] = FALSE;
							curAck = (curAck + 1) % SEQ_SIZE;
						}
					}
					Sleep(200);
					break;
				}
			}
		}
		sendto(sockServer, buffer, strlen(buffer) + 1, 0, (SOCKADDR *)&addrClient, sizeof(SOCKADDR));
		Sleep(500);
	}
	//�ر��׽��֣�ж�ؿ�
	closesocket(sockServer);
	WSACleanup();
	return 0;
}