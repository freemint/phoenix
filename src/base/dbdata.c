/*****************************************************************************
 *
 * Module : DBDATA.C
 * Author : Jrgen & Dieter Geiž
 *
 * Creation date    : 01.07.89
 * Last modification: $Id$
 *
 *
 * Description: This module implements the database data routines.
 *
 * History:
 * 25.07.93: Page zero is always written in flush_data
 * 24.07.93: Allowing data cache to be used to read data dictionary in multiuser mode
 * 19.07.93: Cache added
 * 07.07.93: Component multi_locked in page will no longer underflow
 * 09.06.93: Component multi_locked in page 0 used
 * 26.05.93: Bug in test_multi fixed for multiuser operation
 * 07.03.93: Version is incompatible if byte sex is different in open_data
 * 15.11.92: Macro FIRST_VERSION used
 * 14.11.92: SHORTFUNC renamed to LOCKFUNC
 * 11.11.92: GEMDOS uses old structure for non-optimistic locking
 * 09.11.92: Version number added in read_data
 * 04.11.92: Locking counter incremented in every case
 * 31.10.92: Far pointer definitions for DLL implementation added
 * 26.10.92: lockfunc initialized only once in DLL
 * 25.10.92: Module header added
 * 01.07.89: Creation of body
 *****************************************************************************/

#include "redefs.h"

#include "import.h"
#include "conv.h"
#include "files.h"
#include "utility.h"

#include "dbroot.h"

#include "export.h"
#include "dbdata.h"

/****** DEFINES **************************************************************/

#define FIELDSIZE        8      /* data is stored in blocks of this size */
#define UNITSIZE      1024L     /* datafile will be stored in units of this size */
#define EXPAND_SIZE      8      /* minimum number of kbytes to expand datafile */

#define ENCODE(a, b)            (UCHAR)(((a) + (b)) & 0xFFL)

#define OFF(type, ident)        ((LONG)(((type *)0)->ident))
#define SIZE_DATA_DFN           OFF (DATA_DFN, data)
#define OLD_SIZE_DATA_DFN       (3 * sizeof (LONG) + 2 * sizeof (SHORT))

/****** TYPES ****************************************************************/

typedef UCHAR PAGE [UNITSIZE];

/****** VARIABLES ************************************************************/

LOCAL LOCKFUNC lockfunc;        /* function for unsuccessful locking */

/****** FUNCTIONS ************************************************************/

LOCAL BOOL read_pg0      _((LPDATAINF data));
LOCAL BOOL write_pg0     _((LPDATAINF data));
LOCAL LONG write_record  _((LPDATAINF data, LPDATA_DFN data_dfn, LONG size, HPVOID buffer, LONG address, BOOL encode));
LOCAL LONG append_record _((LPDATAINF data, LPDATA_DFN data_dfn, LONG size, HPVOID buffer, LONG recsize));
LOCAL LONG data_insert   _((LPDATAINF data, SHORT table, LONG size, HPVOID buffer, BOOL encode, SHORT status, LONG version));
LOCAL VOID crypt_buffer  _((LPDPAGE0 p0, LONG adr, HPUCHAR buffer, LONG size));
LOCAL BOOL exp_file      _((LPDATAINF data, LONG expsize));
LOCAL LONG cache_read    _((LPDATAINF data, LONG count, HPVOID buf));
LOCAL LONG cache_write   _((LPDATAINF data, LONG count, HPVOID buf));
LOCAL LONG cache_seek    _((LPDATAINF data, LONG offset, INT mode));

/*****************************************************************************/

GLOBAL VOID WINAPI init_data (lock)
LOCKFUNC lock;

{
#if defined (_WINDLL) && ! defined (WIN32)
  if (lockfunc == NULL)		/* initialize only from DLL */
#endif
    lockfunc = lock;
} /* init_data */

/*****************************************************************************/

GLOBAL LPDATAINF WINAPI create_data (filename, flags, size)
LPSTR  filename;
USHORT flags;
LONG   size;

