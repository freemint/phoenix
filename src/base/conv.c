/*****************************************************************************
 *
 * Module : CONV.C
 * Author : Dieter & JÅrgen Geiû
 *
 * Creation date    : 06.03.89
 * Last modification: $Id$
 *
 *
 * Description: This module implements the convert routines.
 *
 * History:
 * 09.09.98: str2bin is year 2000 compatible
 * 30.07.94: Function scan_stamp will result in FALSE if no correct date or time
 * 29.03.94: Problem with negative numbers in format_num fixed
 * 01.08.93: Using ansi_strcmp for mixing Windows/DOS
 * 18.07.93: Initialize DLLs data segment in WIN32 in init_conv
 * 24.06.93: Warnings removed
 * 23.11.92: PASCAL defined as empty for GEMDOS
 * 16.11.92: sprintf with %.*lf removed for DLL, dtoa added
 * 14.11.92: Function build_pass moved to kernel
 * 02.11.92: Packing warnings removed
 * 01.11.92: Compare functions are far pascal
 * 31.10.92: Far pointer definitions for DLL implementation added
 * 26.10.92: init_conv initialized only once in DLL
 * 25.10.92: Calls to sscanf removed
 * 24.10.92: Module header added
 * 06.03.89: Creation of body
 *****************************************************************************/

#include <ctype.h>

#include "import.h"
#include "utility.h"

#include "export.h"
#include "conv.h"

/****** DEFINES **************************************************************/

#define MAX_WDIGITS  6
#define MAX_LDIGITS 11
#define MAX_RDIGITS 16

#define NULL_FLOAT  -1e38

#define MIN_WORD    (SHORT)0x8000
#define MIN_LONG    (LONG)0x80000000L

#if GEMDOS              /* WINAPI is already empty */
#undef PASCAL           /* LOCAL compare and size functions... */
#define PASCAL          /* should not be PASCAL */
#endif

/****** TYPES ****************************************************************/

typedef struct
{
  INT  id;
  CHAR  *idstr;
} FORMATS;

/****** VARIABLES ************************************************************/

LOCAL CHAR      **month_short;    /* short month names */
LOCAL CHAR      **month_long;     /* long month names */
LOCAL CHAR      sep_dec   = '.';  /* decimal separator */
LOCAL CHAR      sep_thsnd = ',';  /* thousand separator */

LOCAL DOUBLE    null_double;      /* null double value */
LOCAL DATE      null_date;        /* null date value */
LOCAL TIME      null_time;        /* null time value */
LOCAL TIMESTAMP null_stamp;       /* null timestamp value */

LOCAL USHORT    century = 1900;   /* century (1900 or 2000) */

LOCAL FORMATS   formats [] =
{
  {ID_NOCHAR,       NULL},
  {ID_ACHAR,        NULL},
  {ID_DAY,          FORM_DAY},
  {ID_MONTH_LONG,   FORM_MONTH_LONG},
  {ID_MONTH_SHORT,  FORM_MONTH_SHORT},
  {ID_MONTH_NUM,    FORM_MONTH_NUM},
  {ID_YEAR_LONG,    FORM_YEAR_LONG},
  {ID_YEAR_SHORT,   FORM_YEAR_SHORT},
  {ID_HOUR_24,      FORM_HOUR_24},
  {ID_HOUR_AM,      FORM_HOUR_AM},
  {ID_HOUR_PM,      FORM_HOUR_PM},
  {ID_HOUR_am,      FORM_HOUR_am},
  {ID_HOUR_pm,      FORM_HOUR_pm},
  {ID_MINUTE,       FORM_MINUTE},
  {ID_SECOND,       FORM_SECOND},
  {ID_MICROSECOND,  FORM_MICROSECOND},
  {ID_NUM_BLANK,    FORM_NUM_BLANK},
  {ID_NUM_NULL,     FORM_NUM_NULL},
  {ID_NUM_ASTERISK, FORM_NUM_ASTERISK},
  {ID_CHAR_ANY,     FORM_CHAR_ANY},
  {ID_CHAR_UPPER,   FORM_CHAR_UPPER},
  {ID_CHAR_LOWER,   FORM_CHAR_LOWER}
};

/****** FUNCTIONS ************************************************************/

#if ! ANSI
EXTERN CHAR *ecvt _((DOUBLE d, INT ndig, INT *dec, INT *sign));
#endif

LOCAL LONG  get_range    _((CHAR *src, SHORT len, LONG low, LONG high));
LOCAL VOID  build_word   _((SHORT w, CHAR *s));
LOCAL VOID  build_long   _((LONG l, CHAR *s));
LOCAL VOID  build_float  _((CHAR *src, CHAR *dst));

LOCAL INT   get_id       _((SHORT type, CHAR *formatstr, INT *idlen));
LOCAL BOOL  test_id      _((CHAR *formatstr, CHAR *id, INT *idlen));

LOCAL VOID  format_char  _((SHORT type, CHAR *src, CHAR *dst, FORM *format));
LOCAL VOID  format_num   _((SHORT type, CHAR *src, CHAR *dst, FORM *format));
LOCAL VOID  format_stamp _((SHORT type, CHAR *src, CHAR *dst, FORM *format));

LOCAL BOOL  scan_char    _((SHORT type, CHAR *src, CHAR *dst, FORM *format));
LOCAL BOOL  scan_num     _((SHORT type, CHAR *src, CHAR *dst, FORM *format));
LOCAL BOOL  scan_stamp   _((SHORT type, CHAR *src, CHAR *dst, FORM *format));

LOCAL INT   month2bin    _((CHAR *s));
LOCAL VOID  dtoa         _((DOUBLE d, CHAR *s, INT digits));

LOCAL SHORT FAR PASCAL cmp_char    _((HPVOID elt1, HPVOID elt2));
LOCAL SHORT FAR PASCAL cmp_word    _((HPVOID elt1, HPVOID elt2));
LOCAL SHORT FAR PASCAL cmp_long    _((HPVOID elt1, HPVOID elt2));
LOCAL SHORT FAR PASCAL cmp_float   _((HPVOID elt1, HPVOID elt2));
LOCAL SHORT FAR PASCAL cmp_cfloat  _((HPVOID elt1, HPVOID elt2));
LOCAL SHORT FAR PASCAL cmp_date    _((HPVOID elt1, HPVOID elt2));
LOCAL SHORT FAR PASCAL cmp_time    _((HPVOID elt1, HPVOID elt2));
LOCAL SHORT FAR PASCAL cmp_stamp   _((HPVOID elt1, HPVOID elt2));
LOCAL SHORT FAR PASCAL cmp_varbyte _((HPVOID elt1, HPVOID elt2));
LOCAL SHORT FAR PASCAL cmp_blob    _((HPVOID elt1, HPVOID elt2));

LOCAL LONG  FAR PASCAL size_char   _((HPVOID elt));
LOCAL LONG  FAR PASCAL size_word   _((HPVOID elt));
LOCAL LONG  FAR PASCAL size_long   _((HPVOID elt));
LOCAL LONG  FAR PASCAL size_float  _((HPVOID elt));
LOCAL LONG  FAR PASCAL size_cfloat _((HPVOID elt));
LOCAL LONG  FAR PASCAL size_date   _((HPVOID elt));
LOCAL LONG  FAR PASCAL size_time   _((HPVOID elt));
LOCAL LONG  FAR PASCAL size_stamp  _((HPVOID elt));
LOCAL LONG  FAR PASCAL size_byte   _((HPVOID elt));
LOCAL LONG  FAR PASCAL size_blob   _((HPVOID elt));

LOCAL CMP_FUNC cmp_funcs [NUM_TYPES] =
{
  cmp_char,
  cmp_word,
  cmp_long,
  cmp_float,
  cmp_cfloat,
  cmp_date,
  cmp_time,
  cmp_stamp,
  cmp_varbyte,
  cmp_varbyte,
  cmp_varbyte,
  cmp_varbyte,
  cmp_char,
  cmp_long,
  cmp_blob
};

LOCAL SIZE_FUNC size_funcs [NUM_TYPES] =
{
  size_char,
  size_word,
  size_long,
  size_float,
  size_cfloat,
  size_date,
  size_time,
  size_stamp,
  size_byte,
  size_byte,
  size_byte,
  size_byte,
  size_char,
  size_long,
  size_blob
};

/*****************************************************************************/

GLOBAL VOID WINAPI init_conv (short_month, long_month, dec_sep, thsnd_sep)
LPSTR FAR *short_month;
LPSTR FAR *long_month;
INT  dec_sep, thsnd_sep;

