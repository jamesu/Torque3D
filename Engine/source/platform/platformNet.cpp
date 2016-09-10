//-----------------------------------------------------------------------------
// Copyright (c) 2012 GarageGames, LLC
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to
// deal in the Software without restriction, including without limitation the
// rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
// sell copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
// IN THE SOFTWARE.
//-----------------------------------------------------------------------------

#include "platform/platformNet.h"
#include "platform/threads/mutex.h"
#include "core/strings/stringFunctions.h"
#include "core/util/hashFunction.h"

// jamesu - debug DNS
#define TORQUE_DEBUG_LOOKUPS


#if defined (TORQUE_OS_WIN)
#define TORQUE_USE_WINSOCK
#include <errno.h>
#include <ws2tcpip.h>

#ifndef EINPROGRESS
#define EINPROGRESS             WSAEINPROGRESS
#endif // EINPROGRESS

#define ioctl ioctlsocket

typedef S32 socklen_t;

#elif defined ( TORQUE_OS_MAC )

#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/poll.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <errno.h>
#include <sys/ioctl.h>

typedef sockaddr_in SOCKADDR_IN;
typedef sockaddr * PSOCKADDR;
typedef sockaddr SOCKADDR;
typedef in_addr IN_ADDR;
typedef int SOCKET;

#define INVALID_SOCKET -1
#define SOCKET_ERROR   -1

#define closesocket close

#elif defined TORQUE_OS_LINUX

#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/poll.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <errno.h>
#include <sys/ioctl.h>

typedef sockaddr_in SOCKADDR_IN;
typedef sockaddr_in6 SOCKADDR_IN6;
typedef sockaddr * PSOCKADDR;
typedef sockaddr SOCKADDR;
typedef in_addr IN_ADDR;
typedef in6_addr IN6_ADDR;
typedef int SOCKET;

#define INVALID_SOCKET -1
#define SOCKET_ERROR   -1

#define closesocket close

#elif defined( TORQUE_OS_XENON )

#include <Xtl.h>
#include <string>

#define TORQUE_USE_WINSOCK
#define EINPROGRESS WSAEINPROGRESS
#define ioctl ioctlsocket
typedef S32 socklen_t;

DWORD _getLastErrorAndClear()
{
   DWORD err = WSAGetLastError();
   WSASetLastError( 0 );

   return err;
}

#else

#endif

#if defined(TORQUE_USE_WINSOCK)
static const char* strerror_wsa( S32 code )
{
   switch( code )
   {
#define E( name ) case name: return #name;
      E( WSANOTINITIALISED );
      E( WSAENETDOWN );
      E( WSAEADDRINUSE );
      E( WSAEINPROGRESS );
      E( WSAEALREADY );
      E( WSAEADDRNOTAVAIL );
      E( WSAEAFNOSUPPORT );
      E( WSAEFAULT );
      E( WSAEINVAL );
      E( WSAEISCONN );
      E( WSAENETUNREACH );
      E( WSAEHOSTUNREACH );
      E( WSAENOBUFS );
      E( WSAENOTSOCK );
      E( WSAETIMEDOUT );
      E( WSAEWOULDBLOCK );
      E( WSAEACCES );
#undef E
      default:
         return "Unknown";
   }
}
#endif

#include "core/util/tVector.h"
#include "platform/platformNetAsync.h"
#include "console/console.h"
#include "core/util/journal/process.h"
#include "core/util/journal/journal.h"


NetSocket NetSocket::INVALID = NetSocket::fromHandle(-1);

template<class T> class ReservedSocketList
{
public:
   Vector<T> mSocketList;
   Mutex *mMutex;

   ReservedSocketList()
   {
      mMutex = new Mutex;
   }

   ~ReservedSocketList()
   {
      delete mMutex;
   }

   inline void modify() { Mutex::lockMutex(mMutex); }
   inline void endModify() { Mutex::unlockMutex(mMutex); }

   NetSocket reserve(SOCKET reserveId = -1, bool doLock = true)
   {
      MutexHandle handle;
      if (doLock)
      {
         handle.lock(mMutex, true);
      }

      S32 idx = mSocketList.find_next(-1);
      if (idx == -1)
      {
         mSocketList.push_back(reserveId);
         return NetSocket::fromHandle(mSocketList.size() - 1);
      }
      else
      {
         mSocketList[idx] = reserveId;
      }

      return NetSocket::fromHandle(idx);
   }

   void remove(NetSocket socketToRemove, bool doLock = true)
   {
      MutexHandle handle;
      if (doLock)
      {
         handle.lock(mMutex, true);
      }

      if ((U32)socketToRemove.getHandle() >= (U32)mSocketList.size())
         return;

      mSocketList[socketToRemove.getHandle()] = -1;
   }

   T activate(NetSocket socketToActivate, U8 family, bool clearOnFail = false)
   {
      MutexHandle h;
      h.lock(mMutex, true);

      if ((U32)socketToActivate.getHandle() >= (U32)mSocketList.size())
         return -1;

      T socketFd = mSocketList[socketToActivate.getHandle()];
      if (socketFd == -1)
      {
         socketFd = ::socket(family, SOCK_STREAM, 0);

         if (socketFd == InvalidSocketHandle)
         {
            if (clearOnFail)
            {
               remove(socketToActivate, false);
            }
            return InvalidSocketHandle;
         }
         else
         {
            mSocketList[socketToActivate.getHandle()] = socketFd;
            return socketFd;
         }
      }

      return socketFd;
   }

   T resolve(NetSocket socketToResolve)
   {
      MutexHandle h;
      h.lock(mMutex, true);

      if ((U32)socketToResolve.getHandle() >= (U32)mSocketList.size())
         return -1;

      return mSocketList[socketToResolve.getHandle()];
   }
};