{
  LPDATAINF data;
  PAGE      page;
  BOOL      ok;
  LPDPAGE0  p0;

  data = (LPDATAINF)mem_alloc ((LONG)sizeof (DATAINF));

  if (data == NULL)
    set_dberror (DB_NOMEMORY);
  else
  {
    mem_set (data, 0, sizeof (DATAINF));

    data->handle = file_create (filename);

    if (data->handle < 0)
    {
      set_dberror (DB_DNOCREATE);
      mem_free (data);
      data = NULL;
    } /* if */
    else
    {
      if (size < 1) size = 1;

      p0               = &data->page0;
      p0->locking      = 0;
      p0->flags        = flags;
      p0->version      = DB_VERSION;
      p0->fieldsize    = FIELDSIZE;
      p0->multi_vers   = 0;
      p0->file_size    = size * UNITSIZE;
      p0->next_rec     = FIRST_REC;
      p0->opened       = 1;
      p0->multi_locked = 0;

      get_tstamp (&p0->created);
      get_tstamp (&p0->lastuse);

      p0->time       = p0->created.time;
#if GEMDOS					/* use old structure, locking ok with networks */
      p0->dfn_offset = 0;
      p0->dfn_size   = (SHORT)OLD_SIZE_DATA_DFN;
#else
      p0->dfn_offset = sizeof (LONG);
      p0->dfn_size   = (SHORT)SIZE_DATA_DFN;
#endif

      mem_set (page, 0, sizeof (page));
      mem_move (page, p0, sizeof (DPAGE0));

      ok = cache_write (data, (LONG)sizeof (page), page) == sizeof (page);
      size--;                   /* first page has already been written */

      if (! ok)
      {
        file_close (data->handle);
        mem_free (data);
        data = NULL;
        set_dberror (DB_DNOCREATE);
      } /* if */
      else
        if (size > 0) ok = expand_file (data->handle, size);
    } /* else */
  } /* else */

  return (data);
} /* create_data */

/*****************************************************************************/

GLOBAL LPDATAINF WINAPI open_data (filename, flags, cache_size)
LPSTR  filename;
USHORT flags;
LONG   cache_size;

{
  LPDATAINF data;
  SHORT     level;
  INT       mode;
  TIME      *timep;

  level = 0;                                    /* used for error exit */
  data  = (LPDATAINF)mem_alloc ((LONG)sizeof (DATAINF));

  if (data == NULL)
  {
    level = 1;
    set_dberror (DB_NOMEMORY);
  } /* if */
  else
  {
    mem_set (data, 0, sizeof (DATAINF));

    mode = flags & DATA_RDONLY ? O_RDONLY : O_RDWR;

#if GEMDOS
    if (flags & DATA_MULUSER) mode |= O_SHARED; /* GEMDOS has no sharing mechanism without network support */
#else
    if (flags & (DATA_MULUSER | DATA_MULTASK)) mode |= O_SHARED;
#endif

    data->flags  = flags;
    data->handle = file_open (filename, mode);

    if (cache_size != 0)
    {
      data->cache_size   = cache_size;
      data->cache_memory = mem_alloc (data->cache_size);
    } /* if */

    if (data->cache_memory == NULL)             /* sorry, no cache */
      data->cache_size = 0;

    if (data->handle < 0)
    {
      level = 2;
      set_dberror (DB_DNOOPEN);
    } /* if */
    else
    {
      if (data->cache_memory != NULL)
      {
        file_read (data->handle, data->cache_size, data->cache_memory);
        data->cache_low = data->cache_size;     /* so first min compare in write_cache would fire */
      } /* if */

      if (read_pg0 (data))
      {
        timep = &data->page0.time;

        if ((timep->hour == 0) && (timep->minute == 0) && (timep->second == 0) && (timep->micro == 0))
          data->page0.time = data->page0.created.time;

        if (data->page0.dfn_size == 0)                  /* old data file */
           data->page0.dfn_size = OLD_SIZE_DATA_DFN;

        if ((data->page0.version > DB_VERSION) || (data->page0.fieldsize >= 0x0100))       /* if fieldsize is larger, bytes are swapped */
        {
          level = 3;
          set_dberror (DB_DVERSION);
        } /* if */
        else
        {
          if ((data->page0.multi_locked > 0) || (data->page0.opened > 0)) /* not closed on last use */
            set_dberror (DB_DNOTCLOSED);                             /* only a warning, level retains 0 */

          if (! (flags & (DATA_MULUSER | DATA_MULTASK | DATA_RDONLY)))
          {
            data->page0.opened++;

            if (! write_pg0 (data))
            {
              level = 3;
              set_dberror (DB_DNOOPEN);
            } /* if */
          } /* if */
        } /* else */
      } /* if */
      else
      {
        level = 3;
        set_dberror (DB_DNOOPEN);
      } /* else */
    } /* else */
  } /* else */

  switch (level)
  {
    case 3 : file_close (data->handle);
    case 2 : mem_free (data);
    case 1 : data = NULL;
    case 0 : break;
  } /* switch */

  return (data);
} /* open_data */

