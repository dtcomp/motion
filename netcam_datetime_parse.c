/*#define STAND_ALONE 1 */

#include <ctype.h>
#include <time.h>
#include <regex.h>                    /* For datetime parsing  */
#include <stdarg.h>

#ifndef STAND_ALONE
#include "motion.h"
#else
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#endif

#include "netcam_datetime_parse.h"


#ifdef STAND_ALONE
#define mymalloc malloc
#endif


typedef struct rexp_tok {
    char N;
    const char* rexp;
} rexp_tok;

static const rexp_tok t_hour = {'H', ""};
static const rexp_tok r_aday = {'W', "([mtwfs][ouehra][nedit][a-Z]*[, ]+)?"};
static const rexp_tok r_nday = {'D', "0?([1-9]|[1-3][0-9])"};
static const rexp_tok r_amon = {'M', "([jfmasond][aepuco][nbrylgptvc])[a-Z]*"};
static const rexp_tok r_year = {'Y', "(2[0-9][0-9][0-9])"};
static const rexp_tok r_time = {'T', "0?([1-9]|[1][0-9]|[2][0-3]):0?([1-9]|[1-5][0-9]):0?([1-9]|[1-5][0-9])"};
static const rexp_tok r_times = {'S', "0?([1-9]|[1-2][1-9]):0?([1-9]|[1-5][0-9]):0?([1-9]|[1-5][0-9])"};
static const rexp_tok r_zone = {'Z', "([A-z/]*?)"};
static const rexp_tok r_wsoc = {'_', "[, ]+"};
static const rexp_tok r_stop = {'X', ""};




static char *netcam_datetime_re[11];
static char netcam_datetime_re_t[11][11];
static int netcam_datetime_re_cnt = 0;
static regex_t pattbuf[11];


#define W &r_aday
#define D &r_nday
#define M &r_amon
#define Y &r_year
#define T &r_time
#define Z &r_zone
#define _ &r_wsoc
#define X &r_stop


static int netcam_datetime_setup_re(const rexp_tok *exp, ...) {
    va_list ap;

    static char re_tmp[256];
    int l, j = 0, res;
    const rexp_tok *re = exp;

    re_tmp[0] = '\0';
//    printf("processing index %i\n", netcam_datetime_re_cnt);

    va_start(ap, exp);
    while (re->N != 'X') {
//        printf("%c: %s\n", re->N, re->rexp);
        strcat(re_tmp, re->rexp);
        if (re->N != '_') /* space tokens don't count/capture */
            netcam_datetime_re_t[netcam_datetime_re_cnt][j++] = re->N;
        re = va_arg(ap, const rexp_tok *);
    }
    va_end(ap);

    netcam_datetime_re_t[netcam_datetime_re_cnt][j] = '\0'; /* install terminator, JIC */
    l = strlen(re_tmp);
    netcam_datetime_re[netcam_datetime_re_cnt] = mymalloc(l + 1);
    netcam_datetime_re[netcam_datetime_re_cnt][0] = '\0';
    strcpy(netcam_datetime_re[netcam_datetime_re_cnt], re_tmp);

    res = regcomp(&pattbuf[netcam_datetime_re_cnt], netcam_datetime_re[netcam_datetime_re_cnt], REG_EXTENDED | REG_ICASE);

    if (res) {
        printf("[%i] regexp compile failed: %s\n", netcam_datetime_re_cnt, netcam_datetime_re[netcam_datetime_re_cnt]);
    } else {
//        printf("%s\n", netcam_datetime_re[netcam_datetime_re_cnt]);
//        int m;
//        for (m = 0; m < 11; m++) {
//            if (netcam_datetime_re_t[netcam_datetime_re_cnt][m])
//                printf("%c ", netcam_datetime_re_t[netcam_datetime_re_cnt][m]);
//        }
//        printf("\n");
    }

    netcam_datetime_re_cnt++;
//    printf("compiled %i cases\n", netcam_datetime_re_cnt);
    return res;
}

