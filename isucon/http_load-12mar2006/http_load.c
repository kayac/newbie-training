/* http_load - multiprocessing http test client
**
** Copyright © 1998,1999,2001 by Jef Poskanzer <jef@mail.acme.com>.
** All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions
** are met:
** 1. Redistributions of source code must retain the above copyright
**    notice, this list of conditions and the following disclaimer.
** 2. Redistributions in binary form must reproduce the above copyright
**    notice, this list of conditions and the following disclaimer in the
**    documentation and/or other materials provided with the distribution.
** 
** THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
** ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
** IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
** ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
** FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
** DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
** OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
** HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
** LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
** OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
** SUCH DAMAGE.
*/

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>
#include <signal.h>

#ifdef USE_SSL
#include <openssl/ssl.h>
#include <openssl/rand.h>
#include <openssl/err.h>
#endif

#include "version.h"
#include "port.h"
#include "timers.h"

#if defined(AF_INET6) && defined(IN6_IS_ADDR_V4MAPPED)
#define USE_IPV6
#endif

#define max(a,b) ((a)>=(b)?(a):(b))
#define min(a,b) ((a)<=(b)?(a):(b))

/* How long a connection can stay idle before we give up on it. */
#define IDLE_SECS 60

/* Default max bytes/second in throttle mode. */
#define THROTTLE 3360

/* How often to show progress reports. */
#define PROGRESS_SECS 60

/* How many file descriptors to not use. */
#define RESERVED_FDS 3


typedef struct {
    char* url_str;
    int protocol;
    char* hostname;
    unsigned short port;
#ifdef USE_IPV6
    struct sockaddr_in6 sa;
#else /* USE_IPV6 */
    struct sockaddr_in sa;
#endif /* USE_IPV6 */
    int sa_len, sock_family, sock_type, sock_protocol;
    char* filename;
    int got_bytes;
    long bytes;
    int got_checksum;
    long checksum;
    } url;
static url* urls;
static int num_urls, max_urls;

typedef struct {
    char* str;
    struct sockaddr_in sa;
    } sip;
static sip* sips;
static int num_sips, max_sips;

/* Protocol symbols. */
#define PROTO_HTTP 0
#ifdef USE_SSL
#define PROTO_HTTPS 1
#endif

typedef struct {
    int url_num;
    struct sockaddr_in sa;
    int sa_len;
    int conn_fd;
#ifdef USE_SSL
    SSL* ssl;
#endif
    int conn_state, header_state;
    int did_connect, did_response;
    struct timeval started_at;
    struct timeval connect_at;
    struct timeval request_at;
    struct timeval response_at;
    Timer* idle_timer;
    Timer* wakeup_timer;
    long content_length;
    long bytes;
    long checksum;
    int http_status;
    } connection;
static connection* connections;
static int max_connections, num_connections, max_parallel;

static int http_status_counts[1000];	/* room for all three-digit statuses */

#define CNST_FREE 0
#define CNST_CONNECTING 1
#define CNST_HEADERS 2
#define CNST_READING 3
#define CNST_PAUSING 4

#define HDST_LINE1_PROTOCOL 0
#define HDST_LINE1_WHITESPACE 1
#define HDST_LINE1_STATUS 2
#define HDST_BOL 10
#define HDST_TEXT 11
#define HDST_LF 12
#define HDST_CR 13
#define HDST_CRLF 14
#define HDST_CRLFCR 15
#define HDST_C 20
#define HDST_CO 21
#define HDST_CON 22
#define HDST_CONT 23
#define HDST_CONTE 24
#define HDST_CONTEN 25
#define HDST_CONTENT 26
#define HDST_CONTENT_ 27
#define HDST_CONTENT_L 28
#define HDST_CONTENT_LE 29
#define HDST_CONTENT_LEN 30
#define HDST_CONTENT_LENG 31
#define HDST_CONTENT_LENGT 32
#define HDST_CONTENT_LENGTH 33
#define HDST_CONTENT_LENGTH_COLON 34
#define HDST_CONTENT_LENGTH_COLON_WHITESPACE 35
#define HDST_CONTENT_LENGTH_COLON_WHITESPACE_NUM 36

static char* argv0;
static int do_checksum, do_throttle, do_verbose, do_jitter, do_proxy;
static float throttle;
static int idle_secs;
static char* proxy_hostname;
static unsigned short proxy_port;

static struct timeval start_at;
static int fetches_started, connects_completed, responses_completed, fetches_completed;
static long long total_bytes;
static long long total_connect_usecs, max_connect_usecs, min_connect_usecs;
static long long total_response_usecs, max_response_usecs, min_response_usecs;
int total_timeouts, total_badbytes, total_badchecksums;

static long start_interval, low_interval, high_interval, range_interval;

#ifdef USE_SSL
static SSL_CTX* ssl_ctx = (SSL_CTX*) 0;
static char* cipher = (char*) 0;
#endif

/* Forwards. */
static void usage( void );
static void read_url_file( char* url_file );
static void lookup_address( int url_num );
static void read_sip_file( char* sip_file );
static void start_connection( struct timeval* nowP );
static void start_socket( int url_num, int cnum, struct timeval* nowP );
static void handle_connect( int cnum, struct timeval* nowP, int double_check );
static void handle_read( int cnum, struct timeval* nowP );
static void idle_connection( ClientData client_data, struct timeval* nowP );
static void wakeup_connection( ClientData client_data, struct timeval* nowP );
static void close_connection( int cnum );
static void progress_report( ClientData client_data, struct timeval* nowP );
static void start_timer( ClientData client_data, struct timeval* nowP );
static void end_timer( ClientData client_data, struct timeval* nowP );
static void finish( struct timeval* nowP );
static long long delta_timeval( struct timeval* start, struct timeval* finish );
static void* malloc_check( size_t size );
static void* realloc_check( void* ptr, size_t size );
static char* strdup_check( char* str );
static void check( void* ptr );


