/*
	tcpts.c:	functions implementing TCP transport service
			for AMS.

	Author: Scott Burleigh, JPL

	Copyright (c) 2005, California Institute of Technology.
	ALL RIGHTS RESERVED.  U.S. Government Sponsorship
	acknowledged.
									*/
#include "amsP.h"

#define	TCPTS_MAX_MSG_LEN	65535

typedef struct tcptsep
{
	int		fd;
	unsigned int	ipAddress;
	unsigned short	portNbr;
	struct tcptsep	*prev;		/*	In senders pool.	*/
	struct tcptsep	*next;		/*	In senders pool.	*/
	struct tcptssap	*sap;		/*	Back pointer.		*/
} TcpTsep;

typedef struct tcprcvr
{
	int		fd;
	pthread_t	thread;
	AmsSAP		*amsSap;
	struct tcprcvr	*prev;		/*	In receivers pool.	*/
	struct tcprcvr	*next;		/*	In receivers pool.	*/
	struct tcptssap	*sap;		/*	Back pointer.		*/
} TcpRcvr;

typedef struct tcptssap
{
	TcpTsep			*firstInSendPool;
	TcpTsep			*lastInSendPool;
	int			nbrInSendPool;
	pthread_mutex_t		sendPoolMutex;

	TcpRcvr			*firstInRcvrPool;
	TcpRcvr			*lastInRcvrPool;
	int			nbrInRcvrPool;
	pthread_mutex_t		rcvrPoolMutex;

	struct sockaddr		addrbuf;
	struct sockaddr_in	*nm;
	int			accessSocket;
	int			stopped;
} TcptsSap;

static void	removeReceiver(TcpRcvr *rcvr)
{
	TcptsSap	*sap = rcvr->sap;

	pthread_mutex_lock(&sap->rcvrPoolMutex);
	if (rcvr->fd != -1)
	{
		close(rcvr->fd);
	}

	if (rcvr->next)
	{
		rcvr->next->prev = rcvr->prev;
	}
	else
	{
		sap->lastInRcvrPool = rcvr->prev;
	}

	if (rcvr->prev)
	{
		rcvr->prev->next = rcvr->next;
	}
	else
	{
		sap->firstInRcvrPool = rcvr->next;
	}

	sap->nbrInRcvrPool--;
	pthread_mutex_unlock(&sap->rcvrPoolMutex);
	MRELEASE(rcvr);
}

static int	receiveBytesByTCP(int fd, char *into, int length)
{
	int	bytesRead;

	while (1)	/*	Continue until not interrupted.		*/
	{
		bytesRead = read(fd, into, length);
		switch (bytesRead)
		{
		case -1:
			if (errno == EINTR)
			{
				continue;
			}

			putSysErrmsg("tcpts read() error on socket", NULL);
			return -1;

		case 0:			/*	Connection closed.	*/
			return 0;

		default:
			return bytesRead;
		}
	}
}

static int	receiveMsgByTCP(int fd, char *buffer)
{
	unsigned short	preamble;
	char		*into;
	int		bytesToReceive;
	int		bytesReceived;
	unsigned short	msglen;

	/*	Receive length of transmitted message.			*/

	into = (char *) &preamble;
	bytesToReceive = sizeof preamble;
	while (bytesToReceive > 0)
	{
		bytesReceived = receiveBytesByTCP(fd, into, bytesToReceive);
		if (bytesReceived < 1)
		{
			return bytesReceived;
		}

		into += bytesReceived;
		bytesToReceive -= bytesReceived;
	}

	/*	Receive the message itself.				*/

	msglen = ntohs(preamble);
	into = buffer;
	bytesToReceive = msglen;
	while (bytesToReceive > 0)
	{
		bytesReceived = receiveBytesByTCP(fd, into, bytesToReceive);
		if (bytesReceived < 1)
		{
			return bytesReceived;
		}

		into += bytesReceived;
		bytesToReceive -= bytesReceived;
	}

	return msglen;
}

static void	removeSender(TcpTsep *tsep)
{
	TcptsSap	*sap = tsep->sap;

	pthread_mutex_lock(&sap->sendPoolMutex);
	close(tsep->fd);
	tsep->fd = -1;
	if (tsep->next)
	{
		tsep->next->prev = tsep->prev;
	}
	else
	{
		sap->lastInSendPool = tsep->prev;
	}

	if (tsep->prev)
	{
		tsep->prev->next = tsep->next;
	}
	else
	{
		sap->firstInSendPool = tsep->next;
	}

	sap->nbrInSendPool--;
	pthread_mutex_unlock(&sap->sendPoolMutex);
}