{
  DATE date;

#if defined (_WINDLL) && ! defined (WIN32)
  if (month_short == NULL)
#endif
  {
    month_short = short_month;
    month_long  = long_month;
    sep_dec     = (CHAR)dec_sep;
    sep_thsnd   = (CHAR)thsnd_sep;

    mem_set (&null_double, 0xFF, (USHORT)sizeof (DOUBLE));
    mem_set (&null_date, 0xFF, (USHORT)sizeof (DATE));
    mem_set (&null_time, 0xFF, (USHORT)sizeof (TIME));
    mem_set (&null_stamp, 0xFF, (USHORT)sizeof (TIMESTAMP));

    get_date (&date);

    century = date.year / 100 * 100;
  } /* if */
} /* init_conv */

/*****************************************************************************/

GLOBAL LONG WINAPI typesize (type, elt)
SHORT  type;
HPVOID elt;

{
  SIZE_FUNC size_func;

  type      &= ~ USE_WILDCARD;                  /* wildcard not used */
  size_func  = size_funcs [type];

  return (size_func == NULL ? 0L : (*size_func) (elt));
} /* typesize */

/*****************************************************************************/

GLOBAL SHORT WINAPI keysize (type, elt)
SHORT  type;
HPVOID elt;

{
  type &= ~ USE_WILDCARD;                       /* wildcard not used */

  if ((TYPE_VARBYTE <= type) && (type <= TYPE_PICTURE))
    return ((SHORT)sizeof (LONG));              /* use length of data */
  else
    if (type == TYPE_BLOB)
      return ((SHORT)(sizeof (BLOBNAME) + sizeof (BLOBEXT) + MAX_LDIGITS));
    else
      return ((SHORT)typesize (type, elt));     /* one key cannot be larger than PAGESIZE */
} /* keysize */

/*****************************************************************************/

GLOBAL SHORT WINAPI cmp_vals (type, elt1, elt2)
SHORT  type;
HPVOID elt1, elt2;

{
  CMP_FUNC cmp_func;
  BOOL     wild;

  wild      = (type & USE_WILDCARD) != 0;
  type     &= ~ USE_WILDCARD;
  cmp_func  = (wild && HASWILD (type)) ? (CMP_FUNC)str_match : cmp_funcs [type];

  return ((SHORT)(cmp_func == NULL ? 0 : (*cmp_func) (elt1, elt2)));
} /* cmp_vals */

/*****************************************************************************/

GLOBAL BOOL WINAPI set_type (type, cmp_func, size_func)
SHORT     type;
CMP_FUNC  cmp_func;
SIZE_FUNC size_func;

{
  BOOL ok;

  ok = ((TYPE_CHAR <= type) && (type < NUM_TYPES));

  if (ok)
  {
    cmp_funcs [type]  = cmp_func;
    size_funcs [type] = size_func;
  } /* if */

  return (ok);
} /* set_type */

/*****************************************************************************/

GLOBAL BOOL WINAPI str_type (type, name)
SHORT type;
LPSTR name;

{
  BOOL ok;

  strcpy (name, "");

  ok = ((TYPE_CHAR <= type) && (type < NUM_TYPES));

  if (ok)
  {
    switch (type)
    {
      case TYPE_CHAR      : strcpy (name, "TEXT");  break;
      case TYPE_WORD      : strcpy (name, "WORD");  break;
      case TYPE_LONG      : strcpy (name, "LONG");  break;
      case TYPE_FLOAT     : strcpy (name, "FLOAT"); break;
      case TYPE_CFLOAT    : strcpy (name, "CFLOA"); break;
      case TYPE_DATE      : strcpy (name, "DATE");  break;
      case TYPE_TIME      : strcpy (name, "TIME");  break;
      case TYPE_TIMESTAMP : strcpy (name, "TSTMP"); break;
      case TYPE_VARBYTE   : strcpy (name, "BLOB");  break;
      case TYPE_VARWORD   : strcpy (name, "BLOB");  break;
      case TYPE_VARLONG   : strcpy (name, "BLOB");  break;
      case TYPE_PICTURE   : strcpy (name, "PIC");   break;
      case TYPE_EXTERN    : strcpy (name, "FILE");  break;
      case TYPE_DBADDRESS : strcpy (name, "DBADR"); break;
      case TYPE_BLOB      : strcpy (name, "BLOB");  break;
      default             : strcpy (name, "?????"); break;
    } /* switch */
  } /* if */

  return (ok);
} /* str_type */

/*****************************************************************************/

GLOBAL BOOL WINAPI str2bin (type, src, dst)
SHORT  type;
HPCHAR src;
HPVOID dst;

{
  BOOL      result;
  LONG      l;
  DATE      *date;
  TIME      *time;
  TIMESTAMP *timestamp;
  CHAR      s [13];     /* for holding a date or a time */

  result = TRUE;

  set_null (type, dst);                 /* set special NULL value */

  switch (type)
  {
    case TYPE_CHAR      : hstrcpy ((CHAR *)dst, (CHAR *)src);
                          break;
    case TYPE_WORD      : if (*src != EOS)
                          {
                            l = atol ((CHAR *)src);
                            *(SHORT *)dst = (SHORT)l;
                          } /* if */
                          break;
    case TYPE_LONG      : if (*src != EOS) *(LONG *)dst = atol ((CHAR *)src);
                          break;
    case TYPE_FLOAT     : if (*src != EOS) *(DOUBLE *)dst = atof ((CHAR *)src);
                          break;
    case TYPE_CFLOAT    : hstrcpy ((CHAR *)dst, (CHAR *)src);
                          break;
    case TYPE_DATE      : if (*src != EOS)
                          {
                            date = (DATE *)dst;
                            mem_set (date, 0, sizeof (DATE));

                            if (strlen ((CHAR *)src) >= 2) date->day   = (UCHAR)get_range ((CHAR *)src,     2, 1L,   31L);
                            if (strlen ((CHAR *)src) >= 4) date->month = (UCHAR)get_range ((CHAR *)src + 2, 2, 1L,   12L);
                            if (strlen ((CHAR *)src) >= 8)
                              date->year = (USHORT)get_range ((CHAR *)src + 4, 4, 1L, 9999L);
                            else
                              if (strlen ((CHAR *)src) >= 6)
                                date->year = (USHORT)(century + (USHORT)get_range ((CHAR *)src + 4, 2, 0L, 99L));

                            if ((date->day   == 0) &&
                                (date->month == 0) &&
                                (date->year  == 0)) result = FALSE;
                          } /* if */
                          break;
    case TYPE_TIME      : if (*src != EOS)
                          {
                            time = (TIME *)dst;
                            mem_set (time, 0, sizeof (TIME));

                            if (strlen ((CHAR *)src) >=  2) time->hour   = (UCHAR)get_range ((CHAR *)src,     2, 0L,     23L);
                            if (strlen ((CHAR *)src) >=  4) time->minute = (UCHAR)get_range ((CHAR *)src + 2, 2, 0L,     59L);
                            if (strlen ((CHAR *)src) >=  6) time->second = (UCHAR)get_range ((CHAR *)src + 4, 2, 0L,     59L);
                            if (strlen ((CHAR *)src) >= 12) time->micro  = get_range ((CHAR *)src + 6, 6, 0L, 999999L);
                          } /* if */
                          break;
    case TYPE_TIMESTAMP : if (*src != EOS)
                          {
                            timestamp = (TIMESTAMP *)dst;
                            result    = FALSE;
                            mem_set (timestamp, 0, sizeof (TIMESTAMP));

                            strncpy (s, (CHAR *)src, 8);
                            s [8]  = EOS;
                            result = str2bin (TYPE_DATE, s, &timestamp->date);

                            if (result && (strlen ((CHAR *)src) > 8))
                            {
                              strncpy (s, (CHAR *)src + 8, 12);
                              s [12] = EOS;
                              result = str2bin (TYPE_TIME, s, &timestamp->time);
                            } /* if */
                          } /* if */
                          break;
    case TYPE_VARBYTE   :
    case TYPE_VARWORD   :
    case TYPE_VARLONG   :
    case TYPE_PICTURE   : if (*src != EOS) *(LONG *)dst = atol ((CHAR *)src);
                          break;
    case TYPE_EXTERN    : hstrcpy ((CHAR *)dst, (CHAR *)src);
                          break;
    case TYPE_DBADDRESS : if (*src != EOS) *(LONG *)dst = atol ((CHAR *)src);
                          break;
    case TYPE_BLOB      : if (*src != EOS) hstrcpy (((BLOB *)dst)->name, (CHAR *)src);
                          break;
    default             : result = FALSE;
                          break;
  } /* switch */

  return (result);
} /* str2bin */