/*
   in[1] = "Mon, 02 Nov 2015 04:47:04 AK/DST";
  in[2] = "Mon 02 Nov 2015 04:47:04 GMT";
  in[3] = "02 Nov 2015 04:47:04 GMT";

  in[4] = "Mon, Nov 02 2015 04:47:04 GMT";
  in[5] = "Mon Nov 02 2015 04:47:04 GMT";
  in[6] = "Nov 02 2015 04:47:04 GMT";

  in[7] = "Mon, 2 Nov 2015 04:47:04 GMT";
  in[8] = "Mon, 2 Nov 2015 24:59:59 GMT";
  in[10] = "Nov  2 12:28:39 AKST 2015";
  in[11] = "Mon Nov  2 12:28:39 AKST 2015";

 */
void netcam_datetime_init(void) {

    netcam_datetime_setup_re(D, _, M, _, Y, _, T, _, Z, X); /* day and mon reversed */
    netcam_datetime_setup_re(M, _, D, _, T, _, Y, _, Z, X); /* "ctime()" */
    netcam_datetime_setup_re(M, _, D, _, T, _, Z, _, Y, X); /* bash "date" */
    netcam_datetime_setup_re(M, _, D, _, Y, _, T, _, Z, X); /*  */
}

/**
 * netcam_atom
 *
 *      Recognizes first three characters of a string as month name.
 *
 * Parameters:
 *
 *      input      The input string
 *
 * Returns:        1-12 (Jan-Dec) or -1 if string not recognized as month name.
 *
 */

static int netcam_datetime_atom(const char *month) {

    char m[4];

    strncpy(m, month, 3);

    int i;
    for (i = 0; i < 4; i++) {
        m[i] = tolower(m[i]);
    }

    switch (m[0]) {
        case 'a':
        {
            switch (m[1]) {
                case 'p': return 3;
                case 'u': return 7;
            }
        }
        case 'd':
        {
            return 11;
        }
        case 'f':
        {
            return 1;
        }
        case 'j':
        {
            switch (m[1]) {
                case 'a': return 0;
                case 'u':
                {
                    switch (m[2]) {
                        case 'l':
                        {
                            return 6;
                        }
                        case 'n':
                        {
                            return 5;
                        }
                    }
                }
            }
        }
        case 'm':
        {
            switch (m[2]) {
                case 'r':
                {
                    return 2;
                }
                case 'y':
                {
                    return 4;
                }
            }
        }
        case 'n':
        {
            return 10;
        }
        case 'o':
        {
            return 9;
        }
        case 's':
        {
            return 8;
        }
    }

    return -1;
}

/**
 * netcam_datetime_re_match
 *
 *      Finds the matched part of a regular expression
 *
 * Parameters:
 *
 *      m          A structure containing the regular expression to be used
 *      input      The input string
 *
 * Returns:        The string which was matched
 *
 */
static char *netcam_datetime_re_match(regmatch_t m, const char *input) {
    char *match = NULL;
    int len;

    if (m.rm_so != -1) {
        len = m.rm_eo - m.rm_so;

        if ((match = (char *) mymalloc(len + 1)) != NULL) {
            strncpy(match, input + m.rm_so, len);
            match[len] = '\0';
        }
    }

    return match;
}

/**
 * netcam_datetime_parse
 *
 *      parses a string containing a URL into it's components
 *
 * Parameters:
 *      parse_url          A structure which will receive the results
 *                         of the parsing
 *      text_url           The input string containing the URL
 *
 * Returns:                Nothing
 *
 */