static int	sendBytesByTCP(TcpTsep *tsep, char *bytes, int length)
{
	int	result;

	while (1)	/*	Continue until not interrupted.		*/
	{
		result = send(tsep->fd, bytes, length, 0);
		if (result < 0)
		{
			if (errno == EINTR)	/*	Interrupted.	*/
			{
				continue;	/*	Try again.	*/
			}

			break;			/*	Handle failure.	*/
		}

		return 1;	/*	Successful transmission.	*/
	}

	/*	Unsuccessful transmission, for one reason or another.	*/

	removeSender(tsep);
	switch (errno)
	{
	case EPIPE:			/*	Lost connection.	*/
	case EBADF:			/*	Closed.			*/
	case ECONNRESET:		/*	Lost connection.	*/
		return 0;		/*	Non-fatal failure.	*/
		
	default:
		putSysErrmsg("tcpts send() error on socket", NULL);
		return -1;		/*	Fatal failure.		*/
	}
}

/*	*	*	*	MAMS stuff	*	*	*	*/

/*	TCP could be used as a primary transport service, now that we
 *	include MAMS endpoint names in all messages to which replies
 *	are required.  But this hasn't been implemented yet.		*/

static int	tcpComputeCsepName(char *endpointSpec, char *endpointName)
{
	putErrmsg("Sorry, no PTS support implemented in tcpts.", NULL);
	return -1;
}

int	tcpMamsInit(MamsInterface *tsif)
{
	putErrmsg("Sorry, no PTS support implemented in tcpts.", NULL);
	return -1;
}

static void	*tcpMamsAccess(void *parm)
{
	putErrmsg("Sorry, no PTS support implemented in tcpts.", NULL);
	return NULL;
}

static int	tcpParseMamsEndpoint(MamsEndpoint *ep)
{
	putErrmsg("Sorry, no PTS support implemented in tcpts.", NULL);
	return -1;
}

static void	tcpClearMamsEndpoint(MamsEndpoint *ep)
{
	putErrmsg("Sorry, no PTS support implemented in tcpts.", NULL);
}

static int	tcpSendMams(MamsEndpoint *ep, MamsInterface *tsif, char *msg,
			int msgLen)
{
	putErrmsg("Sorry, no PTS support implemented in tcpts.", NULL);
	return -1;
}

/*	*	*	*	AMS stuff	*	*	*	*/

int	tcpAmsInit(AmsInterface *tsif, char *epspec)
{
	unsigned short		portNbr;
	unsigned int		ipAddress;
	char			ownHostName[MAXHOSTNAMELEN + 1];
	TcptsSap		*sap;
	socklen_t		buflen;
	char			endpointNameText[32];
	int			eptLen;

	if (strcmp(epspec, "@") == 0)	/*	Default.		*/
	{
		epspec = NULL;	/*	Force default selection.	*/
	}

	parseSocketSpec(epspec, &portNbr, &ipAddress);
	if (ipAddress == 0)
	{
		getNameOfHost(ownHostName, sizeof ownHostName);
		ipAddress = getInternetAddress(ownHostName);
	}

	sap = (TcptsSap *) MTAKE(sizeof(TcptsSap));
	if (sap == NULL)
	{
		putSysErrmsg("No memory for TCP SAP", NULL);
		return -1;
	}

	memset((char *) sap, 0, sizeof(TcptsSap));
	pthread_mutex_init(&sap->sendPoolMutex, NULL);
	pthread_mutex_init(&sap->rcvrPoolMutex, NULL);
	memset((char *) &(sap->addrbuf), 0, sizeof(struct sockaddr));
	sap->nm = (struct sockaddr_in *) &(sap->addrbuf);
	sap->nm->sin_family = AF_INET;
	sap->nm->sin_port = htons(portNbr);
	sap->nm->sin_addr.s_addr = htonl(ipAddress);
	sap->accessSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (sap->accessSocket < 0)
	{
		putSysErrmsg("tcpts can't open AMS access socket", NULL);
		MRELEASE(sap);
		return -1;
	}

	buflen = sizeof(struct sockaddr);
	if (reUseAddress(sap->accessSocket)
	|| bind(sap->accessSocket, &(sap->addrbuf), buflen) < 0
	|| listen(sap->accessSocket, 5) < 0
	|| getsockname(sap->accessSocket, &(sap->addrbuf), &buflen) < 0)
	{
		putSysErrmsg("tcpts can't open AMS access socket", NULL);
		close(sap->accessSocket);
		MRELEASE(sap);
		return -1;
	}

	ipAddress = sap->nm->sin_addr.s_addr;
	ipAddress = ntohl(ipAddress);
	portNbr = sap->nm->sin_port;
	portNbr = ntohs(portNbr);
	tsif->diligence = AmsAssured;
	tsif->sequence = AmsTransmissionOrder;
	isprintf(endpointNameText, sizeof endpointNameText, "%u:%hu", ipAddress,
			portNbr);
	eptLen = strlen(endpointNameText) + 1;
	tsif->ept = MTAKE(eptLen);
	if (tsif->ept == NULL)
	{
		putSysErrmsg(NoMemoryMemo, NULL);
		close(sap->accessSocket);
		MRELEASE(sap);
		return -1;
	}

	istrcpy(tsif->ept, endpointNameText, eptLen);
	tsif->sap = (void *) sap;
	return 0;
}