int
main( int argc, char** argv )
    {
    int argn;
    int start;
#define START_NONE 0
#define START_PARALLEL 1
#define START_RATE 2
    int start_parallel = -1, start_rate = -1;
    int end;
#define END_NONE 0
#define END_FETCHES 1
#define END_SECONDS 2
    int end_fetches = -1, end_seconds = -1;
    int cnum;
    char* url_file;
    char* sip_file;
#ifdef RLIMIT_NOFILE
    struct rlimit limits;
#endif /* RLIMIT_NOFILE */
    fd_set rfdset;
    fd_set wfdset;
    struct timeval now;
    int i, r;

    max_connections = 64 - RESERVED_FDS;	/* a guess */
#ifdef RLIMIT_NOFILE
    /* Try and increase the limit on # of files to the maximum. */
    if ( getrlimit( RLIMIT_NOFILE, &limits ) == 0 )
	{
	if ( limits.rlim_cur != limits.rlim_max )
	    {
	    if ( limits.rlim_max == RLIM_INFINITY )
		limits.rlim_cur = 8192;		/* arbitrary */
	    else if ( limits.rlim_max > limits.rlim_cur )
		limits.rlim_cur = limits.rlim_max;
	    (void) setrlimit( RLIMIT_NOFILE, &limits );
	    }
	max_connections = limits.rlim_cur - RESERVED_FDS;
	}
#endif /* RLIMIT_NOFILE */

    /* Parse args. */
    argv0 = argv[0];
    argn = 1;
    do_checksum = do_throttle = do_verbose = do_jitter = do_proxy = 0;
    throttle = THROTTLE;
    sip_file = (char*) 0;
    idle_secs = IDLE_SECS;
    start = START_NONE;
    end = END_NONE;
    while ( argn < argc && argv[argn][0] == '-' && argv[argn][1] != '\0' )
	{
	if ( strncmp( argv[argn], "-checksum", strlen( argv[argn] ) ) == 0 )
	    do_checksum = 1;
	else if ( strncmp( argv[argn], "-throttle", strlen( argv[argn] ) ) == 0 )
	    do_throttle = 1;
	else if ( strncmp( argv[argn], "-Throttle", strlen( argv[argn] ) ) == 0 && argn + 1 < argc )
	    {
	    do_throttle = 1;
	    throttle = atoi( argv[++argn] ) / 10.0;
	    }
	else if ( strncmp( argv[argn], "-verbose", strlen( argv[argn] ) ) == 0 )
	    do_verbose = 1;
	else if ( strncmp( argv[argn], "-timeout", strlen( argv[argn] ) ) == 0 && argn + 1 < argc )
	    idle_secs = atoi( argv[++argn] );
	else if ( strncmp( argv[argn], "-jitter", strlen( argv[argn] ) ) == 0 )
	    do_jitter = 1;
	else if ( strncmp( argv[argn], "-parallel", strlen( argv[argn] ) ) == 0 && argn + 1 < argc )
	    {
	    start = START_PARALLEL;
	    start_parallel = atoi( argv[++argn] );
	    if ( start_parallel < 1 )
		{
		(void) fprintf(
		    stderr, "%s: parallel must be at least 1\n", argv0 );
		exit( 1 );
		}
	    if ( start_parallel > max_connections )
		{
		(void) fprintf(
		    stderr, "%s: parallel may be at most %d\n", argv0, max_connections );
		exit( 1 );
		}
	    }
	else if ( strncmp( argv[argn], "-rate", strlen( argv[argn] ) ) == 0 && argn + 1 < argc )
	    {
	    start = START_RATE;
	    start_rate = atoi( argv[++argn] );
	    if ( start_rate < 1 )
		{
		(void) fprintf(
		    stderr, "%s: rate must be at least 1\n", argv0 );
		exit( 1 );
		}
	    if ( start_rate > 1000 )
		{
		(void) fprintf(
		    stderr, "%s: rate may be at most 1000\n", argv0 );
		exit( 1 );
		}
	    }
	else if ( strncmp( argv[argn], "-fetches", strlen( argv[argn] ) ) == 0 && argn + 1 < argc )
	    {
	    end = END_FETCHES;
	    end_fetches = atoi( argv[++argn] );
	    if ( end_fetches < 1 )
		{
		(void) fprintf(
		    stderr, "%s: fetches must be at least 1\n", argv0 );
		exit( 1 );
		}
	    }
	else if ( strncmp( argv[argn], "-seconds", strlen( argv[argn] ) ) == 0 && argn + 1 < argc )
	    {
	    end = END_SECONDS;
	    end_seconds = atoi( argv[++argn] );
	    if ( end_seconds < 1 )
		{
		(void) fprintf(
		    stderr, "%s: seconds must be at least 1\n", argv0 );
		exit( 1 );
		}
	    }
	else if ( strncmp( argv[argn], "-sip", strlen( argv[argn] ) ) == 0 && argn + 1 < argc )
	    sip_file = argv[++argn];
#ifdef USE_SSL
	else if ( strncmp( argv[argn], "-cipher", strlen( argv[argn] ) ) == 0 && argn + 1 < argc )
	    {
	    cipher = argv[++argn];
	    if ( strcasecmp( cipher, "fastsec" ) == 0 )
		cipher = "RC4-MD5";
	    else if ( strcasecmp( cipher, "highsec" ) == 0 )
		cipher = "DES-CBC3-SHA";
	    else if ( strcasecmp( cipher, "paranoid" ) == 0 )
		cipher = "AES256-SHA";
	    }
#endif /* USE_SSL */
	else if ( strncmp( argv[argn], "-proxy", strlen( argv[argn] ) ) == 0 && argn + 1 < argc )
	    {
	    char* colon;
	    do_proxy = 1;
	    proxy_hostname = argv[++argn];
	    colon = strchr( proxy_hostname, ':' );
	    if ( colon == (char*) 0 )
		proxy_port = 80;
	    else
		{
		proxy_port = (unsigned short) atoi( colon + 1 );
		*colon = '\0';
		}
	    }
	else
	    usage();
	++argn;
	}
    if ( argn + 1 != argc )
	usage();
    if ( start == START_NONE || end == END_NONE )
	usage();
    if ( do_jitter && start != START_RATE )
	usage();
    url_file = argv[argn];

    /* Read in and parse the URLs. */
    read_url_file( url_file );

    /* Read in the source IP file, if specified. */
    if ( sip_file != (char*) 0 )
	read_sip_file( sip_file );

    /* Initialize the connections table. */
    if ( start == START_PARALLEL )
	max_connections = start_parallel;
    connections = (connection*) malloc_check(
	max_connections * sizeof(connection) );
    for ( cnum = 0; cnum < max_connections; ++cnum )
	connections[cnum].conn_state = CNST_FREE;
    num_connections = max_parallel = 0;

    /* Initialize the HTTP status-code histogram. */
    for ( i = 0; i < 1000; ++i )
	http_status_counts[i] = 0;

    /* Initialize the statistics. */
    fetches_started = 0;
    connects_completed = 0;
    responses_completed = 0;
    fetches_completed = 0;
    total_bytes = 0;
    total_connect_usecs = 0;
    max_connect_usecs = 0;
    min_connect_usecs = 1000000000L;
    total_response_usecs = 0;
    max_response_usecs = 0;
    min_response_usecs = 1000000000L;
    total_timeouts = 0;
    total_badbytes = 0;
    total_badchecksums = 0;

    /* Initialize the random number generator. */
#ifdef HAVE_SRANDOMDEV
    srandomdev();
#else
    srandom( (int) time( (time_t*) 0 ) ^ getpid() );
#endif

    /* Initialize the rest. */
    tmr_init();
    (void) gettimeofday( &now, (struct timezone*) 0 );
    start_at = now;
    if ( do_verbose )
	(void) tmr_create(
	    &now, progress_report, JunkClientData, PROGRESS_SECS * 1000L, 1 );
    if ( start == START_RATE )
	{
	start_interval = 1000L / start_rate;
	if ( do_jitter )
	    {
	    low_interval = start_interval * 9 / 10;
	    high_interval = start_interval * 11 / 10;
	    range_interval = high_interval - low_interval + 1;
	    }
	(void) tmr_create(
	    &now, start_timer, JunkClientData, start_interval, ! do_jitter );
	}
    if ( end == END_SECONDS )
	(void) tmr_create(
	    &now, end_timer, JunkClientData, end_seconds * 1000L, 0 );
    (void) signal( SIGPIPE, SIG_IGN );

    /* Main loop. */
    for (;;)
	{
	if ( end == END_FETCHES && fetches_completed >= end_fetches )
	    finish( &now );

	if ( start == START_PARALLEL )
	    {
	    /* See if we need to start any new connections; but at most 10. */
	    for ( i = 0;
		  i < 10 &&
		    num_connections < start_parallel &&
		    ( end != END_FETCHES || fetches_started < end_fetches );
		  ++i )
		{
		start_connection( &now );
		(void) gettimeofday( &now, (struct timezone*) 0 );
		tmr_run( &now );
		}
	    }

	/* Build the fdsets. */
	FD_ZERO( &rfdset );
	FD_ZERO( &wfdset );
	for ( cnum = 0; cnum < max_connections; ++cnum )
	    switch ( connections[cnum].conn_state )
		{
		case CNST_CONNECTING:
		FD_SET( connections[cnum].conn_fd, &wfdset );
		break;
		case CNST_HEADERS:
		case CNST_READING:
		FD_SET( connections[cnum].conn_fd, &rfdset );
		break;
		}
	r = select(
	    FD_SETSIZE, &rfdset, &wfdset, (fd_set*) 0, tmr_timeout( &now ) );
	if ( r < 0 )
	    {
	    perror( "select" );
	    exit( 1 );
	    }
	(void) gettimeofday( &now, (struct timezone*) 0 );

	/* Service them. */
	for ( cnum = 0; cnum < max_connections; ++cnum )
	    switch ( connections[cnum].conn_state )
		{
		case CNST_CONNECTING:
		if ( FD_ISSET( connections[cnum].conn_fd, &wfdset ) )
		    handle_connect( cnum, &now, 1 );
		break;
		case CNST_HEADERS:
		case CNST_READING:
		if ( FD_ISSET( connections[cnum].conn_fd, &rfdset ) )
		    handle_read( cnum, &now );
		break;
		}
	/* And run the timers. */
	tmr_run( &now );
	}

    /* NOT_REACHED */
    }


