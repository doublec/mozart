#ifndef __CONNECTION_HH
#define __CONNECTION_HH

class DSite;
class ComObj;
class TransController;

void doConnect(ComObj *comObj);
void transObjReady(ComObj *comObj, TransObj *transObj);
void comObjDone(ComObj *comObj);

Bool initAccept();
int acceptHandler(int fd,void *unused);

void handback(ComObj *comObj, TransObj *transObj);

void changeMaxTCPCacheImpl();

// Used by dpMiscModule
void setIPAddress__(int adr);
int  getIPAddress();
void setIPPort__(int port);
int getIPPort();

#endif