static ReservedSocketList<SOCKET> smReservedSocketList;
const SOCKET InvalidSocketHandle = -1;

static Net::Error getLastError();
static S32 defaultPort = 28000;
static S32 netPort = 0;
static NetSocket udpSocket = NetSocket::INVALID;

ConnectionNotifyEvent   Net::smConnectionNotify;
ConnectionAcceptedEvent Net::smConnectionAccept;
ConnectionReceiveEvent  Net::smConnectionReceive;
PacketReceiveEvent      Net::smPacketReceive;

// local enum for socket states for polled sockets
enum SocketState
{
   InvalidState,
   Connected,
   ConnectionPending,
   Listening,
   NameLookupRequired
};

// the Socket structure helps us keep track of the
// above states
struct PolledSocket
{
   PolledSocket()
   {
      fd = -1;
      handleFd = NetSocket::INVALID;
      state = InvalidState;
      remoteAddr[0] = 0;
      remotePort = -1;
   }

   SOCKET fd;
   NetSocket handleFd;
   S32 state;
   char remoteAddr[256];
   S32 remotePort;
};

// list of polled sockets
static Vector<PolledSocket*> gPolledSockets( __FILE__, __LINE__ );

static PolledSocket* addPolledSocket(NetSocket handleFd, SOCKET fd, S32 state,
                               char* remoteAddr = NULL, S32 port = -1)
{
   PolledSocket* sock = new PolledSocket();
   sock->fd = fd;
   sock->handleFd = handleFd;
   sock->state = state;
   if (remoteAddr)
      dStrcpy(sock->remoteAddr, remoteAddr);
   if (port != -1)
      sock->remotePort = port;
   gPolledSockets.push_back(sock);
   return sock;
}

enum {
   MaxConnections = 1024,
};


bool netSocketWaitForWritable(NetSocket handleFd, S32 timeoutMs)
{  
   fd_set writefds;
   timeval timeout;
   SOCKET socketFd = smReservedSocketList.resolve(handleFd);

   FD_ZERO( &writefds );
   FD_SET( socketFd, &writefds );

   timeout.tv_sec = timeoutMs / 1000;
   timeout.tv_usec = ( timeoutMs % 1000 ) * 1000;

   if( select(socketFd + 1, NULL, &writefds, NULL, &timeout) > 0 )
      return true;

   return false;
}

static S32 initCount = 0;

bool Net::init()
{
#if defined(TORQUE_USE_WINSOCK)
   if(!initCount)
   {
#ifdef TORQUE_OS_XENON
      // Configure startup parameters
      XNetStartupParams xnsp;
      memset( &xnsp, 0, sizeof( xnsp ) );
      xnsp.cfgSizeOfStruct = sizeof( XNetStartupParams );

#ifndef TORQUE_DISABLE_PC_CONNECTIVITY
      xnsp.cfgFlags = XNET_STARTUP_BYPASS_SECURITY;
      Con::warnf("XNET_STARTUP_BYPASS_SECURITY enabled! This build can talk to PCs!");
#endif

      AssertISV( !XNetStartup( &xnsp ), "Net::init - failed to init XNet" );
#endif

      WSADATA stWSAData;
      AssertISV( !WSAStartup( 0x0101, &stWSAData ), "Net::init - failed to init WinSock!" );

      //logprintf("Winsock initialization %s", success ? "succeeded." : "failed!");
   }
#endif
   initCount++;

   Process::notify(&Net::process, PROCESS_NET_ORDER);

   return(true);
}

void Net::shutdown()
{
   Process::remove(&Net::process);

   while (gPolledSockets.size() > 0)
      closeConnectTo(gPolledSockets[0]->handleFd);

   closePort();
   initCount--;

#if defined(TORQUE_USE_WINSOCK)
   if(!initCount)
   {
      WSACleanup();

#ifdef TORQUE_OS_XENON
      XNetCleanup();
#endif
   }
#endif
}

Net::Error getLastError()
{
#if defined(TORQUE_USE_WINSOCK)
   S32 err = WSAGetLastError();
   switch(err)
   {
   case 0:
      return Net::NoError;
   case WSAEWOULDBLOCK:
      return Net::WouldBlock;
   default:
      return Net::UnknownError;
   }
#else
   if (errno == EAGAIN)
      return Net::WouldBlock;
   if (errno == 0)
      return Net::NoError;
   return Net::UnknownError;
#endif
}

// ipv4 version of name routines

