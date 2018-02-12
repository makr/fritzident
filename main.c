/*
 * main.c
 * Copyright (C) 2013 Andre Larbiere <andre@larbiere.eu>
 * Copyright (C) 2015 Nils Naumann <nau@gmx.net>
 * Copyright (C) 2018 - Matthias Kraft <m.kraft@gmx.com>
 *
 * fritzident is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * fritzident is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <errno.h>
#include <systemd/sd-daemon.h>

#include "netinfo.h"
#include "userinfo.h"
#include "debug.h"

#define PORT 14013 /* Fritzident port */ 
#define BUFFER 256
#define REAL_UID_MIN (1000L) /* ordinary users usually start at 1000 */
#define REAL_UID_MAX (65533L) /* 'nobody' as max user is usually 65334 */

void SocketServer(int port);
void usage(const char *cmdname);

static const struct option long_options[] = {
    { "verbose", no_argument,       NULL, 'v' },
    { "domain",  required_argument, NULL, 'd' },
    { "port",    required_argument, NULL, 'p' },
    { "umin",    required_argument, NULL, 'i' },
    { "umax",    required_argument, NULL, 'a' },
    { "help",    no_argument,       NULL, 'h' },
    { 0, 0, 0, 0 }
};

int main(int argc, char *argv[])
{
  uid_t umin = REAL_UID_MIN, umax = REAL_UID_MAX;
  unsigned long port = PORT; /* initializing port with default fritzident port */
  int option_index = 0, c;

  initLogging();

  while ((c = getopt_long(argc, argv, "vd:p:i:a:h",
			  long_options, &option_index)) >= 0) {
    switch (c) {
    case 'v':
      raiseVerbosity();
      break;
    case 'd':
      set_default_domain(optarg);
      break;
    case 'p':
      errno = 0;
      port = strtol(optarg, NULL, 0);
      if (errno != 0) {
	fprintf(stderr, "Unreadable port number: %s\n", strerror(errno));
	return 1;
      } else if (1L > port || 65535L < port) {
	fprintf(stderr, "Port number out of range (1-65535)\n");
	return 1;
      }
      break;
    case 'i':
      errno = 0;
      umin = strtol(optarg, NULL, 0);
      if (errno != 0) {
	fprintf(stderr, "Unreadable min. uid: %s\n", strerror(errno));
	return 1;
      }
      break;
    case 'a':
      errno = 0;
      umax = strtol(optarg, NULL, 0);
      if (errno != 0) {
	fprintf(stderr, "Unreadable max. uid: %s\n", strerror(errno));
	return 1;
      }
      break;
    case 'h':
      usage(argv[0]);
      return 0;
    default:
      fprintf(stderr, "Unknown option\n");
      fprintf(stderr, "Usage: %s [-h] [-v[v[v]]] [-p port] [-d domain] "
		      "[-i min_uid] [-a max_uid]\n", argv[0]);
      return 1;
    }
  }
  if (umin > umax) {
    fprintf(stderr, "Min. uid (%lu) exceeds max. uid (%lu)!\n",
		    (long unsigned int)umin, (long unsigned int)umax);
    return 1;
  }
  add_uid_range(umin, umax);
  add_uid_range(65537, (uid_t) -1);

  SocketServer(port);

  return 0;
}

static void sendResponse(int client_fd, const char *response, ...)
{
  char msg[BUFFER];
  int n;
  ssize_t nSent;

  va_list args;
  va_start(args, response);
  n = vsnprintf(msg, sizeof(msg), response, args);
  va_end(args);

  nSent = send(client_fd, msg, n + 1, 0);
  if (nSent < n + 1) {
    debugLog(LOG_ERR, "Sending '%s' failed: %s", msg, strerror(errno));
    exit(errno);
  } else {
    debugLog(LOG_DEBUG, "sent: %s", msg);
  }
}

