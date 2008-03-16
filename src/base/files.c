/*****************************************************************************
 *
 * Module : FILES.C
 * Author : Dieter Geiž
 *
 * Creation date    : 06.03.89
 * Last modification: $Id$
 *
 *
 * Description: This module implements the file management routines.
 *
 * History:
 * 22.07.96: UNC names are handled correctly in file_split
 * 19.06.96: Functions file_length, file_exist and path_exist accept long filenames
 * 18.06.96: Casting result from Fopen to HFILE
 * 07.01.96: AnsiToOem not used for WIN32
 * 01.01.96: Warnings of C/C++ 10.0 removed
 * 09.12.94: Warnings of C/C++ 9.0 removed
 * 16.06.94: Functions file_exist and path_exist check for empty names because of netware compatibilty
 * 17.10.93: Function path_exist corrected
 * 24.08.93: Using AnsiUpper instead of toupper in Windows
 * 06.05.93: Warnings of C/C++ 8.0 removed
 * 27.11.92: File locking test now ok
 * 12.11.92: Function sopen used for MSDOS for opening files
 * 10.11.92: Function file_locked added
 * 04.11.92: Modifications for NT added
 * 31.10.92: Far pointer definitions for DLL implementation added
 * 29.10.92: SHARE_DENY_NONE used for sharing in windows
 * 25.10.92: Module header added
 * 06.03.89: Creation of body
 *****************************************************************************/

#include <ctype.h>
#include <time.h>

#include "import.h"

#if GEMDOS
#if TURBO_C
#include <ext.h>
#endif
#endif

#if MSDOS | NT | OS2
#include <io.h>
#include <share.h>
#include <sys\types.h>
#include <sys\stat.h>
#include <dos.h>

#if MS_C
#include <sys\locking.h>
#include <direct.h>
#endif

#if TURBO_C
#include <dir.h>
#undef FILENAME
#endif
#endif

#if FLEXOS
#include <ccdosif.h>
#endif

#if UNIX
#include <sys/stat.h>
#if PCC | XL_C
#include <unistd.h>
#endif
#endif

#include "export.h"
#include "files.h"

/****** DEFINES **************************************************************/

#if GEMDOS
#define LOCK_CNT        50              /* retries to do locking */
#define SHARE_WR        (-1)            /* shared write */
#define SHARE_RW        (-2)            /* shared read/write */

#define FCREATE(filename)                   Fcreate (filename, 0)
#define FCLOSE(handle)                      Fclose (handle)
#define FREAD(handle, count, buf)           Fread (handle, count, buf)
#define FWRITE(handle, count, buf)          Fwrite (handle, count, buf)
#define FSEEK(handle, offset, mode)         Fseek (offset, handle, mode)
#define FLOCK(handle, offset, count, wait)  (gemdos (92, (int)handle, (int)0, (long)offset, (long)count) == 0)
#define FUNLOCK(handle, offset, count)      (gemdos (92, (int)handle, (int)1, (long)offset, (long)count) == 0)
#define REMOVE(filename)                    (Fdelete (filename) == 0)
#define RENAME(oldname, newname)            (Frename (0, oldname, newname) == 0)
#define MKDIR(path)                         (Dcreate (path) >= 0)
#define RMDIR(path)                         (Ddelete (path) >= 0)
#endif

#if MSDOS | OS2
#ifdef _WINDOWS
#define FCREATE(filename)                   _lcreat (filename, 0)
#define FCLOSE(handle)                      _lclose (handle)
#define FREAD(handle, count, buf)           _lread (handle, buf, (INT)count)
#define FWRITE(handle, count, buf)          _lwrite (handle, buf, (INT)count)
#define FSEEK(handle, offset, mode)         _llseek (handle, offset, mode)
#else
#define FCREATE(filename)                   open (filename, O_RDWR | O_CREAT | O_TRUNC | O_BINARY, S_IREAD | S_IWRITE)
#define FCLOSE(handle)                      close (handle)
#define FREAD(handle, count, buf)           read (handle, buf, (SIZE_T)count)
#define FWRITE(handle, count, buf)          write (handle, buf, (SIZE_T)count)
#define FSEEK(handle, offset, mode)         lseek (handle, offset, mode)
#endif

