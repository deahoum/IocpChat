#pragma comment(lib, "ws2_32.lib")
#include <stdio.h>
#include <tchar.h>
#include <stdlib.h>
#include <process.h>
#include <WinSock2.h>
#include <Windows.h>
#include <WS2tcpip.h>
#include <list>
#include <MSWSock.h>
#include <mmsystem.h>
#include <fcntl.h>
#include <io.h>

using namespace std;

#define SOCKET_BUFSIZE		(1024*10)
#define MAX_CLNT			256

typedef enum
{
	IO_READ,
	IO_WRITE,
}IO_OPERATION;

struct IO_DATA
{
	OVERLAPPED overlapped;
	WSABUF	 wsaBuf;
	IO_OPERATION ioType;

	int totalBytes;
	int currentBytes;
	char buffer[SOCKET_BUFSIZE];
};

struct SOCKET_DATA
{
	SOCKET socket;
	SOCKADDR_IN addrInfo;
	char ipAddress[16];
	IO_DATA ioData;
};

DWORD WINAPI AcceptThread(LPVOID context);
DWORD WINAPI WorkerThread(LPVOID context);

SOCKET listen_socket;

static int clientCount = 0;
static SOCKET clntSock[MAX_CLNT];
static CRITICAL_SECTION cs;

int _tmain(int argc, _TCHAR* argv[])
{
	SYSTEM_INFO sysInfo;
	HANDLE iocp;

	_tprintf(L"* Server Starting\n");

	WSADATA wsa;
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
	{
		_tprintf(L"! wsa startup fail\n");
		return 1;
	}

	_tprintf(L"# Initializie network base\n");

	GetSystemInfo(&sysInfo);
	iocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, NULL, sysInfo.dwNumberOfProcessors);
	if (!iocp)
	{
		_tprintf(L"* iocp not created io completion port.\n");
		return 1;
	}

	InitializeCriticalSection(&cs);

	for (unsigned int i = 0; i < sysInfo.dwNumberOfProcessors; i++)
	{
		HANDLE thread = CreateThread(NULL, 0, WorkerThread, iocp, 0, 0);
		CloseHandle(thread);
	}

	listen_socket = WSASocket(AF_INET, SOCK_STREAM, NULL, NULL, NULL, WSA_FLAG_OVERLAPPED);
	if (listen_socket == INVALID_SOCKET)
	{
		return false;
	}

	SOCKADDR_IN serverAddr;
	serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	serverAddr.sin_port = htons(9000);
	serverAddr.sin_family = AF_INET;

	try
	{
		int socketError = ::bind(listen_socket, (SOCKADDR*)&serverAddr, sizeof(serverAddr));
		if (socketError == SOCKET_ERROR)
		{
			throw "! bind error";
		}

		socketError = ::listen(listen_socket, 5);
		if (socketError == SOCKET_ERROR)
		{
			throw "! listen error";
		}
		_tprintf(L"# server listening...\n");
	}
	catch (TCHAR* msg)
	{
		_tprintf(msg);
	}

	AcceptThread(iocp);

	DeleteCriticalSection(&cs);
	::closesocket(listen_socket);
	WSACleanup();
	_tprintf(L"# End network base\n");
	return 0;
}

void CloseClient(SOCKET_DATA *socket)
{
	if (!socket)
	{
		return;
	}

	::closesocket(socket->socket);
	delete socket;
	socket = nullptr;
}