static time_t netcam_datetime_parse(const char *text_date) {
    char *s;
    int i, j;
    int rsx;
    struct tm res;
    regmatch_t matches[11];
    time_t unixtime=0;
//    static char buf[128];


    //
    //    MOTION_LOG(DBG, TYPE_NETCAM, NO_ERRNO, "%s: Entry netcam_datetime_parse data %s", 
    //               text_date);

    /*
     * regexec matches the URL string against the regular expression
     * and returns an array of pointers to strings matching each match
     * within (). The results that we need are finally placed in struct res.
     */
//    printf("text: %s\n", text_date);

    for (i = 0; i < netcam_datetime_re_cnt; i++) {
        //    printf("i=%i\n", i);
        rsx = regexec(&pattbuf[i], text_date, 10, matches, 0);
        //    printf("%s%i\n", "regexec returned:", rsx);
        if (rsx != REG_NOMATCH)
            break;
    }

    //  printf("i=%i\n", i);
    char t = 'x', *zone = NULL;
    int k = 0;

    if (rsx != REG_NOMATCH) {
        for (j = 0; j < 10; j++) {
            if ((s = netcam_datetime_re_match(matches[j], text_date)) != NULL) {
                //                  
                //                    MOTION_LOG(DBG, TYPE_NETCAM, NO_ERRNO, "%s: Parse case %d"
                //                               " data %s", i, s);
                switch (t) {
                    case 'W':
//                        printf("%i:%c %s\n", j, t, s);
                        free(s);
                        break;

                    case 'D':
                        res.tm_mday = atoi(s);
//                        printf("%i:%c %s mday=%i\n", j, t, s, res.tm_mday);
                        free(s);
                        break;

                    case 'M':
                        res.tm_mon = netcam_datetime_atom(s);
//                        printf("%i:%c %s mon=%i\n", j, t, s, res.tm_mon);
                        free(s);
                        break;

                    case 'Y':
                        res.tm_year = (atoi(s) - 1900);
//                        printf("%i:%c %s year=%i\n", j, t, s, res.tm_year);
                        free(s);
                        break;

                    case 'T':
                        res.tm_hour = atoi(s);
//                        printf("%i:%c %s hour=%i\n", j, t, s, res.tm_hour);
                        free(s);
                        ++j;
                        s = netcam_datetime_re_match(matches[j], text_date);
                        res.tm_min = atoi(s);
//                        printf("%i:%c %s min=%i\n", j, t, s, res.tm_min);
                        free(s);
                        ++j;
                        s = netcam_datetime_re_match(matches[j], text_date);
                        res.tm_sec = atoi(s);
//                        printf("%i:%c %s sec=%i\n", j, t, s, res.tm_sec);
                        free(s);
                        break;

                    case 'Z':
                        res.tm_zone = zone = s;
//                        printf("%i:%c %s zone=%s\n", j, t, s, res.tm_zone);
                        break;

                        /* Other components ignored */
                    default:
//                        printf("%i:%c (%i) (unused) %s\n", j, t, t, s);
                        free(s);
                        break;
                }
            }
            t = netcam_datetime_re_t[i][k++];
        }

        unixtime = mktime(&res);
//        printf("utime  is %d\n", (unsigned int) unixtime);
//        printf("ctime  is %s", ctime_r(&unixtime, buf));
//        gmtime_r(&unixtime, &res);
//        printf("gmtime is %s", asctime_r(&res, buf));
        free(zone);
    } else {
//        printf("regexec nomatch: %s .\n", text_date);
        return 0;
    }

    return unixtime;
}

#ifdef STAND_ALONE
/**
 * skip_lws
 *  Skip LWS (linear white space), if present.  Returns number of
 *  characters to skip.  
 */
int skip_lws(const char *string) {
    const char *p = string;

    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')
        ++p;

    return p - string;
}


/**
 * header_process
 * 
 *  Check whether HEADER begins with NAME and, if yes, skip the `:' and
 *  the whitespace, and call PROCFUN with the arguments of HEADER's
 *  contents (after the `:' and space) and ARG.  Otherwise, return 0. 
 */