#define REMOVE(filename)                    (remove (filename) == 0)
#define RENAME(oldname, newname)            (rename (oldname, newname) == 0)
#define MKDIR(path)                         (mkdir (path) == 0)
#define RMDIR(path)                         (rmdir (path) == 0)

#if MS_C
#define LOCK_CNT         1              /* retries to do locking */

#define FLOCK(handle, offset, count, wait)  (locking ((INT)handle, wait ? LK_LOCK : LK_NBLCK, count) == 0)
#define FUNLOCK(handle, offset, count)      (locking ((INT)handle, LK_UNLCK, count) == 0)
#endif

#if TURBO_C
#define LOCK_CNT        50              /* retries to do locking */

#define FLOCK(handle, offset, count, wait)  (lock ((INT)handle, offset, count) == 0)
#define FUNLOCK(handle, offset, count)      (unlock ((INT)handle, offset, count) == 0)
#endif
#endif

#if NT
#define FCREATE(filename)                   open (filename, O_RDWR | O_CREAT | O_TRUNC | O_BINARY, S_IREAD | S_IWRITE)
#define FCLOSE(handle)                      close (handle)
#define FREAD(handle, count, buf)           read (handle, buf, (SIZE_T)count)
#define FWRITE(handle, count, buf)          write (handle, buf, (SIZE_T)count)
#define FSEEK(handle, offset, mode)         lseek (handle, offset, mode)

#define REMOVE(filename)                    (remove (filename) == 0)
#define RENAME(oldname, newname)            (rename (oldname, newname) == 0)
#define MKDIR(path)                         (mkdir (path) == 0)
#define RMDIR(path)                         (rmdir (path) == 0)

#define LOCK_CNT         1              /* retries to do locking */

#define FLOCK(handle, offset, count, wait)  (locking ((INT)handle, wait ? LK_LOCK : LK_NBLCK, count) == 0)
#define FUNLOCK(handle, offset, count)      (locking ((INT)handle, LK_UNLCK, count) == 0)
#endif

#if FLEXOS
#define LOCK_CNT        20              /* retries to do locking */

#define FCREATE(filename)                   s_create (0, CDDELOLD, filename, 1, 0, 0L)
#define FCLOSE(handle)                      (s_close (FULL_FCLOSE, handle) < 0) ? FAILURE : SUCCESS
#define FREAD(handle, count, buf)           s_read (CDFILEPTR | CDDIR, handle, buf, count, 0L)
#define FWRITE(handle, count, buf)          s_write (CDFILEPTR | CDDIR, handle, buf, count, 0L)
#define FSEEK(handle, offset, mode)         s_seek ((mode << 8), handle, offset)
#define FLOCK(handle, offset, count, wait)  (s_lock (CD_LOCK, handle, 0L, count) >= 0)
#define FUNLOCK(handle, offset, count)      (s_lock (CD_UNLOCK, handle, 0L, count) >= 0)
#define REMOVE(filename)                    (Fdelete (filename) == 0)
#define RENAME(oldname, newname)            (Frename (0, oldname, newname) == 0)
#define MKDIR(path)                         (mkdir (path) == 0)
#define RMDIR(path)                         (rmdir (path) == 0)
#endif

#if UNIX
#define LOCK_CNT         1              /* retries to do locking */

#define FCREATE(filename)                   open (filename, O_RDWR | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH)
#define FCLOSE(handle)                      close (handle)
#define FREAD(handle, count, buf)           read (handle, buf, (SIZE_T)count)
#define FWRITE(handle, count, buf)          write (handle, buf, (SIZE_T)count)
#define FSEEK(handle, offset, mode)         lseek (handle, offset, mode)
#define FLOCK(handle, offset, count, wait)  (lockf (handle, wait ? F_LOCK : F_TLOCK, count) == 0)
#define FUNLOCK(handle, offset, count)      (lockf (handle, F_ULOCK, count) == 0)
#define REMOVE(filename)                    (unlink (filename) == 0)
#define RENAME(oldname, newname)            (link (oldname, newname) == 0),REMOVE (oldname)
#define MKDIR(path)                         (mkdir (path, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH) == 0)
#define RMDIR(path)                         (rmdir (path) == 0)
#endif

/****** TYPES ****************************************************************/