/*****************************************************************************/

GLOBAL BOOL WINAPI close_data (data)
LPDATAINF data;

{
  BOOL ok;

  ok = TRUE;

  if (! (data->flags & (DATA_MULUSER | DATA_MULTASK | DATA_RDONLY))) data->page0.opened--;

  get_tstamp (&data->page0.lastuse);

  ok = flush_data (data) && ok;
  ok = (file_close (data->handle) == SUCCESS) && ok;

  mem_free (data->cache_memory);
  mem_free (data);

  if (! ok) set_dberror (DB_DNOCLOSE);

  return (ok);
} /* close_data */

/*****************************************************************************/

GLOBAL BOOL WINAPI flush_data (data)
LPDATAINF data;

{
  BOOL ok;
  LONG size, bytes;

  ok = write_pg0 (data);

  if ((data->cache_memory != NULL) && ! (data->flags & DATA_FLUSH))
  {
    ok = file_seek (data->handle, LOFFSET (DPAGE0, flags), SEEK_SET) == LOFFSET (DPAGE0, flags);	/* don't write locking component */

    if (ok && ! (data->flags & DATA_RDONLY))
    {
      bytes = file_write (data->handle, (LONG)sizeof (DPAGE0) - LOFFSET (DPAGE0, flags), &data->page0.flags);
      ok    = bytes == sizeof (DPAGE0) - LOFFSET (DPAGE0, flags);
    } /* if */

    size = data->cache_high - data->cache_low;

    if (size > 0)
    {
      ok = ok && file_seek (data->handle, data->cache_low, SEEK_SET) == data->cache_low;
      ok = ok && file_write (data->handle, size, data->cache_memory + data->cache_low) == size;

      if (ok)
      {
        data->cache_low  = data->cache_size;    /* so first min compare in write_cache would fire */
        data->cache_high = 0;
      } /* if */
    } /* if */
  } /* if */

  return (ok);
} /* flush_data */

/*****************************************************************************/

GLOBAL LONG WINAPI insert_data (data, table, size, buffer, encode)
LPDATAINF data;
SHORT     table;
LONG      size;
HPVOID    buffer;
BOOL      encode;

{
  LONG address;

  address = data_insert (data, table, size, buffer, encode, DATA_OK, FIRST_VERSION);
  if (address == 0) set_dberror (DB_DINSERT);

  return (address);     /* returns address at which data was inserted */
} /* insert_data */

/*****************************************************************************/

GLOBAL BOOL WINAPI delete_data (data, address)
LPDATAINF data;
LONG      address;

{
  DATA_DFN data_dfn;
  BOOL     ok;

  ok = address >= FIRST_REC;

  if (ok)
  {
    ok = read_dfn (data, address, &data_dfn, NULL);

    if (ok)
    {
      data_dfn.status |= DATA_DELETED;          /* just set a flag to delete */
      ok = write_dfn (data, address, &data_dfn);
    } /* if */
  } /* if */

  if (! ok) set_dberror (DB_DDELETE);

  return (ok);
} /* delete_data */

/*****************************************************************************/

GLOBAL BOOL WINAPI undelete_data (data, address)
LPDATAINF data;
LONG      address;

{
  DATA_DFN data_dfn;
  BOOL     ok;

  ok = read_dfn (data, address, &data_dfn, NULL);

  if (ok)
  {
    ok = data_dfn.status & DATA_DELETED;

    if (ok)
    {
      data_dfn.status &= ~DATA_DELETED;                 /* clear deleted flag */
      ok = write_dfn (data, address, &data_dfn);        /* and save it */
    } /* if */
  } /* if */

  if (! ok) set_dberror (DB_DUDELETE);

  return (ok);
} /* undelete_data */

/*****************************************************************************/