static void
usage( void )
    {
    (void) fprintf( stderr,
	"usage:  %s [-checksum] [-throttle] [-proxy host:port] [-verbose] [-timeout secs] [-sip sip_file]\n", argv0 );
#ifdef USE_SSL
    (void) fprintf( stderr,
	"            [-cipher str]\n" );
#endif /* USE_SSL */
    (void) fprintf( stderr,
	"            -parallel N | -rate N [-jitter]\n" );
    (void) fprintf( stderr,
	"            -fetches N | -seconds N\n" );
    (void) fprintf( stderr,
	"            url_file\n" );
    (void) fprintf( stderr,
	"One start specifier, either -parallel or -rate, is required.\n" );
    (void) fprintf( stderr,
	"One end specifier, either -fetches or -seconds, is required.\n" );
    exit( 1 );
    }


static void
read_url_file( char* url_file )
    {
    FILE* fp;
    char line[5000], hostname[5000];
    char* http = "http://";
    int http_len = strlen( http );
#ifdef USE_SSL
    char* https = "https://";
    int https_len = strlen( https );
#endif
    int proto_len, host_len;
    char* cp;

    fp = fopen( url_file, "r" );
    if ( fp == (FILE*) 0 )
	{
	perror( url_file );
	exit( 1 );
	}

    max_urls = 100;
    urls = (url*) malloc_check( max_urls * sizeof(url) );
    num_urls = 0;
    while ( fgets( line, sizeof(line), fp ) != (char*) 0 )
	{
	/* Nuke trailing newline. */
	if ( line[strlen( line ) - 1] == '\n' )
	    line[strlen( line ) - 1] = '\0';

	/* Check for room in urls. */
	if ( num_urls >= max_urls )
	    {
	    max_urls *= 2;
	    urls = (url*) realloc_check( (void*) urls, max_urls * sizeof(url) );
	    }

	/* Add to table. */
	urls[num_urls].url_str = strdup_check( line );

	/* Parse it. */
	if ( strncmp( http, line, http_len ) == 0 )
	    {
	    proto_len = http_len;
	    urls[num_urls].protocol = PROTO_HTTP;
	    }
#ifdef USE_SSL
	else if ( strncmp( https, line, https_len ) == 0 )
	    {
	    proto_len = https_len;
	    urls[num_urls].protocol = PROTO_HTTPS;
	    }
#endif
	else
	    {
	    (void) fprintf( stderr, "%s: unknown protocol - %s\n", argv0, line );
	    exit( 1 );
	    }
	for ( cp = line + proto_len;
	     *cp != '\0' && *cp != ':' && *cp != '/'; ++cp )
	    ;
	host_len = cp - line;
	host_len -= proto_len;
	strncpy( hostname, line + proto_len, host_len );
	hostname[host_len] = '\0';
	urls[num_urls].hostname = strdup_check( hostname );
	if ( *cp == ':' )
	    {
	    urls[num_urls].port = (unsigned short) atoi( ++cp );
	    while ( *cp != '\0' && *cp != '/' )
		++cp;
	    }
	else
#ifdef USE_SSL
	    if ( urls[num_urls].protocol == PROTO_HTTPS )
		urls[num_urls].port = 443;
	    else
		urls[num_urls].port = 80;
#else
	    urls[num_urls].port = 80;
#endif
	if ( *cp == '\0' ) 
	    urls[num_urls].filename = strdup_check( "/" );
	else
	    urls[num_urls].filename = strdup_check( cp );

	lookup_address( num_urls );

	urls[num_urls].got_bytes = 0;
	urls[num_urls].got_checksum = 0;
	++num_urls;
	}
    }