#if MSDOS | FLEXOS | DR_C | LASER_C | LATTICE_C | MW_C
typedef struct
{
  CHAR   d_reserved [21];
  UCHAR  d_attrib;
  USHORT d_time;
  USHORT d_date;
  ULONG  d_length;
  CHAR   d_fname [14];
} DTA;
#endif

/****** VARIABLES ************************************************************/

/****** FUNCTIONS ************************************************************/

#if UNIX
EXTERN CHAR *getcwd _((CHAR *buf, INT size));
#endif

/*****************************************************************************/

GLOBAL HFILE WINAPI file_create (filename)
LPSTR filename;

{
  return ((HFILE)FCREATE (filename));
} /* file_create */

/*****************************************************************************/

GLOBAL HFILE WINAPI file_open (filename, mode)
LPSTR filename;
INT   mode;

{
  HFILE handle;
  BOOL  shared;

  shared  = (mode & O_SHARED) != 0;
  mode   &= ~ O_SHARED;

#if GEMDOS
  mode    = shared ? SHARE_RW : mode;
  handle  = (HFILE)Fopen (filename, mode);
#endif

#if MSDOS | OS2
#ifdef _WINDOWS
  switch (mode)                         /* some compilers use other numbers */
  {
    case O_RDONLY : mode = OF_READ;      break;
    case O_WRONLY : mode = OF_WRITE;     break;
    case O_RDWR   : mode = OF_READWRITE; break;
  } /* switch */

  handle  = _lopen (filename, mode | (shared ? OF_SHARE_DENY_NONE : OF_SHARE_EXCLUSIVE));
#else
  mode   |= O_BINARY;
  handle  = sopen (filename, mode, shared ? SH_DENYNO : SH_DENYRW, 0);
#endif
#endif

#if NT				/* using compiler library because of locking */
  mode   |= O_BINARY;
  handle  = sopen (filename, mode, shared ? SH_DENYNO : SH_DENYRW, 0);
#endif

#if FLEXOS
  mode    = (mode == O_RDONLY) ? CDFREAD : (mode == O_WRONLY) ? CDFWRITE : CDFREAD | CDFWRITE;
  mode   |= shared ? CDSHARED : 0;
  handle  = s_open (mode, filename);
#endif

#if UNIX
  handle = open (filename, mode);
#endif

  return (handle);
} /* file_open */

/*****************************************************************************/

GLOBAL INT WINAPI file_close (handle)
HFILE handle;

{
  return ((INT)FCLOSE (handle));
} /* file_close */

/*****************************************************************************/

GLOBAL LONG WINAPI file_read (handle, count, buf)
HFILE   handle;
LONG    count;
HPVOID  buf;

{
  LONG bytes;

#if MSDOS | OS2
  REG LONG    ret;
  REG HPUCHAR buffer;

  buffer = (HPUCHAR)buf;
  bytes  = 0;

  while (count > 0)
  {
    ret = min (count, 0x4000);
    ret = FREAD (handle, ret, (VOID FAR *)buffer);

    if (ret > 0)
    {
      buffer += ret;
      bytes  += ret;
      count  -= ret;
    } /* if */
    else
      count = 0;
  } /* while */
#else
  bytes = FREAD (handle, count, buf);
#endif

  if (bytes < 0) bytes = 0;

  return (bytes);
} /* file_read */

/*****************************************************************************/

GLOBAL LONG WINAPI file_write (handle, count, buf)
HFILE   handle;
LONG    count;
HPVOID  buf;

{
  LONG bytes;

#if MSDOS | OS2
  REG LONG    ret;
  REG HPUCHAR buffer;

  buffer = (HPUCHAR)buf;
  bytes  = 0;

  while (count > 0)
  {
    ret = min (count, 0x4000);
    ret = FWRITE (handle, ret, (VOID FAR *)buffer);

    if (ret > 0)
    {
      buffer += ret;
      bytes  += ret;
      count  -= ret;
    } /* if */
    else
      count = 0;
  } /* while */
#else
  bytes = FWRITE (handle, count, buf);
#endif

  if (bytes < 0) bytes = 0;

  return (bytes);
} /* file_write */

/*****************************************************************************/

GLOBAL LONG WINAPI file_seek (handle, offset, mode)
HFILE   handle;
LONG    offset;
INT     mode;