/*****************************************************************************/

GLOBAL BOOL WINAPI bin2str (type, src, dst)
SHORT  type;
HPVOID src;
HPCHAR dst;

{
  BOOL      result;
  DATE      *date;
  TIME      *time;
  TIMESTAMP *timestamp;

  result = TRUE;
  *dst   = EOS;

  switch (type)
  {
    case TYPE_CHAR      : hstrcpy ((CHAR *)dst, (CHAR *)src);
                          break;
    case TYPE_WORD      : if (! is_null (type, src))
                          sprintf ((CHAR *)dst, "%ld", (LONG)(*(SHORT *)src));
                          break;
    case TYPE_LONG      : if (! is_null (type, src))
                          sprintf ((CHAR *)dst, "%ld", *(LONG *)src);
                          break;
    case TYPE_FLOAT     : if (! is_null (type, src))
                          dtoa (*(DOUBLE *)src, (CHAR *)dst, MAX_RDIGITS);
                          break;
    case TYPE_CFLOAT    : hstrcpy ((CHAR *)dst, (CHAR *)src);
                          break;
    case TYPE_DATE      : if (! is_null (type, src))
                          {
                            date = (DATE *)src;
                            sprintf ((CHAR *)dst, "%02d%02d%04d", date->day, date->month, date->year);
                          } /* if */
                          break;
    case TYPE_TIME      : if (! is_null (type, src))
                          {
                            time = (TIME *)src;
                            sprintf ((CHAR *)dst, "%02d%02d%02d%06ld", time->hour, time->minute, time->second, time->micro);
                          } /* if */
                          break;
    case TYPE_TIMESTAMP : if (! is_null (type, src))
                          {
                            timestamp = (TIMESTAMP *)src;
                            sprintf ((CHAR *)dst, "%02d%02d%04d%02d%02d%02d%06ld",
                                     timestamp->date.day,
                                     timestamp->date.month,
                                     timestamp->date.year,
                                     timestamp->time.hour,
                                     timestamp->time.minute,
                                     timestamp->time.second,
                                     timestamp->time.micro);
                          } /* if */
                          break;
    case TYPE_VARBYTE   :
    case TYPE_VARWORD   :
    case TYPE_VARLONG   :
    case TYPE_PICTURE   : if (! is_null (type, src))
                            sprintf ((CHAR *)dst, "%ld", *(LONG *)src);
                          break;
    case TYPE_EXTERN    : hstrcpy ((CHAR *)dst, (CHAR *)src);
                          break;
    case TYPE_DBADDRESS : if (! is_null (type, src))
                            sprintf ((CHAR *)dst, "%ld", *(LONG *)src);
                          break;
    case TYPE_BLOB      : hstrcpy ((CHAR *)dst, ((BLOB *)src)->name);
                          break;
    default             : result = FALSE;
                          break;
  } /* switch */

  return (result);
} /* bin2str */

/*****************************************************************************/

GLOBAL VOID WINAPI str2format (type, strval, format)
SHORT  type;
LPSTR  strval;
LPFORM format;

{
  CHAR src [256];

  if ((strval [0] != EOS) && (format [0].letter != EOS)) /* there is a source string and any format */
  {
    strcpy (src, strval);

    switch (type)
    {
      case TYPE_CHAR      :
      case TYPE_EXTERN    : format_char (type, src, strval, format);
                            break;
      case TYPE_WORD      :
      case TYPE_LONG      :
      case TYPE_FLOAT     :
      case TYPE_CFLOAT    :
      case TYPE_DBADDRESS : format_num (type, src, strval, format);
                            break;
      case TYPE_DATE      :
      case TYPE_TIME      :
      case TYPE_TIMESTAMP : format_stamp (type, src, strval, format);
                            break;
    } /* switch */
  } /* if */
} /* str2format */

/*****************************************************************************/

GLOBAL BOOL WINAPI format2str (type, strval, format)
SHORT  type;
LPSTR  strval;
LPFORM format;

{
  BOOL ok;
  CHAR src [256];

  ok = TRUE;

  if ((strval [0] != EOS) && (format [0].letter != EOS)) /* there is a source string and any format */
  {
    strcpy (src, strval);

    switch (type)
    {
      case TYPE_CHAR      :
      case TYPE_EXTERN    : ok = scan_char (type, src, strval, format);
                            break;
      case TYPE_WORD      :
      case TYPE_LONG      :
      case TYPE_FLOAT     :
      case TYPE_CFLOAT    :
      case TYPE_DBADDRESS : ok = scan_num (type, src, strval, format);
                            break;
      case TYPE_DATE      :
      case TYPE_TIME      :
      case TYPE_TIMESTAMP : ok = scan_stamp (type, src, strval, format);
                            break;
    } /* switch */
  } /* if */

  return (ok);
} /* format2str */

/*****************************************************************************/

GLOBAL VOID WINAPI build_format (type, formatstr, format)
SHORT  type;
LPSTR  formatstr;
LPFORM format;

{
  REG INT sinx, dinx, len, id;
  INT     idlen;

  sinx = dinx = 0;
  len  = (SHORT)strlen (formatstr);

  while (sinx < len)
  {
    id = get_id (type, formatstr + sinx, &idlen);

    format [dinx].letter = formatstr [sinx];
    format [dinx].id     = (UCHAR)id;

    sinx += idlen;
    dinx++;
  } /* while */

  format [dinx].letter = EOS;
  format [dinx].id     = ID_NOCHAR;
} /* build_format */

/*****************************************************************************/
/* The value NULL is described as follows:                                   */
/* DATATYPE                         VALUE                                    */
/* CHAR, CFLOAT, EXTERN           : the empty string (*dst = EOS)            */
/* WORD                           : MIN_WORD (-32768)                        */
/* LONG, MONEY, DBADDRESS         : MIN_LONG (-2147483648)                   */
/* FLOAT, DATE, TIME, TIMESTAMP   : sizeof (type) filled with 0xFF           */
/* VARBYTE, -WORD, -LONG, PICTURE : first LONG (length of buffer) = 0        */
/* BLOB                           : name, ext and size = 0                   */
/*****************************************************************************/

GLOBAL VOID WINAPI set_null (type, dst)
SHORT  type;
HPVOID dst;

{
  switch (type)
  {
    case TYPE_CHAR      : *(HPCHAR)dst = EOS;                                break;
    case TYPE_WORD      : *(HPSHORT)dst = MIN_WORD;                          break;
    case TYPE_LONG      : *(HPLONG)dst = MIN_LONG;                           break;
    case TYPE_FLOAT     : mem_set (dst, 0xFF, (USHORT)sizeof (DOUBLE));      break;
    case TYPE_CFLOAT    : *(HPCHAR)dst = EOS;                                break;
    case TYPE_DATE      : mem_set (dst, 0xFF, (USHORT)sizeof (DATE));        break;
    case TYPE_TIME      : mem_set (dst, 0xFF, (USHORT)sizeof (TIME));        break;
    case TYPE_TIMESTAMP : mem_set (dst, 0xFF, (USHORT)sizeof (TIMESTAMP));   break;
    case TYPE_VARBYTE   : *(HPLONG)dst = 0;                                  break;
    case TYPE_VARWORD   : *(HPLONG)dst = 0;                                  break;
    case TYPE_VARLONG   : *(HPLONG)dst = 0;                                  break;
    case TYPE_PICTURE   : *(HPLONG)dst = 0;                                  break;
    case TYPE_EXTERN    : *(HPCHAR)dst = EOS;                                break;
    case TYPE_DBADDRESS : *(HPLONG)dst = MIN_LONG;                           break;
    case TYPE_BLOB      : mem_set (dst, 0, (USHORT)sizeof (BLOB));           break;
  } /* switch */
} /* set_null */

/*****************************************************************************/

GLOBAL VOID WINAPI set_default (type, dst)
SHORT  type;
HPVOID dst;