GLOBAL BOOL WINAPI update_data (data, table, size, buffer, encode, address)
LPDATAINF data;
SHORT   table;
LONG    size;
HPVOID  buffer;
BOOL    encode;
LONG    address;

{
  DATA_DFN   data_dfn;
  DATA_DFN   dfn_indirect;
  LPDATA_DFN new_dfn;
  LPDPAGE0   p0;
  LONG       new_adr;
  BOOL       ok;

  ok = address >= FIRST_REC;

  if (ok)
  {
    ok = read_dfn (data, address, &data_dfn, &dfn_indirect);
    if (ok) ok = (data_dfn.status & DATA_DELETED) == 0;

    if (ok)
    {
      /* use new_dfn and new_adr if data has already moved (is indirect) */
      new_dfn = (data_dfn.status & DATA_INDIRECT) ? &dfn_indirect : &data_dfn;
      new_adr = (data_dfn.status & DATA_INDIRECT) ? data_dfn.indirect : address;
      encode  = encode || (data->page0.flags & DATA_ENCODE);

      if (size <= new_dfn->recsize + new_dfn->free)     /* fits into old position */
      {
        new_dfn->free    -= size - new_dfn->recsize;    /* set new free bytes */
        new_dfn->recsize  = size;                       /* set new recsize */
        new_dfn->version++;

        ok = write_record (data, new_dfn, size, buffer, new_adr, encode) > 0;
      } /* if */
      else                                              /* append at end of datafile */
      {
        new_adr = data_insert (data, table, size, buffer, encode, DATA_MOVED, new_dfn->version + 1);
        ok      = new_adr > 0;

        if (ok)
        {
          data_dfn.indirect  = new_adr;                 /* fix original data_dfn... */
          data_dfn.status   |= DATA_INDIRECT;           /* ...to point to new record address */
          p0                 = &data->page0;
          p0->size_released += new_dfn->recsize + new_dfn->free;

          ok = write_dfn (data, address, &data_dfn);    /* save modified data_dfn */
          if (ok) ok = write_pg0 (data);
        } /* if */
      } /* else */
    } /* if */
  } /* if */

  if (! ok) set_dberror (DB_DUPDATE);

  return (ok);
} /* update_data */

/*****************************************************************************/

GLOBAL LONG WINAPI read_data (data, address, buffer, table, size, version, deleted, moved)
LPDATAINF data;
LONG      address;
HPVOID    buffer;
LPSHORT   table;
LPLONG    size, version;
LPBOOL    deleted, moved;

{
  DATA_DFN data_dfn;
  DATA_DFN dfn_indirect;
  LONG     recsize;
  LPDPAGE0 p0;
  BOOL     ok;

  recsize = 0;
  p0      = &data->page0;
  ok      = address >= FIRST_REC;

  if (ok)
  {
    ok = read_dfn (data, address, &data_dfn, &dfn_indirect);    /* file pointer points now to data of record */

    if (table   != NULL) *table   = data_dfn.table;
    if (size    != NULL) *size    = data_dfn.recsize + data_dfn.free + p0->dfn_size;
    if (version != NULL) *version = (data_dfn.status & DATA_INDIRECT) ? dfn_indirect.version : data_dfn.version;
    if (deleted != NULL) *deleted = (data_dfn.status & DATA_DELETED) != 0;
    if (moved   != NULL) *moved   = (data_dfn.status & DATA_MOVED) != 0;

    if (ok)
    {
      recsize = (data_dfn.status & DATA_INDIRECT) ? dfn_indirect.recsize : data_dfn.recsize;

      if (buffer != NULL)
      {
        ok = cache_read (data, recsize, buffer) == recsize;

        if (ok)
        {
          if (data_dfn.status & DATA_INDIRECT) address = data_dfn.indirect;
          if (data_dfn.status & DATA_CRYPTED) crypt_buffer (p0, address, buffer, recsize); /* decode buffer */
        } /* if */
      } /* if */
    } /* if */
  } /* if */

  if (! ok)
  {
    set_dberror (DB_DREAD);
    recsize = 0;
  } /* if */

  return (recsize);
} /* read_data */

/*****************************************************************************/

GLOBAL BOOL WINAPI test_multi (data, do_write)
LPDATAINF data;
BOOL      do_write;