{
  return (FSEEK (handle, offset, mode));
} /* file_seek */

/*****************************************************************************/

GLOBAL BOOL WINAPI file_eof (handle)
HFILE handle;

{
  BOOL eof;
  LONG cur, end;

  cur = file_seek (handle, 0L, SEEK_CUR);
  end = file_seek (handle, 0L, SEEK_END);
  eof = cur == end;
  file_seek (handle, cur, SEEK_SET);

  return (eof);
} /* file_eof */

/*****************************************************************************/

GLOBAL BOOL WINAPI file_lock (handle, offset, count, wait)
HFILE handle;
LONG  offset, count;
BOOL  wait;

{
  BOOL    res;
  INT     cnt;
#ifdef _WINDOWS
  DWORD   sysclock;
#else
  clock_t sysclock;
#endif

#if GEMDOS
  res = TRUE;
#else
  res = file_seek (handle, offset, SEEK_SET) == offset;
#endif

  if (res)
    for (cnt = 0, res = FALSE; (cnt < LOCK_CNT) && ! res; cnt++)
    {
      res = FLOCK (handle, offset, count, wait);

      if (! res)
      {
#ifdef _WINDOWS
        sysclock = GetTickCount ();
        while ((GetTickCount () - sysclock) < 100); /* wait 1/10 s */
#else
        sysclock = clock ();
        if (sysclock != -1)
          while ((clock () - sysclock) * 10 / CLK_TCK < 1); /* wait 1/10 s */
#endif
      } /* if */
    } /* for, if */

  return (res);
} /* file_lock */

/*****************************************************************************/

GLOBAL BOOL WINAPI file_unlock (handle, offset, count)
HFILE handle;
LONG  offset, count;

{
  BOOL res;

  res = file_seek (handle, offset, SEEK_SET) == offset;

  if (res) res = FUNLOCK (handle, offset, count);

  return (res);
} /* file_unlock */

/*****************************************************************************/

GLOBAL BOOL WINAPI file_locked (handle, offset, count)
HFILE handle;
LONG  offset, count;

{
  BOOL locked, ok;

  locked = FALSE;
  ok     = file_seek (handle, offset, SEEK_SET) == offset;

  if (ok)
  {
    locked = ! FLOCK (handle, offset, count, FALSE);
    if (! locked) ok = FUNLOCK (handle, offset, count);
  } /* if */

  return (locked);
} /* file_locked */

/*****************************************************************************/

GLOBAL BOOL WINAPI file_remove (filename)
LPCSTR filename;

{
  FULLNAME fullname;

#if defined (_WINDOWS) && ! defined (WIN32)
  AnsiToOem (filename, fullname);
#else
  strcpy (fullname, filename);
#endif

  return (REMOVE (fullname));
} /* file_remove */

/*****************************************************************************/

GLOBAL BOOL WINAPI file_rename (oldname, newname)
LPCSTR oldname, newname;

{
  FULLNAME oldfullname, newfullname;

#if defined (_WINDOWS) && ! defined (WIN32)
  AnsiToOem (oldname, oldfullname);
  AnsiToOem (newname, newfullname);
#else
  strcpy (oldfullname, oldname);
  strcpy (newfullname, newname);
#endif

  return (RENAME (oldfullname, newfullname));
} /* file_rename */

/*****************************************************************************/

GLOBAL LONG WINAPI file_length (filename)
LPCSTR filename;

{
  FULLNAME fullname;
  LONG     length;
  BOOL     ok;
#if GEMDOS | FLEXOS
  DTA      dta, *old_dta;
#endif

#if GEMDOS
  XATTR    xattr;
  LONG     res;

  strcpy (fullname, filename);

  if ((res = Fxattr (0, fullname, &xattr)) == -32)
  {
    old_dta = (DTA *)Fgetdta ();
    Fsetdta (&dta);

    ok     = Fsfirst (fullname, 0x00) == 0;
    length = ok ? dta.d_length : 0;

    Fsetdta (old_dta);
  } /* if */
  else
    length = (res == 0) ? xattr.size : 0;
#endif

#if FLEXOS
  strcpy (fullname, filename);

  old_dta = (DTA *)Fgetdta ();
  Fsetdta (&dta);

  ok     = Fsfirst (fullname, 0x00) > 0;
  length = ok ? dta.d_length : 0;

  Fsetdta (old_dta);
#endif

#if MSDOS | NT | UNIX | OS2
  struct stat buf;

#if defined (_WINDOWS) && ! defined (WIN32)
  AnsiToOem (filename, fullname);
#else
  strcpy (fullname, filename);
#endif

  ok     = stat (fullname, &buf) == 0;
  length = ok ? buf.st_size : 0;
#endif

  return (length);
} /* file_length */

