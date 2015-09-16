#include <unistd.h>
#include <errno.h>
#include <iostream>
#include <string.h>
#include <string>
#include <cstdlib>
#include "manager.h"
#include "config.h"
#include <dirent.h>
#include <sys/epoll.h>
#include "tools.h"
#include <ofstream>
#include <fstream>
using namespace std;

Manager::Manager()
{
	m_pSocket = NULL;
	m_pClientSock = NULL;
	m_semRequest = NULL;
	m_mtxRequest = NULL;
	m_pUserProcess = NULL;
}

Manager::~Manager()
{
	if(m_pClientSock)
	{
		delete m_pClientSock;
		m_pClientSock = NULL;
	}
	if(m_pSocket)
	{
		delete m_pSocket;
		m_pSocket = NULL;
	}
	if(m_semRequest)
	{
		delete m_semRequest;
		m_semRequest = NULL;
	}
	if(m_mtxRequest)
	{
		delete m_mtxRequest;
		m_mtxRequest = NULL;
	}
	for(unsigned int i = 0; i < m_vecProcessThread.size(); i++)
	{
		if(m_vecProcessThread[i])
		{
			delete m_vecProcessThread[i];
			m_vecProcessThread[i] = NULL;
		}
	}
	if(m_pUserProcess)
	{
		delete m_pUserProcess;
		m_pUserProcess = NULL;
	}
}

int Manager::Start()
{
	int pid = fork();
	if(pid == -1)
	{
		cout<<"fork failed"<<endl;
		return -1;
	}
	else if (pid == 0)
	{
		if(Init() < 0)
		{
			cout<<"manager init failed"<<endl;
			return 0;
		}
		if(Listen() < 0)
		{
			cout<<"manager listen failed"<<endl;
			return -1;
		}
		Loop();
	}
	return 0;
}

int Manager::Init()
{
	m_pSocket = new Socket(peer_serverIp.c_str(), peer_serverPort, ST_TCP);
	m_semRequest = new Sem(0, 0);
	m_mtxRequest = new Mutex();
	for(unsigned int i = 0; i < peer_serverThreadNumber; i++)
	{
		Thread* thread = new Thread(Process, this);
		m_vecProcessThread.push_back(thread);
	}

	m_pUserProcess = new Thread(UserCmdProcess, this);

	return 0;
}

int Manager::Listen()
{
	if (m_pSocket->Create() < 0)
	{
		cout<<"socket create failed"<<endl;
		return -1;
	}
	if(m_pSocket->Bind() < 0)
	{
		cout<<"socket bind failed"<<endl;
		return -1;
	}
	if(m_pSocket->Listen() < 0)
	{
		cout<<"socket listen failed"<<endl;
		return -1;
	}
	cout<<"now begin listen"<<endl;
	return 0;
}

int Manager::Loop()
{
	int listenfd = m_pSocket->GetSocket();
	struct epoll_event ev, events[MAX_EPOLL_FD];
	int epfd = epoll_create(MAX_EPOLL_FD);
	ev.data.fd = listenfd;
	ev.events = EPOLLIN;
	epoll_ctl(epfd, EPOLL_CTL_ADD, listenfd, &ev);
	while(1)
	{
		int nfds = epoll_wait(epfd, events, MAX_EPOLL_FD, -1);
		if(nfds <= 0) continue;
		for (int i = 0; i < nfds; i++)
		{
			Socket* s = new Socket();
			int iRet = m_pSocket->Accept(s);
			if(iRet < 0)
			{
				cout<<"socket accept failed"<<endl;
				continue;
			}
			m_mtxRequest->Lock();
			m_rq.push(s);
			m_semRequest->Post();
			m_mtxRequest->Unlock();
		}
		usleep(100000);
	}
}