static void netToIPSocketAddress(const NetAddress *address, struct sockaddr_in *sockAddr)
{
   dMemset(sockAddr, 0, sizeof(struct sockaddr_in));
   sockAddr->sin_family = AF_INET;
   sockAddr->sin_port = htons(address->port);
   #if !defined(TORQUE_OS_WIN)
   sockAddr->sin_len = sizeof(struct sockaddr_in);
   #endif

   char tAddr[20];
   dSprintf(tAddr, 20, "%d.%d.%d.%d\n", address->address.ipv4.netNum[0], address->address.ipv4.netNum[1], address->address.ipv4.netNum[2], address->address.ipv4.netNum[3]);
   inet_pton(AF_INET, tAddr, &sockAddr->sin_addr.s_addr);
}

static void IPSocketToNetAddress(const struct sockaddr_in *sockAddr, NetAddress *address)
{
   address->type = NetAddress::IPAddress;
   address->port = ntohs(sockAddr->sin_port);
   address->address.ipv4.netNum[0] = sockAddr->sin_addr.s_net;
   address->address.ipv4.netNum[1] = sockAddr->sin_addr.s_host;
   address->address.ipv4.netNum[2] = sockAddr->sin_addr.s_lh;
   address->address.ipv4.netNum[3] = sockAddr->sin_addr.s_impno;
}

// ipv6 version of name routines

static void netToIPSocket6Address(const NetAddress *address, struct sockaddr_in6 *sockAddr)
{
   dMemset(sockAddr, 0, sizeof(struct sockaddr_in6));
#ifdef SIN6_LEN
   sockAddr->sin6_len = sizeof(struct sockaddr_in6);
#endif
   sockAddr->sin6_family = AF_INET6;
   sockAddr->sin6_port = ntohs(address->port);
   sockAddr->sin6_flowinfo = address->address.ipv6.netFlow;
   sockAddr->sin6_scope_id = address->address.ipv6.netScope;
   dMemcpy(&sockAddr->sin6_addr, address->address.ipv6.netNum, sizeof(address->address.ipv6.netNum));
}

static void IPSocket6ToNetAddress(const struct sockaddr_in6 *sockAddr, NetAddress *address)
{
   address->type = NetAddress::IPV6Address;
   address->port = ntohs(sockAddr->sin6_port);
   dMemcpy(address->address.ipv6.netNum, &sockAddr->sin6_addr, sizeof(address->address.ipv6.netNum));
   address->address.ipv6.netFlow = sockAddr->sin6_flowinfo;
   address->address.ipv6.netScope = sockAddr->sin6_scope_id;
}

//


NetSocket Net::openListenPort(U16 port)
{
   if(Journal::IsPlaying())
   {
      U32 ret;
      Journal::Read(&ret);
      return NetSocket::fromHandle(ret);
   }

   NetSocket handleFd = openSocket();
   SOCKET sockId = smReservedSocketList.activate(handleFd, AF_INET6, true);

   if (handleFd == NetSocket::INVALID)
   {
      Con::errorf("Unable to open listen socket: %s", strerror(errno));
      return NetSocket::INVALID;
   }

   if (bind(handleFd, port) != NoError)
   {
      Con::errorf("Unable to bind port %d: %s", port, strerror(errno));
      closeSocket(handleFd);
      return NetSocket::INVALID;
   }
   if (listen(handleFd, 4) != NoError)
   {
      Con::errorf("Unable to listen on port %d: %s", port,  strerror(errno));
      closeSocket(handleFd);
      return NetSocket::INVALID;
   }

   setBlocking(handleFd, false);
   addPolledSocket(handleFd, sockId, Listening);

   if(Journal::IsRecording())
      Journal::Write(U32(handleFd.getHandle()));

   return handleFd;
}