{
  switch (type)
  {
    case TYPE_CHAR      : *(HPCHAR)dst = EOS;                      break;
    case TYPE_WORD      : *(HPSHORT)dst = 0;                       break;
    case TYPE_LONG      : *(HPLONG)dst = 0;                        break;
    case TYPE_FLOAT     : *(HPDOUBLE)dst = 0;                      break;
    case TYPE_CFLOAT    : hstrcpy ((HPCHAR)dst, "0");              break;
    case TYPE_DATE      : get_date ((HPDATE)dst);                  break;
    case TYPE_TIME      : get_time ((HPTIME)dst);                  break;
    case TYPE_TIMESTAMP : get_tstamp ((HPTIMESTAMP)dst);           break;
    case TYPE_VARBYTE   : *(HPLONG)dst = 0;                        break;
    case TYPE_VARWORD   : *(HPLONG)dst = 0;                        break;
    case TYPE_VARLONG   : *(HPLONG)dst = 0;                        break;
    case TYPE_PICTURE   : *(HPLONG)dst = 0;                        break;
    case TYPE_EXTERN    : *(HPCHAR)dst = EOS;                      break;
    case TYPE_DBADDRESS : *(HPLONG)dst = 0;                        break;
    case TYPE_BLOB      : mem_set (dst, 0, (USHORT)sizeof (BLOB)); break;
  } /* switch */
} /* set_default */

/*****************************************************************************/

GLOBAL BOOL WINAPI is_null (type, src)
SHORT  type;
HPVOID src;

{
  BOOL isnull;
  BLOB *blob;

  isnull = TRUE;

  switch (type)
  {
    case TYPE_CHAR      : isnull = *(HPCHAR)src == EOS;                                        break;
    case TYPE_WORD      : isnull = *(HPSHORT)src == MIN_WORD;                                  break;
    case TYPE_LONG      : isnull = *(HPLONG)src == MIN_LONG;                                   break;
    case TYPE_FLOAT     : isnull = memcmp ((VOID *)src, &null_double, sizeof (DOUBLE)) == 0;   break;
    case TYPE_CFLOAT    : isnull = *(HPCHAR)src == EOS;                                        break;
    case TYPE_DATE      : isnull = memcmp ((VOID *)src, &null_date, sizeof (DATE)) == 0;       break;
    case TYPE_TIME      : isnull = memcmp ((VOID *)src, &null_time, sizeof (TIME)) == 0;       break;
    case TYPE_TIMESTAMP : isnull = memcmp ((VOID *)src, &null_stamp, sizeof (TIMESTAMP)) == 0; break;
    case TYPE_VARBYTE   : isnull = *(HPLONG)src == 0;                                          break;
    case TYPE_VARWORD   : isnull = *(HPLONG)src == 0;                                          break;
    case TYPE_VARLONG   : isnull = *(HPLONG)src == 0;                                          break;
    case TYPE_PICTURE   : isnull = *(HPLONG)src == 0;                                          break;
    case TYPE_EXTERN    : isnull = *(HPCHAR)src == EOS;                                        break;
    case TYPE_DBADDRESS : isnull = *(HPLONG)src == MIN_LONG;                                   break;
    case TYPE_BLOB      : blob   = (BLOB *)src;
                          isnull = (blob->name [0] == EOS) && (blob->ext [0] == EOS) && (blob->size == 0);
                          break;
  } /* switch */

  return (isnull);
} /* is_null */

/*****************************************************************************/

GLOBAL VOID WINAPI build_str (type, len, src, dst)
SHORT  type;
INT    len;
HPVOID src;
HPCHAR dst;

{
  CHAR      s [2 * MAX_RDIGITS + 3];
  DATE      *date;
  TIMESTAMP *timestamp;
  BLOB      *blob;

  dst [0] = EOS;

  switch (type)
  {
    case TYPE_CHAR      :
    case TYPE_EXTERN    : sprintf ((CHAR *)dst, "%-*.*s", len, len, src);
                          break;
    case TYPE_WORD      : build_word (*(SHORT *)src, (CHAR *)dst);
                          break;
    case TYPE_LONG      :
    case TYPE_VARBYTE   :
    case TYPE_VARWORD   :
    case TYPE_VARLONG   :
    case TYPE_PICTURE   :
    case TYPE_DBADDRESS : build_long (*(LONG *)src, (CHAR *)dst);
                          break;
    case TYPE_FLOAT     :
    case TYPE_CFLOAT    : bin2str (type, src, s);
                          build_float (s, (CHAR *)dst);
                          break;
    case TYPE_DATE      : if (is_null (type, src))
                          {
                            mem_set (dst, ' ', 8);
                            dst [8] = EOS;
                          } /* if */
                          else
                          {
                            date = (DATE *)src;
                            sprintf ((CHAR *)dst, "%04d%02d%02d", date->year, date->month, date->day);
                          } /* else */
                          break;
    case TYPE_TIME      : if (is_null (type, src))
                          {
                            mem_set (dst, ' ', 12);
                            dst [12] = EOS;
                          } /* if */
                          else
                            bin2str (type, src, dst);
                          break;
    case TYPE_TIMESTAMP : if (is_null (type, src))
                          {
                            mem_set (dst, ' ', 20);
                            dst [20] = EOS;
                          } /* if */
                          else
                          {
                            timestamp = (TIMESTAMP *)src;
                            sprintf ((CHAR *)dst, "%04d%02d%02d%02d%02d%02d%06ld",
                                     timestamp->date.year,
                                     timestamp->date.month,
                                     timestamp->date.day,
                                     timestamp->time.hour,
                                     timestamp->time.minute,
                                     timestamp->time.second,
                                     timestamp->time.micro);
                          } /* else */
                          break;
    case TYPE_BLOB      : blob = (BLOB *)src;
                          sprintf ((CHAR *)dst, "%-*.*s%-*.*s%*ld", MAX_BLOBNAME, MAX_BLOBNAME, blob->name, MAX_BLOBEXT, MAX_BLOBEXT, blob->ext, MAX_LDIGITS, blob->size);
                          break;
  } /* switch */
} /* build_str */

/*****************************************************************************/

LOCAL LONG get_range (src, len, low, high)
CHAR  *src;
SHORT len;
LONG  low, high;

{
  CHAR s [21];
  LONG l;

  l = 0;
  strncpy (s, src, 20);
  s [len] = EOS;

  if (s [0] != EOS)
  {
    l = atol (s);
    if ((l < low) || (l > high)) l = 0;
  } /* if */

  return (l);
} /* get_range */

/*****************************************************************************/

LOCAL VOID build_word (w, s)
SHORT w;
CHAR  *s;

{
  INT  sign;
  CHAR str [MAX_WDIGITS + 1];

  sign = (w < 0) ? -1 : 1;
  w    = (SHORT)abs (w);

  bin2str (TYPE_WORD, &w, str);
  mem_set (s, ' ', MAX_WDIGITS);
  s [0]           = (CHAR)((sign < 0) ? ' ' : '+');     /* '-' isn't ascii-smaller than '+' */
  s [MAX_WDIGITS] = EOS;

  if (! is_null (TYPE_WORD, &w)) strcpy (s + MAX_WDIGITS - strlen (str), str);
} /* build_word */

/*****************************************************************************/

LOCAL VOID build_long (l, s)
LONG l;
CHAR *s;

{
  INT  sign;
  CHAR str [MAX_LDIGITS + 1];

  sign = (l < 0) ? -1 : 1;
  l    = labs (l);

  bin2str (TYPE_LONG, &l, str);
  mem_set (s, ' ', MAX_LDIGITS);
  s [0]           = (CHAR)((sign < 0) ? ' ' : '+');     /* '-' isn't ascii-smaller than '+' */
  s [MAX_LDIGITS] = EOS;

  if (! is_null (TYPE_LONG, &l)) strcpy (s + MAX_LDIGITS - strlen (str), str);
} /* build_long */

/*****************************************************************************/

LOCAL VOID build_float (src, dst)
CHAR *src, *dst;

{
  CHAR *p;
  SHORT digits;

  if (is_null (TYPE_CHAR, src))
  {
    mem_set (dst, ' ', MAX_RDIGITS * 2 + 2);
    dst [2 * MAX_RDIGITS + 2] = EOS;
  } /* if */
  else
  {
    mem_set (dst, '0', MAX_RDIGITS * 2 + 2);
    dst [2 * MAX_RDIGITS + 2] = EOS;

    if (*src == '-')
    {
      *dst++ = ' ';
      src++;
    } /* if */
    else
      *dst++ = '+';

    p = strchr (src, '.');

    if (p == NULL)
    {
      dst [MAX_RDIGITS] = '.';
      mem_move (dst + MAX_RDIGITS - strlen (src), src, (USHORT)strlen (src));
    } /* if */
    else
    {
      digits = (SHORT)(p - src);
      mem_move (dst + MAX_RDIGITS - digits, src, (USHORT)strlen (src));
    } /* else */
  } /* else */
} /* build_float */

/*****************************************************************************/

LOCAL INT get_id (type, formatstr, idlen)
SHORT type;
CHAR  *formatstr;
INT   *idlen;

