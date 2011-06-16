/*
	ltpcounter.c:	a test block counter.  Hard-coded for client
			ID = 1, i.e., bundle protocol.
									*/
/*									*/
/*	Copyright (c) 2007, California Institute of Technology.		*/
/*	All rights reserved.						*/
/*	Author: Scott Burleigh, Jet Propulsion Laboratory		*/
/*									*/

#include "platform.h"
#include "ltpP.h"

static int	_running(int *newState)
{
	int	state = 1;

	if (newState)
	{
		state = *newState;
	}

	return state;
}

static int	_sessionsCanceled(int increment)
{
	static int	count = 0;
	
	if (increment)
	{
		count += increment;
	}
	
	return count;
}

static int	_blocksReceived(int increment)
{
	static int	count = 0;
	
	if (increment)
	{
		count += increment;
	}
	
	return count;

}
static int	_bytesReceived(int increment)
{
	static int	count = 0;
	
	if (increment)
	{
		count += increment;
	}
	
	return count;
}

static void	showProgress()
{
	PUTS("v");
	fflush(stdout);
	alarm(5);
}

static void	handleQuit()
{
	int	stop = 0;

	oK(_running(&stop));
	ltp_interrupt(1);
}

static void	printCount()
{
	PUTMEMO("Sessions canceled", itoa(_sessionsCanceled(0)));
	PUTMEMO("Blocks received", itoa(_blocksReceived(0)));
	PUTMEMO("Bytes received", itoa(_bytesReceived(0)));
	fflush(stdout);
}

#if defined (VXWORKS) || defined (RTEMS)
int	ltpcounter(int a1, int a2, int a3, int a4, int a5,
		int a6, int a7, int a8, int a9, int a10)
{
	int		maxBytes = a1;
#else
int	main(int argc, char **argv)
{
	int		maxBytes = (argc > 1 ? atoi(argv[1]) : 0);
#endif
	int		state = 1;
	LtpNoticeType	type;
	LtpSessionId	sessionId;
	unsigned char	reasonCode;
	unsigned char	endOfBlock;
	unsigned long	dataOffset;
	unsigned long	dataLength;
	Object		data;
	char		buffer[255];

	if (maxBytes < 1)
	{
		maxBytes = 2000000000;
	}

	if (ltp_attach() < 0)
	{
		putErrmsg("ltpcounter can't initialize LTP.", NULL);
		return 1;
	}

	if (ltp_open(1) < 0)
	{
		putErrmsg("ltpcounter can't open client access.", "1");
		return 1;
	}

	isignal(SIGALRM, showProgress);
	alarm(5);
	isignal(SIGINT, handleQuit);
	oK((_running(&state)));
	while (_running(NULL))
	{
		if (ltp_get_notice(1, &type, &sessionId,
				&reasonCode, &endOfBlock, &dataOffset,
				&dataLength, &data) < 0)
		{
			putErrmsg("Can't get LTP notice.", NULL);
			state = 0;
			oK((_running(&state)));
			continue;
		}

		switch (type)
		{
		case LtpExportSessionCanceled:
			isprintf(buffer, sizeof buffer, "Transmission \
canceled: source engine %lu, session %lu, reason code %d.",
					sessionId.sourceEngineId,
					sessionId.sessionNbr, reasonCode);
			writeMemo(buffer);
			if (data)
			{
				ltp_release_data(data);
			}

			break;

		case LtpImportSessionCanceled:
			oK(_sessionsCanceled(1));
			isprintf(buffer, sizeof buffer, "Reception canceled: \
source engine %lu, session %lu, reason code %d.", sessionId.sourceEngineId,
					sessionId.sessionNbr, reasonCode);
			writeMemo(buffer);
			break;

		case LtpRecvGreenSegment:
			isprintf(buffer, sizeof buffer, "Green segment \
received, discarded: source engine %lu, session %lu, offset %lu, length %lu, \
eob=%d.", sessionId.sourceEngineId, sessionId.sessionNbr, dataOffset,
					dataLength, endOfBlock);
			writeMemo(buffer);
			ltp_release_data(data);
			break;

		case LtpRecvRedPart:
			oK(_blocksReceived(1));
			oK(_bytesReceived(dataLength));
			ltp_release_data(data);
			break;

		default:
			break;
		}

		if (_bytesReceived(0) >= maxBytes)
		{
			state = 0;
			oK((_running(&state)));
		}
	}

	writeErrmsgMemos();
	printCount();
	PUTS("Stopping ltpcounter.");
	ltp_close(1);
	ltp_detach();
	return 0;
}