{
  BOOL changed;
  LONG version, bytes;

  changed = FALSE;

  if ((data->flags & (DATA_MULUSER | DATA_MULTASK)) && (data->locked == 1))
  {
    cache_seek (data, LOFFSET (DPAGE0, multi_vers), SEEK_SET);
    bytes = cache_read (data, (LONG)sizeof (version), &version);

    if (bytes != sizeof (version))
      set_dberror (DB_DREAD);
    else
    {
      changed = version != data->page0.multi_vers;

      if (do_write)
      {
        data->page0.multi_vers = ++version;

        cache_seek (data, LOFFSET (DPAGE0, multi_vers), SEEK_SET);
        bytes = cache_write (data, (LONG)sizeof (version), &version);

        if (bytes != sizeof (version))
          set_dberror (DB_DWRITE);
      } /* if */
      else
        data->page0.multi_vers = version;
    } /* else */
  } /* if */

  return (changed);
} /* test_multi */

/*****************************************************************************/

GLOBAL BOOL WINAPI read_dpg0 (data)
LPDATAINF data;

{
  return (read_pg0 (data));
} /* read_dpg0 */

/*****************************************************************************/

GLOBAL BOOL WINAPI write_dpg0 (data)
LPDATAINF data;

{
  return (write_pg0 (data));
} /* write_dpg0 */

/*****************************************************************************/

GLOBAL BOOL WINAPI db_lock (data)
LPDATAINF data;

{
  BOOL locked;
  INT  answer;

  locked = TRUE;

  if (data->flags & (DATA_MULUSER | DATA_MULTASK))
  {
    if (data->locked == 0)
    {
      locked = FALSE;
      answer = LOCK_RETRY;

      while (! locked && (answer == LOCK_RETRY))
      {
        locked = (data->flags & DATA_MULTASK) ? TRUE : file_lock (data->handle, LOFFSET (DPAGE0, locking), (LONG)sizeof (USHORT), TRUE);

        if (! locked)
        {
          if (lockfunc == NULL)
            answer = LOCK_CANCEL;
          else
          {
            answer = (*lockfunc) (TRUE);
            locked = answer == LOCK_IGNORE;
          } /* else */

          if (answer == LOCK_CANCEL) set_dberror (DB_DNOLOCK);
        } /* if */

        if (locked && ! (data->flags & DATA_RDONLY))
        {
          data->page0.multi_locked++;
          cache_seek (data, LOFFSET (DPAGE0, multi_locked), SEEK_SET);
          if (cache_write (data, (LONG)sizeof (USHORT), &data->page0.multi_locked) != sizeof (USHORT)) set_dberror (DB_DWRITE);
        } /* if */
      } /* while */
    } /* if */

    data->locked++;
  } /* if */

  return (locked);
} /* db_lock */

/*****************************************************************************/

GLOBAL BOOL WINAPI db_unlock (data)
LPDATAINF data;

{
  BOOL unlocked;
  INT  answer;

  unlocked = TRUE;

  if (data->flags & (DATA_MULUSER | DATA_MULTASK))
  {
    data->locked--;

    if (data->locked <= 0)                      /* prevent underflow */
    {
      data->locked = 0;
      unlocked     = FALSE;
      answer       = LOCK_RETRY;

      while (! unlocked && (answer == LOCK_RETRY))
      {
        unlocked = (data->flags & DATA_MULTASK) ? TRUE : file_unlock (data->handle, LOFFSET (DPAGE0, locking), (LONG)sizeof (USHORT));

        if (! unlocked)
        {
          if (lockfunc == NULL)
            answer = LOCK_CANCEL;
          else
          {
            answer   = (*lockfunc) (FALSE);
            unlocked = answer == LOCK_IGNORE;
          } /* else */

          if (answer == LOCK_CANCEL) set_dberror (DB_DNOUNLOCK);
        } /* if */

        if (unlocked && ! (data->flags & DATA_RDONLY))
        {
          if (data->page0.multi_locked > 0) data->page0.multi_locked--;
          cache_seek (data, LOFFSET (DPAGE0, multi_locked), SEEK_SET);
          if (cache_write (data, (LONG)sizeof (USHORT), &data->page0.multi_locked) != sizeof (USHORT)) set_dberror (DB_DWRITE);
        } /* if */
      } /* while */
    } /* if */
  } /* if */

  return (unlocked);
} /* db_unlock */