static void
lookup_address( int url_num )
    {
    char* hostname;
    unsigned short port;
#ifdef USE_IPV6
    struct addrinfo hints;
    char portstr[10];
    int gaierr;
    struct addrinfo* ai;
    struct addrinfo* ai2;
    struct addrinfo* aiv4;
    struct addrinfo* aiv6;
#else /* USE_IPV6 */
    struct hostent *he;
#endif /* USE_IPV6 */

    urls[url_num].sa_len = sizeof(urls[url_num].sa);
    (void) memset( (void*) &urls[url_num].sa, 0, urls[url_num].sa_len );

    if ( do_proxy )
	{
	hostname = proxy_hostname;
	port = proxy_port;
	}
    else
	{
	hostname = urls[url_num].hostname;
	port = urls[url_num].port;
	}

#ifdef USE_IPV6

    (void) memset( &hints, 0, sizeof(hints) );
    hints.ai_family = PF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    (void) snprintf( portstr, sizeof(portstr), "%d", (int) port );
    if ( (gaierr = getaddrinfo( hostname, portstr, &hints, &ai )) != 0 )
	{
	(void) fprintf(
	    stderr, "%s: getaddrinfo %s - %s\n", argv0, hostname,
	    gai_strerror( gaierr ) );
	exit( 1 );
	}

    /* Find the first IPv4 and IPv6 entries. */
    aiv4 = (struct addrinfo*) 0;
    aiv6 = (struct addrinfo*) 0;
    for ( ai2 = ai; ai2 != (struct addrinfo*) 0; ai2 = ai2->ai_next )
	{
	switch ( ai2->ai_family )
	    {
	    case AF_INET: 
	    if ( aiv4 == (struct addrinfo*) 0 )
		aiv4 = ai2;
	    break;
	    case AF_INET6:
	    if ( aiv6 == (struct addrinfo*) 0 )
		aiv6 = ai2;
	    break;
	    }
	}

    /* If there's an IPv4 address, use that, otherwise try IPv6. */
    if ( aiv4 != (struct addrinfo*) 0 )
	{
	if ( sizeof(urls[url_num].sa) < aiv4->ai_addrlen )
	    {
	    (void) fprintf(
		stderr, "%s - sockaddr too small (%lu < %lu)\n", hostname,
		(unsigned long) sizeof(urls[url_num].sa),
		(unsigned long) aiv4->ai_addrlen );
	    exit( 1 );
	    }
	urls[url_num].sock_family = aiv4->ai_family;
	urls[url_num].sock_type = aiv4->ai_socktype;
	urls[url_num].sock_protocol = aiv4->ai_protocol;
	urls[url_num].sa_len = aiv4->ai_addrlen;
	(void) memmove( &urls[url_num].sa, aiv4->ai_addr, aiv4->ai_addrlen );
	freeaddrinfo( ai );
	return;
	}
    if ( aiv6 != (struct addrinfo*) 0 )
	{
	if ( sizeof(urls[url_num].sa) < aiv6->ai_addrlen )
	    {
	    (void) fprintf(
		stderr, "%s - sockaddr too small (%lu < %lu)\n", hostname,
		(unsigned long) sizeof(urls[url_num].sa),
		(unsigned long) aiv6->ai_addrlen );
	    exit( 1 );
	    }
	urls[url_num].sock_family = aiv6->ai_family;
	urls[url_num].sock_type = aiv6->ai_socktype;
	urls[url_num].sock_protocol = aiv6->ai_protocol;
	urls[url_num].sa_len = aiv6->ai_addrlen;
	(void) memmove( &urls[url_num].sa, aiv6->ai_addr, aiv6->ai_addrlen );
	freeaddrinfo( ai );
	return;
	}

    (void) fprintf(
	stderr, "%s: no valid address found for host %s\n", argv0, hostname );
    exit( 1 );

#else /* USE_IPV6 */

    he = gethostbyname( hostname );
    if ( he == (struct hostent*) 0 )
	{
	(void) fprintf( stderr, "%s: unknown host - %s\n", argv0, hostname );
	exit( 1 );
	}
    urls[url_num].sock_family = urls[url_num].sa.sin_family = he->h_addrtype;
    urls[url_num].sock_type = SOCK_STREAM;
    urls[url_num].sock_protocol = 0;
    urls[url_num].sa_len = sizeof(urls[url_num].sa);
    (void) memmove( &urls[url_num].sa.sin_addr, he->h_addr, he->h_length );
    urls[url_num].sa.sin_port = htons( port );

#endif /* USE_IPV6 */

    }


static void
read_sip_file( char* sip_file )
    {
    FILE* fp;
    char line[5000];

    fp = fopen( sip_file, "r" );
    if ( fp == (FILE*) 0 )
	{
	perror( sip_file );
	exit( 1 );
	}

    max_sips = 100;
    sips = (sip*) malloc_check( max_sips * sizeof(sip) );
    num_sips = 0;
    while ( fgets( line, sizeof(line), fp ) != (char*) 0 )
	{
	/* Nuke trailing newline. */
	if ( line[strlen( line ) - 1] == '\n' )
	    line[strlen( line ) - 1] = '\0';

	/* Check for room in sips. */
	if ( num_sips >= max_sips )
	    {
	    max_sips *= 2;
	    sips = (sip*) realloc_check( (void*) sips, max_sips * sizeof(sip) );
	    }

	/* Add to table. */
	sips[num_sips].str = strdup_check( line );
	(void) memset( (void*) &sips[num_sips].sa, 0, sizeof(sips[num_sips].sa) );
	if ( ! inet_aton( sips[num_sips].str, &sips[num_sips].sa.sin_addr ) )
	    {
	    (void) fprintf(
		stderr, "%s: cannot convert source IP address %s\n",
		argv0, sips[num_sips].str );
	    exit( 1 );
	    }
	++num_sips;
	}
    }


static void
start_connection( struct timeval* nowP )
    {
    int cnum, url_num;

    /* Find an empty connection slot. */
    for ( cnum = 0; cnum < max_connections; ++cnum )
	if ( connections[cnum].conn_state == CNST_FREE )
	    {
	    /* Choose a URL. */
	    url_num = ( (unsigned long) random() ) % ( (unsigned int) num_urls );
	    /* Start the socket. */
	    start_socket( url_num, cnum, nowP );
	    if ( connections[cnum].conn_state != CNST_FREE )
		{
		++num_connections;
		if ( num_connections > max_parallel )
		    max_parallel = num_connections;
		}
	    ++fetches_started;
	    return;
	    }
    /* No slots left. */
    (void) fprintf( stderr, "%s: ran out of connection slots\n", argv0 );
    finish( nowP );
    }


static void
start_socket( int url_num, int cnum, struct timeval* nowP )
    {
    ClientData client_data;
    int flags;
    int sip_num;

    /* Start filling in the connection slot. */
    connections[cnum].url_num = url_num;
    connections[cnum].started_at = *nowP;
    client_data.i = cnum;
    connections[cnum].did_connect = 0;
    connections[cnum].did_response = 0;
    connections[cnum].idle_timer = tmr_create(
	nowP, idle_connection, client_data, idle_secs * 1000L, 0 );
    connections[cnum].wakeup_timer = (Timer*) 0;
    connections[cnum].content_length = -1;
    connections[cnum].bytes = 0;
    connections[cnum].checksum = 0;
    connections[cnum].http_status = -1;

    /* Make a socket. */
    connections[cnum].conn_fd = socket(
	urls[url_num].sock_family, urls[url_num].sock_type,
	urls[url_num].sock_protocol );
    if ( connections[cnum].conn_fd < 0 )
	{
	perror( urls[url_num].url_str );
	return;
	}

    /* Set the file descriptor to no-delay mode. */
    flags = fcntl( connections[cnum].conn_fd, F_GETFL, 0 );
    if ( flags == -1 )
	{
	perror( urls[url_num].url_str );
	(void) close( connections[cnum].conn_fd );
	return;
	}
    if ( fcntl( connections[cnum].conn_fd, F_SETFL, flags | O_NDELAY ) < 0 ) 
	{
	perror( urls[url_num].url_str );
	(void) close( connections[cnum].conn_fd );
	return;
	}

    if ( num_sips > 0 )
	{
	/* Try a random source IP address. */
	sip_num = ( (unsigned long) random() ) % ( (unsigned int) num_sips );
	if ( bind(
		 connections[cnum].conn_fd,
		 (struct sockaddr*) &sips[sip_num].sa,
	         sizeof(sips[sip_num].sa) ) < 0 )
	    {
	    perror( "binding local address" );
	    (void) close( connections[cnum].conn_fd );
	    return;
	    }
	}

    /* Connect to the host. */
    connections[cnum].sa_len = urls[url_num].sa_len;
    (void) memmove(
	(void*) &connections[cnum].sa, (void*) &urls[url_num].sa,
	urls[url_num].sa_len );
    connections[cnum].connect_at = *nowP;
    if ( connect(
	     connections[cnum].conn_fd,
	     (struct sockaddr*) &connections[cnum].sa,
	     connections[cnum].sa_len ) < 0 )
	{
	if ( errno == EINPROGRESS )
	    {
	    connections[cnum].conn_state = CNST_CONNECTING;
	    return;
	    }
	else
	    {
	    perror( urls[url_num].url_str );
	    (void) close( connections[cnum].conn_fd );
	    return;
	    }
	}

    /* Connect succeeded instantly, so handle it now. */
    (void) gettimeofday( nowP, (struct timezone*) 0 );
    handle_connect( cnum, nowP, 0 );
    }