static void	*tcpAmsReceiver(void *parm)
{
	TcpRcvr		*me = (TcpRcvr *) parm;
	TcptsSap	*sap = me->sap;
	char		*buffer;
	sigset_t	signals;
	int		length;

	buffer = MTAKE(TCPTS_MAX_MSG_LEN);
	if (buffer == NULL)
	{
		putSysErrmsg(NoMemoryMemo, NULL);
		return NULL;
	}

	sigfillset(&signals);
	pthread_sigmask(SIG_BLOCK, &signals, NULL);
	while (1)
	{
		length = receiveMsgByTCP(me->fd, buffer);
		switch (length)
		{
		case -1:
			if (errno != EBADF)
			{
				putSysErrmsg("tcpts receiver crashed", NULL);
			}

			/*	Intentional fall-through to next case.	*/

		case 0:	/*	Connection simply closed by sender.	*/
			MRELEASE(buffer);
			removeReceiver(me);
			return NULL;

		default:
			break;
		}

		/*	Got an AMS message.				*/

		if (enqueueAmsMsg(me->amsSap, (unsigned char *) buffer, length)
				< 0)
		{
			putErrmsg("tcpts discarded AMS message.", NULL);
		}

		/*	Promote self to top of AMS receiver pool.	*/

		pthread_mutex_lock(&sap->rcvrPoolMutex);
		if (sap->firstInRcvrPool != me)
		{
			if (me->prev)
			{
				me->prev->next = me->next;
			}

			if (me->next)
			{
				me->next->prev = me->prev;
			}
			else
			{
				sap->lastInRcvrPool = me->prev;
			}

			me->next = sap->firstInRcvrPool;
			me->prev = NULL;
			sap->firstInRcvrPool->prev = me;
			sap->firstInRcvrPool = me;
		}

		pthread_mutex_unlock(&sap->rcvrPoolMutex);
	}
}

static void	*tcpAmsAccess(void *parm)
{
	AmsInterface		*tsif = (AmsInterface *) parm;
	TcptsSap		*sap = (TcptsSap *) (tsif->sap);
	int			childSocket;
	struct sockaddr		clientSockname;
	socklen_t		len;
	sigset_t		signals;
	TcpRcvr			*rcvr;

	sigfillset(&signals);
	pthread_sigmask(SIG_BLOCK, &signals, NULL);
	while (1)
	{
		len = sizeof clientSockname;
		childSocket = accept(sap->accessSocket, &clientSockname, &len);
		if (childSocket < 0)
		{
			putSysErrmsg("tcpts failed on accept()", NULL);
			break;
		}

		if (sap->stopped)
		{
			close(childSocket);
			break;
		}

		rcvr = MTAKE(sizeof(TcpRcvr));
		if (rcvr == NULL)
		{
			close(childSocket);
			putSysErrmsg("tcpts out of memory", NULL);
			break;
		}

		memset((char *) rcvr, 0, sizeof(TcpRcvr));
		rcvr->fd = childSocket;
		rcvr->amsSap = tsif->amsSap;
		rcvr->prev = NULL;
		rcvr->sap = sap;

		/*	Manage pool of 32 most active inbound sockets.	*/

		pthread_mutex_lock(&sap->rcvrPoolMutex);
		if (sap->nbrInRcvrPool < 32)
		{
			pthread_mutex_unlock(&sap->rcvrPoolMutex);
		}
		else
		{
			close(sap->lastInRcvrPool->fd);
			sap->lastInRcvrPool->fd = -1;
			pthread_mutex_unlock(&sap->rcvrPoolMutex);
			pthread_join(sap->lastInRcvrPool->thread, NULL);
		}

		pthread_mutex_lock(&sap->rcvrPoolMutex);
		rcvr->next = sap->firstInRcvrPool;
		if (sap->firstInRcvrPool == NULL)
		{
			sap->lastInRcvrPool = rcvr;
		}
		else
		{
			sap->firstInRcvrPool->prev = rcvr;
		}

		sap->firstInRcvrPool = rcvr;
		sap->nbrInRcvrPool++;
		pthread_mutex_unlock(&sap->rcvrPoolMutex);

		/*	Animate the new receiver.			*/

		if (pthread_create(&(rcvr->thread), NULL, tcpAmsReceiver, rcvr)
				< 0)
		{
			putSysErrmsg("tcpts can't start Mams receiver thread",
					NULL);
			removeReceiver(rcvr);
			break;
		}
	}

	/*	Shutting down this Tcpts service access point.		*/

	while (sap->firstInSendPool)
	{
		removeSender(sap->firstInSendPool);
	}

	pthread_mutex_lock(&sap->rcvrPoolMutex);
	while (sap->firstInRcvrPool)
	{
		close(sap->firstInRcvrPool->fd);
		sap->firstInRcvrPool->fd = -1;
		pthread_mutex_unlock(&sap->rcvrPoolMutex);
		pthread_join(sap->firstInRcvrPool->thread, NULL);
		pthread_mutex_lock(&sap->rcvrPoolMutex);
	}

	pthread_mutex_unlock(&sap->rcvrPoolMutex);
	if (sap->accessSocket != -1)
	{
		close(sap->accessSocket);
	}

	MRELEASE(sap);
	tsif->sap = NULL;
	return NULL;
}

