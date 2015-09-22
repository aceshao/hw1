#include <unistd.h>
#include <errno.h>
#include <iostream>
#include <string.h>
#include <string>
#include <cstdlib>
#include "manager.h"
#include <dirent.h>
#include <sys/epoll.h>
#include "tools.h"
#include <fstream>
#include <sys/time.h>
#include <arpa/inet.h>

using namespace std;

Manager::Manager(string configfile)
{
	m_pSocket = NULL;
	m_pClientSock = NULL;
	m_semRequest = NULL;
	m_mtxRequest = NULL;
	m_pUserProcess = NULL;

	Config* config = Config::Instance();
	if( config->ParseConfig(configfile.c_str(), "SYSTEM") != 0)
	{
		cout<<"parse config file:["<<configfile<<"] failed, exit"<<endl;
		exit(-1);
	}

	m_strServerIp = config->GetStrVal("SYSTEM", "serverip", "0.0.0.0");
	m_iServerPort = config->GetIntVal("SYSTEM", "serverport", 5550);

	m_strPeerIp = config->GetStrVal("SYSTEM", "peerip", "0.0.0.0");
	m_iPeerPort = config->GetIntVal("SYSTEM", "peerport", 5555);
	m_iPeerThreadPoolNum = config->GetIntVal("SYSTEM", "threadnum", 5);

	m_strPeerFileBufferDir = config->GetStrVal("SYSTEM", "filebufferdir", "./file/");

	m_iTestMode = config->GetIntVal("SYSTEM", "testmode", 0);
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
	m_pSocket = new Socket(m_strPeerIp.c_str(), m_iPeerPort, ST_TCP);
	m_semRequest = new Sem(0, 0);
	m_mtxRequest = new Mutex();
	for(int i = 0; i <= m_iPeerThreadPoolNum; i++)
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
	if(m_pSocket->SetSockAddressReuse(true) < 0)
		cout<<"set socket address reuse failed"<<endl;
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
return 0;
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
				
				cout<<"request to download file["<<downloadFilename<<"]"<<endl;
				string dirname = "";
				DIR* dir = NULL;
				struct dirent* direntry;

				if((dir = opendir(pmgr->m_strPeerFileBufferDir.c_str())) != NULL)
				{
					bool findDownloadFile = false;
					while((direntry = readdir(dir)) != NULL)
					{
						string fn = direntry->d_name;
						if(fn == downloadFilename)
						{
							findDownloadFile = true;
							break;
						}
					}
					if(findDownloadFile)
					{
						char filepath [100] = {0};
						strncpy(filepath, pmgr->m_strPeerFileBufferDir.c_str(), 99);
						strncpy(filepath+strlen(pmgr->m_strPeerFileBufferDir.c_str()), downloadFilename.c_str(), 99 - strlen(pmgr->m_strPeerFileBufferDir.c_str()));
						ifstream istream (filepath, std::ifstream::binary);
						istream.seekg(0, istream.end);
						int fileLen = istream.tellg();
						istream.seekg(0, istream.beg);
						char* buffer = new char[fileLen + sizeof(MsgPkg)];
						MsgPkg* msg = (MsgPkg*)buffer;
						msg->msgcmd = MSG_CMD_DOWNLOAD_RESPONSE;
						msg->msglength = fileLen;
						cout<<"file len["<<fileLen<<"]"<<endl;
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
						msg->msglength = -1;
						client->Send(buffer, MSG_HEAD_LEN);
					}
				}
				else
				{
					char buffer[MSG_HEAD_LEN] = {0};
					MsgPkg* msg = (MsgPkg*)buffer;
					msg->msgcmd = MSG_CMD_DOWNLOAD_RESPONSE;
					msg->msglength = -1;
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
	msg->msglength = 4;
	RegisterPkg* reg = (RegisterPkg*)(registerPkg + sizeof(MsgPkg));
	reg->port = m_iPeerPort;

	cout<<"please input the directory to register"<<endl;
	cin >> dirname;
	if((dir = opendir(dirname.c_str())) == NULL)
	{
		cout<<"Sorry! can not open this directory: "<<dirname<<endl;
		delete [] registerPkg;
		return -1;
	}
	int offset = 0;
	int count = 0;
	while((direntry = readdir(dir)) != NULL)
	{
		if(direntry -> d_type != DT_REG)
			continue;
		string nametemp = direntry->d_name;
		//strncpy(reg->filename + offset, iTo4ByteString(direntry->d_reclen).c_str(), 4);
		strncpy(reg->filename + offset, iTo4ByteString(nametemp.length()).c_str(), 4);
		offset += 4;
		strncpy(reg->filename + offset, direntry->d_name, nametemp.length());
		offset += nametemp.length();
		msg->msglength += 4;
		msg->msglength += nametemp.length();
		//cout<<"filename: " << direntry->d_name<<endl;
		//cout<<"filesize: " <<nametemp.length()<<endl;
		count++;
	}
	//cout<<"register file length:" <<msg->msglength<<endl;

	if( m_pClientSock->Connect() != 0)
	{
		cout<<"connect to index server failed"<<endl;
		delete [] registerPkg;
		return -1;		
	}

	m_pClientSock->Send(registerPkg, sizeof(MsgPkg) + msg->msglength);

	char* szBack = new char[sizeof(MsgPkg)];
	m_pClientSock->Recv(szBack, sizeof(MsgPkg));
	msg = (MsgPkg*)szBack;
	int iRet = 0;
	if(msg->msgcmd == MSG_CMD_REGISTER && msg->msglength == 0)
	{
		cout<<"Register Success. register total file number:" <<count<<endl;
		iRet = 0;		
	}
	else
	{
		cout<<"Register Failed"<<endl;
		iRet = -1;
	}
	m_pClientSock->Close();
	delete [] registerPkg;
	delete [] szBack;

	return iRet;
}

int Manager::Register_TEST()
{
	string dirname = "";
	DIR* dir = NULL;
	struct dirent* direntry;


	cout<<"please input the directory to register"<<endl;
	cin >> dirname;
	if((dir = opendir(dirname.c_str())) == NULL)
	{
		cout<<"Sorry! can not open this directory: "<<dirname<<endl;
		return -1;
	}
	int count = 0;
	struct timeval begin;
	gettimeofday(&begin, NULL);
	while((direntry = readdir(dir)) != NULL)
	{
		char* registerPkg = new char[MAX_PKG_LEN];
		bzero(registerPkg, MAX_PKG_LEN);
		char* szBack = new char[sizeof(MsgPkg)];
		MsgPkg* msg = (MsgPkg*)registerPkg;
		msg->msgcmd = MSG_CMD_REGISTER;
		RegisterPkg* reg = (RegisterPkg*)(registerPkg + sizeof(MsgPkg));
		reg->port = m_iPeerPort;
		int offset = 0;
		msg->msglength = 4;
		if(direntry -> d_type != DT_REG)
			continue;
		string nametemp = direntry->d_name;
		//strncpy(reg->filename + offset, iTo4ByteString(direntry->d_reclen).c_str(), 4);
		strncpy(reg->filename + offset, iTo4ByteString(nametemp.length()).c_str(), 4);
		offset += 4;
		strncpy(reg->filename + offset, direntry->d_name, nametemp.length());
		offset += nametemp.length();
		msg->msglength += 4;
		msg->msglength += nametemp.length();

		m_pClientSock->Create();
		if( m_pClientSock->Connect() != 0)
		{
			cout<<"connect to index server failed"<<endl;
			delete [] registerPkg;
			return -1;		
		}

		int length = msg->msglength;
		m_pClientSock->Send(registerPkg, sizeof(MsgPkg) + length);
		
		bzero(szBack, sizeof(MsgPkg));
		m_pClientSock->Recv(szBack, sizeof(MsgPkg));
		msg = (MsgPkg*)szBack;
		int iRet = 0;
		if(msg->msgcmd == MSG_CMD_REGISTER && msg->msglength == 0)
		{
			count++;
		}
		else
		{
			cout<<"Register Failed"<<endl;
		}
		m_pClientSock->Close();
		delete [] registerPkg;
		delete [] szBack;
	}

	

	

	struct timeval end;
	gettimeofday(&end, NULL);
	int timeelaspe = 1000000*end.tv_sec + end.tv_usec - begin.tv_usec - 1000000*begin.tv_sec;
	cout<<"Register total file:["<<count<<"] and cost: ["<< timeelaspe <<"] us"<<endl;
	if(count > 0)
		cout<<"Average time: [" << timeelaspe/count <<"]us"<<endl;
	return 0;
}


int Manager::SearchFile(string filename)
{
	m_vecIp.clear();
	m_vecPort.clear();
	char* searchPkg = new char[MAX_PKG_LEN];
	MsgPkg* msg = (MsgPkg*)searchPkg;
	msg->msgcmd = MSG_CMD_SEARCH;
	msg->msglength = filename.length();
	strncpy((char*)msg+sizeof(MsgPkg), filename.c_str(), filename.length());
	
	m_pClientSock->Create();

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
		// do nothing
	}
	else
	{
		char* searchResult = new char[msg->msglength + 1];
		bzero(searchResult, msg->msglength + 1);
		if(m_pClientSock->Recv(searchResult, msg->msglength) != msg->msglength)
		{
			cout<<"search file recv from index server failed"<<endl;
			m_pClientSock->Close();
			delete [] searchResult;
			return -1;
		}

		//string files = searchResult;
		cout<<"Get the search result: ["<<searchResult<<"]"<<endl;
		char* temp = NULL;
		bool isIp = false;
		char* brkt = NULL;
		for(temp = strtok_r(searchResult, &SPLIT_CHARACTER, &brkt); temp; temp = strtok_r(NULL, &SPLIT_CHARACTER, &brkt))
		{
			if(isIp)
			{
				m_vecIp.push_back(temp);
				isIp = false;
			}
			else
			{
				m_vecPort.push_back(atoi(temp));
				isIp = true;
			}

		}
		delete [] searchResult;
	}

	m_pClientSock->Close();
	return 0;
}

