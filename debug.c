/*
 * debug.c
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
 * Note: non-portable functionality used: vsyslog(), LOG_UPTO
 */

#include <stdarg.h>
//#include <stdio.h> /* for vfprintf() only */
#include <stdlib.h>

#include "debug.h"

/**
 * @brief Write a message into syslog.
 */
void debugLog(int pri, const char *fmsg, ...)
{
  va_list args;

  va_start(args, fmsg);
//  vfprintf(stderr, fmsg, args); /* debugging only, mutual exclusive with vsyslog() */
  vsyslog(pri, fmsg, args);
  va_end(args);
}

/**
 * @brief Increase verbosity by one, max value = LOG_DEBUG (-vvv)
 * 
 * @return void
 */
void raiseVerbosity()
{
  int p = (setlogmask(0) << 1) + 1;
  if ((LOG_UPTO(LOG_PRIMASK)) >= p)
    setlogmask(p);
}

/**
 * @brief Initialize logging up to level 'Warning'.
 */
void initLogging()
{
  setlogmask(LOG_UPTO(LOG_WARNING));
}