static void
handle_connect( int cnum, struct timeval* nowP, int double_check )
    {
    int url_num;
    char buf[600];
    int bytes, r;

    url_num = connections[cnum].url_num;
    if ( double_check )
	{
	/* Check to make sure the non-blocking connect succeeded. */
	int err, errlen;

	if ( connect(
		 connections[cnum].conn_fd,
		 (struct sockaddr*) &connections[cnum].sa,
		 connections[cnum].sa_len ) < 0 )
	    {
	    switch ( errno )
		{
		case EISCONN:
		/* Ok! */
		break;
		case EINVAL:
		errlen = sizeof(err);
		if ( getsockopt( connections[cnum].conn_fd, SOL_SOCKET, SO_ERROR, (void*) &err, &errlen ) < 0 )
		    (void) fprintf(
			stderr, "%s: unknown connect error\n",
			urls[url_num].url_str );
		else
		    (void) fprintf(
			stderr, "%s: %s\n", urls[url_num].url_str,
			strerror( err ) );
		close_connection( cnum );
		return;
		default:
		perror( urls[url_num].url_str );
		close_connection( cnum );
		return;
		}
	    }
	}
#ifdef USE_SSL
    if ( urls[url_num].protocol == PROTO_HTTPS )
	{
	int flags;

	/* Make SSL connection. */
	if ( ssl_ctx == (SSL_CTX*) 0 )
	    {
	    SSL_load_error_strings();
	    SSLeay_add_ssl_algorithms();
	    ssl_ctx = SSL_CTX_new( SSLv23_client_method() );
	    if ( cipher != (char*) 0 )
		{
		if ( ! SSL_CTX_set_cipher_list( ssl_ctx, cipher ) )
		    {
		    (void) fprintf(
			stderr, "%s: cannot set cipher list\n", argv0 );
		    ERR_print_errors_fp( stderr );
		    close_connection( cnum );
		    return;
		    }
		}
	    }
	if ( ! RAND_status() )
	    {
	    unsigned char bytes[1024];
	    int i;
	    for ( i = 0; i < sizeof(bytes); ++i )
		bytes[i] = random() % 0xff;
	    RAND_seed( bytes, sizeof(bytes) );
	    }
	flags = fcntl( connections[cnum].conn_fd, F_GETFL, 0 );
	if ( flags != -1 )
	    (void) fcntl(
		connections[cnum].conn_fd, F_SETFL, flags & ~ (int) O_NDELAY );
	connections[cnum].ssl = SSL_new( ssl_ctx );
	SSL_set_fd( connections[cnum].ssl, connections[cnum].conn_fd );
	r = SSL_connect( connections[cnum].ssl );
	if ( r <= 0 )
	    {
	    (void) fprintf(
		stderr, "%s: SSL connection failed - %d\n", argv0, r );
	    ERR_print_errors_fp( stderr );
	    close_connection( cnum );
	    return;
	    }
	}
#endif
    connections[cnum].did_connect = 1;

    /* Format the request. */
    if ( do_proxy )
	{
#ifdef USE_SSL
	bytes = snprintf(
	    buf, sizeof(buf), "GET %s://%.500s:%d%.500s HTTP/1.0\r\n",
	    urls[url_num].protocol == PROTO_HTTPS ? "https" : "http",
	    urls[url_num].hostname, (int) urls[url_num].port,
	    urls[url_num].filename );
#else
	bytes = snprintf(
	    buf, sizeof(buf), "GET http://%.500s:%d%.500s HTTP/1.0\r\n",
	    urls[url_num].hostname, (int) urls[url_num].port,
	    urls[url_num].filename );
#endif
	}
    else
	bytes = snprintf(
	    buf, sizeof(buf), "GET %.500s HTTP/1.1\r\n",
	    urls[url_num].filename );
    bytes += snprintf(
	&buf[bytes], sizeof(buf) - bytes, "Host: %s\r\n",
	urls[url_num].hostname );
    bytes += snprintf(
	&buf[bytes], sizeof(buf) - bytes, "User-Agent: %s\r\n", VERSION );
    bytes += snprintf(
        &buf[bytes], sizeof(buf) - bytes, "Accept-Encoding: gzip,deflate\r\n" );
    bytes += snprintf(
        &buf[bytes], sizeof(buf) - bytes, "Connection: close\r\n" );
    bytes += snprintf( &buf[bytes], sizeof(buf) - bytes, "\r\n" );

    /* Send the request. */
    connections[cnum].request_at = *nowP;
#ifdef USE_SSL
    if ( urls[url_num].protocol == PROTO_HTTPS )
	r = SSL_write( connections[cnum].ssl, buf, bytes );
    else
	r = write( connections[cnum].conn_fd, buf, bytes );
#else
    r = write( connections[cnum].conn_fd, buf, bytes );
#endif
    if ( r < 0 )
	{
	perror( urls[url_num].url_str );
	close_connection( cnum );
	return;
	}
    connections[cnum].conn_state = CNST_HEADERS;
    connections[cnum].header_state = HDST_LINE1_PROTOCOL;
    }


