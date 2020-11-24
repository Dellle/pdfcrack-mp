/**
 * Copyright (C) 2006 Henning Norén
 * 
 * Multi-core (OpenMP) and pattern support by 
 *       Shreepad Shukla, Copyright (C) 2015
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, 
 * USA.
 *
 */

#ifndef _PDFPASSWORDS_H_
#define _PDFPASSWORDS_H_

#include <stdio.h>
#include "common.h"

#define PASSLENGTH 33

void
initPasswords(const passwordMethod pm, FILE *file, const char *wl,
	      const char *cs, const char* pat, const unsigned int minPw,
	      const unsigned int maxPw, const bool qt, int numthreads);

bool
nextPassword(void);

unsigned int
setPassword(uint8_t *outbuf);

bool
pw_loadState(FILE *file, char **wl);

void
pw_saveState(FILE *file);

int getPatternPassword(long long int n, uint8_t* patPassword);

unsigned long long int getMaxPatternPasswords();

#endif /** _PDFPASSWORDS_H_ */