{
  INT id, i, from, to;

  id   = ID_ACHAR;
  from = ID_NOCHAR;
  to   = from - 1;              /* prevent going into loop */

  switch (type)
  {
    case TYPE_CHAR      :
    case TYPE_EXTERN    : from = ID_NUM_BLANK;
                          to   = ID_CHAR_LOWER;
                          break;
    case TYPE_WORD      :
    case TYPE_LONG      :
    case TYPE_FLOAT     :
    case TYPE_CFLOAT    :
    case TYPE_DBADDRESS : from = ID_NUM_BLANK;
                          to   = ID_NUM_ASTERISK;
                          break;
    case TYPE_DATE      :
    case TYPE_TIME      :
    case TYPE_TIMESTAMP : from = ID_DAY;
                          to   = ID_MICROSECOND;
                          break;
  } /* switch */

  for (i = from; i <= to; i++)
    if (test_id (formatstr, formats [i].idstr, idlen))
    {
      id = formats [i].id;
      break;
    } /* if, for */

  if (id == ID_ACHAR) *idlen = 1;

  return (id);
} /* get_id */

/*****************************************************************************/

LOCAL BOOL test_id (formatstr, id, idlen)
CHAR *formatstr, *id;
INT  *idlen;

{
  BOOL   res;
  SIZE_T len;

  len    = strlen (id);
  res    = strncmp (formatstr, id, len) == 0;
  *idlen = (INT)len;

  return (res);
} /* test_id */

/*****************************************************************************/

LOCAL VOID format_char (type, src, dst, format)
SHORT type;
CHAR  *src, *dst;
FORM  *format;

{
  REG INT  sinx, dinx, maxsinx;
  REG FORM *f;

  maxsinx = strlen (src) - 1;

  for (f = format, sinx = dinx = 0; f->letter != EOS; f++)
    switch (f->id)
    {
      case ID_ACHAR        : dst [dinx++] = f->letter;
                             break;
      case ID_NUM_BLANK    : dst [dinx++] = (CHAR)((sinx <= maxsinx) ? src [sinx++] : ' ');
                             break;
      case ID_NUM_NULL     : dst [dinx++] = (CHAR)((sinx <= maxsinx) ? src [sinx++] : '0');
                             break;
      case ID_NUM_ASTERISK : dst [dinx++] = (CHAR)((sinx <= maxsinx) ? src [sinx++] : '*');
                             break;
      case ID_CHAR_ANY     : dst [dinx++] = (CHAR)((sinx <= maxsinx) ? src [sinx++] : ' ');
                             break;
      case ID_CHAR_UPPER   : dst [dinx++] = (CHAR)((sinx <= maxsinx) ? ch_upper (src [sinx++]) : ' ');
                             break;
      case ID_CHAR_LOWER   : dst [dinx++] = (CHAR)((sinx <= maxsinx) ? ch_lower (src [sinx++]) : ' ');
                             break;
    } /* switch, for */

  dst [dinx] = EOS;
} /* format_char */

/*****************************************************************************/

LOCAL VOID format_num (type, src, dst, format)
SHORT type;
CHAR  *src, *dst;
FORM  *format;

{
  REG INT  sinx, dinx, i;
  REG FORM *f;
  INT      len, srcper, formper, flen, maxsinx;
  INT      srcdigits, formdigits;
  CHAR     *p;
  CHAR     pad;
  CHAR     digitstr [MAX_FORMATSTR + 1];
  DOUBLE   d;

  pad    = ' ';
  srcper = 0;
  p      = strchr (src, '.');

  if (p == NULL) strcat (src, ".");

  for (i = 0, len = strlen (src); i < len; i++)
    if (src [i] == '.') srcper = i;

  for (f = format, i = 0, formper = FAILURE; f->letter != EOS; f++, i++)
    if (f->letter == sep_dec) formper = i;

  if ((type == TYPE_FLOAT) || (type == TYPE_CFLOAT))
  {
    srcdigits = strlen (src + srcper + 1);

    strcpy (digitstr, FORM_NUM_BLANK);
    strcat (digitstr, FORM_NUM_NULL);
    strcat (digitstr, FORM_NUM_ASTERISK);

    formdigits = 0;
    if (formper != FAILURE)
      for (f = format + formper + 1; f->letter != EOS; f++)
        if (strchr (digitstr, f->letter) != NULL) formdigits++;

    if (srcdigits > formdigits)                         /* round */
    {
      if (src [srcper + formdigits + 1] >= '5')         /* .5 will not be rounded on all computers */
        src [srcper + formdigits + 1] = '9';            /* for sure, '6' would have been enough to round */

      d = atof (src);
#ifdef _WINDLL
      for (p = src; (*p != '.') && (*p != EOS); p++)
        if (isdigit (*p))
          formdigits++;

      dtoa (d, src, formdigits);
#else
      sprintf (src, "%.*lf", formdigits, d);
#endif

      for (i = 0, len = strlen (src); i < len; i++) /* position of period could have been changed */
        if (src [i] == '.') srcper = i;
    } /* if */
  } /* if */

  for (f = format, flen = 0; f->letter != EOS; f++, flen++)
    dst [flen] = f->letter;

  dst [flen] = EOS;
  maxsinx    = strlen (src) - 1;

  if (formper != FAILURE)
  {
    for (f = &format [formper], sinx = srcper, dinx = formper; f->letter != EOS; f++)
      switch (f->id)
      {
        case ID_ACHAR        : dst [dinx++] = f->letter;
                               sinx++;
                               break;
        case ID_NUM_BLANK    : dst [dinx++] = (CHAR)((sinx <= maxsinx) ? src [sinx++] : ' ');
                               break;
        case ID_NUM_NULL     : dst [dinx++] = (CHAR)((sinx <= maxsinx) ? src [sinx++] : '0');
                               break;
        case ID_NUM_ASTERISK : dst [dinx++] = (CHAR)((sinx <= maxsinx) ? src [sinx++] : '*');
                               break;
      } /* switch, for, if */

    dst [dinx] = EOS;
    flen       = formper;                       /* use first char left to digit delimiter */
  } /* if */

  flen--;                                       /* go to last char */

  for (f = &format [flen], sinx = srcper - 1, dinx = flen; flen >= 0; flen--, f--)
    switch (f->id)
    {
      case ID_ACHAR        : dst [dinx--] = (CHAR)((sinx < 0) && (f->letter == sep_thsnd) ? pad : (sinx == 0) && (src [sinx] == '-') && (f->letter == sep_thsnd) ? src [sinx--] : f->letter);
                             break;
      case ID_NUM_BLANK    : dst [dinx--] = (CHAR)((sinx >= 0) ? src [sinx--] : ' ');
                             pad = ' ';
                             break;
      case ID_NUM_NULL     : dst [dinx--] = (CHAR)((sinx >= 0) ? src [sinx--] : '0');
                             pad = '0';
                             break;
      case ID_NUM_ASTERISK : dst [dinx--] = (CHAR)((sinx >= 0) ? src [sinx--] : '*');
                             pad = '*';
                             break;
    } /* switch, for */

  dinx = strlen (dst);

  if (sinx >= 0) mem_set (dst, OVERFLOW_CHAR, (USHORT)dinx);
} /* format_num */

/*****************************************************************************/

LOCAL VOID format_stamp (type, src, dst, format)
SHORT type;
CHAR  *src, *dst;
FORM  *format;

