#include <sys/resource.h>
#include <stdlib.h>
#include <stdio.h>
#include "manager.h"
#include <errno.h>
#include <unistd.h>

int Daemon(int maxfd, int coredump)
{
	struct rlimit rl;
	if(maxfd)
	{
		rl.rlim_cur = maxfd;
		rl.rlim_max = maxfd;
		if(setrlimit(RLIMIT_NOFILE, &rl) == -1)
		{
			printf("set resource number of files limit failed. error code[%d]", errno);
			return -1;
		}
	}

	if(coredump)
	{
		rl.rlim_cur = RLIM_INFINITY;
		rl.rlim_max = RLIM_INFINITY;
		if(setrlimit(RLIMIT_CORE, &rl) == -1)
		{
			printf("set resource coredump failed. error code[%d]", errno);
			return -1;
		}
	}

//	return daemon(1, 0);
	return 0;
}

int main()
{
	if(Daemon(1024, 1) < 0)
	{
		printf("daemon start failed");
		return -1;
	}

	Manager manager;
	manager.Start();
	
	while(1)
	{
           sleep(2);
	}
	
	// it should never return
	return -1;
}

