/*****************************************************************************/

GLOBAL BOOL WINAPI file_exist (filename)
LPCSTR filename;

{
  BOOL     result;
  FULLNAME s;
#if GEMDOS | FLEXOS
  DTA      dta, *old_dta;
#endif

#if GEMDOS
  XATTR    xattr;
  LONG     res;

  strcpy (s, filename);

  if ((res = Fxattr (0, s, &xattr)) == -32)
  {
    old_dta = (DTA *)Fgetdta ();
    Fsetdta (&dta);

    result = (s [0] != EOS) && (Fsfirst (s, 0x00) == 0);

    Fsetdta (old_dta);
  } /* if */
  else
    result = res == 0;
#endif

#if FLEXOS
  strcpy (s, filename);

  old_dta = (DTA *)Fgetdta ();
  Fsetdta (&dta);

  result = (s [0] != EOS) && (Fsfirst (s, 0x00) > 0);

  Fsetdta (old_dta);
#endif

#if MSDOS | NT | OS2 | UNIX
  struct stat buf;

  strcpy (s, filename);

#if defined (_WINDOWS) && ! defined (WIN32)
  AnsiToOem (s, s);
#endif

  result = (s [0] != EOS) && (stat (s, &buf) == 0);
#endif

  return (result);
} /* file_exist */

/*****************************************************************************/

GLOBAL VOID WINAPI file_split (fullname, drive, path, filename, ext)
LPSTR fullname;
LPINT drive;
LPSTR path, filename, ext;

{
  FULLNAME s, name;
  CHAR     *p, *f;
  INT      drv;
  BOOL     bHasDrive;

  strcpy (s, fullname);

#if UNIX
  p = NULL;                                     /* no drive with  UNIX */
#else
  p = strchr (s, DRIVESEP);
#endif

  bHasDrive = p != NULL;

  if (p == NULL)                                /* no drive found */
  {
    drv = get_drive ();
    p   = s;
  } /* if */
  else
  {
#ifdef _WINDOWS
    drv = LOBYTE (LOWORD ((ULONG)AnsiUpper ((LPSTR)(ULONG)(p [-1] & 0xFF)))) - 'A';
#else
    drv = toupper (p [-1]) - 'A';
#endif
    p++;
  } /* else */

  if (drive != NULL) *drive = drv;

  f = strrchr (p, PATHSEP);

  if (f == NULL)                                /* no path found */
  {
    strcpy (name, p);                           /* save filename */

    if (path != NULL)
    {
      if (bHasDrive)
        get_dpath (drv, path);
      else
        get_path (path);

      if (bHasDrive && (drive == NULL) && ((path [0] != PATHSEP) || (path [1] != PATHSEP)))	/* add drive to path */
      {
#if GEMDOS | MSDOS | NT | OS2 | FLEXOS
        strcpy (s, "A:");
        s [0] = (CHAR)(s [0] + drv);
        strcat (s, path);
        strcpy (path, s);
#endif
      } /* if */
    } /* if */
  } /* if */
  else
  {
    strcpy (name, f + 1);                       /* save file name */
    f [1] = EOS;

    if (path != NULL)
    {
      if (*p != PATHSEP)                        /* no root found */
        if (bHasDrive)
          get_dpath (drv, path);
        else
          get_path (path);
      else
        *path = EOS;

      strcat (path, p);

      if (bHasDrive && (drive == NULL) && ((path [0] != PATHSEP) || (path [1] != PATHSEP)))	/* add drive to path */
      {
#if GEMDOS | MSDOS | NT | OS2 | FLEXOS
        strcpy (s, "A:");
        s [0] = (CHAR)(s [0] + drv);
        strcat (s, path);
        strcpy (path, s);
#endif
      } /* if */
    } /* if */
  } /* else */

  if (filename != NULL) strcpy (filename, name);

  if (ext != NULL)
  {
    p = strrchr ((filename != NULL) ? filename : name, SUFFSEP);

    if (p == NULL)
      *ext = EOS;
    else
    {
      strcpy (ext, p + 1);
      if (filename != NULL) *p = EOS;
    } /* else */
  } /* if */
} /* file_split */