/*****************************************************************************/

GLOBAL BOOL WINAPI read_dfn (data, address, data_dfn, data_idfn)
LPDATAINF  data;
LONG       address;
LPDATA_DFN data_dfn;
LPDATA_DFN data_idfn;

{
  BOOL ok;

  address += data->page0.dfn_offset;            /* skip locking portion */
  ok       = cache_seek (data, address, SEEK_SET) == address;

  if (ok)
  {
    ok = cache_read (data, (LONG)data->page0.dfn_size, data_dfn) == (LONG)data->page0.dfn_size;

    if (data->page0.dfn_size == OLD_SIZE_DATA_DFN)
      data_dfn->version = FIRST_VERSION;

    if (ok)
      if (data_dfn->status & DATA_INDIRECT)     /* if indirect seek to new address and fill struct */
      {
        ok = cache_seek (data, data_dfn->indirect + data->page0.dfn_offset, SEEK_SET) == data_dfn->indirect + data->page0.dfn_offset;

        if (data_idfn != NULL)
        {
          if (ok) ok = cache_read (data, (LONG)data->page0.dfn_size, data_idfn) == data->page0.dfn_size;
          if (data->page0.dfn_size == OLD_SIZE_DATA_DFN)
            data_idfn->version = FIRST_VERSION;
        } /* if */
      } /* if */
  } /* if */

  if (! ok) set_dberror (DB_DREAD);

  return (ok);
} /* read_dfn */

/*****************************************************************************/

GLOBAL BOOL WINAPI write_dfn (data, address, data_dfn)
LPDATAINF  data;
LONG       address;
LPDATA_DFN data_dfn;

{
  BOOL ok;

  address += data->page0.dfn_offset;            /* skip locking portion */
  ok       = cache_seek (data, address, SEEK_SET) == address;
  if (ok) ok = cache_write (data, (LONG)data->page0.dfn_size, data_dfn) == (LONG)data->page0.dfn_size;

  if (! ok) set_dberror (DB_DWRITE);

  return (ok);
} /* write_dfn */

/*****************************************************************************/

GLOBAL BOOL WINAPI expand_data (data, size)
LPDATAINF data;
LONG      size;

{
  BOOL ok;
  LONG bytes;

  ok    = TRUE;
  bytes = (size * 1024L) - data->page0.file_size;

  if (bytes > 0)
  {
    ok = expand_file (data->handle, bytes / 1024L);

    data->page0.file_size = file_seek (data->handle, 0L, SEEK_END);     /* assure new filesize */
    data->cache_seekpos   = data->page0.file_size;
    ok = write_pg0 (data) && ok;
  } /* if */

  return (ok);
} /* expand_data */

/*****************************************************************************/

GLOBAL BOOL WINAPI inc_released (data, address)
LPDATAINF data;
LONG      address;

{
  BOOL     ok;
  DATA_DFN data_dfn, data_idfn;
  LONG     size;

  ok = read_dfn (data, address, &data_dfn, &data_idfn);

  if (ok)
  {
    size = (data_dfn.status & DATA_INDIRECT) ? data_idfn.recsize + data_idfn.free : data_dfn.recsize + data_dfn.free;
    data->page0.size_released += size + data->page0.dfn_offset + data->page0.dfn_size;
    if (data->flags & (DATA_MULUSER | DATA_MULTASK)) ok = write_pg0 (data);
    if (! ok) set_dberror (DB_DWRITE);
  } /* if */

  return (ok);
} /* inc_released */

/*****************************************************************************/

LOCAL BOOL read_pg0 (data)
LPDATAINF data;

{
  BOOL ok;
  LONG bytes;

  ok = cache_seek (data, 0L, SEEK_SET) == 0L;

  if (ok)
  {
    bytes = cache_read (data, (LONG)sizeof (DPAGE0), &data->page0);
    ok    = bytes == sizeof (DPAGE0);
  } /* if */

  return (ok);
} /* read_pg0 */

/*****************************************************************************/

LOCAL BOOL write_pg0 (data)
LPDATAINF data;