void* Process(void* arg)
{
	Manager* pmgr = (Manager*)arg;
	while(1)
	{
		pmgr->m_semRequest->Wait();
		pmgr->m_mtxRequest->Lock();
		Socket* client = pmgr->m_rq.front();
		pmgr->m_rq.pop();
		pmgr->m_mtxRequest->Unlock();

		char szBuffer[MSG_HEAD_LEN] = {0};
		if(client->Recv(szBuffer, MSG_HEAD_LEN) != MSG_HEAD_LEN)
		{
			cout<<"recv failed"<<endl;
			client->Close();
			continue;
		}
		MsgPkg* msg = (MsgPkg*)szBuffer;
		char* szData = new char[msg->msglength + 1];
		bzero(szData, msg->msglength + 1);
		if(msg->msglength != 0)
		{
			if(client->Recv(szData, msg->msglength) != msg->msglength)
			{
				cout<<"recv failed"<<endl;
				client->Close();
				delete [] szData;
				continue;
			}
		}

		switch(msg->msgcmd)
		{
			case MSG_CMD_DOWNLOAD:
			{
				string downloadFilename = szData;
				delete [] szData;

				string dirname = "";
				DIR* dir = NULL;
				struct dirent* direntry;

				if((dir = opendir(fileBufferDir)) != NULL)
				{
					bool findDownloadFile = false;
					int fileLen = 0;
					while((direntry = readdir(dir)) != NULL)
					{
						string fn = direntry->d_name;
						if(fn == downloadFilename)
						{
							findDownloadFile = true;
							fileLen = direntry->d_reclen;
							break;
						}
					}
					if(findDownloadFile)
					{
						char filepath [100] = {0};
						strncpy(filepath, fileBufferDir, 99);
						strncpy(filepath+strlen(fileBufferDir), downloadFilename.c_str(), 99 - strlen(fileBufferDir));
						ifstream istream (filepath, std::ifstream::binary);
						char* buffer = new char[fileLen + sizeof(MsgPkg)];
						MsgPkg* msg = (MsgPkg*)buffer;
						msg->msgcmd = MSG_CMD_DOWNLOAD_RESPONSE;
						msg->msglength = fileLen;
						istream.read(buffer + sizeof(MsgPkg), fileLen);
						istream.close();

						client->Send(buffer, fileLen+sizeof(MsgPkg));
						delete [] buffer;
					}
					else
					{
						char buffer[MSG_HEAD_LEN] = {0};
						MsgPkg* msg = (MsgPkg*)buffer;
						msg->msgcmd = MSG_CMD_DOWNLOAD_RESPONSE;
						msg->msglength = 0;
						client->Send(buffer, MSG_HEAD_LEN);
					}
				}
				else
				{
					char buffer[MSG_HEAD_LEN] = {0};
					MsgPkg* msg = (MsgPkg*)buffer;
					msg->msgcmd = MSG_CMD_DOWNLOAD_RESPONSE;
					msg->msglength = 0;
					client->Send(buffer, MSG_HEAD_LEN);
				}
				client->Close();
				break;	
			}
			default:
			cout<<"msg not support"<<endl;
		}
	}
return 0;
}

int Manager::Register()
{
	string dirname = "";
	DIR* dir = NULL;
	struct dirent* direntry;

	char* registerPkg = new char[MAX_PKG_LEN];
	MsgPkg* msg = (MsgPkg*)registerPkg;
	msg->msgcmd = MSG_CMD_REGISTER;
	msg->msglength = 0;
	RegisterPkg* reg = (RegisterPkg*)(registerPkg + sizeof(MsgPkg));

	cout<<"please input the directory to register"<<endl;
	cin >> dirname;
	if((dir = opendir(dirname.c_str())) == NULL)
	{
		cout<<"can not open this directory: "<<dirname<<endl;
		delete [] registerPkg;
		return -1;
	}
	while((direntry = readdir(dir)) != NULL)
	{
		strncpy(reg, iTo4ByteString(direntry->d_reclen).c_str(), 4);
		reg += 4;
		strncpy(reg, direntry->d_name, direntry->d_reclen);
		reg += direntry->d_reclen;
		msg->msglength += 4
		msg->msglength += direntry->d_reclen;
	}

	if( m_pClientSock->Connect() != 0)
	{
		cout<<"connect to index server failed"<<endl;
		delete [] registerPkg;
		return -1;		
	}

	m_pClientSock->Send(registerPkg, sizeof(MsgPkg) + msg->msglength);
	m_pClientSock->Close();
	delete m_pClientSock;
	m_pClientSock = NULL;
	delete [] registerPkg;
	return 0;
}

