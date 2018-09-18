// Simple example (single event source only)
// of how to handle DNSServiceDiscovery events using a select() loop

#include <dns_sd.h>
#include <stdio.h>			// For stdout, stderr
#include <string.h>			// For strlen(), strcpy(), bzero()
#include <errno.h>          // For errno, EINTR
#include <time.h>

#ifdef _WIN32
#include <process.h>
typedef	int	pid_t;
#define	getpid	_getpid
#define	strcasecmp	_stricmp
#define snprintf _snprintf
#else
#include <sys/time.h>		// For struct timeval
#include <unistd.h>         // For getopt() and optind
#include <arpa/inet.h>		// For inet_addr()
#endif

// Note: the select() implementation on Windows (Winsock2)
//fails with any timeout much larger than this
#define LONG_TIME 100000000

static volatile int stopNow = 0;
static volatile int timeOut = LONG_TIME;

void HandleEvents(DNSServiceRef serviceRef)
	{
	int dns_sd_fd = DNSServiceRefSockFD(serviceRef);
	int nfds = dns_sd_fd + 1;
	fd_set readfds;
	struct timeval tv;
	int result;

	while (!stopNow)
		{
		FD_ZERO(&readfds);
		FD_SET(dns_sd_fd, &readfds);
		tv.tv_sec = timeOut;
		tv.tv_usec = 0;

		result = select(nfds, &readfds, (fd_set*)NULL, (fd_set*)NULL, &tv);
		if (result > 0)
			{
			DNSServiceErrorType err = kDNSServiceErr_NoError;
			if (FD_ISSET(dns_sd_fd, &readfds))
                  err = DNSServiceProcessResult(serviceRef);
			if (err) { fprintf(stderr,
                           "DNSServiceProcessResult returned %d\n", err);
                        stopNow = 1; }
			}
		else
			{
			printf("select() returned %d errno %d %s\n",
                      result, errno, strerror(errno));
			if (errno != EINTR) stopNow = 1;
			}
		}
	}