{
  BOOL ok;
  LONG bytes;
  CHAR buffer [512];

  ok = cache_seek (data, LOFFSET (DPAGE0, flags), SEEK_SET) == LOFFSET (DPAGE0, flags);	/* don't write locking component */

  if (ok && ! (data->flags & DATA_RDONLY))
  {
    bytes = cache_write (data, (LONG)sizeof (DPAGE0) - LOFFSET (DPAGE0, flags), &data->page0.flags);
    ok    = bytes == sizeof (DPAGE0) - LOFFSET (DPAGE0, flags);
    bytes = cache_read (data, 512L, buffer);     /* force writing */
  } /* if */

  return (ok);
} /* write_pg0 */

/*****************************************************************************/

LOCAL LONG write_record (data, data_dfn, size, buffer, address, encode)
LPDATAINF  data;
LPDATA_DFN data_dfn;
LONG       size;
HPVOID     buffer;
LONG       address;
BOOL       encode;

{
  BOOL     ok;
  LONG     bytes;
  LPDPAGE0 p0;

  ok       = address >= FIRST_REC;
  address += data->page0.dfn_offset;            /* skip locking portion */
  if (ok) ok = cache_seek (data, address, SEEK_SET) == address;
  address -= data->page0.dfn_offset;            /* use original address */
  p0       = &data->page0;

  if (ok)
  {
    if (encode)
    {
      crypt_buffer (p0, address, buffer, size);                 /* encode buffer */
      data_dfn->status |= DATA_CRYPTED;
    } /* if */

    bytes  = cache_write (data, (LONG)data->page0.dfn_size, data_dfn);
    bytes += cache_write (data, size, buffer);
    ok     = bytes == data->page0.dfn_size + size;
    if (encode) crypt_buffer (p0, address, buffer, size);       /* decode buffer */
  } /* if */

  if (! ok) address = 0;

  return (address);
} /* write_record */

/*****************************************************************************/

LOCAL LONG append_record (data, data_dfn, size, buffer, recsize)
LPDATAINF  data;
LPDATA_DFN data_dfn;
LONG       size;
HPVOID     buffer;
LONG       recsize;

{
  BOOL ok;
  LONG bytes;
  LONG address;

  address  = data->page0.next_rec;                      /* get next address */
  address += data->page0.dfn_offset;                    /* skip locking portion */
  ok       = cache_seek (data, address, SEEK_SET) == address;
  address -= data->page0.dfn_offset;                    /* use old value */

  if (ok)
  {
    bytes  = cache_write (data, (LONG)data->page0.dfn_size, data_dfn);
    bytes += cache_write (data, size, buffer);
    ok     = bytes == data->page0.dfn_size + size;

    if (ok)
    {
      data->page0.next_rec += recsize;
      if (data->flags & (DATA_MULUSER | DATA_MULTASK)) ok = write_pg0 (data);
    } /* if */
  } /* if */

  if (! ok) address = 0;

  return (address);
} /* append_record */

/*****************************************************************************/

LOCAL LONG data_insert (data, table, size, buffer, encode, status, version)
LPDATAINF data;
SHORT     table;
LONG      size;
HPVOID    buffer;
BOOL      encode;
SHORT     status;
LONG      version;

{
  LONG     address;
  LONG     recsize;
  LONG     realsize;
  LONG     last_adr;
  DATA_DFN data_dfn;
  LPDPAGE0 p0;
  BOOL     ok;

  address  = 0;
  ok       = TRUE;
  p0       = &data->page0;
  realsize = data->page0.dfn_offset + data->page0.dfn_size + size;              /* minimum record size */
  recsize  = ((realsize + p0->fieldsize - 1) / p0->fieldsize) * p0->fieldsize;

  if (p0->file_size < p0->next_rec + realsize) ok = exp_file (data, recsize);   /* if not fit expand */

  if (ok)
  {
    data_dfn.indirect = 0;                              /* enter new values */
    data_dfn.recsize  = size;
    data_dfn.free     = recsize - realsize;
    data_dfn.table    = table;
    data_dfn.status   = status;
    data_dfn.version  = version;

    last_adr = p0->next_rec;
    encode   = encode || (p0->flags & DATA_ENCODE);

    if (encode)
    {
      crypt_buffer (p0, last_adr, buffer, size);        /* encode buffer */
      data_dfn.status |= DATA_CRYPTED;
    } /* if */

    address = append_record (data, &data_dfn, size, buffer, recsize);
    ok      = address > 0;

    if (encode) crypt_buffer (p0, last_adr, buffer, size); /* decode buffer */
  } /* if */

  if (! ok) address = 0;

  return (address);
} /* data_insert */