NetSocket Net::openConnectTo(const char *addressString)
{
   if(!dStrnicmp(addressString, "ipx:", 4))
      // ipx support deprecated
      return NetSocket::INVALID;
   if(!dStrnicmp(addressString, "ip:", 3))
      addressString += 3;  // eat off the ip:
   else if (!dStrnicmp(addressString, "ip6:", 4))
      addressString += 4;  // eat off the ip6:
   char remoteAddr[256];
   dStrcpy(remoteAddr, addressString);

   char *portString = dStrchr(remoteAddr, ':');

   U16 port;
   if(portString)
   {
      *portString++ = 0;
      port = htons(dAtoi(portString));
   }
   else
      port = htons(defaultPort);

   if(!dStricmp(remoteAddr, "broadcast"))
      return NetSocket::INVALID;

   if(Journal::IsPlaying())
   {
      U32 ret;
      Journal::Read(&ret);
      return NetSocket::fromHandle(ret);
   }

   NetSocket handleFd = openSocket();
   
   sockaddr_in ipAddr;
   sockaddr_in6 ipAddr6;
   dMemset(&ipAddr, 0, sizeof(ipAddr));
   dMemset(&ipAddr6, 0, sizeof(ipAddr6));

   bool isipv4, isipv6, ishost;
   isipv4 = isipv6 = ishost = false;

   // Check if we've got a simple ipv4 / ipv6
   if (inet_pton(AF_INET, remoteAddr, &ipAddr.sin_addr) == 1)
   {
      isipv4 = true;
   }
   else if (inet_pton(AF_INET, remoteAddr, &ipAddr6.sin6_addr) == 1)
   {
      isipv6 = true;
   }
   else
   {
      ishost = true;
   }

   // Let getaddrinfo fill in the structure
   if ((isipv4 || isipv6))
   {
      struct addrinfo hint, *res = NULL;
      memset(&hint, 0, sizeof(hint));
      hint.ai_family = PF_UNSPEC;
      hint.ai_flags = AI_NUMERICHOST;
      if (getaddrinfo(addressString, NULL, &hint, &res) == 0)
      {
         if (res->ai_family == AF_INET)
         {
            isipv4 = true;
            isipv6 = false;
            dMemcpy(&ipAddr, res->ai_addr, sizeof(ipAddr));
         }
         else if (res->ai_family == AF_INET6)
         {
            isipv4 = false;
            isipv6 = true;
            dMemcpy(&ipAddr6, res->ai_addr, sizeof(ipAddr6));
         }
      }
      else
      {
         isipv4 = isipv6 = false;
      }
   }

   // Attempt to connect or queue a lookup
   if (isipv4)
   {
      SOCKET socketFd = smReservedSocketList.activate(handleFd, AF_INET, true);
      if (socketFd != InvalidSocketHandle)
      {
         setBlocking(handleFd, false);
         if (::connect(socketFd, (struct sockaddr *)&ipAddr, sizeof(ipAddr)) == -1 &&
            errno != EINPROGRESS)
         {
            Con::errorf("Error connecting %s: %s",
               addressString, strerror(errno));
            closeSocket(handleFd);
            handleFd = NetSocket::INVALID;
         }
      }
      else
      {
         smReservedSocketList.remove(handleFd);
         handleFd = NetSocket::INVALID;
      }

      if (handleFd != NetSocket::INVALID)
      {
         // add this socket to our list of polled sockets
         addPolledSocket(handleFd, socketFd, ConnectionPending);
      }
   }
   else if (isipv6)
   {
      SOCKET socketFd = smReservedSocketList.activate(handleFd, AF_INET6, true);
      if (::connect(socketFd, (struct sockaddr *)&ipAddr6, sizeof(ipAddr6)) == -1 &&
         errno != EINPROGRESS)
      {
         setBlocking(handleFd, false);
         Con::errorf("Error connecting %s: %s",
            addressString, strerror(errno));
         closeSocket(handleFd);
         handleFd = NetSocket::INVALID;
      }
      else
      {
         smReservedSocketList.remove(handleFd);
         handleFd = NetSocket::INVALID;
      }

      if (handleFd != NetSocket::INVALID)
      {
         // add this socket to our list of polled sockets
         addPolledSocket(handleFd, socketFd, ConnectionPending);
      }
   }
   else if (ishost)
   {
      // need to do an asynchronous name lookup.  first, add the socket
      // to the polled list
      addPolledSocket(handleFd, InvalidSocketHandle, NameLookupRequired, remoteAddr, port);
      // queue the lookup
      gNetAsync.queueLookup(remoteAddr, handleFd);
   }
   else
   {
      handleFd = NetSocket::INVALID;
   }

   if(Journal::IsRecording())
      Journal::Write(U32(handleFd.getHandle()));
   return handleFd;
}

void Net::closeConnectTo(NetSocket handleFd)
{
   if(Journal::IsPlaying())
      return;

   // if this socket is in the list of polled sockets, remove it
   for (S32 i = 0; i < gPolledSockets.size(); ++i)
   {
      if (gPolledSockets[i]->handleFd == handleFd)
      {
         delete gPolledSockets[i];
         gPolledSockets.erase(i);
         break;
      }
   }

   closeSocket(handleFd);
}

Net::Error Net::sendtoSocket(NetSocket handleFd, const U8 *buffer, S32  bufferSize)
{
   if(Journal::IsPlaying())
   {
      U32 e;
      Journal::Read(&e);

      return (Net::Error) e;
   }

   Net::Error e = send(handleFd, buffer, bufferSize);

   if(Journal::IsRecording())
      Journal::Write(U32(e));

   return e;
}

bool Net::openPort(S32 port, bool doBind)
{
   if (udpSocket != NetSocket::INVALID)
   {
      closeSocket(udpSocket);
      udpSocket = NetSocket::INVALID;
   }

   // we turn off VDP in non-release builds because VDP does not support broadcast packets
   // which are required for LAN queries (PC->Xbox connectivity).  The wire protocol still
   // uses the VDP packet structure, though.
   S32 protocol = 0;
   bool useVDP = false;
#ifdef TORQUE_DISABLE_PC_CONNECTIVITY
   // Xbox uses a VDP (voice/data protocol) socket for networking
   protocol = IPPROTO_VDP;
   useVDP = true;
#endif

   SOCKET socketFd = ::socket(AF_INET6, SOCK_DGRAM, protocol);

   if(socketFd != InvalidSocketHandle)
   {
     udpSocket = smReservedSocketList.reserve(socketFd);

     Net::Error error = NoError;
	  if (doBind)
	  {
        error = bind(udpSocket, port);
	  }

      if(error == NoError)
         error = setBufferSize(udpSocket, 32768*8);

      if(error == NoError && !useVDP)
         error = setBroadcast(udpSocket, true);

      if(error == NoError)
         error = setBlocking(udpSocket, false);

      if (error == NoError)
      {
         Con::printf("UDP initialized on port %d", port);
      }
      else
      {
         closeSocket(udpSocket);
         udpSocket = NetSocket::INVALID;
         Con::printf("Unable to initialize UDP - error %d", error);
      }
   }
   netPort = port;
   return udpSocket != NetSocket::INVALID;
}

