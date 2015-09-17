#ifndef TOOLS_H
#define TOOLS_H

#include <stdio.h>

string  iTo4ByteString(int iNum)
{
	char szName[12] = {0};
	sprintf(szName,"%4d",iNum);
	return szName;
}



#endif
