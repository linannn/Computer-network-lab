// GBN_client.cpp : �������̨Ӧ�ó������ڵ㡣
//
//#include "stdafx.h"
#include <stdio.h>
#include <stdlib.h>
#include <WinSock2.h>
#include <time.h>
#pragma comment(lib, "ws2_32.lib")
#pragma warning( disable : 4996 )
#define _CRT_SECURE_NO_WARNINGS 1
#define SERVER_PORT 12340	 //�������ݵĶ˿ں�
#define SERVER_IP_CLIENT "127.0.0.1" // �������� IP ��ַ
const int BUFFER_LENGTH = 1026;
const int windowsSize = 5;
const int SEQ_SIZE = 20; //���ն����кŸ�����Ϊ 1~20
						 /*
						 -time �ӷ������˻�ȡ��ǰʱ��
						 -quit �˳��ͻ���
						 -testgbn [X] ���� GBN Э��ʵ�ֿɿ����ݴ���
						 [X] [0,1] ģ�����ݰ���ʧ�ĸ���
						 [Y] [0,1] ģ�� ACK ��ʧ�ĸ���
						 */
void printTips()
{
	printf("*****************************************\n");
	printf("| -time to get current time |\n");
	printf("| -quit to exit client |\n");
	printf("| -testsr [X] [Y] to test the sr |\n");
	printf("*****************************************\n");
}
//************************************
// Method: lossInLossRatio
// FullName: lossInLossRatio
// Access: public
// Returns: BOOL
// Qualifier: ���ݶ�ʧ���������һ�����֣��ж��Ƿ�ʧ,��ʧ�򷵻�TRUE�����򷵻� FALSE
// Parameter: float lossRatio [0,1]
//************************************
BOOL lossInLossRatio(float lossRatio)
{
	int lossBound = (int)(lossRatio * 100);
	int r = rand() % 101;
	if (r <= lossBound)
	{
		return TRUE;
	}
	return FALSE;

}
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
		return 1;
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
	SOCKET socketClient = socket(AF_INET, SOCK_DGRAM, 0);
	SOCKADDR_IN addrServer;
	addrServer.sin_addr.S_un.S_addr = inet_addr(SERVER_IP_CLIENT);
	addrServer.sin_family = AF_INET;
	addrServer.sin_port = htons(SERVER_PORT);
	//���ջ�����
	char buffer[BUFFER_LENGTH];
	char receiveBuffer[5][BUFFER_LENGTH];
	BOOL receiveBufferStage[5];
	for (int i = 0; i < 5; i++)
		receiveBufferStage[i] = FALSE;
	ZeroMemory(buffer, sizeof(buffer));
	int position = 0;
	int len = sizeof(SOCKADDR);
	//Ϊ�˲���������������ӣ�����ʹ�� -time ����ӷ������˻�õ�ǰʱ��
	//ʹ�� -testgbn [X] [Y] ���� GBN ����[X]��ʾ���ݰ���ʧ����
	// [Y]��ʾ ACK ��������
	printTips();
	int ret;
	int interval = 1; //�յ����ݰ�֮�󷵻� ack �ļ����Ĭ��Ϊ 1 ��ʾÿ�������� ack�� 0 ���߸�������ʾ���еĶ������� ack
	char cmd[128];
	float packetLossRatio = 0.2; //Ĭ�ϰ���ʧ�� 0.2
	float ackLossRatio = 0.2;	 //Ĭ�� ACK ��ʧ�� 0.2
								 //��ʱ����Ϊ������ӣ�����ѭ����������
	srand((unsigned)time(NULL));
	while (true)
	{
		gets_s(buffer);
		ret = sscanf(buffer, "%s%f%f", &cmd, &packetLossRatio, &ackLossRatio);
		//��ʼ GBN ���ԣ�ʹ�� GBN Э��ʵ�� UDP �ɿ��ļ�����
		if (!strcmp(cmd, "-testsr"))
		{
			printf("%s\n", "Begin to test GBN protocol, please don't abort theprocess");
			printf("The loss ratio of packet is %.2f,the loss ratio of ack is %.2f\n", packetLossRatio, ackLossRatio);
			int stage = 0;
			BOOL b;
			unsigned char u_code;   //״̬��
			unsigned short seq;		//�������к�
			unsigned short recvSeq; //���մ��ڴ�СΪ 1����ȷ�ϵ����к�
			unsigned short waitSeq; //�ȴ������к�
			unsigned short rcvBase;
			sendto(socketClient, "-testsr", strlen("-testsr") + 1, 0,
				(SOCKADDR *)&addrServer, sizeof(SOCKADDR));
			while (true)
			{
				//�ȴ� server �ظ����� UDP Ϊ����ģʽ
				recvfrom(socketClient, buffer, BUFFER_LENGTH, 0, (SOCKADDR *)&addrServer, &len);
				switch (stage)
				{
				case 0: //�ȴ����ֽ׶�
					u_code = (unsigned char)buffer[0];
					if ((unsigned char)buffer[0] == 205)
					{
						printf("Ready for file transmission\n");
						buffer[0] = 200;
						buffer[1] = '\0';
						sendto(socketClient, buffer, 2, 0, (SOCKADDR *)&addrServer, sizeof(SOCKADDR));
						stage = 1;
						recvSeq = 0;
						waitSeq = 1;
					}
					break;
				case 1: //�ȴ��������ݽ׶�
					// ����Ϊ5
					seq = (unsigned short)buffer[0];
					//�����ģ����Ƿ�ʧ
					b = lossInLossRatio(packetLossRatio);
					if (b)
					{
						printf("The packet with a seq of %d loss\n", seq);
						continue;
					}
					printf("recv a packet with a seq of %d\n", seq);
					printf("waitSeq:%d		seq:%d\n", waitSeq, seq);
					//������ڴ��İ�����ȷ���գ�����ȷ�ϼ���
					if (!(waitSeq - seq))
					{
						++waitSeq;
						//�������
						//printf("%s\n",&buffer[1]);
						int i = 1;
						position = (position + 1) % 5;
						while (receiveBufferStage[position]) {
							printf("data in buffer[%d] is packet with a seq of %d\n", position, waitSeq);
							receiveBufferStage[position] = FALSE;
							position = (position + 1) % 5;
							++waitSeq;
						}
						if (waitSeq == 21)
						{
							waitSeq = 1;
						}
						else {
							if (waitSeq == 20)
								continue;
							else
								waitSeq = waitSeq % 20;
						}
						buffer[0] = seq;
						recvSeq = seq;
						buffer[1] = '\0';
					}
					else
					{
						if ((seq - waitSeq < 5 && seq > waitSeq)
							|| (waitSeq - seq > 15 && waitSeq > 10 && seq < 5)){
							int bufferSeq = (seq - waitSeq + position) % 5;
							if (bufferSeq < 0)
								bufferSeq = bufferSeq + 5;
							printf("the packet is not in order now BUFFER in %d\n", bufferSeq);
							strcpy(receiveBuffer[bufferSeq], &buffer[1]);
							receiveBufferStage[bufferSeq] = TRUE;
							buffer[0] = seq;
							buffer[1] = '\0';
						}
						else {
							if ((seq - waitSeq >= 5 && seq - waitSeq < 10) || 
								(waitSeq - seq <= 15 && waitSeq - seq >10 
								&& waitSeq > 10 && seq < 5)) {
								printf("the packet %d is ignore\n", seq);
								buffer[0] = waitSeq - 1;
								buffer[1] = '\0';
							}
							else {
								//�����ǰһ������û���յ�����ȴ� Seq Ϊ 1 �����ݰ��������򲻷��� ACK����Ϊ��û����һ����ȷ�� ACK��
								if (!recvSeq)
								{
									continue;
								}
								buffer[0] = seq;
								buffer[1] = '\0';
							}
						}
					}
					b = lossInLossRatio(ackLossRatio);
					if (b)
					{
						printf("The ack of %d loss\n", (unsigned char)buffer[0]);
						continue;
					}
					sendto(socketClient, buffer, 2, 0,
						(SOCKADDR *)&addrServer, sizeof(SOCKADDR));
					printf("send a ack of %d\n", (unsigned char)buffer[0]);
					break;
				}
				Sleep(500);
			}
		}
		sendto(socketClient, buffer, strlen(buffer) + 1, 0, (SOCKADDR *)&addrServer, sizeof(SOCKADDR));
		ret = recvfrom(socketClient, buffer, BUFFER_LENGTH, 0, (SOCKADDR *)&addrServer, &len);
		printf("%s\n", buffer);
		if (!strcmp(buffer, "Good bye!"))
		{
			break;
		}
		printTips();
	}
	//�ر��׽���
	closesocket(socketClient);
	WSACleanup();
	return 0;
}