NetSocket Net::getPort()
{
   return udpSocket;
}

void Net::closePort()
{
   if (udpSocket != NetSocket::INVALID)
      closeSocket(udpSocket);
}

Net::Error Net::sendto(const NetAddress *address, const U8 *buffer, S32  bufferSize)
{
   if(Journal::IsPlaying())
      return NoError;

   SOCKET socketFd = smReservedSocketList.resolve(udpSocket);

   if(address->type == NetAddress::IPAddress)
   {
      sockaddr_in ipAddr;
      netToIPSocketAddress(address, &ipAddr);
      if(::sendto(socketFd, (const char*)buffer, bufferSize, 0,
         (sockaddr *) &ipAddr, sizeof(sockaddr_in)) == SOCKET_ERROR)
         return getLastError();
      else
         return NoError;
   }
   else if (address->type == NetAddress::IPV6Address)
   {
      sockaddr_in6 ipAddr;
      netToIPSocket6Address(address, &ipAddr);
      if (::sendto(socketFd, (const char*)buffer, bufferSize, 0,
         (struct sockaddr *) &ipAddr, sizeof(ipAddr)) == -1)
         return getLastError();
   }

   return WrongProtocolType;
}

void Net::process()
{
   sockaddr sa;
   sa.sa_family = AF_UNSPEC;
   NetAddress srcAddress;
   RawData tmpBuffer;
   tmpBuffer.alloc(MaxPacketDataSize);

   SOCKET socketFd = smReservedSocketList.resolve(udpSocket);

   for(;;)
   {
      socklen_t addrLen = sizeof(sa);
      S32 bytesRead = -1;

      if(udpSocket != NetSocket::INVALID)
         bytesRead = ::recvfrom(socketFd, (char *) tmpBuffer.data, MaxPacketDataSize, 0, &sa, &addrLen);

      if(bytesRead == -1)
         break;

      if(sa.sa_family == AF_INET)
         IPSocketToNetAddress((sockaddr_in *) &sa,  &srcAddress);
      else
         continue;

      if(bytesRead <= 0)
         continue;

      if(srcAddress.type == NetAddress::IPAddress &&
         srcAddress.address.ipv4.netNum[0] == 127 &&
         srcAddress.address.ipv4.netNum[1] == 0 &&
         srcAddress.address.ipv4.netNum[2] == 0 &&
         srcAddress.address.ipv4.netNum[3] == 1 &&
         srcAddress.port == netPort)
         continue;

      tmpBuffer.size = bytesRead;

      Net::smPacketReceive.trigger(srcAddress, tmpBuffer);
   }

   // process the polled sockets.  This blob of code performs functions
   // similar to WinsockProc in winNet.cc

   if (gPolledSockets.size() == 0)
      return;

   S32 optval;
   socklen_t optlen = sizeof(S32);
   S32 bytesRead;
   Net::Error err;
   bool removeSock = false;
   PolledSocket *currentSock = NULL;
   NetSocket incomingHandleFd = NetSocket::INVALID;
   struct addrinfo out_h_addr;
   S32 out_h_length = 0;
   RawData readBuff;

   for (S32 i = 0; i < gPolledSockets.size();
      /* no increment, this is done at end of loop body */)
   {
      removeSock = false;
      currentSock = gPolledSockets[i];
      switch (currentSock->state)
      {
      case ::InvalidState:
         Con::errorf("Error, InvalidState socket in polled sockets  list");
         break;
      case ::ConnectionPending:
         // see if it is now connected
#ifdef TORQUE_OS_XENON
         // WSASetLastError has no return value, however part of the SO_ERROR behavior
         // is to clear the last error, so this needs to be done here.
         if( ( optval = _getLastErrorAndClear() ) == -1 ) 
#else
         if (getsockopt(currentSock->fd, SOL_SOCKET, SO_ERROR,
            (char*)&optval, &optlen) == -1)
#endif
         {
            Con::errorf("Error getting socket options: %s",  strerror(errno));

            Net::smConnectionNotify.trigger(currentSock->handleFd, Net::ConnectFailed);
            removeSock = true;
         }
         else
         {
            if (optval == EINPROGRESS)
               // still connecting...
               break;

            if (optval == 0)
            {
               // poll for writable status to be sure we're connected.
               bool ready = netSocketWaitForWritable(currentSock->handleFd,0);
               if(!ready)
                  break;

               currentSock->state = ::Connected;
               Net::smConnectionNotify.trigger(currentSock->handleFd, Net::Connected);
            }
            else
            {
               // some kind of error
               Con::errorf("Error connecting: %s", strerror(errno));
               Net::smConnectionNotify.trigger(currentSock->handleFd, Net::ConnectFailed);
               removeSock = true;
            }
         }
         break;
      case ::Connected:

         // try to get some data
         bytesRead = 0;
         readBuff.alloc(MaxPacketDataSize);
         err = Net::recv(currentSock->handleFd, (U8*)readBuff.data, MaxPacketDataSize, &bytesRead);
         if(err == Net::NoError)
         {
            if (bytesRead > 0)
            {
               // got some data, post it
               readBuff.size = bytesRead;
               Net::smConnectionReceive.trigger(currentSock->handleFd, readBuff);
            }
            else
            {
               // ack! this shouldn't happen
               if (bytesRead < 0)
                  Con::errorf("Unexpected error on socket: %s", strerror(errno));

               // zero bytes read means EOF
               Net::smConnectionNotify.trigger(currentSock->handleFd, Net::Disconnected);

               removeSock = true;
            }
         }
         else if (err != Net::NoError && err != Net::WouldBlock)
         {
            Con::errorf("Error reading from socket: %s",  strerror(errno));
            Net::smConnectionNotify.trigger(currentSock->handleFd, Net::Disconnected);
            removeSock = true;
         }
         break;
      case ::NameLookupRequired:
         U32 newState;

         // is the lookup complete?
         if (!gNetAsync.checkLookup(
            currentSock->handleFd, &out_h_addr, &out_h_length,
            sizeof(out_h_addr)))
            break;

         if (out_h_length == -1)
         {
            Con::errorf("DNS lookup failed: %s", currentSock->remoteAddr);
            newState = Net::DNSFailed;
            removeSock = true;
         }
         else
         {
            // try to connect
            struct addrinfo *res = &out_h_addr;

            if (res->ai_family == AF_INET)
            {
               struct sockaddr_in* socketAddress = (struct sockaddr_in*)res->ai_addr;
               socketAddress->sin_port = currentSock->remotePort;
               currentSock->fd = smReservedSocketList.activate(currentSock->handleFd, AF_INET);
               setBlocking(currentSock->handleFd, false);

#ifdef TORQUE_DEBUG_LOOKUPS
               char addrString[256];
               NetAddress addr;
               IPSocketToNetAddress(socketAddress, &addr);
               Net::addressToString(&addr, addrString);
               Con::printf("DNS: lookup resolved to %s", addrString);
#endif
            }
            else if (res->ai_family == AF_INET6)
            {
               struct sockaddr_in6* socketAddress = (struct sockaddr_in6*)res->ai_addr;
               socketAddress->sin6_port = currentSock->remotePort;
               currentSock->fd = smReservedSocketList.activate(currentSock->handleFd, AF_INET6);
               setBlocking(currentSock->handleFd, false);

#ifdef TORQUE_DEBUG_LOOKUPS
               char addrString[256];
               NetAddress addr;
               IPSocket6ToNetAddress(socketAddress, &addr);
               Net::addressToString(&addr, addrString);
               Con::printf("DNS: lookup resolved to %s", addrString);
#endif
            }

            if (::connect(currentSock->fd, res->ai_addr,
               res->ai_addrlen) == -1)
            {
               if (errno == EINPROGRESS)
               {
                  newState = Net::DNSResolved;
                  currentSock->state = ConnectionPending;
               }
               else
               {
                  Con::errorf("Error connecting to %s: %s",
                     currentSock->remoteAddr, strerror(errno));
                  newState = Net::ConnectFailed;
                  removeSock = true;
               }
            }
            else
            {
               newState = Net::Connected;
               currentSock->state = Connected;
            }
         }

         Net::smConnectionNotify.trigger(currentSock->handleFd, newState);
         break;
      case ::Listening:
         NetAddress incomingAddy;

         incomingHandleFd = Net::accept(currentSock->handleFd, &incomingAddy);
         if(incomingHandleFd != NetSocket::INVALID)
         {
            setBlocking(incomingHandleFd, false);
            addPolledSocket(incomingHandleFd, smReservedSocketList.resolve(incomingHandleFd), Connected);
            Net::smConnectionAccept.trigger(currentSock->handleFd, incomingHandleFd, incomingAddy);
         }
         break;
      }

      // only increment index if we're not removing the connection,  since
      // the removal will shift the indices down by one
      if (removeSock)
         closeConnectTo(currentSock->handleFd);
      else
         i++;
   }
}