{
  REG INT  dinx, minx;
  REG FORM *f;
  INT      month, hour, inxminute, inxsecond;
  CHAR     s [3];
  CHAR     *moshort, *molong;
  BOOL     hour12, am, hashour, hasminute, hassecond;

  if (type == TYPE_TIME)
  {
    src     -= 8;               /* 8 = index of hour in timestamp */
    moshort  = molong = NULL;
  } /* if */
  else
  {
    strncpy (s, src + 2, 2);    /* get number of month */
    s [2]   = EOS;
    month   = atoi (s);
    moshort = (month_short != NULL) && (month != 0) ? month_short [month - 1] : NULL;
    molong  = (month_long != NULL) && (month != 0) ? month_long [month - 1] : NULL;
  } /* else */

  strncpy (s, src + 8, 2);
  s [2]     = EOS;
  hour      = atoi (s);
  am        = hour < 12;
  hour12    = hashour = hasminute = hassecond = FALSE;
  inxsecond = inxminute = 0;

  for (f = format; f->letter != EOS; f++)
    if ((ID_HOUR_AM <= f->id) && (f->id <= ID_HOUR_pm)) hour12 = TRUE;

  for (f = format, dinx = minx = 0; f->letter != EOS; f++)
    switch (f->id)
    {
      case ID_ACHAR       : dst [dinx++] = f->letter;
                            break;
      case ID_DAY         : strncpy (dst + dinx, src, 2);
                            dinx += 2;
                            break;
      case ID_MONTH_LONG  : if (molong != NULL)
                            {
                              strcpy (dst + dinx, molong);
                              dinx += strlen (molong);
                            } /* if */
                            break;
      case ID_MONTH_SHORT : if (molong != NULL)
                            {
                              strcpy (dst + dinx, moshort);
                              dinx += strlen (moshort);
                            } /* if */
                            break;
      case ID_MONTH_NUM   : strncpy (dst + dinx, src + 2, 2);
                            dinx += 2;
                            break;
      case ID_YEAR_LONG   : strncpy (dst + dinx, src + 4, 4);
                            dinx += 4;
                            break;
      case ID_YEAR_SHORT  : strncpy (dst + dinx, src + 6, 2);
                            dinx += 2;
                            break;
      case ID_HOUR_24     : if (! hour12)
                              strncpy (dst + dinx, src + 8, 2);
                            else
                              if (am)
                                sprintf (dst + dinx, "%2d", (hour == 0) ? 12 : hour);
                              else
                                sprintf (dst + dinx, "%2d", (hour == 12) ? 12 : hour - 12);
                            hashour  = TRUE;
                            dinx    += 2;
                            break;
      case ID_HOUR_AM     :
      case ID_HOUR_PM     :
      case ID_HOUR_am     :
      case ID_HOUR_pm     : sprintf (dst + dinx, isupper (f->letter) ? (am ? FORM_HOUR_AM : FORM_HOUR_PM) : (am ? FORM_HOUR_am : FORM_HOUR_pm));
                            dinx += 2;
                            break;
      case ID_MINUTE      : strncpy (dst + dinx, src + 10, 2);
                            hasminute  = TRUE;
                            inxminute  = dinx;
                            dinx      += 2;
                            break;
      case ID_SECOND      : strncpy (dst + dinx, src + 12, 2);
                            hassecond  = TRUE;
                            inxsecond  = dinx;
                            dinx      += 2;
                            break;
      case ID_MICROSECOND : if (minx < 6) dst [dinx++] = src [14 + minx++];
                            break;
    } /* switch, for */

  if (! hashour && hasminute && (src [9] == '1'))
    if (dst [inxminute] <= '3') dst [inxminute] += 6;

  if (! hasminute && hassecond && (src [11] == '1'))
    if (dst [inxsecond] <= '3') dst [inxsecond] += 6;

  dst [dinx] = EOS;
} /* format_stamp */

/*****************************************************************************/

LOCAL BOOL scan_char (type, src, dst, format)
SHORT type;
CHAR  *src, *dst;
FORM  *format;

{
  BOOL     ok;
  REG INT  sinx, dinx;
  REG FORM *f;

  ok = TRUE;

  for (f = format, sinx = dinx = 0; (f->letter != EOS) && (src [sinx] != EOS); f++)
    switch (f->id)
    {
      case ID_ACHAR        : if (src [sinx] == f->letter) sinx++;
                             break;
      case ID_NUM_BLANK    : if (! isdigit (src [sinx]) && (src [sinx] != ' '))
                               ok = FALSE;
                             else
                               dst [dinx++] = src [sinx];
                             sinx++;
                             break;
      case ID_NUM_NULL     : if (! isdigit (src [sinx]))
                               ok = FALSE;
                             else
                               dst [dinx++] = src [sinx];
                             sinx++;
                             break;
      case ID_NUM_ASTERISK : if (! isdigit (src [sinx]) && (src [sinx] != '*'))
                               ok = FALSE;
                             else
                               dst [dinx++] = src [sinx];
                             sinx++;
                             break;
      case ID_CHAR_ANY     :
      case ID_CHAR_UPPER   :
      case ID_CHAR_LOWER   : dst [dinx++] = src [sinx++];
                             break;
    } /* switch, for */

  dst [dinx] = EOS;

  return (ok);
} /* scan_char */

/*****************************************************************************/

LOCAL BOOL scan_num (type, src, dst, format)
SHORT type;
CHAR  *src, *dst;
FORM  *format;

{
  BOOL    ok, sepdec, sign;
  REG INT sinx, dinx, c;
  CHAR    *p;

  ok     = TRUE;
  sign   = FALSE;
  sepdec = (type != TYPE_FLOAT) && (type != TYPE_CFLOAT);

  for (sinx = dinx = 0; src [sinx] != EOS; sinx++)
  {
    c = src [sinx];

    if (isdigit (c) || ! sepdec && (c == sep_dec) || ! sign && ((c == '+') || (c == '-')))
    {
      if (c == sep_dec) sepdec = TRUE;
      if ((c == '+') || (c == '-')) sign = TRUE;
      dst [dinx++] = (CHAR)c;
    } /* if */
  } /* for */

  dst [dinx] = EOS;

  p = strchr (dst, sep_dec);
  if (p != NULL) *p = '.';

  return (ok);
} /* scan_num */

/*****************************************************************************/

LOCAL BOOL scan_stamp (type, src, dst, format)
SHORT type;
CHAR *src, *dst;
FORM *format;

