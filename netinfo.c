/*
 * netinfo.c
 *
 * Copyright (C) 2013 - Andre Larbiere <andre@larbiere.eu>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <syslog.h>

#include "netinfo.h"
#include "debug.h"

#define IPV4_TCP_PORTS  "/proc/net/tcp"
#define IPV4_UDP_PORTS  "/proc/net/udp"
#define IPV6_TCP_PORTS  "/proc/net/tcp6"
#define IPV6_UDP_PORTS  "/proc/net/udp6"

// convert an IPv4 address & port number to a hex string in the format
//  B0B1B2B3:PORT
// with Bx representing the IP address in host byte order
// the string is returned in a static buffer, which is overwritten at every call
static const char *ipv4_bindstring(const char *ipv4, unsigned int port,
				   char **buffer, size_t bsize)
{
  union {
    uint8_t  b[4];
    uint32_t bin;
  } ip_s;

  if (!buffer) return NULL;

  sscanf(ipv4, "%hhu.%hhu.%hhu.%hhu",
	 &(ip_s.b[0]), &(ip_s.b[1]), &(ip_s.b[2]), &(ip_s.b[3]));
  snprintf(*buffer, bsize, "%08X:%04X", ip_s.bin, port);

  debugLog(LOG_DEBUG, "Bindstring \"%s\"\n", *buffer);
  return *buffer;
}

static uid_t uid_from_portlist(const char *procfile, const char *bindstring)
{
  char buffer[1024];
  uid_t id = UID_NOT_FOUND;
  FILE *portlist=fopen(procfile, "r");

  while (fgets(buffer, sizeof(buffer), portlist)) {
    char *uid;
    strtok(buffer, " "); // INDEX (ignored)
    char *local=strtok(NULL, " "); // LOCAL ADDRESS
    strtok(NULL, " "); // remote address (ignored)
    strtok(NULL, " "); // st (ignored)
    strtok(NULL, " "); // tx:rx queues (ignored)
    strtok(NULL, " "); // tr:tm-when (ignored)
    strtok(NULL, " "); // retrnsmt (ignored)
    uid=strtok(NULL, " "); // UID
			   // timeout, inode, etc. (ingored)
    if (strcmp(local, bindstring)==0) {
      id = strtol(uid, NULL, 10);
      debugLog(LOG_DEBUG, "Found UID=%lu\n", id);
      break;
    }
  }
  fclose(portlist);
  return (uid_t)id;
}

// find the UID associated with a specific local ipv4 TCP port
uid_t ipv4_tcp_port_uid(const char *ipv4, unsigned int port)
{
  char bindstring[32], *pbstr = bindstring;
  uid_t id;

  ipv4_bindstring(ipv4, port, &pbstr, sizeof(bindstring));
  id=uid_from_portlist(IPV4_TCP_PORTS, bindstring);
  if (UID_NOT_FOUND == id)
    debugLog(LOG_NOTICE, "UID for TCP port %i not found\n", port);
  return id;
}

// find the UID associated with a specific local ipv4 UDP port
uid_t ipv4_udp_port_uid(const char *ipv4, unsigned int port)
{
  char bindstring[32], *pbstr = bindstring;
  uid_t id;

  ipv4_bindstring(ipv4, port, &pbstr, sizeof(bindstring));
  id=uid_from_portlist(IPV4_UDP_PORTS, bindstring);
  if (UID_NOT_FOUND == id)
    debugLog(LOG_NOTICE, "UID for UDP port %i not found\n", port);
  return id;
}