NetSocket Net::openSocket()
{
   return smReservedSocketList.reserve();
}

Net::Error Net::closeSocket(NetSocket handleFd)
{
   if(handleFd != NetSocket::INVALID)
   {
      SOCKET socketFd = smReservedSocketList.resolve(handleFd);

      if(!::closesocket(socketFd))
         return NoError;
      else
         return getLastError();
   }
   else
      return NotASocket;
}

Net::Error Net::connect(NetSocket handleFd, const NetAddress *address)
{
   if(!(address->type == NetAddress::IPAddress || address->type == NetAddress::IPV6Address))
      return WrongProtocolType;

   SOCKET socketFd = smReservedSocketList.resolve(handleFd);

   if (address->type == NetAddress::IPAddress)
   {
      sockaddr_in socketAddress;
      netToIPSocketAddress(address, &socketAddress);

      if (socketFd == InvalidSocketHandle)
      {
         socketFd = smReservedSocketList.activate(handleFd, AF_INET);
      }

      if (!::connect(socketFd, (struct sockaddr *) &socketAddress, sizeof(socketAddress)))
         return NoError;
   }
   else if (address->type == NetAddress::IPV6Address)
   {
      sockaddr_in6 socketAddress;
      netToIPSocket6Address(address, &socketAddress);

      if (socketFd == InvalidSocketHandle)
      {
         socketFd = smReservedSocketList.activate(handleFd, AF_INET6);
      }

      if (!::connect(socketFd, (struct sockaddr *) &socketAddress, sizeof(socketAddress)))
         return NoError;
   }

   return getLastError();
}

