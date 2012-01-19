/* $Id$ */

#ifndef __PROCINFO_H__
#define __PROCINFO_H__

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <glib.h>
#include "foreach.h"

#define dot(n) (((n)==0) ? (_procinfo_cnt = (fputc('\n', stderr), 0)) :\
		(++_procinfo_cnt && ((n)<64)) ? fputc('.', stderr) :\
		(_procinfo_cnt % ((n)>>2) == 0) ? fprintf(stderr, "%d%%", 25*_procinfo_cnt/(n>>2)) :\
		(_procinfo_cnt % ((n)>>5) == 0) ? fputc('.', stderr) : 0)

guint32 _procinfo_cnt = 0;

static guint64 memory() {
  guint64 mem = 0;
#ifndef CYGWIN
  int i = 0;
  foreach_line(buf, "/proc/self/stat") {
    foreach_token(tok, buf) {
      if (++i == 23) {
	mem = atoll(tok);
	break;
      }
    }
    break;
  }
#endif
  return mem;
}

static guint32 runtime() {
  static time_t t0 = 0;
  time_t t1 = time(NULL);
  if (t0 == 0) t0 = t1;
  return t1-t0;
}

/* Issue: clock() starts giving negative numbers after a while.  I
   checked the header files, CLOCKS_PER_SEC is defined to be 1000000
   and the return value of clock, clock_t, is defined as signed long.
   This means after 2000 seconds things go negative.  Need to use some
   other function for longer programs. 

   Switched to using times, which works with _SC_CLK_TCK defined as
   100 so the overflow should take longer.

   That also didn't work, finally decided to use the time function
   which has a resolution of seconds.
*/

static void my_log_func(const gchar *log_domain,
		 GLogLevelFlags log_level,
		 const gchar *message,
		 gpointer user_data) {
  fprintf(stderr, "[t=%d m=%" G_GUINT64_FORMAT "] %s\n", 
	  runtime(), memory(), message);
}

static void g_message_init()
{
  g_log_set_handler(NULL, G_LOG_LEVEL_MESSAGE, my_log_func, NULL);
}

#endif