int Manager::DownloadFile(string filename, string ip, int port)
{
	cout<<"Begin to download from server: ["<< ip<<"] port: [" <<port<<"]"<< endl;
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
	if(downloadSocket.Send(downloadPkg, msg->msglength + sizeof(MsgPkg)) != msg->msglength + MSG_HEAD_LEN)
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
	
	if(msg->msglength < 0)
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
	cout<<"Welcome to the peer client. happy share~~"<<endl;
	Manager* mgr = (Manager*)arg;
	mgr->m_pClientSock = new Socket(mgr->m_strServerIp.c_str(), mgr->m_iServerPort, ST_TCP);
	cout<<"wanna to connect server["<<mgr->m_strServerIp<<"] and port: ["<<mgr->m_iServerPort<<"]"<<endl;
	mgr->m_pClientSock->Create();

	if(mgr->m_iTestMode != 0)
	{
		cout<<"This is in TEST MODE"<<endl;

		// calculate the average time for registeration
		mgr->Register_TEST();

		return 0;
	}

	while(1)
	{	
		if(mgr->Register() != 0)
		{
			cout<<"Register failed. Wanna try again please press y"<<endl;
			char respond;
			cin>>respond;
			if(respond == 'y' || respond == 'Y')
				continue;
			else
				break;
		}
		else
		{
			break;
		}
	}


	while(1)
	{
		string filename = "";
		cout<<"Please enter the file name you wanna download"<<endl;
		cin>>filename;

		if(mgr->SearchFile(filename) != 0)
		{
			cout<<"search file: "<<filename<<" failed"<<endl;
			continue;
		}

		if(mgr->m_vecIp.size() == 0)
		{
			cout<<"file :"<<filename<<" Not exist"<<endl;
			continue;
		}
		
		cout<<"Search file: "<<filename<< "success"<<endl;
		for(unsigned int i = 0; i < mgr->m_vecIp.size(); i++)
			cout<<"Peer server :"<<mgr->m_vecIp[i] <<" port :"<<mgr->m_vecPort[i]<<" has this file"<<endl;

		cout<<"If you wanna to downlaod, press number to reprensent server (0 represents the first peer server"<<endl;
		int respond;
		cin>>respond;

		if(respond < 0 || respond >= mgr->m_vecIp.size())
		{
			cout<<"You wanna to downlaod from the server which is non exist"<<endl;
			continue;
		}

		string ip = mgr->m_vecIp[respond];
		int port = mgr->m_vecPort[respond];
		if(mgr->DownloadFile(filename, ip, port) != 0)
		{
			cout<<"download file failed"<<endl;
			continue;
		}

	}
	return 0;
}