Net::Error Net::listen(NetSocket handleFd, S32 backlog)
{
   SOCKET socketFd = smReservedSocketList.resolve(handleFd);
   if (socketFd == InvalidSocketHandle)
      return NotASocket;

   if(!::listen(socketFd, backlog))
      return NoError;
   return getLastError();
}

NetSocket Net::accept(NetSocket handleFd, NetAddress *remoteAddress)
{
   sockaddr_in socketAddress;
   socklen_t addrLen = sizeof(socketAddress);
   SOCKET socketFd = smReservedSocketList.resolve(handleFd);
   if (socketFd == InvalidSocketHandle)
      return NetSocket::INVALID;

   SOCKET acceptedSocketFd = ::accept(socketFd, (sockaddr *) &socketAddress,  &addrLen);
   if(acceptedSocketFd != InvalidSocketHandle)
   {
      if (socketAddress.sin_family == AF_INET)
      {
         // ipv4
         IPSocketToNetAddress(((struct sockaddr_in*)&socketAddress.sin_addr), remoteAddress);
      }
      else if (socketAddress.sin_family == AF_INET6)
      {
         // ipv6
         IPSocket6ToNetAddress(((struct sockaddr_in6*)&socketAddress.sin_addr), remoteAddress);
      }

      NetSocket newHandleFd = smReservedSocketList.reserve(acceptedSocketFd);
      return newHandleFd;
   }
   return NetSocket::INVALID;
}

Net::Error Net::bind(NetSocket handleFd, U16 port)
{
   S32 error;
   bool isipv4, isipv6;
   isipv4 = isipv6 = false;

   // thanks to [TPG]P1aGu3 for the name
   const char* serverIP = Con::getVariable("pref::Net::BindAddress");
   // serverIP is guaranteed to be non-0.
   AssertFatal(serverIP, "serverIP is NULL!");

   if (!dStrnicmp(serverIP, "ip6:", 4))
   {
      serverIP += 4;
      isipv6 = true;
   }
   else if (!dStrnicmp(serverIP, "ip:", 3))
   {
      serverIP += 3;
      isipv4 = true;
   }

   SOCKET socketFd = smReservedSocketList.resolve(handleFd);
   if (socketFd == InvalidSocketHandle)
   {
      if (handleFd.getHandle() == -1)
         return NotASocket;
      socketFd = smReservedSocketList.activate(handleFd, isipv4 ? AF_INET : AF_INET6);
   }

   sockaddr_in socketAddress;
   dMemset((char *)&socketAddress, 0, sizeof(socketAddress));
   socketAddress.sin_family = isipv4 ? AF_INET : AF_INET6;
   // It's entirely possible that there are two NIC cards.
   // We let the user specify which one the server runs on.

   if( serverIP[0] != '\0' ) {

      // TODO: use host info thingy

      if( socketAddress.sin_addr.s_addr != INADDR_NONE ) {
         Con::printf( "Binding server port to %s", serverIP );
      } else {
         Con::warnf( ConsoleLogEntry::General,
            "inet_addr() failed for %s while binding!",
            serverIP );
         socketAddress.sin_addr.s_addr = INADDR_ANY;
      }

   } else {
      // TODO
      Con::printf( "Binding server port to default IP" );
      socketAddress.sin_addr.s_addr = INADDR_ANY;
      socketAddress.sin_port = htons(port);
   }

   error = ::bind(socketFd, (sockaddr *) &socketAddress,  sizeof(socketAddress));

   if(!error)
      return NoError;
   return getLastError();
}

Net::Error Net::setBufferSize(NetSocket handleFd, S32 bufferSize)
{
   S32 error;
   SOCKET socketFd = smReservedSocketList.resolve(handleFd);
   if (socketFd == InvalidSocketHandle)
      return NotASocket;

   error = ::setsockopt(socketFd, SOL_SOCKET, SO_RCVBUF, (char *)  &bufferSize, sizeof(bufferSize));
   if(!error)
      error = ::setsockopt(socketFd, SOL_SOCKET, SO_SNDBUF, (char *)  &bufferSize, sizeof(bufferSize));
   if(!error)
      return NoError;
   return getLastError();
}

Net::Error Net::setBroadcast(NetSocket handleFd, bool broadcast)
{
   S32 bc = broadcast;
   SOCKET socketFd = smReservedSocketList.resolve(handleFd);
   if (socketFd == InvalidSocketHandle)
      return NotASocket;
   S32 error = ::setsockopt(socketFd, SOL_SOCKET, SO_BROADCAST, (char*)&bc,  sizeof(bc));
   if(!error)
      return NoError;
   return getLastError();
}