static void execUSERS(int client_fd)
{
  struct passwd *userinfo;

  setpwent();
  while ((userinfo = getpwent()) != NULL) {
    if (included_uid(userinfo->pw_uid)) {
      debugLog(LOG_INFO, "USERS: %s\n", add_default_domain(userinfo->pw_name));
      sendResponse(client_fd, "%s\r\n", add_default_domain(userinfo->pw_name));
    } else {
      debugLog(LOG_DEBUG, "Ignoring user entry for uid %lu.\n", userinfo->pw_uid);
    }
  }
  endpwent();
}

static void execTCP(int client_fd, const char *ipv4, const char *port)
{
  uid_t uid;

  uid = ipv4_tcp_port_uid(ipv4, (unsigned int)strtol(port, NULL, 10));
  if (uid != UID_NOT_FOUND) {
    if (included_uid(uid)) {
      struct passwd *user = getpwuid(uid);
      debugLog(LOG_INFO, "TCP %s: %s\n", port, user->pw_name);
      sendResponse(client_fd, "USER %s\r\n", add_default_domain(user->pw_name));
    } else
      sendResponse(client_fd, "ERROR SYSTEM_USER\r\n");
  } else {
    sendResponse(client_fd, "ERROR NOT_FOUND\r\n");
  }
}

static void execUDP(int client_fd, const char *ipv4, const char *port)
{
  uid_t uid;

  uid = ipv4_udp_port_uid(ipv4, (unsigned int)strtol(port, NULL, 10));
  if (uid != UID_NOT_FOUND) {
    if (included_uid(uid)) {
      struct passwd *user = getpwuid(uid);
      debugLog(LOG_INFO, "UDP %s: %s\n", port, user->pw_name);
      sendResponse(client_fd, "USER %s\r\n", add_default_domain(user->pw_name));
    } else
      sendResponse(client_fd, "ERROR SYSTEM_USER\r\n");
  } else {
    sendResponse(client_fd, "ERROR NOT_FOUND\r\n");
  }
}

