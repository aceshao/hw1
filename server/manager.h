#ifndef MANAGER_H
#define MANAGER_H

#include "socket.h"
#include <queue>
#include "thread.h"
#include <vector>
#include "config.h"

using namespace std;



class ResourceManager
{
public:
	ResourceManager();
	~ResourceManager();

	int Insert(PeerInfo* pi);
	int LookUp(string filename, PeerInfo** pi);
	int Update(PeerInfo* pi);
	int UpdateAckTime(unsigned int time, string ip, int port );


private:
	Mutex* m_mtxResource;
	vector<PeerInfo*> m_vecPeerInfo;

};

typedef queue<Socket*> RequestQueue;
//process function for each thread
void* Process(void* arg);

class Manager
{
	friend void* Process(void* arg);
public:
	Manager();
	~Manager();

	int Start();

protected:
	int Init();
	int Listen();
	int Loop();

private:
	Socket* m_pSocket;
	RequestQueue m_rq;
	ResourceManager m_pResMan;
	

	Sem* m_semRequest;
	Mutex* m_mtxRequest;

	vector<Thread*> m_vecProcessThread;
};






#endif