Net::Error Net::setBlocking(NetSocket handleFd, bool blockingIO)
{
   SOCKET socketFd = smReservedSocketList.resolve(handleFd);
   if (socketFd == InvalidSocketHandle)
      return NotASocket;

   unsigned long notblock = !blockingIO;
   S32 error = ioctl(socketFd, FIONBIO, &notblock);
   if(!error)
      return NoError;
   return getLastError();
}

Net::Error Net::send(NetSocket handleFd, const U8 *buffer, S32 bufferSize)
{
   SOCKET socketFd = smReservedSocketList.resolve(handleFd);
   if (socketFd == InvalidSocketHandle)
      return NotASocket;

   errno = 0;
   S32 bytesWritten = ::send(socketFd, (const char*)buffer, bufferSize, 0);
   if(bytesWritten == -1)
#if defined(TORQUE_USE_WINSOCK)
      Con::errorf("Could not write to socket. Error: %s",strerror_wsa( WSAGetLastError() ));
#else
      Con::errorf("Could not write to socket. Error: %s",strerror(errno));
#endif

   return getLastError();
}

Net::Error Net::recv(NetSocket handleFd, U8 *buffer, S32 bufferSize, S32  *bytesRead)
{
   SOCKET socketFd = smReservedSocketList.resolve(handleFd);
   if (socketFd == InvalidSocketHandle)
      return NotASocket;

   *bytesRead = ::recv(socketFd, (char*)buffer, bufferSize, 0);
   if(*bytesRead == -1)
      return getLastError();
   return NoError;
}

bool Net::compareAddresses(const NetAddress *a1, const NetAddress *a2)
{
   return a1->isSameAddress(*a2);
}

bool Net::stringToAddress(const char *addressString, NetAddress  *address)
{
   if(!dStrnicmp(addressString, "ipx:", 4))
      // ipx support deprecated
      return false;

   if(!dStrnicmp(addressString, "ip:", 3))
      addressString += 3;  // eat off the ip:
   else if (!dStrnicmp(addressString, "ip6:", 4))
      addressString += 4;  // eat off the ip6:

   sockaddr_in ipAddr;
   char remoteAddr[256];
   if(strlen(addressString) > 255)
      return false;

   dStrcpy(remoteAddr, addressString);

   char *portString = dStrchr(remoteAddr, ':');
   if(portString)
      *portString++ = '\0';

   if (!dStricmp(remoteAddr, "broadcast"))
   {
      ipAddr.sin_addr.s_addr = htonl(INADDR_BROADCAST);
      if (portString)
         ipAddr.sin_port = htons(dAtoi(portString));
      else
         ipAddr.sin_port = htons(defaultPort);
      ipAddr.sin_family = AF_INET;
      IPSocketToNetAddress(&ipAddr, address);
   }
   else
   {
      struct addrinfo hint, *res = NULL;

      memset(&hint, 0, sizeof(hint));
      hint.ai_family = PF_UNSPEC;
      hint.ai_flags = 0;
      if (getaddrinfo(addressString, NULL, &hint, &res) == 0)
      {
         if (res->ai_family == AF_INET)
         {
            // ipv4
            IPSocketToNetAddress(((struct sockaddr_in*)res->ai_addr), address);
         }
         else if (res->ai_family == AF_INET6)
         {
            // ipv6
            IPSocket6ToNetAddress(((struct sockaddr_in6*)res->ai_addr), address);
         }
         else
         {
            // unknown
            return false;
         }
      }
   }
   return true;
}

void Net::addressToString(const NetAddress *address, char  addressString[256])
{
   if(address->type == NetAddress::IPAddress)
   {
      sockaddr_in ipAddr;
      netToIPSocketAddress(address, &ipAddr);

      if(ipAddr.sin_addr.s_addr == htonl(INADDR_BROADCAST))
         dSprintf(addressString, 256, "IP:Broadcast:%d",  ntohs(ipAddr.sin_port));
      else
      {
         char buffer[256];
         buffer[0] = '\0';
         sockaddr_in ipAddr;
         netToIPSocketAddress(address, &ipAddr);
         inet_ntop(AF_INET, &(ipAddr.sin_addr), buffer, sizeof(buffer));
         dSprintf(addressString, 256, "IP:%s:%i", buffer, ntohs(ipAddr.sin_port));
      }
   }
   else if (address->type == NetAddress::IPV6Address)
   {
      char buffer[256];
      buffer[0] = '\0';
      sockaddr_in6 ipAddr;
      netToIPSocket6Address(address, &ipAddr);
      inet_ntop(AF_INET6, &(ipAddr.sin6_addr), buffer, sizeof(buffer));
      dSprintf(addressString, 256, "IP6:%s:%i", buffer, ntohs(ipAddr.sin6_port));
   }
   else
   {
      *addressString = 0;
      return;
   }
}

U32 NetAddress::getHash() const
{
   U32 value = 0;
   switch (type)
   {
   case NetAddress::IPAddress:
      value = Torque::hash((const U8*)&address, 4, 0);
      break;
   case NetAddress::IPV6Address:
      value = Torque::hash((const U8*)&address, 24, 0);
      break;
   default:
      value = 0;
      break;
   }
   return value;
}
