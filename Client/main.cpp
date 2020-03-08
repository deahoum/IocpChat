#define _WINSOCK_DEPRECATED_NO_WARNINGS
#pragma comment(lib, "ws2_32.lib")
#include <stdio.h>
#include <stdlib.h>
#include <WinSock2.h>

#define BUF_SIZE 1024
void ErrorHandling(char* message);

DWORD WINAPI ChatReceivedThread(void *arg);
DWORD WINAPI ChatSendThread(void *arg);

int main(int argc, char *argv[])
{
	WSADATA wsaData;
	SOCKET hSocket;
	SOCKADDR_IN servAdr;

	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
	{
		ErrorHandling("WSAStartup() error");
	}

	hSocket = socket(PF_INET, SOCK_STREAM, 0);
	if (hSocket == INVALID_SOCKET)
	{
		ErrorHandling("socket() error");
	}

	ZeroMemory(&servAdr, sizeof(servAdr));
	servAdr.sin_family = AF_INET;
	servAdr.sin_addr.s_addr = inet_addr("127.0.0.1");
	servAdr.sin_port = htons(9000);

	if (connect(hSocket, (SOCKADDR*)&servAdr, sizeof(servAdr)) == SOCKET_ERROR)
	{
		ErrorHandling("connect() error");
	}
	else
	{
		puts("Connected.....");
	}

	HANDLE receiveThread = CreateThread(NULL, 0, ChatReceivedThread, (void*)&hSocket, 0, 0);
	HANDLE sendThread = CreateThread(NULL, 0, ChatSendThread, (void*)&hSocket, 0, 0);

	WaitForSingleObject(receiveThread, INFINITE);
	WaitForSingleObject(sendThread, INFINITE);

	closesocket(hSocket);
	WSACleanup();
	return 0;
}

void ErrorHandling(char* message)
{
	fputs(message, stderr);
	fputc('\n', stderr);
	exit(1);
}

DWORD WINAPI ChatSendThread(void *arg)
{
	SOCKET hSock = *((SOCKET*)arg);
	char msg[BUF_SIZE];
	while (1)
	{
		fgets(msg, BUF_SIZE, stdin);
		if (!strcmp(msg, "q\n") || !strcmp(msg, "Q\n"))
		{
			closesocket(hSock);
			exit(0);
		}
		send(hSock, msg, strlen(msg), 0);
	}

	return 0;
}

DWORD WINAPI ChatReceivedThread(void *arg)
{
	int hSock = *((SOCKET*)arg);
	int strLen;
	char msg[BUF_SIZE];
	while (1)
	{
		strLen = recv(hSock, msg, BUF_SIZE, 0);
		if (strLen == -1)
		{
			return -1;
		}
		msg[strLen] = 0;
		fputs(msg, stdout);
	}

	return 0;
}