void SocketServer(int port)
{
  int socket_fd;
  int client_fd;
  struct sockaddr_in self;
  struct sockaddr_in client_addr;
  socklen_t addrlen = sizeof(client_addr);
  ssize_t bytes;
  char cmd[BUFFER];
  int n;

  n = sd_listen_fds(0); /* number of file descriptors passed by systemd */

  /* debugLog("Got %i file descriptors from systemd", n); */
  if (n > 1) {
    debugLog(LOG_ERR, "Too many file descriptors received.\n");
    exit(1);
  } else if (n == 1) {
    debugLog(LOG_DEBUG, "Socket passed by systemd.\n");
    socket_fd = SD_LISTEN_FDS_START + 0;
  } else {
    debugLog(LOG_DEBUG, "Creating socket ...\n");
    if ((socket_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
      debugLog(LOG_ERR, "socket: %s\n", strerror(errno));
      exit(errno);
    }

    memset(&self, 0, sizeof(self));
    self.sin_family = AF_INET;
    self.sin_port = htons(port);
    self.sin_addr.s_addr = INADDR_ANY;

    debugLog(LOG_DEBUG, "Binding port to socket\n");
    if (bind(socket_fd, (struct sockaddr*) &self, sizeof(self)) != 0) {
      debugLog(LOG_ERR, "bind: %s\n", strerror(errno));
      exit(errno);
    }

    /* Make it a "listening socket" */
    if (listen(socket_fd, 20) != 0) {
      debugLog(LOG_ERR, "listen: %s\n", strerror(errno));
      exit(errno);
    }
  }

  debugLog(LOG_NOTICE, "Daemon is listening on port %i\n", port);

  /* Infinite loop */
  while (1) {
    /* Bind port to socket */
    /* Await a connection on socket_fd*/

    /* debugLog("Waiting for connection from fritz!box...\n"); */
    client_fd = accept(socket_fd, (struct sockaddr*) &client_addr, &addrlen);
    if (client_fd < 0) {
      debugLog(LOG_ERR, "accept %s:%d connection failed: %s\n",
	  inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port),
	  strerror(errno));
      exit(errno);
    } else {
      debugLog(LOG_INFO, "Accepted connection from  %s:%d\n",
	       inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
    }

    bytes = send(client_fd, "AVM IDENT\r\n", strlen("AVM IDENT\r\n") + 1, 0);
    if (bytes < 0) {
      debugLog(LOG_ERR, "Sending \"AVM IDENT\" failed: %m");
      exit(errno);
    }

    /* wait for data */
    memset(&cmd, 0, sizeof(cmd));
    bytes = recv(client_fd, cmd, sizeof(cmd) - 1, 0);

    if (bytes == 0)
      continue;
    else if (bytes < 0) {
      debugLog(LOG_ERR, "recv: %s\n", strerror(errno));
      exit(errno);
    }

    cmd[bytes] = '\0';
    /* the first token is containing the command which might be
     * USERS, TCP, or UDP and is usually separated by spaces */
    debugLog(LOG_DEBUG, "recv'd: %s", cmd);
    char *cmdVerb = strtok(cmd, "\r\n ");

    /* debugLog("Received command: \"%s\"\n", cmdVerb); */
    /* command: USERS */
    if (strcmp(cmdVerb, "USERS") == 0) {
      execUSERS(client_fd);

    /* command: TCP ip:port*/
    } else if (strcmp(cmdVerb, "TCP") == 0) {
      char *localIp = strtok(NULL, ":");
      char *localPort = strtok(NULL, "\r\n");
      debugLog(LOG_INFO, "Searching for TCP \"%s:%s\"\n",
	       (localIp ? localIp : "(none)"),
	       (localPort ? localPort : "(none)"));
      if (localIp != NULL && localPort != NULL)
	execTCP(client_fd, localIp, localPort);
      else
	sendResponse(client_fd, "ERROR UNSPECIFIED\r\n");

    /* command: UDP ip:port */
    } else if (strcmp(cmdVerb, "UDP") == 0) {
      char *localIp = strtok(NULL, ":");
      char *localPort = strtok(NULL, "\r\n");
      debugLog(LOG_INFO, "Searching for UDP \"%s:%s\"\n",
	       (localIp ? localIp : "(none)"),
	       (localPort ? localPort : "(none)"));
      if (localIp != NULL && localPort != NULL)
	execUDP(client_fd, localIp, localPort);
      else
	sendResponse(client_fd, "ERROR UNSPECIFIED\r\n");
    } else {
      debugLog(LOG_WARNING, "Unrecognized command \"%s\"\n", cmd);
      sendResponse(client_fd, "ERROR UNSPECIFIED\r\n");
    }
    /* Close data connection */
    close(client_fd);
  }

  /* Finally some housekeeping */
  close(socket_fd);
}

void usage(const char *cmdname)
{
  printf("Usage: %s [-h] [-v[v[v]]] [-p port] [-d domain] [-i min_uid] [-a max_uid]\n\n", cmdname);
  printf("Answer Fritz!Box user identification requests.\n\n");
  printf("fritzident mimics the standard AVM Windows application that allows the "
         "Fritz!Box to recognize individual users connecting to the Internet.\n");
  printf("Options:\n");
  printf("\t-h - this usage information\n");
  printf("\t-v - increase verbosity (may be assed multiple times.)\n");
  printf("\t-p port - to listen on if not %u (for debugging only)\n", PORT);
  printf("\t-d domain - fake a Windows domain\n");
  printf("\t-i min_uid - min. user id to interpret as real user (default: %lu)\n", REAL_UID_MIN);
  printf("\t-a max_uid - max. user id to interpret as real user (default: %lu)\n", REAL_UID_MAX);
  printf("\nLICENSE:\n");
  printf("This utility is provided under the GNU GENERAL PUBLIC LICENSE v3.0\n"
         "(see http://www.gnu.org/licenses/gpl-3.0.txt)\n");
}
