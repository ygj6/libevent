/*
  This example program provides a trivial server program that listens for TCP
  connections on port 9995.  When they arrive, it writes a short message to
  each client connection, and closes each connection once it is flushed.

  Where possible, it exits cleanly in response to a SIGINT (ctrl-c).
*/


#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <signal.h>
#ifndef _WIN32
#include <netinet/in.h>
# ifdef _XOPEN_SOURCE_EXTENDED
#  include <arpa/inet.h>
# endif
#include <sys/socket.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/un.h>
#endif

#include <event2/event-config.h>
#include <event2/bufferevent.h>
#include <event2/buffer.h>
#include <event2/listener.h>
#include <event2/util.h>
#include <event2/event.h>
#ifdef _WIN32
#include <getopt.h>
#include <event2/thread.h>
#endif
#ifdef EVENT__HAVE_AFUNIX_H
#include <afunix.h>
#include "../util-internal.h"
#endif

#ifdef _WIN32
static int use_iocp = 0;
#endif
static int use_afunix = 0;
static const char *server_path = NULL;
static const char MESSAGE[] = "Hello, World!\n";

static const int PORT = 9995;

static void listener_cb(struct evconnlistener *, evutil_socket_t,
    struct sockaddr *, int socklen, void *);
static void conn_writecb(struct bufferevent *, void *);
static void conn_eventcb(struct bufferevent *, short, void *);
static void signal_cb(evutil_socket_t, short, void *);
static void print_usage(FILE *out, const char *name);
static void parse_opts(int argc, char **argv);

int
main(int argc, char **argv)
{
	struct event_base *base;
	struct evconnlistener *listener;
	struct event *signal_event;
	struct event_config *cfg;
	struct sockaddr *addr;
	struct sockaddr_in sin;
	struct sockaddr_un sun;
	int socklen;

#ifdef _WIN32
	WSADATA wsa_data;
	WSAStartup(0x0201, &wsa_data);
#endif

	parse_opts(argc, argv);

	cfg = event_config_new();
	if (!cfg) {
		fprintf(stderr, "Could not new event_config!\n");
		return 1;
	}

#ifdef _WIN32
	if (use_iocp) {
		evthread_use_windows_threads();

		event_config_set_flag(cfg, EVENT_BASE_FLAG_STARTUP_IOCP);
		SYSTEM_INFO si;
		GetSystemInfo(&si);
		event_config_set_num_cpus_hint(cfg, si.dwNumberOfProcessors);
	}
#endif

	base = event_base_new_with_config(cfg);
	event_config_free(cfg);
	if (!base) {
		fprintf(stderr, "Could not initialize libevent!\n");
		return 1;
	}

	if (use_afunix) {
#ifdef _WIN32
		DeleteFileA(server_path);
#else
		unlink(server_path);
#endif
		socklen = sizeof(sun);
		memset(&sun, 0, socklen);
		sun.sun_family = AF_UNIX;
#ifdef _WIN32
		strcpy_s(sun.sun_path, sizeof(sun.sun_path), server_path);
#else
		strlcpy(sun.sun_path, server_path, sizeof(sun.sun_path));
#endif
		addr = (struct sockaddr *)&sun;
	} else {
		socklen = sizeof(sin);
		memset(&sin, 0, socklen);
		sin.sin_family = AF_INET;
		sin.sin_port = htons(PORT);
		addr = (struct sockaddr *)&sin;
	}

	listener = evconnlistener_new_bind(base, listener_cb, (void *)base,
	    LEV_OPT_REUSEABLE|LEV_OPT_CLOSE_ON_FREE, -1,
	    addr, socklen);

	if (!listener) {
		fprintf(stderr, "Could not create a listener!\n");
		return 1;
	}

	signal_event = evsignal_new(base, SIGINT, signal_cb, (void *)base);

	if (!signal_event || event_add(signal_event, NULL)<0) {
		fprintf(stderr, "Could not create/add a signal event!\n");
		return 1;
	}

	event_base_dispatch(base);

	evconnlistener_free(listener);
	event_free(signal_event);
	event_base_free(base);
#ifdef _WIN32
		DeleteFileA(server_path);
#else
		unlink(server_path);
#endif
	printf("done\n");
	return 0;
}

static void
listener_cb(struct evconnlistener *listener, evutil_socket_t fd,
    struct sockaddr *sa, int socklen, void *user_data)
{
	struct event_base *base = user_data;
	struct bufferevent *bev;

	bev = bufferevent_socket_new(base, fd, BEV_OPT_CLOSE_ON_FREE);
	if (!bev) {
		fprintf(stderr, "Error constructing bufferevent!");
		event_base_loopbreak(base);
		return;
	}
	bufferevent_setcb(bev, NULL, conn_writecb, conn_eventcb, NULL);
	bufferevent_enable(bev, EV_WRITE);
	bufferevent_disable(bev, EV_READ);

	bufferevent_write(bev, MESSAGE, strlen(MESSAGE));
}

static void
conn_writecb(struct bufferevent *bev, void *user_data)
{
	struct evbuffer *output = bufferevent_get_output(bev);
	if (evbuffer_get_length(output) == 0) {
		printf("flushed answer\n");
		bufferevent_free(bev);
	}
}

static void
conn_eventcb(struct bufferevent *bev, short events, void *user_data)
{
	if (events & BEV_EVENT_EOF) {
		printf("Connection closed.\n");
	} else if (events & BEV_EVENT_ERROR) {
		printf("Got an error on the connection: %s\n",
		    strerror(errno));/*XXX win32*/
	}
	/* None of the other events can happen here, since we haven't enabled
	 * timeouts */
	bufferevent_free(bev);
}

static void
signal_cb(evutil_socket_t sig, short events, void *user_data)
{
	struct event_base *base = user_data;
	struct timeval delay = { 2, 0 };

	printf("Caught an interrupt signal; exiting cleanly in two seconds.\n");

	event_base_loopexit(base, &delay);
}

static void
print_usage(FILE *out, const char *name)
{
	fprintf(out, "Syntax: %s [ OPTS ]\n", name);
	fprintf(out,
#ifdef _WIN32
		"-I    Enable IOCP on Windows\n"
#endif
		"-U    Use Unix domain socket\n"
		"-s    If set -U, you must set a temporary file path as server path\n"
		"-h    Print usage\n"
	);
}

static void
parse_opts(int argc, char **argv)
{
	int opt;
	const char *opts;
	int no_afunix = 0;

#ifdef _WIN32
	opts = "IUs:h";
#else
	opts = "Us:h";
#endif

	while ((opt = getopt(argc, argv, opts)) != -1) {
		switch (opt) {
#ifdef _WIN32
		case 'I': use_iocp = 1; break;
#endif
		case 'U': use_afunix = 1; break;
		case 's': server_path = optarg; break;
		case 'h':
			print_usage(stdout, argv[0]);
			exit(EXIT_SUCCESS);
		default:
			print_usage(stdout, argv[0]);
			exit(EXIT_FAILURE);
		}		
	}

	if (use_afunix) {
#ifdef _WIN32
#ifdef EVENT__HAVE_AFUNIX_H
		if (!evutil_check_working_afunix_())
#endif
			no_afunix = 1;
#endif // _WIN32
	}
	if (no_afunix) {
		fprintf(stderr, "Does't support AF_UNIX socket, will omit -U option\n");
		use_afunix = 0;
		server_path = NULL;
	}
	if (use_afunix && !server_path) {
		fprintf(stderr, "The option -s is required if -U is set\n");
		print_usage(stdout, argv[0]);
		exit(EXIT_FAILURE);
	}
}