{
  BOOL     ok, hour12, am;
  REG INT  sinx, minx;
  REG FORM *f;
  INT      i, inc;
  CHAR     s [MAX_FORMATSTR];
  CHAR     datestr [9];
  DATE     date;
  BOOL     hasday, hasmonth, hasyear;

  ok     = TRUE;
  hour12 = FALSE;
  am     = FALSE;

  get_date (&date);                             /* defaults are actual values */
  bin2str (TYPE_DATE, &date, datestr);
  mem_set (dst, '0', 20);
  dst [20] = EOS;
  hasday   = hasmonth = hasyear = FALSE;

  for (f = format; f->letter != EOS; f++)
    switch (f->id)
    {
      case ID_DAY         : hasday = TRUE;   break;
      case ID_MONTH_LONG  :
      case ID_MONTH_SHORT :
      case ID_MONTH_NUM   : hasmonth = TRUE; break;
      case ID_YEAR_LONG   :
      case ID_YEAR_SHORT  : hasyear = TRUE;  break;
      case ID_HOUR_AM     :
      case ID_HOUR_PM     :
      case ID_HOUR_am     :
      case ID_HOUR_pm     : hour12 = TRUE;   break;
    } /* switch, for */

  for (f = format, sinx = minx = 0; (f->letter != EOS) && (src [sinx] != EOS); f++)
    switch (f->id)
    {
      case ID_ACHAR       : if ((src [sinx] == f->letter) || ! isdigit (src [sinx]) && ! isalpha (ch_ascii (src [sinx]))) sinx++;
                            break;
      case ID_DAY         : if (! isdigit (src [sinx]))
                            {
                              if ((src [sinx] == WILD_CHAR) && (src [sinx + 1] == WILD_CHAR))
                              {
                                strncpy (dst, src + sinx, 2);
                                sinx += 2;
                              } /* if */
                              else
                              {
                                f--;
                                sinx++;
                                ok = FALSE;
                              } /* else */
                            } /* if */
                            else
                              if (isdigit (src [sinx + 1]))
                              {
                                strncpy (dst, src + sinx, 2);
                                sinx += 2;
                              } /* if */
                              else
                              {
                                dst [0] = '0';
                                dst [1] = src [sinx++];
                              } /* else, else */
                            break;
      case ID_MONTH_LONG  :
      case ID_MONTH_SHORT :
      case ID_MONTH_NUM   : if (isalpha (ch_ascii (src [sinx])))
                            {
                              for (i = 0; isalpha (ch_ascii (src [sinx])) && (src [sinx] != EOS); i++)
                                s [i] = src [sinx++];

                              s [i] = EOS;
                              i     = month2bin (s);

                              if (i == 0)
                                ok = FALSE;
                              else
                              {
                                dst [2] = (CHAR)('0' + i / 10);
                                dst [3] = (CHAR)('0' + i % 10);
                              } /* else */
                            } /* if */
                            else
                              if (! isdigit (src [sinx]))
                              {
                                if ((src [sinx] == WILD_CHAR) && (src [sinx + 1] == WILD_CHAR))
                                {
                                  strncpy (dst + 2, src + sinx, 2);
                                  sinx += 2;
                                } /* if */
                                else
                                {
                                  f--;
                                  sinx++;
                                  ok = FALSE;
                                } /* else */
                              } /* if */
                              else
                                if (isdigit (src [sinx + 1]))
                                {
                                  strncpy (dst + 2, src + sinx, 2);
                                  sinx += 2;
                                } /* if */
                                else
                                {
                                  dst [2] = '0';
                                  dst [3] = src [sinx++];
                                } /* else, else, else */
                            break;
      case ID_YEAR_LONG   :
      case ID_YEAR_SHORT  : if (! isdigit (src [sinx]))
                            {
                              if ((src [sinx] == WILD_CHAR) && (src [sinx + 1] == WILD_CHAR))
                              {
                                strncpy (dst + 4, src + sinx, 2);
                                strncpy (dst + 6, src + sinx, 2);
                                sinx += 2;
                              } /* if */
                              else
                              {
                                f--;
                                sinx++;
                                ok = FALSE;
                              } /* else */
                            } /* if */
                            else
                              if (! isdigit (src [sinx + 1]))
                              {
                                strncpy (dst + 7, src + sinx, 1);
                                sinx++;
                              } /* if */
                              else
                                if (! isdigit (src [sinx + 2]) || (f->id == ID_YEAR_SHORT))
                                {
                                  strncpy (dst + 6, src + sinx, 2);
                                  strncpy (dst + 4, datestr + 4, 2);
                                  sinx += 2;
                                } /* if */
                                else
                                  if (! isdigit (src [sinx + 3]))
                                  {
                                    strncpy (dst + 5, src + sinx, 3);
                                    sinx += 3;
                                  } /* if */
                                  else
                                  {
                                    strncpy (dst + 4, src + sinx, 4);
                                    sinx += 4;
                                  } /* else, else, else, else */
                            break;
      case ID_HOUR_24     : if (! isdigit (src [sinx]))
                            {
                              f--;
                              sinx++;
                              ok = FALSE;
                            } /* if */
                            else
                              if (isdigit (src [sinx + 1]))
                              {
                                strncpy (dst + 8, src + sinx, 2);
                                sinx += 2;
                              } /* if */
                              else
                              {
                                dst [8] = '0';
                                dst [9] = src [sinx++];
                              } /* else, else */
                            break;
      case ID_HOUR_AM     :
      case ID_HOUR_PM     :
      case ID_HOUR_am     :
      case ID_HOUR_pm     : am    = toupper (src [sinx]) == 'A';
                            sinx += 2;
                            break;
      case ID_MINUTE      : if (! isdigit (src [sinx]))
                            {
                              f--;
                              sinx++;
                              ok = FALSE;
                            } /* if */
                            else
                              if (isdigit (src [sinx + 1]))
                              {
                                strncpy (dst + 10, src + sinx, 2);
                                sinx += 2;
                              } /* if */
                              else
                              {
                                dst [10] = '0';
                                dst [11] = src [sinx++];
                              } /* else, else */
                            break;
      case ID_SECOND      : if (! isdigit (src [sinx]))
                            {
                              f--;
                              sinx++;
                              ok = FALSE;
                            } /* if */
                            else
                              if (isdigit (src [sinx + 1]))
                              {
                                strncpy (dst + 12, src + sinx, 2);
                                sinx += 2;
                              } /* if */
                              else
                              {
                                dst [12] = '0';
                                dst [13] = src [sinx++];
                              } /* else, else */
                            break;
      case ID_MICROSECOND : if (minx < 6) dst [14 + minx++] = (CHAR)(isdigit (src [sinx]) ? src [sinx++] : '0');
                            break;
    } /* switch, for */

  s [2] = EOS;                  /* for strncpy, see later */

  if ((type == TYPE_TIME) || (type == TYPE_TIMESTAMP))
  {
    if (hour12)
    {
      strncpy (s, dst + 8, 2);
      i = atoi (s);

      if (am)
      {
        if (i == 12) i = 0;
      } /* if */
      else
        if (i < 12) i += 12;

      dst [8] = (CHAR)('0' + i / 10);
      dst [9] = (CHAR)('0' + i % 10);
    } /* if */

    strncpy (s, dst + 12, 2);           /* copy seconds */
    i   = atoi (s);
    inc = i / 60;

    if (inc > 0) i %= 60;               /* from 1 to 99 seconds */

    dst [12] = (CHAR)('0' + i / 10);
    dst [13] = (CHAR)('0' + i % 10);

    strncpy (s, dst + 10, 2);           /* copy minutes */
    i   = atoi (s) + inc;
    inc = i / 60;

    if (inc > 0) i %= 60;               /* from 1 to 99 minutes */

    dst [10] = (CHAR)('0' + i / 10);
    dst [11] = (CHAR)('0' + i % 10);

    strncpy (s, dst + 8, 2);            /* copy hours */
    i = atoi (s) + inc;

    if (i >= 24)                        /* not over 24 hours */
    {
      ok = FALSE;
      i  = i % 24;
    } /* if */

    dst [8] = (CHAR)('0' + i / 10);
    dst [9] = (CHAR)('0' + i % 10);
  } /* if */

  if (type == TYPE_DATE || (type == TYPE_TIMESTAMP))
  {
    strncpy (s, dst, 2);

    if (s [0] == WILD_CHAR)
      strncpy (dst, "00", 2);
    else
    {
      i = atoi (s);

      if (i == 0)
        if (hasday)
          strncpy (dst, datestr, 2);    /* copy actual day */
        else
          strncpy (dst, "01", 2);       /* no day, presume first of... */

      if (i >= 32)
      {
        ok = FALSE;
        i  = i % 32;

        dst [0] = (CHAR)('0' + i / 10);
        dst [1] = (CHAR)('0' + i % 10);
      } /* if */
    } /* else */

    strncpy (s, dst + 2, 2);

    if (s [0] == WILD_CHAR)
      strncpy (dst + 2, "00", 2);
    else
    {
      i = atoi (s) - 1;

      if (i == -1)
        if (hasmonth)
          strncpy (dst + 2, datestr + 2, 2); /* copy actual month */
        else
          strncpy (dst + 2, "01", 2);        /* no month, presume january */

      if (i >= 12)
      {
        ok = FALSE;
        i  = i % 12 + 1;

        dst [2] = (CHAR)('0' + i / 10);
        dst [3] = (CHAR)('0' + i % 10);
      } /* if */
    } /* else */

    strncpy (s, dst + 4, 4);

    if (s [0] == WILD_CHAR)
      strncpy (dst + 4, "0000", 4);
    else
    {
      s [4] = EOS;
      i     = atoi (s);

      if (i == 0)
        if (hasyear)
          strncpy (dst + 4, datestr + 4, 4); /* copy actual year */
        else
          strncpy (dst + 4, "0001", 4);      /* no year, presume year 1 anno domini */
    } /* else */
  } /* if */

  if (type == TYPE_DATE) dst [8] = EOS;
  if (type == TYPE_TIME) strcpy (dst, dst + 8);

  return (ok);
} /* scan_stamp */

/*****************************************************************************/

LOCAL INT month2bin (s)
CHAR *s;

{
  SHORT month, n;
  CHAR  smonth [MAX_FORMATSTR];

  str_ascii (s);
  str_upper (s);

  if (month_short != NULL)
    for (n = 3; n >= 1; n--) /* compare three, two and one letter from the beginning */
      for (month = 0; month < 12; month++)
      {
        strcpy (smonth, month_short [month]);
        str_ascii (smonth);
        str_upper (smonth);
        if (strncmp (smonth, s, n) == 0) return (month + 1);
      } /* for, for, if */

  return (0);
} /* month2bin */

/*****************************************************************************/

LOCAL VOID dtoa (d, s, digits)
DOUBLE d;
CHAR   *s;
INT    digits;

{
  INT  ndig, dec, sign;
  INT  i, adjust;
  CHAR *str;

  ndig   = digits;
  str    = ecvt (d, ndig, &dec, &sign);
  s [0]  = EOS;
  adjust = 0;

  if (sign != 0)
  {
    strcat (s, "-");
    adjust++;
  } /* if */

  if (d == 0.0)
    strcpy (s, "0");
  else
  {
    if (dec > 0)
    {
      strncat (s, str, dec + adjust);
      s [dec + adjust] = EOS;
      strcat (s, ".");
      strcat (s, str + dec);
    } /* if */
    else
    {
      strcat (s, "0.");
      mem_set (s + 2 + adjust, '0', (USHORT)abs (dec));
      s [2 + adjust + abs (dec)] = EOS;
      strcat (s, str);
    } /* else */

    i = strlen (s) - 1;         /* delete trailing '0' */
    while (s [i] == '0') i--;
    if (s [i] == '.') i--;      /* delete decimal point if necessary */
    s [i + 1] = EOS;
  } /* else */
} /* dtoa */

/*****************************************************************************/

LOCAL SHORT FAR PASCAL cmp_char (elt1, elt2)
HPVOID elt1, elt2;

