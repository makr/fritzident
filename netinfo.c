/*
 * netinfo.c
 *
 * Copyright (C) 2013 - Andre Larbiere <andre@larbiere.eu>
 * Copyright (C) 2018 - Matthias Kraft <m.kraft@gmx.com>
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
 *
 * Note: This code is non-portable as it highly depends on /proc/net/{tcp,udp}.
 *       Other non-portable stuff: %m in error handling
 */

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <arpa/inet.h>

#include "netinfo.h"
#include "debug.h"

// convert an IPv4 address & port number to a hex string in the format
//  B0B1B2B3:PORT
// with Bx representing the IP address in host byte order
// the string is written into the provided buffer (>=14 Bytes)
static const char *ipv4_bindstring(const char *ipv4, unsigned int port,
				   char **buffer, size_t bsize)
{
  int rc;
  uint32_t ipn;

  if (!buffer) return NULL;
  if (port > 65535) return NULL;

  if ((rc = inet_pton(AF_INET, ipv4, &ipn)) > 0) {
    snprintf(*buffer, bsize, "%08X:%04X", ipn, port);
    debugLog(LOG_DEBUG, "Bindstring \"%s\"\n", *buffer);
    return *buffer;
  } else if (rc < 0) {
    debugLog(LOG_ERR, "Error while converting IPv4 address '%s': %m\n", ipv4);
    return NULL;
  } else {
    debugLog(LOG_ERR, "Invalid IPv4 address '%s'!\n", ipv4);
    return NULL;
  }
}

// The files /proc/net/{tcp,udp} are almost identical, at least up to the point
// of the uid.
static uid_t uid_from_ipv4_connections(const char *procfile, const char *bindstring)
{
  char buffer[256];
  uid_t id = UID_NOT_FOUND;
  FILE *portlist=fopen(procfile, "r");

  if (!portlist) {
    debugLog(LOG_ERR, "Unable to open '%s': %m", procfile);
  } else {
    while ((fgets(buffer, sizeof(buffer), portlist)) != NULL) {
      char *uid, *local;
      if (!(strtok(buffer, " "))) break; // INDEX (ignored)
      if (!(local = strtok(NULL, " "))) break; // LOCAL ADDRESS
      if (!(strtok(NULL, " "))) break; // remote address (ignored)
      if (!(strtok(NULL, " "))) break; // st (ignored)
      if (!(strtok(NULL, " "))) break; // tx:rx queues (ignored)
      if (!(strtok(NULL, " "))) break; // tr:tm-when (ignored)
      if (!(strtok(NULL, " "))) break; // retrnsmt (ignored)
      if (!(uid = strtok(NULL, " "))) break; // UID
      if ((strcmp(local, bindstring)) == 0) {
	id = strtol(uid, NULL, 10);
	debugLog(LOG_DEBUG, "Found UID=%lu\n", id);
	break;
      }
    }
    // ignoring EOF and possible I/O errors at this stage
    fclose(portlist);
  }
  return (uid_t)id;
}

// find the UID associated with a specific local ipv4 TCP port
uid_t ipv4_tcp_port_uid(const char *ipv4, unsigned int port)
{
  char bindstring[16], *pbstr = bindstring;
  uid_t id = UID_NOT_FOUND;

  if ((ipv4_bindstring(ipv4, port, &pbstr, sizeof(bindstring))) != NULL) {
    if ((id=uid_from_ipv4_connections(IPV4_TCP_PORTS, bindstring)) == UID_NOT_FOUND) {
      debugLog(LOG_NOTICE, "UID for TCP port %i not found\n", port);
    }
  }
  return id;
}

// find the UID associated with a specific local ipv4 UDP port
uid_t ipv4_udp_port_uid(const char *ipv4, unsigned int port)
{
  char bindstring[16], *pbstr = bindstring;
  uid_t id = UID_NOT_FOUND;

  if ((ipv4_bindstring(ipv4, port, &pbstr, sizeof(bindstring))) != NULL) {
    if ((id=uid_from_ipv4_connections(IPV4_UDP_PORTS, bindstring)) == UID_NOT_FOUND) {
      debugLog(LOG_NOTICE, "UID for UDP port %i not found\n", port);
    }
  }
  return id;
}
