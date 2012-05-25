/*	bpcpd.c:	bpcpd, a remote copy daemon that utilizes CFDP
 *
 *	Copyright (c) 2012, California Institute of Technology.
 *	All rights reserved.
 *	Author: Samuel Jero <sj323707@ohio.edu>, Ohio University
 */
#include "bpcp.h"



int debug = 0;	/*Set to non-zero to enable debug output. */



void poll_cfdp_messages();
void dbgprintf(int level, const char *fmt, ...);
void usage(void);
void version();
#ifdef CLEAN_ON_EXIT
void exit_cleanup();
#endif

/*Start Here*/
#if defined (VXWORKS) || defined (RTEMS)
int	bpcp(int a1, int a2, int a3, int a4, int a5,
		int a6, int a7, int a8, int a9, int a10)
{
/*a1 is the debug flag*/
debug=atoi((char*)a1);

/*a2 is the version option*/
if(atoi((char*)a2)==1)
{
	version();
	return 0;
}

{
#else
int main(int argc, char **argv)
{
	int ch;

	/*Parse commandline options*/
	while ((ch = getopt(argc, argv, "dv")) != -1)
	{
		switch (ch)
		{
			case 'd':
				/*Debug*/
				debug++;
				break;
			case 'v':
				version();
				break;
			default:
				usage();
		}
	}
#endif

	/*Initialize CFDP*/
	if (cfdp_init() < 0)
	{
		dbgprintf(0, "Error: Can't initialize CFDP. Is ION running?\n");
		exit(1);
	}

	poll_cfdp_messages();

	exit(0);
}

/*CFDP Event Polling loop*/
void poll_cfdp_messages()
{
	char *eventTypes[] =	{
					"no event",
					"transaction started",
					"EOF sent",
					"transaction finished",
					"metadata received",
					"file data segment received",
					"EOF received",
					"suspended",
					"resumed",
					"transaction report",
					"fault",
					"abandoned"
				};
	CfdpEventType		type;
	time_t				time;
	int					reqNbr;
	CfdpTransactionId	transactionId;
	char				sourceFileNameBuf[256];
	char				destFileNameBuf[256];
	unsigned int		fileSize;
	MetadataList		messagesToUser;
	unsigned int		offset;
	unsigned int		length;
	CfdpCondition		condition;
	unsigned int		progress;
	CfdpFileStatus		fileStatus;
	CfdpDeliveryCode	deliveryCode;
	CfdpTransactionId	originatingTransactionId;
	char				statusReportBuf[256];
	unsigned char		usrmsgBuf[256];
	MetadataList		filestoreResponses;
	unsigned long 		TID11;
	unsigned long		TID12;

#ifdef CLEAN_ON_EXIT
	/*Set SIGTERM and SIGINT handlers*/
	isignal(SIGTERM, exit_cleanup);
	isignal(SIGINT, exit_cleanup);
#endif

	/*Main Event loop*/
	while (1) {

		/*Grab a CFDP event*/
		if (cfdp_get_event(&type, &time, &reqNbr, &transactionId,
						sourceFileNameBuf, destFileNameBuf,
						&fileSize, &messagesToUser, &offset, &length,
						&condition, &progress, &fileStatus,
						&deliveryCode, &originatingTransactionId,
						statusReportBuf, &filestoreResponses) < 0)
		{
			dbgprintf(0, "Error: Failed getting CFDP event.", NULL);
			exit(1);
		}

		if (type == CfdpNoEvent)
		{
			continue;	/*	Interrupted.		*/
		}

		/*Decompress transaction ID*/
		cfdp_decompress_number(&TID11,&transactionId.sourceEntityNbr);
		cfdp_decompress_number(&TID12,&transactionId.transactionNbr);

		/*Print Event type if debugging*/
		dbgprintf(1,"\nEvent: type %d, '%s', From Node: %d, Transaction ID: %d.%d.\n", type,
				(type > 0 && type < 12) ? eventTypes[type]
				: "(unknown)",TID11, TID11, TID12);

		/*Parse Messages to User to get directory information*/
		while (messagesToUser)
		{
			/*Get user message*/
			memset(usrmsgBuf, 0, 256);
			if (cfdp_get_usrmsg(&messagesToUser, usrmsgBuf,
					(int *) &length) < 0)
			{
				putErrmsg("Failed getting user msg.", NULL);
				continue;
			}

			/*Set Null character at end of string*/
			if (length > 0)
			{
				usrmsgBuf[length] = '\0';
				dbgprintf(2,"\tUser Message '%s'\n", usrmsgBuf);
			}
		}

	}
	return;
}

/*Debug Printf*/
void dbgprintf(int level, const char *fmt, ...)
{
    va_list args;
    if(debug>=level)
    {
    	va_start(args, fmt);
    	vfprintf(stderr, fmt, args);
    	va_end(args);
    }
}

/*Print Command usage to stderr and exit*/
void usage(void)
{
	(void) fprintf(stderr, "usage: bpcpd [-d | -v]\n");
	exit(1);
}

/*Print Version Information*/
void version()
{
	dbgprintf(0, BPCP_VERSION_STRING);
	exit(1);
}

#ifdef CLEAN_ON_EXIT
void exit_cleanup()
{
	/*Reset signal handlers for portability*/
	isignal(SIGTERM, exit_cleanup);
	isignal(SIGINT, exit_cleanup);

#if defined (VXWORKS) || defined (RTEMS)
	/*DO NOTHING. VXWORKS doesn't implement system()!*/
#else

	/*Cleanup all directory listing files*/
	if (system("rm dirlist_* >/dev/null 2>/dev/null")<0)
	{
		dbgprintf(0, "Error running cleanup\n");
	}
#endif

	/*Drop to new line*/
	printf("\n");

	exit(0);
}
#endif