/*****************************************************************************/

GLOBAL INT WINAPI get_drive ()

{
  INT drive;

#if GEMDOS | FLEXOS
  drive = Dgetdrv ();
#endif

#if MSDOS
#if MS_C
  _dos_getdrive ((UINT *)&drive);
  drive--;
#endif

#if TURBO_C
  drive = getdisk ();
#endif
#endif

#if NT
  FULLNAME sz;

  GetCurrentDirectory (sizeof (sz), sz);
  drive = sz [0] - 'A';
#endif

#if OS2
  ULONG drives;

  DosQCurDisk (&drive, &drives);
  drive--;
#endif

#if UNIX
  drive = 0;
#endif

  return (drive);
} /* get_drive */

/*****************************************************************************/

GLOBAL INT WINAPI set_drive (drive)
INT drive;

{
#if GEMDOS | FLEXOS
  return (Dsetdrv (drive));
#endif

#if MSDOS
#if MS_C
  UINT drives;

  _dos_setdrive (drive + 1, &drives);
  return (drives);
#endif

#if TURBO_C
  return (setdisk (drive));
#endif
#endif

#if NT
  CHAR sz [3];

  sz [0] = (CHAR)('A' + drive);
  sz [1] = DRIVESEP;
  sz [2] = EOS;

  SetCurrentDirectory (sz);

  return (GetLogicalDrives ());
#endif

#if OS2
  ULONG drives;

  drive++;
  DosSelectDisk (drive);
  DosQCurDisk (&drive, &drives);
  return ((INT)drives);
#endif

#if UNIX
  return (1);
#endif
} /* set_drive */

/*****************************************************************************/

GLOBAL BOOL WINAPI get_path (path)
LPSTR path;

{
#if GEMDOS | MSDOS | NT | OS2 | UNIX
  CHAR   sep [2];
  SIZE_T l;
  CHAR   *p;

  sep [0] = PATHSEP;
  sep [1] = EOS;

  p = getcwd (path, MAX_FULLNAME);
  l = strlen (path);
  if ((l > 1) && (path [l - 1] != PATHSEP)) strcat (path, sep);

#ifdef _WINDOWS
  OemToAnsi (path, path);
#endif

  return (p != NULL);
#endif

#if FLEXOS
  CHAR     sep [2];
  FULLNAME s;
  INT      ret;

  sep [0]  = PATHSEP;
  sep [1]  = EOS;
  path [0] = 'A' + get_drive ();
  path [1] = DRIVESEP;
  path [2] = PATHSEP;

  ret = Dgetpath (path + 3, 0);

  l = strlen (path);
  if ((l > 1) && (path [l - 1] != PATHSEP)) strcat (path, sep);

  return (ret == 1);
#endif
} /* get_path */

/*****************************************************************************/

GLOBAL BOOL WINAPI get_dpath (drive, path)
INT   drive;
LPSTR path;

{
  BOOL ok;
  INT  actdrive;

  actdrive = get_drive ();
  set_drive (drive);
  ok = get_path (path);

#if GEMDOS | MSDOS | NT | OS2 | FLEXOS
  strcpy (path, path + 2);
#endif

  set_drive (actdrive);

  return (ok);
} /* get_dpath */

/*****************************************************************************/

GLOBAL BOOL WINAPI set_path (path)
LPCSTR path;

{
  FULLNAME s;
  CHAR     *p;
  SIZE_T   l;

  strcpy (s, path);

#if defined (_WINDOWS) && ! defined (WIN32)
  AnsiToOem (s, s);
#endif

  if (*s)
  {
    p = s;
    if (p [1] == DRIVESEP) p += 2;

    l = strlen (p);
    if ((l > 1) && (p [l - 1] == PATHSEP)) p [l - 1] = EOS;
  } /* if */

#if GEMDOS
  return ((SHORT)Dsetpath (s) == 0);
#endif

#if FLEXOS
  Dsetpath (s);
  return (! DOS_ERR);
#endif

#if MSDOS | NT | OS2 | UNIX
  return (chdir (s) == 0);
#endif
} /* set_path */