/*****************************************************************************/

LOCAL VOID crypt_buffer (p0, adr, buffer, size)
LPDPAGE0 p0;
LONG     adr;
HPUCHAR  buffer;
LONG     size;

{
  REG LONG i;
  REG LONG crypt;

  crypt = (p0->time.hour +
           p0->time.minute +
           p0->time.second) & 0xF0L;

  for (i = 0; i < size; i++) buffer [i] ^= ENCODE (i, crypt + (adr >> 3));
} /* crypt_buffer */

/*****************************************************************************/

LOCAL BOOL exp_file (data, expsize)
LPDATAINF data;
LONG      expsize;

{
  LONG pages;
  BOOL ok;

  pages = max ((expsize + UNITSIZE - 1) / UNITSIZE, EXPAND_SIZE);       /* calc expand size */
  ok    = expand_file (data->handle, pages);

  data->page0.file_size = file_seek (data->handle, 0L, SEEK_END);       /* assure new filesize */
  data->cache_seekpos   = data->page0.file_size;
  ok = write_pg0 (data) && ok;

  return (ok);
} /* exp_file */

/*****************************************************************************/

LOCAL LONG cache_read (data, count, buf)
LPDATAINF data;
LONG      count;
HPVOID    buf;

{
  LONG res;

  data->num_calls++;

  if (data->cache_seekpos + count > data->cache_size)
  {
    if (data->cache_size != 0)                          /* first part of record could have been in cache, so file pointer would not have been moved */
      if (file_seek (data->handle, 0, SEEK_CUR) != data->cache_seekpos)
        file_seek (data->handle, data->cache_seekpos, SEEK_SET);

    res = file_read (data->handle, count, buf);
  } /* if */
  else
  {
    data->num_hits++;
    mem_lmove (buf, data->cache_memory + data->cache_seekpos, count);
    res = count;
  } /* else */

  data->cache_seekpos += res;

  return (res);
} /* cache_read */

/*****************************************************************************/

LOCAL LONG cache_write (data, count, buf)
LPDATAINF data;
LONG      count;
HPVOID    buf;

{
  LONG res;

  data->num_calls++;

  if (data->cache_seekpos + count > data->cache_size)
  {
    if (data->cache_size != 0)                          /* first part of record could have been in cache, so file pointer would not have been moved */
      if (file_seek (data->handle, 0, SEEK_CUR) != data->cache_seekpos)
        file_seek (data->handle, data->cache_seekpos, SEEK_SET);

    res = file_write (data->handle, count, buf);
  } /* if */
  else
  {
    data->num_hits++;
    mem_lmove (data->cache_memory + data->cache_seekpos, buf, count);

    if (data->flags & DATA_FLUSH)
    {
      if (file_seek (data->handle, 0, SEEK_CUR) != data->cache_seekpos)
        file_seek (data->handle, data->cache_seekpos, SEEK_SET);

      res = file_write (data->handle, count, buf);
    } /* if */
    else
    {
      res = count;

      if (data->cache_seekpos >= FIRST_REC)     /* page zero is always written in flush_data */
      {
        data->cache_low  = min (data->cache_seekpos, data->cache_low);
        data->cache_high = max (data->cache_seekpos + count, data->cache_high);
      } /* if */
    } /* else */
  } /* else */

  data->cache_seekpos += res;

  return (res);
} /* cache_write */

/*****************************************************************************/

LOCAL LONG cache_seek (data, offset, mode)
LPDATAINF data;
LONG      offset;
INT       mode;

{
  LONG res, pos;

  switch (mode)
  {
    case SEEK_SET : pos = offset;                         break;
    case SEEK_CUR : pos = data->cache_seekpos + offset;   break;
    case SEEK_END : pos = data->page0.file_size - offset; break;
    default       : pos = data->cache_seekpos;            break;
  } /* switch */

  data->cache_seekpos = pos;

  if ((data->page0.file_size > data->cache_size) || (data->page0.fieldsize == 0)) /* second condition: page 0 has not been read yet */
    res = file_seek (data->handle, pos, SEEK_SET);
  else
    res = pos;

  return (res);
} /* cache_seek */

/*****************************************************************************/