static void
handle_read( int cnum, struct timeval* nowP )
    {
    char buf[30000];	/* must be larger than throttle / 2 */
    int bytes_to_read, bytes_read, bytes_handled;
    float elapsed;
    ClientData client_data;
    register long checksum;

    tmr_reset( nowP, connections[cnum].idle_timer );

    if ( do_throttle )
	bytes_to_read = throttle / 2.0;
    else
	bytes_to_read = sizeof(buf);
    if ( ! connections[cnum].did_response )
	{
	connections[cnum].did_response = 1;
	connections[cnum].response_at = *nowP;
	}
#ifdef USE_SSL
    if ( urls[connections[cnum].url_num].protocol == PROTO_HTTPS )
	bytes_read = SSL_read( connections[cnum].ssl, buf, bytes_to_read );
    else
	bytes_read = read( connections[cnum].conn_fd, buf, bytes_to_read );
#else
    bytes_read = read( connections[cnum].conn_fd, buf, bytes_to_read );
#endif
    if ( bytes_read <= 0 )
	{
	close_connection( cnum );
	return;
	}

    for ( bytes_handled = 0; bytes_handled < bytes_read; )
	{
	switch ( connections[cnum].conn_state )
	    {
	    case CNST_HEADERS:
	    /* State machine to read until we reach the file part.  Looks for
	    ** Content-Length header too.
	    */
	    for ( ; bytes_handled < bytes_read && connections[cnum].conn_state == CNST_HEADERS; ++bytes_handled )
		{
		switch ( connections[cnum].header_state )
		    {

		    case HDST_LINE1_PROTOCOL:
		    switch ( buf[bytes_handled] )
			{
			case ' ': case '\t':
			connections[cnum].header_state = HDST_LINE1_WHITESPACE;
			break;
			case '\n':
			connections[cnum].header_state = HDST_LF;
			break;
			case '\r':
			connections[cnum].header_state = HDST_CR;
			break;
			}
		    break;

		    case HDST_LINE1_WHITESPACE:
		    switch ( buf[bytes_handled] )
			{
			case ' ': case '\t':
			break;
			case '0': case '1': case '2': case '3': case '4':
			case '5': case '6': case '7': case '8': case '9':
			connections[cnum].http_status =
			    buf[bytes_handled] - '0';
			connections[cnum].header_state = HDST_LINE1_STATUS;
			break;
			case '\n':
			connections[cnum].header_state = HDST_LF;
			break;
			case '\r':
			connections[cnum].header_state = HDST_CR;
			break;
			default:
			connections[cnum].header_state = HDST_TEXT;
			break;
			}
		    break;

		    case HDST_LINE1_STATUS:
		    switch ( buf[bytes_handled] )
			{
			case '0': case '1': case '2': case '3': case '4':
			case '5': case '6': case '7': case '8': case '9':
			connections[cnum].http_status =
			    connections[cnum].http_status * 10 +
			    buf[bytes_handled] - '0';
			break;
			case '\n':
			connections[cnum].header_state = HDST_LF;
			break;
			case '\r':
			connections[cnum].header_state = HDST_CR;
			break;
			default:
			connections[cnum].header_state = HDST_TEXT;
			break;
			}
		    break;

		    case HDST_BOL:
		    switch ( buf[bytes_handled] )
			{
			case '\n':
			connections[cnum].header_state = HDST_LF;
			break;
			case '\r':
			connections[cnum].header_state = HDST_CR;
			break;
			case 'C': case 'c':
			connections[cnum].header_state = HDST_C;
			break;
			default:
			connections[cnum].header_state = HDST_TEXT;
			break;
			}
		    break;

		    case HDST_TEXT:
		    switch ( buf[bytes_handled] )
			{
			case '\n':
			connections[cnum].header_state = HDST_LF;
			break;
			case '\r':
			connections[cnum].header_state = HDST_CR;
			break;
			default:
			break;
			}
		    break;

		    case HDST_LF:
		    switch ( buf[bytes_handled] )
			{
			case '\n':
			connections[cnum].conn_state = CNST_READING;
			break;
			case '\r':
			connections[cnum].header_state = HDST_CR;
			break;
			case 'C': case 'c':
			connections[cnum].header_state = HDST_C;
			break;
			default:
			connections[cnum].header_state = HDST_TEXT;
			break;
			}
		    break;

		    case HDST_CR:
		    switch ( buf[bytes_handled] )
			{
			case '\n':
			connections[cnum].header_state = HDST_CRLF;
			break;
			case '\r':
			connections[cnum].conn_state = CNST_READING;
			break;
			case 'C': case 'c':
			connections[cnum].header_state = HDST_C;
			break;
			default:
			connections[cnum].header_state = HDST_TEXT;
			break;
			}
		    break;

		    case HDST_CRLF:
		    switch ( buf[bytes_handled] )
			{
			case '\n':
			connections[cnum].conn_state = CNST_READING;
			break;
			case '\r':
			connections[cnum].header_state = HDST_CRLFCR;
			break;
			case 'C': case 'c':
			connections[cnum].header_state = HDST_C;
			break;
			default:
			connections[cnum].header_state = HDST_TEXT;
			break;
			}
		    break;

		    case HDST_CRLFCR:
		    switch ( buf[bytes_handled] )
			{
			case '\n': case '\r':
			connections[cnum].conn_state = CNST_READING;
			break;
			case 'C': case 'c':
			connections[cnum].header_state = HDST_C;
			break;
			default:
			connections[cnum].header_state = HDST_TEXT;
			break;
			}
		    break;

		    case HDST_C:
		    switch ( buf[bytes_handled] )
			{
			case 'O': case 'o':
			connections[cnum].header_state = HDST_CO;
			break;
			case '\n':
			connections[cnum].header_state = HDST_LF;
			break;
			case '\r':
			connections[cnum].header_state = HDST_CR;
			break;
			default:
			connections[cnum].header_state = HDST_TEXT;
			break;
			}
		    break;

		    case HDST_CO:
		    switch ( buf[bytes_handled] )
			{
			case 'N': case 'n':
			connections[cnum].header_state = HDST_CON;
			break;
			case '\n':
			connections[cnum].header_state = HDST_LF;
			break;
			case '\r':
			connections[cnum].header_state = HDST_CR;
			break;
			default:
			connections[cnum].header_state = HDST_TEXT;
			break;
			}
		    break;

		    case HDST_CON:
		    switch ( buf[bytes_handled] )
			{
			case 'T': case 't':
			connections[cnum].header_state = HDST_CONT;
			break;
			case '\n':
			connections[cnum].header_state = HDST_LF;
			break;
			case '\r':
			connections[cnum].header_state = HDST_CR;
			break;
			default:
			connections[cnum].header_state = HDST_TEXT;
			break;
			}
		    break;

		    case HDST_CONT:
		    switch ( buf[bytes_handled] )
			{
			case 'E': case 'e':
			connections[cnum].header_state = HDST_CONTE;
			break;
			case '\n':
			connections[cnum].header_state = HDST_LF;
			break;
			case '\r':
			connections[cnum].header_state = HDST_CR;
			break;
			default:
			connections[cnum].header_state = HDST_TEXT;
			break;
			}
		    break;

		    case HDST_CONTE:
		    switch ( buf[bytes_handled] )
			{
			case 'N': case 'n':
			connections[cnum].header_state = HDST_CONTEN;
			break;
			case '\n':
			connections[cnum].header_state = HDST_LF;
			break;
			case '\r':
			connections[cnum].header_state = HDST_CR;
			break;
			default:
			connections[cnum].header_state = HDST_TEXT;
			break;
			}
		    break;

		    case HDST_CONTEN:
		    switch ( buf[bytes_handled] )
			{
			case 'T': case 't':
			connections[cnum].header_state = HDST_CONTENT;
			break;
			case '\n':
			connections[cnum].header_state = HDST_LF;
			break;
			case '\r':
			connections[cnum].header_state = HDST_CR;
			break;
			default:
			connections[cnum].header_state = HDST_TEXT;
			break;
			}
		    break;

		    case HDST_CONTENT:
		    switch ( buf[bytes_handled] )
			{
			case '-':
			connections[cnum].header_state = HDST_CONTENT_;
			break;
			case '\n':
			connections[cnum].header_state = HDST_LF;
			break;
			case '\r':
			connections[cnum].header_state = HDST_CR;
			break;
			default:
			connections[cnum].header_state = HDST_TEXT;
			break;
			}
		    break;

		    case HDST_CONTENT_:
		    switch ( buf[bytes_handled] )
			{
			case 'L': case 'l':
			connections[cnum].header_state = HDST_CONTENT_L;
			break;
			case '\n':
			connections[cnum].header_state = HDST_LF;
			break;
			case '\r':
			connections[cnum].header_state = HDST_CR;
			break;
			default:
			connections[cnum].header_state = HDST_TEXT;
			break;
			}
		    break;

		    case HDST_CONTENT_L:
		    switch ( buf[bytes_handled] )
			{
			case 'E': case 'e':
			connections[cnum].header_state = HDST_CONTENT_LE;
			break;
			case '\n':
			connections[cnum].header_state = HDST_LF;
			break;
			case '\r':
			connections[cnum].header_state = HDST_CR;
			break;
			default:
			connections[cnum].header_state = HDST_TEXT;
			break;
			}
		    break;

		    case HDST_CONTENT_LE:
		    switch ( buf[bytes_handled] )
			{
			case 'N': case 'n':
			connections[cnum].header_state = HDST_CONTENT_LEN;
			break;
			case '\n':
			connections[cnum].header_state = HDST_LF;
			break;
			case '\r':
			connections[cnum].header_state = HDST_CR;
			break;
			default:
			connections[cnum].header_state = HDST_TEXT;
			break;
			}
		    break;

		    case HDST_CONTENT_LEN:
		    switch ( buf[bytes_handled] )
			{
			case 'G': case 'g':
			connections[cnum].header_state = HDST_CONTENT_LENG;
			break;
			case '\n':
			connections[cnum].header_state = HDST_LF;
			break;
			case '\r':
			connections[cnum].header_state = HDST_CR;
			break;
			default:
			connections[cnum].header_state = HDST_TEXT;
			break;
			}
		    break;

		    case HDST_CONTENT_LENG:
		    switch ( buf[bytes_handled] )
			{
			case 'T': case 't':
			connections[cnum].header_state = HDST_CONTENT_LENGT;
			break;
			case '\n':
			connections[cnum].header_state = HDST_LF;
			break;
			case '\r':
			connections[cnum].header_state = HDST_CR;
			break;
			default:
			connections[cnum].header_state = HDST_TEXT;
			break;
			}
		    break;

		    case HDST_CONTENT_LENGT:
		    switch ( buf[bytes_handled] )
			{
			case 'H': case 'h':
			connections[cnum].header_state = HDST_CONTENT_LENGTH;
			break;
			case '\n':
			connections[cnum].header_state = HDST_LF;
			break;
			case '\r':
			connections[cnum].header_state = HDST_CR;
			break;
			default:
			connections[cnum].header_state = HDST_TEXT;
			break;
			}
		    break;

		    case HDST_CONTENT_LENGTH:
		    switch ( buf[bytes_handled] )
			{
			case ':':
			connections[cnum].header_state = HDST_CONTENT_LENGTH_COLON;
			break;
			case '\n':
			connections[cnum].header_state = HDST_LF;
			break;
			case '\r':
			connections[cnum].header_state = HDST_CR;
			break;
			default:
			connections[cnum].header_state = HDST_TEXT;
			break;
			}
		    break;

		    case HDST_CONTENT_LENGTH_COLON:
		    switch ( buf[bytes_handled] )
			{
			case ' ': case '\t':
			connections[cnum].header_state = HDST_CONTENT_LENGTH_COLON_WHITESPACE;
			break;
			case '\n':
			connections[cnum].header_state = HDST_LF;
			break;
			case '\r':
			connections[cnum].header_state = HDST_CR;
			break;
			default:
			connections[cnum].header_state = HDST_TEXT;
			break;
			}
		    break;

		    case HDST_CONTENT_LENGTH_COLON_WHITESPACE:
		    switch ( buf[bytes_handled] )
			{
			case ' ': case '\t':
			break;
			case '0': case '1': case '2': case '3': case '4':
			case '5': case '6': case '7': case '8': case '9':
			connections[cnum].content_length = buf[bytes_handled] - '0';
			connections[cnum].header_state = HDST_CONTENT_LENGTH_COLON_WHITESPACE_NUM;
			break;
			case '\n':
			connections[cnum].header_state = HDST_LF;
			break;
			case '\r':
			connections[cnum].header_state = HDST_CR;
			break;
			default:
			connections[cnum].header_state = HDST_TEXT;
			break;
			}
		    break;

		    case HDST_CONTENT_LENGTH_COLON_WHITESPACE_NUM:
		    switch ( buf[bytes_handled] )
			{
			case '0': case '1': case '2': case '3': case '4':
			case '5': case '6': case '7': case '8': case '9':
			connections[cnum].content_length =
			    connections[cnum].content_length * 10 +
			    buf[bytes_handled] - '0';
			break;
			case '\n':
			connections[cnum].header_state = HDST_LF;
			break;
			case '\r':
			connections[cnum].header_state = HDST_CR;
			break;
			default:
			connections[cnum].header_state = HDST_TEXT;
			break;
			}
		    break;

		    }
		}
	    break;

	    case CNST_READING:
	    connections[cnum].bytes += bytes_read - bytes_handled;
	    if ( do_throttle )
		{
		/* Check if we're reading too fast. */
		elapsed = delta_timeval( &connections[cnum].started_at, nowP ) / 1000000.0;
		if ( elapsed > 0.01 && connections[cnum].bytes / elapsed > throttle )
		    {
		    connections[cnum].conn_state  = CNST_PAUSING;
		    client_data.i = cnum;
		    connections[cnum].wakeup_timer = tmr_create(
			nowP, wakeup_connection, client_data, 1000L, 0 );
		    }
		}
	    if ( do_checksum )
		{
		checksum = connections[cnum].checksum;
		for ( ; bytes_handled < bytes_read; ++bytes_handled )
		    {
		    if ( checksum & 1 )
			checksum = ( checksum >> 1 ) + 0x8000;
		    else
			checksum >>= 1;
		    checksum += buf[bytes_handled];
		    checksum &= 0xffff;
		    }
		connections[cnum].checksum = checksum;
		}
	    else
		bytes_handled = bytes_read;

	    if ( connections[cnum].content_length != -1 &&
		 connections[cnum].bytes >= connections[cnum].content_length )
		{
		close_connection( cnum );
		return;
		}

	    break;
	    }
	}
    }