static int	tcpParseAmsEndpoint(AmsEndpoint *dp)
{
	TcpTsep	tsep;

	if (dp == NULL || dp->ept == NULL)
	{
		errno = EINVAL;
		putErrmsg("tcpts can't parse AMS endpoint.", NULL);
		return -1;
	}

	memset((char *) &tsep, 0, sizeof(TcpTsep));
	tsep.fd = -1;		/*	Not connected yet.		*/
	if (sscanf(dp->ept, "%u:%hu", &tsep.ipAddress, &tsep.portNbr) != 2)
	{
		errno = EINVAL;
		putErrmsg("tcpts found AMS endpoint name invalid.", dp->ept);
		return -1;
	}

	/*	Note: fill in tsep.sap when the tsep is first used
	 *	for transmission.					*/

	dp->tsep = MTAKE(sizeof(TcpTsep));
	if (dp->tsep == NULL)
	{
		putSysErrmsg("tcpts can't record parsed AMS endpoint name.",
				NULL);
		return -1;
	}

	memcpy((char *) (dp->tsep), (char *) &tsep, sizeof(TcpTsep));

	/*	Also parse out the QOS of this endpoint.		*/

	dp->diligence = AmsAssured;
	dp->sequence = AmsTransmissionOrder;
	return 0;
}

static void	tcpClearAmsEndpoint(AmsEndpoint *dp)
{
	TcpTsep	*tsep = dp->tsep;

	if (tsep == NULL)
	{
		return;
	}

	if (tsep->fd != -1)
	{
		removeSender(tsep);
	}

	MRELEASE(tsep);
}

