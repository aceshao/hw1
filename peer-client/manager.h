#ifndef MANAGER_H
#define MANAGER_H

#include "socket.h"
#include <queue>
#include "thread.h"
#include <vector>
#include "model.h"
#include <string.h>
#include "config.h"

using namespace std;


typedef queue<Socket*> RequestQueue;
//process function for each thread
void* Process(void* arg);

void* UserCmdProcess(void* arg);

class Manager
{
	friend void* Process(void* arg);  // thread pool to handler all the file request
	friend void* UserCmdProcess(void* arg); //single thread to handler user input
public:
	Manager(string configfile = "../config/client.config");
	~Manager();

	int Start();
	int IsStoped();

protected:
	int Init();
	int Listen();
	int Loop();

	int Register();

	int Register_TEST();
	int SearchFile_TEST();
	int DownloadFile_TEST();

	int SearchFile(string filename, string* ip = NULL, int* port = NULL);
	int DownloadFile(string filename, string ip, int port);

	int SendFile();

private:
	Socket* m_pClientSock;
	Socket* m_pSocket;
	RequestQueue m_rq;

	string m_strServerIp;
	int m_iServerPort;

	string m_strPeerIp;
	int m_iPeerPort;
	int m_iPeerThreadPoolNum;

	string m_strPeerFileBufferDir;
	string m_strPeerFileDownloadDir;

	int m_iTestMode; 

	vector<string> m_vecIp;
	vector<int> m_vecPort;

	vector<FilePeer> m_vecTestFilePeer;

	Sem* m_semRequest;
	Mutex* m_mtxRequest;

	vector<Thread*> m_vecProcessThread;
	Thread* m_pUserProcess;
	
	int m_iPid;
	unsigned int m_iRegisterTestTimeelaspe;
    unsigned int m_iRegisterCount;
    unsigned int  m_iSearchTestTimeelaspe;
    unsigned int m_iSearchCount;
    unsigned int m_iDownloadTestTimeelaspe;
    unsigned int m_iDownloadCount;

};






#endif