static void
idle_connection( ClientData client_data, struct timeval* nowP )
    {
    int cnum;

    cnum = client_data.i;
    connections[cnum].idle_timer = (Timer*) 0;
    (void) fprintf(
	stderr, "%s: timed out\n", urls[connections[cnum].url_num].url_str );
    close_connection( cnum );
    ++total_timeouts;
    }


static void
wakeup_connection( ClientData client_data, struct timeval* nowP )
    {
    int cnum;

    cnum = client_data.i;
    connections[cnum].wakeup_timer = (Timer*) 0;
    connections[cnum].conn_state = CNST_READING;
    }


static void
close_connection( int cnum )
    {
    int url_num;

#ifdef USE_SSL
    if ( urls[connections[cnum].url_num].protocol == PROTO_HTTPS )
	SSL_free( connections[cnum].ssl );
#endif
    (void) close( connections[cnum].conn_fd );
    connections[cnum].conn_state = CNST_FREE;
    if ( connections[cnum].idle_timer != (Timer*) 0 )
	tmr_cancel( connections[cnum].idle_timer );
    if ( connections[cnum].wakeup_timer != (Timer*) 0 )
	tmr_cancel( connections[cnum].wakeup_timer );
    --num_connections;
    ++fetches_completed;
    total_bytes += connections[cnum].bytes;
    if ( connections[cnum].did_connect )
	{
	long long connect_usecs = delta_timeval(
	    &connections[cnum].connect_at, &connections[cnum].request_at );
	total_connect_usecs += connect_usecs;
	max_connect_usecs = max( max_connect_usecs, connect_usecs );
	min_connect_usecs = min( min_connect_usecs, connect_usecs );
	++connects_completed;
	}
    if ( connections[cnum].did_response )
	{
	long long response_usecs = delta_timeval(
	    &connections[cnum].request_at, &connections[cnum].response_at );
	total_response_usecs += response_usecs;
	max_response_usecs = max( max_response_usecs, response_usecs );
	min_response_usecs = min( min_response_usecs, response_usecs );
	++responses_completed;
	}
    if ( connections[cnum].http_status >= 0 && connections[cnum].http_status <= 999 )
	++http_status_counts[connections[cnum].http_status];

    url_num = connections[cnum].url_num;
    if ( do_checksum )
	{
	if ( ! urls[url_num].got_checksum )
	    {
	    urls[url_num].checksum = connections[cnum].checksum;
	    urls[url_num].got_checksum = 1;
	    }
	else
	    {
	    if ( connections[cnum].checksum != urls[url_num].checksum )
		{
		(void) fprintf(
		    stderr, "%s: checksum wrong\n", urls[url_num].url_str );
		++total_badchecksums;
		}
	    }
	}
    else
	{
	if ( ! urls[url_num].got_bytes )
	    {
	    urls[url_num].bytes = connections[cnum].bytes;
	    urls[url_num].got_bytes = 1;
	    }
	else
	    {
	    if ( connections[cnum].bytes != urls[url_num].bytes )
		{
		(void) fprintf(
		    stderr, "%s: byte count wrong\n", urls[url_num].url_str );
		++total_badbytes;
		}
	    }
	}
    }


