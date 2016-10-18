#include "types.h"
#include "config.h"
#include "kiwi.h"
#include "misc.h"
#include "web.h"
#include "spi.h"
#include "coroutines.h"
#include "debug.h"
#include "printf.h"

#include <sys/file.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <setjmp.h>
#include <ctype.h>
#include <syslog.h>
#include <stdlib.h>
#include <stdarg.h>
#include <time.h>

static bool log_foreground_mode = false;
static bool log_ordinary_printfs = false;

void xit(int err)
{
	void exit(int);
	
	fflush(stdout);
	spin_ms(1000);	// needed for syslog messages to be properly recorded
	exit(err);
}

void _panic(const char *str, bool coreFile, const char *file, int line)
{
	char *buf;
	
	if (ev_dump) ev(EC_DUMP, EV_PANIC, -1, "panic", "dump");
	asprintf(&buf, "%s: \"%s\" (%s, line %d)", coreFile? "DUMP":"PANIC", str, file, line);

	if (background_mode || log_foreground_mode) {
		syslog(LOG_ERR, "%s\n", buf);
	}
	
	printf("%s\n", buf);
	if (coreFile) abort();
	xit(-1);
}

void _sys_panic(const char *str, const char *file, int line)
{
	char *buf;
	
	// errno might be overwritten if the malloc inside asprintf fails
	asprintf(&buf, "SYS_PANIC: \"%s\" (%s, line %d)", str, file, line);

	if (background_mode || log_foreground_mode) {
		syslog(LOG_ERR, "%s: %m\n", buf);
	}
	
	perror(buf);
	xit(-1);
}

static bool appending;
static char *buf, *last_s, *start_s;
static int brem;

static void ll_printf(u4_t type, conn_t *c, const char *fmt, va_list ap)
{
	int i, sl;
	char *s, *cp;
	#define VBUF 1024
	
	if (!do_sdr) {
	//if (!background_mode) {
		if ((buf = (char*) malloc(VBUF)) == NULL)
			panic("log malloc");
		vsnprintf(buf, VBUF, fmt, ap);

		// remove our override and call the actual underlying printf
		#undef printf
			printf("%s", buf);
		#define printf ALT_PRINTF
		
		evPrintf(EC_EVENT, EV_PRINTF, -1, "printf", buf);
	
		if (buf) free(buf);
		buf = 0;
		return;
	}
	
	if (appending) {
		s = last_s;
	} else {
		brem = VBUF;
		if ((buf = (char*) malloc(VBUF)) == NULL)
			panic("log malloc");
		s = buf;
		start_s = s;
	}

	vsnprintf(s, brem, fmt, ap);
	sl = strlen(s);		// because vsnprintf returns length disregarding limit, not the actual length
	brem -= sl+1;
	
	cp = &s[sl-1];
	if (*cp != '\n' && brem && !(type & PRINTF_MSG)) {
		last_s = cp+1;
		appending = true;
		return;
	} else {
		appending = false;
	}
	
	// for logging, don't print an empty line at all
	if ((type & (PRINTF_REG | PRINTF_LOG)) && (!background_mode || strcmp(start_s, "\n") != 0)) {

		// remove non-ASCII since "systemctl status" gives [blob] message
		// unlike "systemctl log" which prints correctly
		int sl = strlen(buf);
		for (i=0; i < sl; i++)
			if (buf[i] > 0x7f) buf[i] = '?';

		char up_chan_stat[64], *s = up_chan_stat;
		
		// uptime
		u4_t up = timer_sec();
		u4_t sec = up % 60; up /= 60;
		u4_t min = up % 60; up /= 60;
		u4_t hr  = up % 24; up /= 24;
		u4_t days = up;
		if (days)
			sl = sprintf(s, "%dd:%02d:%02d:%02d ", days, hr, min, sec);
		else
			sl = sprintf(s, "%d:%02d:%02d ", hr, min, sec);
		s += sl;
	
		// show state of all rx channels
		rx_chan_t *rx;
		for (rx = rx_chan, i=0; rx < &rx_chan[RX_CHANS]; rx++, i++) {
			*s++ = rx->busy? '0'+i : '.';
		}
		*s++ = ' ';
		
		// show rx channel number if message is associated with a particular rx channel
		if (c != NULL) {
			for (i=0; i < RX_CHANS; i++)
				*s++ = (i == c->rx_channel)? '0'+i : ' ';
		} else {
			for (i=0; i < RX_CHANS; i++) *s++ = ' ';
		}
		*s = 0;
		
		if (((type & PRINTF_LOG) && (background_mode || log_foreground_mode)) || log_ordinary_printfs) {
			syslog(LOG_INFO, "%s %s", up_chan_stat, buf);
		}
	
		time_t t;
		char tb[32];
		time(&t);
		ctime_r(&t, tb);
		tb[24]=0;
		
		// remove our override and call the actual underlying printf
		#undef printf
			printf("%s %s %s", tb, up_chan_stat, buf);
		#define printf ALT_PRINTF

		evPrintf(EC_EVENT, EV_PRINTF, -1, "printf", buf);
	}
	
	// attempt to also record message remotely
	if ((type & PRINTF_MSG) && msgs_mc) {
		if (type & PRINTF_FF)
			send_encoded_msg_mc(msgs_mc, "MSG", "status_msg", "\f%s", buf);
		else
			send_encoded_msg_mc(msgs_mc, "MSG", "status_msg", "%s", buf);
	}
	
	if (buf) free(buf);
	buf = 0;
}

void alt_printf(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	ll_printf(PRINTF_REG, NULL, fmt, ap);
	va_end(ap);
}

void lfprintf(u4_t printf_type, const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	ll_printf(printf_type, NULL, fmt, ap);
	va_end(ap);
}

void cprintf(conn_t *c, const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	ll_printf(PRINTF_REG, c, fmt, ap);
	va_end(ap);
}

void clprintf(conn_t *c, const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	ll_printf(PRINTF_LOG, c, fmt, ap);
	va_end(ap);
}

void lprintf(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	ll_printf(PRINTF_LOG, NULL, fmt, ap);
	va_end(ap);
}

void mprintf(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	ll_printf(PRINTF_MSG, NULL, fmt, ap);
	va_end(ap);
}

void mprintf_ff(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	ll_printf(PRINTF_MSG|PRINTF_FF, NULL, fmt, ap);
	va_end(ap);
}

void mlprintf(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	ll_printf(PRINTF_MSG|PRINTF_LOG, NULL, fmt, ap);
	va_end(ap);
}

void mlprintf_ff(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	ll_printf(PRINTF_MSG|PRINTF_LOG|PRINTF_FF, NULL, fmt, ap);
	va_end(ap);
}

// encoded snprintf()
int esnprintf(char *str, size_t slen, const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	int rv = vsnprintf(str, slen, fmt, ap);
	va_end(ap);

	size_t slen2 = strlen(str) * ENCODE_EXPANSION_FACTOR;	// c -> %xx
	slen2++;	// null terminated
	char *str2 = (char *) kiwi_malloc("eprintf", slen2);
	mg_url_encode(str, str2, slen2);
	slen2 = strlen(str2);
	
	// Passed sizeof str[slen] is meant to be far larger than current strlen(str)
	// so there is room to return the larger encoded result.
	check(slen2 <= slen);
	strcpy(str, str2);
	kiwi_free("eprintf", str2);
	return slen2;
}
