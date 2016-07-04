#define _WINSOCK_DEPRECATED_NO_WARNINGS
#pragma comment (lib,"ws2_32.lib")
#include <stdio.h>
#include <iostream>
#include <WinSock2.h>
#include <string.h>

using namespace std;
#define BUF_SIZE 1024


void Errhandling(char *msg) {
  puts(msg);
  putc('\n', stdout);
  exit(1);
}
int main() {
  WSADATA wsaData;
  SOCKET ServSock, ClntSock;
  SOCKADDR_IN servAddr, clntAddr;

  SOCKET sockarr[WSA_MAXIMUM_WAIT_EVENTS]; //64
  WSAEVENT eventarr[WSA_MAXIMUM_WAIT_EVENTS];
  WSAEVENT newEvent;
  WSANETWORKEVENTS netEvents; // signaled상태의 소켓, 받아올 이벤트 정보 구조체

  int numOfclntSock = 0;
  int strLen, i;
  int posInfo, startIdx;
  int clntAddrlen;
  char msg[BUF_SIZE];

  if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
    Errhandling("wsatartup err");
  }
  ServSock = socket(PF_INET, SOCK_STREAM, 0);
  memset(&servAddr, 0, sizeof(servAddr));
  servAddr.sin_family = AF_INET;
  servAddr.sin_addr.S_un.S_addr = htonl(INADDR_ANY);
  servAddr.sin_port = htons(9090);

  if (bind(ServSock, (SOCKADDR*)&servAddr, sizeof(servAddr)) == SOCKET_ERROR) {
    Errhandling("bind err");
  }
  if (listen(ServSock, 5) == SOCKET_ERROR) {
    Errhandling("listen err");
  }

  newEvent = WSACreateEvent();
  if (WSAEventSelect(ServSock, newEvent, FD_ACCEPT) == SOCKET_ERROR) { // ACCEPT 이벤트 발생하면 newEvent가 signaled상태
    Errhandling("wsaselect err");
  }
  //소켓 추가
  sockarr[numOfclntSock] = ServSock;
  eventarr[numOfclntSock] = newEvent;
  numOfclntSock++;

  while (1) {
    posInfo = WSAWaitForMultipleEvents(numOfclntSock, eventarr, FALSE, WSA_INFINITE, FALSE);
    startIdx = posInfo - WSA_WAIT_EVENT_0;

    for (i = startIdx; i < numOfclntSock; i++) {
      int signaled_idx = WSAWaitForMultipleEvents(1, &eventarr[i], TRUE, 0, FALSE); // timeout이 0이면 signaled상태만 판단. 바로 리턴한다.
      if (signaled_idx == WSA_WAIT_FAILED || signaled_idx == WSA_WAIT_TIMEOUT) {
        continue;
      }
      else {
        signaled_idx = i;
        WSAEnumNetworkEvents(sockarr[signaled_idx], eventarr[signaled_idx], &netEvents);
        if (netEvents.lNetworkEvents & FD_ACCEPT) { // ACCEPT시
          if (netEvents.iErrorCode[FD_ACCEPT_BIT] != 0) { // 에러시 0이 아닌 값이 들어감
            puts("accept err");
            break;
          }
          clntAddrlen = sizeof(clntAddr);
          ClntSock = accept(sockarr[signaled_idx], (SOCKADDR*)&clntAddr, &clntAddrlen);
          newEvent = WSACreateEvent();
          WSAEventSelect(ClntSock, newEvent, FD_READ | FD_CLOSE); // 소켓과 이벤트 등록 read, close일때 event signaeld상태로 변환
          sockarr[numOfclntSock] = ClntSock;
          eventarr[numOfclntSock] = newEvent;
          numOfclntSock++;
          printf("%s clint connected..\n", inet_ntoa(clntAddr.sin_addr));
        }// accept

        if (netEvents.lNetworkEvents & FD_READ) {
          if (netEvents.iErrorCode[FD_READ_BIT] != 0) { // 에러시 0이 아닌 값이 들어감
            puts("read err");
            break;
          }
          strLen = recv(sockarr[signaled_idx], msg, BUF_SIZE, 0);
          if (strLen <= 0) {
            Errhandling("recv err");
            break;
          }
          fputs(msg, stdout);
          send(sockarr[signaled_idx], msg, sizeof(msg), 0);


          
        }//read

        if (netEvents.lNetworkEvents & FD_CLOSE) {
          if (netEvents.iErrorCode[FD_CLOSE_BIT] != 0) { // 에러시 0이 아닌 값이 들어감
            puts("close err");
            printf("code : %d, posidx : %d\n", netEvents.iErrorCode[FD_CLOSE_BIT],startIdx);
            
          }
          WSACloseEvent(eventarr[signaled_idx]);
          closesocket(sockarr[signaled_idx]);

          numOfclntSock--;
          for (i = signaled_idx; i < numOfclntSock; i++) {
            sockarr[i] = sockarr[i + 1];
            eventarr[i] = eventarr[i + 1];
          }
        }//close
      }
    }//for i
  }//while1
  WSACleanup();
  return 0;
}