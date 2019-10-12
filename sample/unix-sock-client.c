#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
#include <afunix.h>
#else
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#endif
#include <stdio.h>
#include <string.h>

#define BUFFER_LENGTH    250

static void print_err(const char *info)
{
#ifdef _WIN32
	printf("%s: %d\n", info, WSAGetLastError());
#else
	perror(info);
#endif
}

int main(int argc, char *argv[])
{
	int sd = -1;
	int rc;
	char   buffer[BUFFER_LENGTH];
	struct sockaddr_un addr;

	if (argc < 2) {
		printf("Syntax:\n\tunix-sock-client <server-path>\n");
		printf("Example:\n\tunix-sock-client /tmp/AAAAAA\n");
		return 1;
	}

	do {
#ifdef _WIN32
		WSADATA WsaData;
		rc = WSAStartup(MAKEWORD(2, 2), &WsaData);
		if (rc != 0) {
			printf("WSAStartup failed with error: %d\n", rc);
			break;
		}
#endif
		sd = socket(AF_UNIX, SOCK_STREAM, 0);
		if (sd < 0) {
			print_err("socket() failed");
			break;
		}

		memset(&addr, 0, sizeof(addr));
		addr.sun_family = AF_UNIX;
#ifdef _WIN32
		strcpy_s(addr.sun_path, sizeof(addr.sun_path), argv[1]);
#else
		strlcpy(addr.sun_path, argv[1], sizeof(addr.sun_path));
#endif
		rc = connect(sd, (struct sockaddr *)&addr, sizeof(addr));
		if (rc < 0) {
			print_err("connect() failed");
			break;
		}

		memset(buffer, 0, sizeof(buffer));
		rc = recv(sd, buffer, BUFFER_LENGTH - 1, 0);
		if (rc < 0) {
			print_err("recv() failed");
		} else if (rc == 0) {
			printf("The server closed the connection\n");
		} else {
			printf("recv: %s\n", buffer);
		}

	} while (0);

	if (sd > 0)
#ifdef _WIN32
		shutdown(sd, 0);
#else
		close(sd);
#endif
#ifdef _WIN32
	WSACleanup();
#endif
	return 0;
}
