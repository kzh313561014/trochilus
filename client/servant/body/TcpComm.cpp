#include "stdafx.h"
#include "socket/MySocket.h"
#include "TcpComm.h"
#include "../shell/Exports.h"

TcpComm::TcpComm(BOOL isSecure):
m_xorKey1(0),
m_xorKey2(0),
m_isSecure(FALSE)
{
	srand(GetTickCount());
	m_xorKey1 = (BYTE)(rand() % 255);
	m_xorKey2 = (BYTE)(rand() % 255);

	m_isSecure = isSecure;
}

TcpComm::~TcpComm()
{

}

BOOL TcpComm::Send( ULONG targetIP, const LPBYTE pData, DWORD dwSize )
{
	return Send(m_sock, targetIP, pData, dwSize);
}

BOOL TcpComm::SendAndRecv( ULONG targetIP, const LPBYTE pSendData, DWORD dwSendSize, LPBYTE* pRecvData, DWORD& dwRecvSize )
{
	TCP_HEADER sendHead;
	sendHead.flag = TCP_FLAG;
	sendHead.nSize = dwSendSize;
	BOOL ret  = FALSE;

	do 
	{
		ret = Send(m_sock, targetIP, (PBYTE)&sendHead, sizeof(TCP_HEADER));

		if (!ret)
			break;;

		if (m_isSecure)
			XFC(pSendData,dwSendSize,pSendData,m_xorKey1,m_xorKey2);

		ret = Send(m_sock, targetIP, pSendData, dwSendSize);

		if (!ret)
			break;

		TCP_HEADER recvHead = {0};

		ret = m_sock.ReceiveAll((LPBYTE)&recvHead, sizeof(TCP_HEADER));

		if ( !ret )
			break;


		ByteBuffer buffer;
		buffer.Alloc(recvHead.nSize);
		ret = m_sock.ReceiveAll(buffer,recvHead.nSize);

		if (!ret)
		{
			buffer.Free();
			break;
		}

		//��������
		*pRecvData = Alloc(recvHead.nSize);
		memcpy(*pRecvData, (LPBYTE)buffer, recvHead.nSize);
		dwRecvSize =  recvHead.nSize;

		if(m_isSecure)
			XFC(*pRecvData,recvHead.nSize,*pRecvData,m_xorKey1,m_xorKey2);

		buffer.Free();

	} while (FALSE);
	

	if ( !ret )
		m_sock.Close();

	return ret;
}

BOOL TcpComm::Connect( ULONG targetIP, MySocket& sock )
{
	BOOL ret = FALSE;

	sock.Close();

	do 
	{
		if (! sock.Create(TRUE))
		{
			errorLogE(_T("create socket failed."));
			break;
		}

		if (! sock.Connect(targetIP, g_ConfigInfo.nPort, 10))
		{
			errorLog(_T("connect [%u] failed"), targetIP);
			break;
		}

		int timeout = 20000; 
		setsockopt(sock,SOL_SOCKET,SO_SNDTIMEO,(char*)&timeout,sizeof(timeout));
		setsockopt(sock,SOL_SOCKET,SO_RCVTIMEO,(char*)&timeout,sizeof(timeout));

		if (m_isSecure)
		{
			int key1 = 0;
			int key2 = 0;

			int flag = TCP_FLAG;

			ret = m_sock.SendAll((LPVOID)&flag,sizeof(int));

			if ( !ret )
				break;

			ret = m_sock.ReceiveAll(&m_rsaKey,sizeof(RSA::RSA_PUBLIC_KEY));

			if ( !ret )
				break;

			RSA::RSAEncrypt((char*)&m_xorKey1,(int*)&key1,m_rsaKey.d,m_rsaKey.n,1);
			RSA::RSAEncrypt((char*)&m_xorKey2,(int*)&key2,m_rsaKey.d,m_rsaKey.n,1);

			ret = m_sock.SendAll(&key1,sizeof(int));

			if ( !ret )
				break;

			ret = m_sock.SendAll(&key2,sizeof(int));

			if ( !ret )
				break;
		}

	} while (FALSE);

	if ( !ret )
		sock.Close();

	return ret;
}

BOOL TcpComm::Send( MySocket& sock, ULONG targetIP, const LPBYTE pData, DWORD dwSize )
{
	IN_ADDR addr;
	addr.S_un.S_addr = targetIP;


	ByteBuffer sendByteBuffer;
	sendByteBuffer.Alloc(dwSize);
	memcpy((LPBYTE)sendByteBuffer, pData, dwSize);

	BOOL bSentOK = FALSE;

	bSentOK = sock.SendAll((LPBYTE)sendByteBuffer, sendByteBuffer.Size());
	if (!bSentOK)
	{
		sock.Close();

		if ( Connect(targetIP, sock))
		{
			bSentOK = sock.SendAll((LPBYTE)sendByteBuffer, sendByteBuffer.Size());
		}
		else
		{
			debugLog(_T("connect %x %s failed"), targetIP, a2t(inet_ntoa(addr)));
		}
	}

	return bSentOK;
}