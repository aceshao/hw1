#ifndef TOOLS_H
#define TOOLS_H

string  iTo4ByteString(int iNum)
{
	char szName[12] = {0};
	sprintf(szName,"%4d",iNum);
	return szName;
}



#endif