static void
progress_report( ClientData client_data, struct timeval* nowP )
    {
    float elapsed;

    elapsed = delta_timeval( &start_at, nowP ) / 1000000.0;
    (void) fprintf( stderr,
        "--- %g secs, %d fetches started, %d completed, %d current\n",
	elapsed, fetches_started, fetches_completed, num_connections );
    }


static void
start_timer( ClientData client_data, struct timeval* nowP )
    {
    start_connection( nowP );
    if ( do_jitter )
	(void) tmr_create(
	    nowP, start_timer, JunkClientData,
	    (long) ( random() % range_interval ) + low_interval, 0 );
    }


static void
end_timer( ClientData client_data, struct timeval* nowP )
    {
    finish( nowP );
    }


static void
finish( struct timeval* nowP )
    {
    float elapsed;
    int i;

    /* Report statistics. */
    elapsed = delta_timeval( &start_at, nowP ) / 1000000.0;
    (void) printf(
	"%d fetches, %d max parallel, %g bytes, in %g seconds\n",
	fetches_completed, max_parallel, (float) total_bytes, elapsed );
    if ( fetches_completed > 0 )
	(void) printf(
	    "%g mean bytes/connection\n",
	    (float) total_bytes / (float) fetches_completed );
    if ( elapsed > 0.01 )
	{
	(void) printf(
	    "%g fetches/sec, %g bytes/sec\n",
	    (float) fetches_completed / elapsed,
	    (float) total_bytes / elapsed );
	}
    if ( connects_completed > 0 )
	(void) printf(
	    "msecs/connect: %g mean, %g max, %g min\n",
	    (float) total_connect_usecs / (float) connects_completed / 1000.0,
	    (float) max_connect_usecs / 1000.0,
	    (float) min_connect_usecs / 1000.0 );
    if ( responses_completed > 0 )
	(void) printf(
	    "msecs/first-response: %g mean, %g max, %g min\n",
	    (float) total_response_usecs / (float) responses_completed / 1000.0,
	    (float) max_response_usecs / 1000.0,
	    (float) min_response_usecs / 1000.0 );
    if ( total_timeouts != 0 )
	(void) printf( "%d timeouts\n", total_timeouts );
    if ( do_checksum )
	{
	if ( total_badchecksums != 0 )
	    (void) printf( "%d bad checksums\n", total_badchecksums );
	}
    else
	{
	if ( total_badbytes != 0 )
	    (void) printf( "%d bad byte counts\n", total_badbytes );
	}

    (void) printf( "HTTP response codes:\n" );
    for ( i = 0; i < 1000; ++i )
	if ( http_status_counts[i] > 0 )
	    (void) printf( "  code %03d -- %d\n", i, http_status_counts[i] );

    tmr_destroy();
#ifdef USE_SSL
    if ( ssl_ctx != (SSL_CTX*) 0 )
	SSL_CTX_free( ssl_ctx );
#endif
    exit( 0 );
    }


static long long
delta_timeval( struct timeval* start, struct timeval* finish )
    {
    long long delta_secs = finish->tv_sec - start->tv_sec;
    long long delta_usecs = finish->tv_usec - start->tv_usec;
    return delta_secs * (long long) 1000000L + delta_usecs;
    }


static void*
malloc_check( size_t size )
    {
    void* ptr = malloc( size );
    check( ptr );
    return ptr;
    }


static void*
realloc_check( void* ptr, size_t size )
    {
    ptr = realloc( ptr, size );
    check( ptr );
    return ptr;
    }


static char*
strdup_check( char* str )
    {
    str = strdup( str );
    check( (void*) str );
    return str;
    }


static void
check( void* ptr )
    {
    if ( ptr == (void*) 0 )
	{
	(void) fprintf( stderr, "%s: out of memory\n", argv0 );
	exit( 1 );
	}
    }