DWORD WINAPI AcceptThread(LPVOID iocpHandler)
{
	HANDLE iocp = (HANDLE)iocpHandler;
	while (true)
	{
		SOCKET acceptSocket = INVALID_SOCKET;
		SOCKADDR_IN recvAddr;
		static int addrLen = sizeof(recvAddr);
		acceptSocket = WSAAccept(listen_socket, (struct sockaddr*)&recvAddr, &addrLen, NULL, 0);
		if (acceptSocket == SOCKET_ERROR)
		{
			_tprintf(L"! Accept fail\n");
			continue;
		}

		EnterCriticalSection(&cs);
		clntSock[clientCount++] = acceptSocket;
		LeaveCriticalSection(&cs);

		getpeername(acceptSocket, (struct sockaddr*)&recvAddr, &addrLen);
		char clientAddr[64];
		inet_ntop(AF_INET, &(recvAddr.sin_addr), clientAddr, _countof(clientAddr));

		SOCKET_DATA *session = new SOCKET_DATA;
		if (session == NULL)
		{
			_tprintf(L"! accept alloc fail\n");
		}

		ZeroMemory(session, sizeof(SOCKET_DATA));
		session->socket = acceptSocket;
		strcpy_s(session->ipAddress, clientAddr);

		session->ioData.ioType = IO_READ;
		session->ioData.totalBytes = sizeof(session->ioData.buffer);
		session->ioData.currentBytes = 0;
		session->ioData.wsaBuf.buf = session->ioData.buffer;
		session->ioData.wsaBuf.len = sizeof(session->ioData.buffer);

		iocp = CreateIoCompletionPort((HANDLE)acceptSocket, iocp, (DWORD)session, NULL);
		if (!iocp)
		{
			::closesocket(acceptSocket);
			return NULL;
		}

		session->socket = acceptSocket;
		IO_DATA ioData = session->ioData;
		DWORD flags = 0;
		DWORD recvBytes;
		DWORD errorCode = WSARecv(session->socket, &(ioData.wsaBuf), 1, &recvBytes, &flags, &(ioData.overlapped), NULL);
		if (errorCode == SOCKET_ERROR && (WSAGetLastError() != ERROR_IO_PENDING))
		{
			CloseClient(session);
		}
	}

	return 0;
}

DWORD WINAPI WorkerThread(LPVOID iocpHandler)
{
	HANDLE iocp = (HANDLE)iocpHandler;

	while (true)
	{
		IO_DATA * ioData = NULL;
		SOCKET_DATA *session = NULL;
		DWORD trafficSize;

		BOOL success = GetQueuedCompletionStatus(iocp, &trafficSize, (LPDWORD)&session, (LPOVERLAPPED*)&ioData, INFINITE);

		if (!success)
		{
			_tprintf(L"# queue data getting fail\n");
			continue;
		}

		if (session == NULL)
		{
			_tprintf(L"! socket data broken\n");
			return 0;
		}

		if (trafficSize == 0)
		{
			CloseClient(session);
			continue;
		}

		ioData->currentBytes += trafficSize;
		DWORD flags = 0;

		switch (ioData->ioType)
		{
		case IO_WRITE:
		{
			ioData->wsaBuf.buf[trafficSize] = '\0';
			_tprintf(L"@ send message : %s\n", ioData->wsaBuf.buf);
		}
			break;
		case IO_READ:
		{
			ioData->ioType = IO_WRITE;
			ioData->wsaBuf.len = trafficSize;
			flags = 0;
			for (int i = 0; i < clientCount; i++)
			{
				DWORD sendBytes;
				DWORD errorCode = WSASend(clntSock[i], &(ioData->wsaBuf), 1, &sendBytes, flags, &(ioData->overlapped), NULL);
				if (errorCode == SOCKET_ERROR && (WSAGetLastError() != ERROR_IO_PENDING))
				{
					CloseClient(session);
					_tprintf(L"! error code = %d\n", errorCode);
				}
			}

			ioData->wsaBuf.buf[trafficSize] = '\0';
			_tprintf(L"@ recv client message : %s\n", ioData->wsaBuf.buf);

			ioData->wsaBuf.len = SOCKET_BUFSIZE;
			{
				ZeroMemory(ioData->buffer, sizeof(ioData->buffer));
				ioData->ioType = IO_READ;
				DWORD recvBytes;
				DWORD errorCode = WSARecv(session->socket, &(ioData->wsaBuf), 1, &recvBytes, &flags, &(ioData->overlapped), NULL);
				if (errorCode == SOCKET_ERROR && (WSAGetLastError() != ERROR_IO_PENDING))
				{
					CloseClient(session);
				}
			}
		}
			break;
		}

	}
}



