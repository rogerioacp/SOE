/*-------------------------------------------------------------------------
 *
 * logger.h
 *	  log definitions for any logging system to be integrated
 *
 *
 * Copyright (c) 2018-2019, HASLab
 *
 *
 *-------------------------------------------------------------------------
 */

#ifndef SOE_LOGGER_H
#define SOE_LOGGER_H

#include <stdarg.h>

#define BUFSIZE 200

/* Error level codes */
#define DEBUG5		10			/* Debugging messages, in categories of
								 * decreasing detail. */
#define DEBUG4		11
#define DEBUG3		12
#define DEBUG2		13
#define DEBUG1		14			/* used by GUC debug_* variables */
#define LOG			15			/* Server operational messages; sent only to
								 * server log by default. */
#define LOG_SERVER_ONLY 16		/* Same as LOG for server reporting, but never
								 * sent to client. */
#define COMMERROR	LOG_SERVER_ONLY /* Client communication problems; same as
									 * LOG for server reporting, but never
									 * sent to client. */
#define INFO		17			/* Messages specifically requested by user (eg
								 * VACUUM VERBOSE output); always sent to
								 * client regardless of client_min_messages,
								 * but by default not sent to server log. */
#define NOTICE		18			/* Helpful messages to users about query
								 * operation; sent to client and not to server
								 * log by default. */
#define WARNING		19			/* Warnings.  NOTICE is for expected messages
								 * like implicit sequence creation by SERIAL.
								 * WARNING is for unexpected messages. */
#define ERROR		20			/* user error - abort transaction; return to
								 * known state */

void selog(int level, const char* message, ...);

#endif /* SOE_LOG_H */