int header_process(const char *header, const char *name,
        int (*procfun)(const char *, void *), void *arg) {
    /* Check whether HEADER matches NAME. */
    while (*name && (tolower(*name) == tolower(*header)))
        ++name, ++header;

    if (*name || *header++ != ':')
        return 0;

    header += skip_lws(header);
    return ((*procfun) (header, arg));
}
#endif

/**
 * header_extract_time
 * 
 *  Extract a long integer from HEADER and store it to CLOSURE.  If an
 *  error is encountered, return 0, else 1.  
 */
int header_extract_time(const char *header, void *closure) {
    time_t unixtime=0;

    unixtime = netcam_datetime_parse(header);
        printf("utime2  is %d\n", (unsigned int) unixtime);
    /* Failure if no number present. */
    if (unixtime == 0)
        return 0;

    /* Skip trailing whitespace. */
    //    p += skip_lws (p);

    /* We return the value, even if a format error follows. */
    *(time_t *) closure = unixtime;

    /* Indicate failure if trailing garbage is present. */
    //    if (*p)
    //        return 0;

    return 1;
}

/**
 * netcam_check_last_modified
 *
 *     Analyse an HTTP-header line to see if it is a Content-length.
 *
 * Parameters:
 *
 *      header          Pointer to a string containing the header line.
 *
 * Returns:
 *      -1              Not a Content-length line.
 *      >=0             Value of Content-length field.
 *
 */
time_t netcam_check_last_modified(char *header) {
    time_t unixtime=0;

    if (!header_process(header, "Last-Modified", header_extract_time, &unixtime)) {
        /*
         * Some netcams deliver some bad-format data, but if
         * we were able to recognize the header section and the
         * number we might as well try to use it.
         */
        /*
        if (length > 0)
            MOTION_LOG(WRN, TYPE_NETCAM, NO_ERRNO, "%s: malformed token"
                       " Content-Length but value %ld", length);
         */
    }
    //
    //    MOTION_LOG(INF, TYPE_NETCAM, NO_ERRNO, "%s: Content-Length %ld", 
    //               length);
    return unixtime;
}

#ifdef STAND_ALONE
int main(int argc, char **argv) {
    netcam_datetime_init();

    char *in[25];
    static char buf[128];
    struct tm res;
    


    in[1] = "Last-Modified: Mon, 02 Nov 2015 04:47:04 AK/DST";
    in[2] = "Last-Modified: Mon 02 Nov 2015 04:47:04 GMT";
    in[3] = "Last-Modified: 02 Nov 2015 04:47:04 GMT";

    in[4] = "Last-Modified: Mon, Nov 02 2015 04:47:04 GMT";
    in[5] = "Last-Modified: Mon Nov 02 2015 04:47:04 GMT";
    in[6] = "Last-Modified: Nov 02 2015 04:47:04 GMT";

    in[7] = "Last-Modified: Mon, 2 Nov 2015 04:47:04 GMT";
    in[8] = "Last-Modified: Mon, 2 Nov 2015 24:59:59 GMT";
    in[9] = "Last-Modified: Nov  2 12:28:39 AKST 2015";
    in[10] = "Last-Modified: Mon Nov  2 12:28:39 AKST 2015";
    in[11] = "Last-Modified: Mon Nov  2 12:28:39 UTC 2015";
    in[12] = "Last-Modified: 1997-07-16T19:20:30.45+01:00";
    in[13] =  "date:create: 2015-11-04T23:50:15-09:00";
    time_t ut = 0;

    int i;

    //  
    //  for (i = 1; i < 12; i++) {
    //    ut = netcam_datetime_parse(in[i]);
    //  }
    //  
    for (i = 1; i < 12; i++) {
        ut = netcam_check_last_modified(in[i]);
        memset(&res,0,sizeof(time_t));
        gmtime_r(&ut, &res);
        printf("Last-Modified: %s\n", asctime_r(&res, buf));
    }

}
#endif