{
#ifdef _WINDOWS
  return ((SHORT)lstrcmp ((CHAR *)elt1, (CHAR *)elt2));
#elif MSDOS && ! defined (USE_DOS_CHARSET)
  return ((SHORT)ansi_strcmp ((CHAR *)elt1, (CHAR *)elt2));
#else
  return ((SHORT)strcmp ((CHAR *)elt1, (CHAR *)elt2));
#endif
} /* cmp_char */

/*****************************************************************************/

LOCAL SHORT FAR PASCAL cmp_word (elt1, elt2)
HPVOID elt1, elt2;

{
  REG SHORT l1, l2;

  l1 = *(HPSHORT)elt1;
  l2 = *(HPSHORT)elt2;

  return ((SHORT)((l1 < l2) ? -1 : (l1 == l2) ? 0 : 1));
} /* cmp_word */

/*****************************************************************************/

LOCAL SHORT FAR PASCAL cmp_long (elt1, elt2)
HPVOID elt1, elt2;

{
  REG LONG l1, l2;

  l1 = *(HPLONG)elt1;
  l2 = *(HPLONG)elt2;

  return ((SHORT)((l1 < l2) ? -1 : (l1 == l2) ? 0 : 1));
} /* cmp_long */

/*****************************************************************************/

LOCAL SHORT FAR PASCAL cmp_float (elt1, elt2)
HPVOID elt1, elt2;

{
  DOUBLE d1, d2;

  d1 = *(HPDOUBLE)elt1;
  d2 = *(HPDOUBLE)elt2;

  if (memcmp ((VOID *)&d1, &null_double, sizeof (DOUBLE)) == 0) d1 = NULL_FLOAT;
  if (memcmp ((VOID *)&d2, &null_double, sizeof (DOUBLE)) == 0) d2 = NULL_FLOAT;

  return ((SHORT)((d1 < d2) ? -1 : (d1 == d2) ? 0 : 1));
} /* cmp_float */

/*****************************************************************************/

LOCAL SHORT FAR PASCAL cmp_cfloat (elt1, elt2)
HPVOID elt1, elt2;

{
  DOUBLE d1, d2;

  d1 = d2 = 0;
  d1 = atof ((CHAR *)elt1);
  d2 = atof ((CHAR *)elt2);

  if (*(HPCHAR)elt1 == EOS) d1 = NULL_FLOAT;
  if (*(HPCHAR)elt2 == EOS) d2 = NULL_FLOAT;

  return ((SHORT)((d1 < d2) ? -1 : (d1 == d2) ? 0 : 1));
} /* cmp_cfloat */

/*****************************************************************************/

LOCAL SHORT FAR PASCAL cmp_date (elt1, elt2)
HPVOID elt1, elt2;

{
  REG SHORT cmp;
  SHORT     day1, month1, year1;
  SHORT     day2, month2, year2;
  DATE      d1, d2;

  d1 = *(HPDATE)elt1;
  d2 = *(HPDATE)elt2;

  day1   = d1.day;
  month1 = d1.month;
  year1  = d1.year;
  day2   = d2.day;
  month2 = d2.month;
  year2  = d2.year;

  if (memcmp ((VOID *)&d1, &null_date, sizeof (DATE)) == 0)
  {
    day1   = -1;
    month1 = -1;
    year1  = -1;
  } /* if */

  if (memcmp ((VOID *)&d2, &null_date, sizeof (DATE)) == 0)
  {
    day2   = -1;
    month2 = -1;
    year2  = -1;
  } /* if */

  cmp = (SHORT)(year1 - year2);

  if ((cmp == 0) || (year1 == 0) || (year2 == 0))
  {
    cmp = (SHORT)(month1 - month2);
    if ((cmp == 0) || (month1 == 0) || (month2 == 0))
      cmp = (SHORT)(((day1 == 0) || (day2 == 0)) ? 0 : day1 - day2);
  } /* if */

  return (cmp);
} /* cmp_date */

/*****************************************************************************/

LOCAL SHORT FAR PASCAL cmp_time (elt1, elt2)
HPVOID elt1, elt2;

{
  REG SHORT cmp;
  TIME      t1, t2;

  t1 = *(HPTIME)elt1;
  t2 = *(HPTIME)elt2;

  if (memcmp ((VOID *)&t1, &null_time, sizeof (TIME)) == 0)
  {
    mem_set (&t1, 0, sizeof (TIME));
    t1.micro = -1;
  } /* if */

  if (memcmp ((VOID *)&t2, &null_time, sizeof (TIME)) == 0)
  {
    mem_set (&t2, 0, sizeof (TIME));
    t2.micro = -1;
  } /* if */

  cmp = (SHORT)(t1.hour - t2.hour);

  if (cmp == 0)
  {
    cmp = (SHORT)(t1.minute - t2.minute);
    if (cmp == 0)
    {
      cmp = (SHORT)(t1.second - t2.second);
      if (cmp == 0) cmp = cmp_long (&t1.micro, &t2.micro);
    } /* if */
  } /* if */

  return (cmp);
} /* cmp_time */

/*****************************************************************************/

LOCAL SHORT FAR PASCAL cmp_stamp (elt1, elt2)
HPVOID elt1, elt2;

{
  REG SHORT       cmp;
  REG HPTIMESTAMP t1, t2;

  t1  = (HPTIMESTAMP)elt1;
  t2  = (HPTIMESTAMP)elt2;
  cmp = cmp_date (&t1->date, &t2->date);
  if (cmp == 0) cmp = cmp_time (&t1->time, &t2->time);

  return (cmp);
} /* cmp_stamp */

/*****************************************************************************/

LOCAL SHORT FAR PASCAL cmp_varbyte (elt1, elt2)
HPVOID elt1, elt2;

{
  REG LONG l1, l2;

  l1 = *(HPLONG)elt1;
  l2 = *(HPLONG)elt2;

  return ((SHORT)((l1 < l2) ? -1 : (l1 == l2) ? 0 : 1));
} /* cmp_varbyte */

/*****************************************************************************/

LOCAL SHORT FAR PASCAL cmp_blob (elt1, elt2)
HPVOID elt1, elt2;

{
  REG BLOB  *blob1, *blob2;
  REG SHORT cmp;

  blob1 = (BLOB *)elt1;
  blob2 = (BLOB *)elt2;
  cmp   = cmp_char (blob1->name, blob2->name);

  if (cmp == 0)
  {
    cmp = cmp_char (blob1->ext, blob2->ext);
    if (cmp == 0) cmp = cmp_long (&blob1->size, &blob2->size);
  } /* if */

  return (cmp);
} /* cmp_blob */

/*****************************************************************************/

LOCAL LONG FAR PASCAL size_char (elt)
HPVOID elt;

{
  LONG len;

  len = hstrlen ((HPCHAR)elt) + 1;
  return (len + odd (len));
} /* size_char */

/*****************************************************************************/

LOCAL LONG FAR PASCAL size_word (elt)
HPVOID elt;

{
  return (sizeof (SHORT));
} /* size_word */

/*****************************************************************************/

LOCAL LONG FAR PASCAL size_long (elt)
HPVOID elt;

{
  return (sizeof (LONG));
} /* size_long */

/*****************************************************************************/

LOCAL LONG FAR PASCAL size_float (elt)
HPVOID elt;

{
  return (sizeof (DOUBLE));
} /* size_float */

/*****************************************************************************/

LOCAL LONG FAR PASCAL size_cfloat (elt)
HPVOID elt;

{
  LONG len;

  len = hstrlen ((HPCHAR)elt) + 1;
  return (len + odd (len));
} /* size_cfloat */

/*****************************************************************************/

LOCAL LONG FAR PASCAL size_date (elt)
HPVOID elt;

{
  return (sizeof (DATE));
} /* size_date */

/*****************************************************************************/

LOCAL LONG FAR PASCAL size_time (elt)
HPVOID elt;

{
  return (sizeof (TIME));
} /* size_time */

/*****************************************************************************/

LOCAL LONG FAR PASCAL size_stamp (elt)
HPVOID elt;

{
  return (sizeof (TIMESTAMP));
} /* size_stamp */

/*****************************************************************************/

LOCAL LONG FAR PASCAL size_byte (elt)
HPVOID elt;

{
  LONG len;

  len = *(HPLONG)elt + sizeof (LONG);
  return (len + odd (len));
} /* size_byte */

/*****************************************************************************/

LOCAL LONG FAR PASCAL size_blob (elt)
HPVOID elt;

{
  return (sizeof (BLOB));
} /* size_blob */

/*****************************************************************************/

