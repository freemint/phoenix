/*****************************************************************************
 *
 * Module : DBROOT.C
 * Author : Dieter Geiž
 *
 * Creation date    : 01.07.89
 * Last modification: $Id$
 *
 *
 * Description: This module implements the database root routines.
 *
 * History:
 * 14.11.92: Function build_pass moved from conv
 * 29.10.92: FHANDLE changed to HFILE
 * 25.10.92: Module header added
 * 01.07.89: Creation of body
 *****************************************************************************/

#include "redefs.h"

#include "import.h"
#include "conv.h"
#include "files.h"
#include "utility.h"

#include "export.h"
#include "dbroot.h"

/****** DEFINES **************************************************************/

#define CRYPT(pass, l) (0xE5 ^ pass [l] ^ (0x10 + l))

/****** TYPES ****************************************************************/

/****** VARIABLES ************************************************************/

LOCAL SHORT db_error = SUCCESS;

/****** FUNCTIONS ************************************************************/

GLOBAL SHORT WINAPI get_dberror ()

{
  return (db_error);
} /* get_dberror */

/*****************************************************************************/

GLOBAL VOID WINAPI set_dberror (error)
SHORT error;

{
  db_error = error;
} /* set_dberror */

/*****************************************************************************/

GLOBAL VOID WINAPI build_pass (src, dst)
LPSTR src, dst;

{
  INT i;

  for (i = 0; (src [i] != EOS); i++)
    dst [i] = (CHAR)CRYPT (src, i);

  dst [i] = EOS;
} /* build_pass */

/*****************************************************************************/

GLOBAL BOOL WINAPI expand_file (handle, pages)
HFILE handle;
LONG  pages;  /* 1024 bytes per page */

{
  LONG    m, bufsize;
  UCHAR   page [1024];
  HPUCHAR p;
  BOOL    ok, enough;

  ok     = TRUE;
  m      = min (pages, 64L);
  p      = (HPUCHAR)mem_alloc (m * 1024L);
  enough = (p != NULL);

  if (! enough)
  {
    p = page;
    m = 1;
  } /* else */

  bufsize = m * 1024L;
  mem_lset (p, 0, bufsize);
  file_seek (handle, 0L, SEEK_END);

  while (ok && (pages > m))             /* write m pages */
  {
    ok     = (file_write (handle, bufsize, p) == bufsize);
    pages -= m;
  } /* while */

  if (ok && (pages > 0))                /* write trailing pages */
    ok = (file_write (handle, pages * 1024L, p) == pages * 1024L);

  if (! ok) set_dberror (DB_DWRITE);
  if (enough) mem_free (p);

  return (ok);
} /* expand_file */

/*****************************************************************************/