static int	tcpSendAms(AmsEndpoint *dp, AmsSAP *sap,
			unsigned char flowLabel, char *header, int headerLen,
			char *content, int contentLen)
{
	static char		tcpAmsBuf[TCPTS_MAX_MSG_LEN];
	unsigned int		xmitlen;
	TcpTsep			*tsep;
	int			i;
	AmsInterface		*tsif;
	TcptsSap		*tcpSap;
	struct sockaddr		buf;
	struct sockaddr_in	*nm = (struct sockaddr_in *) &buf;
	unsigned short		checksum;
	unsigned short		preamble;
	int			result;

	if (dp == NULL || sap == NULL || header == NULL || headerLen < 0
	|| contentLen < 0 || (contentLen > 0 && content == NULL)
	|| (xmitlen = (headerLen + contentLen + 2)) > TCPTS_MAX_MSG_LEN)
	{
		errno = EINVAL;
		putErrmsg("Can't use TCP to send AMS message.", NULL);
		return -1;
	}

	tsep = (TcpTsep *) (dp->tsep);
	if (tsep == NULL)	/*	Lost connectivity to endpoint.	*/
	{
		return 0;
	}

	for (i = 0, tsif = sap->amsTsifs; i < sap->transportServiceCount; i++,
			tsif++)
	{
		if (tsif->ts == dp->ts)
		{
			break;	/*	Have found interface to use.	*/
		}
	}

	if (i == sap->transportServiceCount)	/*	No match.	*/
	{
		return 0;	/*	Cannot send msg to endpoint.	*/
	}

	tcpSap = (TcptsSap *) (tsif->sap);
	memcpy(tcpAmsBuf, header, headerLen);
	if (contentLen > 0)
	{
		memcpy(tcpAmsBuf + headerLen, content, contentLen);
	}

	checksum = computeAmsChecksum((unsigned char *) tcpAmsBuf,
			headerLen + contentLen);
	checksum = htons(checksum);
	memcpy(tcpAmsBuf + headerLen + contentLen, (char *) &checksum, 2);
	if (tsep->fd < 0)	/*	Must open socket connection.	*/
	{
		memset((char *) &buf, 0, sizeof buf);
		nm->sin_family = AF_INET;
		nm->sin_port = htons(tsep->portNbr);
		nm->sin_addr.s_addr = htonl(tsep->ipAddress);
		tsep->fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (tsep->fd < 0)
		{
			putSysErrmsg("tcpts can't open TCP socket", NULL);
			return -1;
		}

		if (connect(tsep->fd, &buf, sizeof(struct sockaddr)) < 0)
		{
			close(tsep->fd);
			tsep->fd = -1;
			putSysErrmsg("tcpts can't connect to TCP socket", NULL);
			return -1;
		}

		/*	Must be inserted into pool of 32 most
		 *	active outbound sockets.		*/

		pthread_mutex_lock(&tcpSap->sendPoolMutex);
		if (tcpSap->nbrInSendPool < 32)
		{
			pthread_mutex_unlock(&tcpSap->sendPoolMutex);
		}
		else	/*	Must first ditch least active.	*/
		{
			pthread_mutex_unlock(&tcpSap->sendPoolMutex);
			removeSender(tcpSap->lastInSendPool);
		}

		pthread_mutex_lock(&tcpSap->sendPoolMutex);
		tsep->next = tcpSap->firstInSendPool;
		if (tcpSap->firstInSendPool == NULL)
		{
			tcpSap->lastInSendPool = tsep;
		}
		else
		{
			tcpSap->firstInSendPool->prev = tsep;
		}

		tcpSap->firstInSendPool = tsep;
		tcpSap->nbrInSendPool++;
		pthread_mutex_unlock(&tcpSap->sendPoolMutex);
		tsep->sap = tcpSap;
	}

	preamble = xmitlen;
	preamble = htons(preamble);
	result = sendBytesByTCP(tsep, (char *) &preamble, sizeof preamble);
	if (result < 1)		/*	Data not transmitted.		*/
	{
		return result;
	}

	result = sendBytesByTCP(tsep, tcpAmsBuf, xmitlen);
	if (result < 1)		/*	Data not transmitted.		*/
	{
		return result;
	}

	/*	Succeeded; promote self to top of AMS sender pool.	*/

	pthread_mutex_lock(&tcpSap->sendPoolMutex);
	if (tcpSap->firstInSendPool != tsep)
	{
		tsep->prev->next = tsep->next;
		if (tsep->next)
		{
			tsep->next->prev = tsep->prev;
		}
		else
		{
			tcpSap->lastInSendPool = tsep->prev;
		}

		tsep->next = tcpSap->firstInSendPool;
		tsep->prev = NULL;
		tcpSap->firstInSendPool->prev = tsep;
		tcpSap->firstInSendPool = tsep;
	}

	pthread_mutex_unlock(&tcpSap->sendPoolMutex);
	return 0;
}

static void	tcpShutdown(void *abstract_sap)
{
	TcptsSap	*tcpSap = (TcptsSap *) (abstract_sap);
	int		fd;

	tcpSap->stopped = 1;

	/*	Wake up our own access thread by connecting to it.	*/

	fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (fd >= 0)
	{
		connect(fd, &(tcpSap->addrbuf), sizeof(struct sockaddr));

		/*	Immediately discard the connected socket.	*/

		close(fd);
	}
}

void	tcptsLoadTs(TransSvc *ts)
{
	ts->name = "tcp";
	ts->csepNameFn = tcpComputeCsepName;
	ts->mamsInitFn = tcpMamsInit;
	ts->mamsReceiverFn = tcpMamsAccess;
	ts->parseMamsEndpointFn = tcpParseMamsEndpoint;
	ts->clearMamsEndpointFn = tcpClearMamsEndpoint;
	ts->sendMamsFn = tcpSendMams;
	ts->amsInitFn = tcpAmsInit;
	ts->amsReceiverFn = tcpAmsAccess;
	ts->parseAmsEndpointFn = tcpParseAmsEndpoint;
	ts->clearAmsEndpointFn = tcpClearAmsEndpoint;
	ts->sendAmsFn = tcpSendAms;
	ts->shutdownFn = tcpShutdown;
	signal(SIGPIPE, SIG_IGN);
}