/*****************************************************************************/

GLOBAL BOOL WINAPI path_exist (path)
LPCSTR path;

{
  BOOL     result;
  FULLNAME s;
  SIZE_T   l;
#if GEMDOS | FLEXOS
  DTA      dta, *old_dta;
#endif

#if GEMDOS
  XATTR    xattr;
  LONG     res;

  strcpy (s, path);

  l = strlen (s);

  if (l > 0)
    if (s [l - 1] == PATHSEP) s [l - 1] = EOS;

  if ((res = Fxattr (0, s, &xattr)) == -32)
  {
    old_dta = (DTA *)Fgetdta ();
    Fsetdta (&dta);

    result = (s [0] != EOS) && (Fsfirst (s, 0x10) == 0);

    if (result)
      result = (dta.d_attrib & 0x10) != 0;

    Fsetdta (old_dta);
  } /* if */
  else
    result = (res == 0) && (xattr.attr & 0x10);
#endif

#if FLEXOS
  strcpy (s, path);

  old_dta = (DTA *)Fgetdta ();
  Fsetdta (&dta);

  l = strlen (s);

  if (l > 0)
    if (s [l - 1] == PATHSEP) s [l - 1] = EOS;

  result = (s [0] != EOS) && (Fsfirst (s, 0x10) > 0);

  if (result)
    result = (dta.d_attrib & 0x10) != 0;

  Fsetdta (old_dta);
#endif

#if MSDOS | NT | OS2 | UNIX
  struct stat buf;

  strcpy (s, path);

#if defined (_WINDOWS) && ! defined (WIN32)
  AnsiToOem (s, s);
#endif

  l = strlen (s);

  if (l > 1)
    if (s [l - 1] == PATHSEP) s [l - 1] = EOS;

#if MSDOS | NT | OS2
  if ((strlen (s) == 2) && (s [1] == DRIVESEP))		/* look for root directory */
  {
    s [2] = PATHSEP;
    s [3] = EOS;
  } /* if */
#endif

  result = (s [0] != EOS) && (stat (s, &buf) == 0);

  if (result)
    result = (buf.st_mode & S_IFDIR) != 0;
#endif

  return (result);
} /* path_exist */

/*****************************************************************************/

GLOBAL BOOL WINAPI path_create (path)
LPCSTR path;

{
  FULLNAME pathname;

#if defined (_WINDOWS) && ! defined (WIN32)
  AnsiToOem (path, pathname);
#else
  strcpy (pathname, path);
#endif

  return (MKDIR (pathname));
} /* path_create */

/*****************************************************************************/

GLOBAL BOOL WINAPI path_remove (path)
LPCSTR path;

{
  FULLNAME pathname;

#if defined (_WINDOWS) && ! defined (WIN32)
  AnsiToOem (path, pathname);
#else
  strcpy (pathname, path);
#endif

  return (RMDIR (pathname));
} /* path_remove */

/*****************************************************************************/

#ifndef _WINDLL

GLOBAL BOOL WINAPI text_rdln (file, s, maxlen)
FILE   *file;
CHAR   *s;
SIZE_T maxlen;

{
  CHAR   *res;
  SIZE_T l;

  res = fgets (s, (INT)maxlen, file);
  if (res == NULL) return (FALSE);

  if ((l = strlen (s)) != 0)
  {
    l--;
    if (s [l] == '\n') s [l] = EOS;
  } /* if */

  return (TRUE);
} /* text_rdln */

/*****************************************************************************/

GLOBAL BOOL WINAPI text_write (file, s)
FILE  *file;
LPSTR s;

{
  fputs (s, file);

  return (! ferror (file));
} /* text_write */

/*****************************************************************************/

GLOBAL BOOL WINAPI text_wrln (file, s)
FILE  *file;
LPSTR s;

{
  if (! text_write (file, s)) return (FALSE);

  return (text_write (file, "\n"));
} /* text_wrln */

#endif /* _WINDLL */

/*****************************************************************************/