int Manager::SearchFile(string filename, string& ip, int& port)
{
	char* searchPkg = new char[MAX_PKG_LEN];
	MsgPkg* msg = (MsgPkg*)searchPkg;
	msg->msgcmd = MSG_CMD_SEARCH;
	msg->msglength = filename.length();
	strncpy(msg+sizeof(MsgPkg), filename.c_str(), filename.length());

	if(m_pClientSock->Connect() != 0)
	{
		cout<<"search file connect to index server failed"<<endl;
		delete [] searchPkg;
		return -1;
	}

	m_pClientSock->Send(searchPkg, sizeof(MsgPkg) + msg->msglength);
	delete [] searchPkg;

	char szBuffer[MSG_HEAD_LEN] = {0};
	if (m_pClientSock->Recv(szBuffer, MSG_HEAD_LEN) != MSG_HEAD_LEN)
	{
		cout<<"search file recv from index server failed"<<endl;
		m_pClientSock->Close();
		return -1;
	}

	msg = (MsgPkg*)szBuffer;
	if(msg->msgcmd != MSG_CMD_SEARCH_RESPONSE)
	{
		cout<<"search file recv from index server cmd error"<<endl;
		m_pClientSock->Close();
		return -1;
	}
	if(msg->msglength == 0)
	{
		port = 0;
		ip = "";
	}
	else
	{
		char* searchResult = new char[msg->msglength];
		if(m_pClientSock->Recv(searchResult, msg->msglength) ! msg->msglength)
		{
			cout<<"search file recv from index server failed"<<endl;
			m_pClientSock->Close();
			delete [] searchResult;
			return -1;
		}

		SearchResponsePkg* sr = (SearchResponsePkg*)searchResult;
		port = sr->port;
		ip = sr->ip;
	}

	m_pClientSock->Close();
	delete [] searchResult;
	return 0;
}

int Manager::DownloadFile(string filename, string ip, int port)
{
	char downloadPkg[MAX_PKG_LEN] = {0};
	MsgPkg* msg = (MsgPkg*)downloadPkg;
	msg->msgcmd = MSG_CMD_DOWNLOAD;
	msg->msglength = filename.length();
	DownloadPkg* down = (DownloadPkg*)(downloadPkg + sizeof(MsgPkg));
	strncpy(down->file, filename.c_str(), filename.length());

	Socket downloadSocket = Socket(ip.c_str(), port, ST_TCP);
	if(downloadSocket.Create() != 0)
	{
		cout<<"download socket create failed"<<endl;
		return -1;
	}
	if(downloadSocket.Connect() != 0)
	{
		cout<<"download socket connect failed"<<endl;
		return -1;
	}
	if(downloadSocket.Send(downloadPkg, msg->msglength + sizeof(MsgPkg)) != msg->msglength + sizeof(MsgPkg))
	{
		cout<<"download socket send failed"<<endl;
		downloadSocket.Close();
		return -1;
	}

	char szBuffer[MSG_HEAD_LEN] = {0};
	if(downloadSocket.Recv(szBuffer, MSG_HEAD_LEN) != MSG_HEAD_LEN)
	{
		cout<<"downlaod socket recv failed"<<endl;
		downloadSocket.Close();
		return -1;
	}
	msg = (MsgPkg*)szBuffer;
	if(msg->msgcmd != MSG_CMD_DOWNLOAD_RESPONSE)
	{
		cout<<"download response cmd error"<<endl;
		return -1;
	}
	
	if(msg->msglength <= 0)
	{
		cout<<"peer server ip: "<<ip<<" port: "<<port<<" does not contain file: "<< filename<<endl;
		return 0;
	}
	else
	{
		char* file = new char[msg->msglength];
		if(downloadSocket.Recv(file, msg->msglength) != msg->msglength)
		{
			cout<<"download recv failed"<<endl;
			downloadSocket.Close();
			delete [] file;
			return -1;
		}

		ofstream out;
		out.open(filename.c_str(), ios::out|ios::binary);
		out.write(file, msg->msglength);
		out.close();
		delete [] file;
	}
	return 0;
}

void* UserCmdProcess(void* arg)
{
	cout<<"Welcome to the peer client. download happy~~"<<endl;
	Manager* mgr = (Manager*)arg;
	mgr->m_pClientSock->Create();

	if(mgr->Register() != 0)
		cout<<"Register failed"<<endl;
	cout<<"Register success"<<endl;

	while(1)
	{
		stirng filename = "";
		cout<<"enter the file name you wanna download"<<endl;
		cin>>filename;

		string ip = "";
		int port = 0;
		if(mgr->SearchFile(filename, ip, port) != 0)
		{
			cout<<"search file: "<<filename<<" failed"<<endl;
			continue;
		}

		if(ip == "" && port == 0)
		{
			cout<<"file :"<<filename<<" Not exist"<<endl;
			continue;
		}
		cout<<"search file: "<<filename<< "success"<<endl;
		cout<<"peer server :"<<ip <<" port :"<<port<<" has this file"<<endl;

		cout<<"if you wanna to downlaod, press y"<<endl;
		char respond;
		cin>>respond;

		if(respond != 'y' || respond != 'Y')
			continue;

		if(mgr->DownloadFile(filename, ip, port) != 0)
		{
			cout<<"download file failed"<<endl;
			continue;
		}

	}
	return 0;
}





