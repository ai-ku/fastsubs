/* foreach.h: C needs some good looping macros.
 *
 * Copyright (c) 2004-2012, Deniz Yuret <denizyuret@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __FOREACH_H__
#define __FOREACH_H__
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#define LINE (1<<20)

/* foreach_line(str, file): iterates with str = each line of file */
/* Use file = path string for a regular file */
/* Use file = empty string to represent stdin */
/* Use file = "< cmd" to represent the output of a command */

#define _myopen(f) ((*(f)==0)?stdin:(*(f)=='<')?popen((f)+1,"r"):fopen((f),"r"))
#define _myclose(f,fp) ((*(f)==0)?0:(*(f)=='<')?pclose(fp):fclose(fp))

#define foreach_line(str, fname)\
  errno = 0; \
  for (FILE *_fp = _myopen(fname); \
       (_fp != NULL) || (errno && (perror(fname), exit(errno), 0)); \
       _fp = (_myclose(fname, _fp), NULL)) \
  for (char str[LINE];\
       ((str[LINE - 1] = -1) &&\
        fgets(str, LINE, _fp) &&\
        ((str[LINE - 1] != 0) ||\
         (perror("Line too long"), exit(-1), 0))); )

#define foreach_token(tok, str)\
  for (char *_ptr = NULL, *(tok) = strtok_r((str), " \t\n\r\f\v", &_ptr);\
       (tok) != NULL; (tok) = strtok_r(NULL," \t\n\r\f\v", &_ptr))

#define foreach_token3(tok, str, sep)\
  for (char *_ptr = NULL, *(tok) = strtok_r((str), (sep), &_ptr);\
       (tok) != NULL; (tok) = strtok_r(NULL, (sep), &_ptr))

#define foreach_int(var, lo, hi)\
  for (register int var = (lo), _hi = (hi); var <= _hi; var++)

#define foreach_char(var, str)\
  for (register char var, *_p = (str);\
       (var = *_p) != 0; _p++)

#ifdef __G_LIB_H__
#define foreach(type, var, array)\
  for (register type (var), *_p = (type*)(array)->pdata;\
       _p != NULL; _p = NULL)\
  for (register int _i = 0, _l = (array)->len;\
       (_i < _l) && ((var = _p[_i]) || 1); _i++)

#define foreach_ptr(ptr, array)\
  for (register gpointer *ptr = (array)->pdata;\
       ptr != NULL; ptr = NULL)\
  for (register int _i = 0, _l = (array)->len;\
       (_i < _l); _i++, ptr++)
#endif

#endif

