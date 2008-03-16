/*****************************************************************************/
/*                                                                           */
/* Modul: BASETEST.C                                                         */
/* Datum: 18/07/93                                                           */
/*                                                                           */
/*****************************************************************************/

#include "import.h"
#include "conv.h"
#include "files.h"
#include "utility.h"

#include "dbroot.h"
#include "dbdata.h"
#include "dbtree.h"
#include "dbcall.h"

#include "export.h"

/****** DEFINES **************************************************************/

/****** TYPES ****************************************************************/

/****** VARIABLES ************************************************************/

#if GEMDOS
#if MW_C
LONG _stksize = 16384;          /* 16 KBytes stack for Mark Williams C */
#endif
#endif

#if MSDOS
#if TURBO_C
USHORT _stklen = 16384;         /* 16 KBytes stack for Turbo C */
#endif
#endif

LOCAL CHAR *moshort [] =
{
  "Jan",
  "Feb",
  "M„r",
  "Apr",
  "Mai",
  "Jun",
  "Jul",
  "Aug",
  "Sep",
  "Okt",
  "Nov",
  "Dez"
};

LOCAL CHAR *molong [] =
{
  "Januar",
  "Februar",
  "M„rz",
  "April",
  "Mai",
  "Juni",
  "Juli",
  "August",
  "September",
  "Oktober",
  "November",
  "Dezember"
};

/****** FUNCTIONS ************************************************************/

LOCAL VOID test_files  _((VOID));
LOCAL VOID test_conv   _((VOID));
LOCAL VOID test_dbcall _((VOID));

/*****************************************************************************/

#define MAIN   1
#define FILES  0
#define CONV   0
#define DBCALL 1

#if MAIN
GLOBAL INT main ()
{
#if FILES
  test_files ();
#endif

#if CONV
  test_conv ();
#endif

#if DBCALL
  test_dbcall ();
#endif

  return (0);
} /* main */
#endif

/*****************************************************************************/

#if FILES

#include <ext.h>

LOCAL VOID test_files ()

{
  FHANDLE f;
  LONG    bytes;
  SHORT   res;
  CHAR    s [80];
  struct stat buf;

  printf ("file_length (FILES.H): %ld\n", file_length ("FILES.H"));
  printf ("file_exist (FILES.H): %d\n", file_exist ("FILES.H"));
  printf ("file_exist (TEST.H): %d\n", file_exist ("TEST.H"));
  printf ("path_exist (E:\\PHOENIX\\BASE): %d\n", path_exist ("E:\\PHOENIX\\BASE"));
  printf ("path_exist (E:\\PHOENIX\\BASE\\TEST): %d\n", path_exist ("E:\\PHOENIX\\BASE\\TEST"));

#if 0
  f = file_create ("TEST");
  printf ("file_create:%d\n", f);
  if (f > 0)
  {
    strcpy (s, "Dies ist ein Test\r\n");
    bytes = file_write (f, (LONG)strlen (s), s);
    printf ("bytes(%ld):%ld\n", (LONG)strlen (s), bytes);
    res = file_close (f);
    printf ("file_close:%d\n", res);
  } /* if */

  f = file_open ("TEST", O_RDWR);
  printf ("file_open:%d\n", f);
  if (f > 0)
  {
    strcpy (s, "Dies ist ein Test\r\n");
    bytes = file_seek (f, 5L, SEEK_SET);
    printf ("bytes nach seek(%ld):%ld\n", (LONG)5, bytes);
    bytes = file_read (f, (LONG)3, s);
    s [3] = EOS;
    printf ("bytes nach read (%ld):%ld, s:%s\n", (LONG)3, bytes, s);

    bytes = file_seek (f, 4L, SEEK_SET);
    printf ("bytes nach seek(%ld):%ld\n", (LONG)4, bytes);
    strcpy (s, "XXX");
    bytes = file_write (f, (LONG)3, s);
    printf ("bytes nach write(%ld):%ld, s:%s\n", (LONG)3, bytes, s);

    res = file_close (f);
    printf ("file_close:%d\n", res);
  } /* if */
#endif
} /* test_files */

#endif

/*****************************************************************************/

#if CONV

LOCAL VOID test_conv ()

{
  BOOL      result1, result2;
  SHORT     type;
  CHAR      s [80], str [80], bin [80], formstr [80];
  FORMAT    format;
  DATE      date;
  TIME      time;
  TIMESTAMP timestamp;

  init_conv (moshort, molong, ',', '.');

  str [0] = EOS;

  while (str [0] != 'n')
  {
    printf ("Types:\n");
    printf (" 0 = CHAR\n");
    printf (" 1 = WORD\n");
    printf (" 2 = LONG\n");
    printf (" 3 = FLOAT\n");
    printf (" 4 = CHARACTER FLOAT\n");
    printf (" 5 = DATE\n");
    printf (" 6 = TIME\n");
    printf (" 7 = TIMESTAMP\n");
    printf (" 8 = VARBYTE\n");
    printf (" 9 = VARWORD\n");
    printf ("10 = VARLONG\n");
    printf ("11 = PICTURE\n");
    printf ("12 = EXTERN\n");
    printf ("13 = DBADRESS\n");
    printf ("\n");

    printf ("Type: ");
    gets (str);
    type = (str [0] == EOS) ? 0 : atoi (str);

    printf ("Enter character stream: ");
    gets (str);

    printf ("Enter format string: ");
    gets (formstr);
    build_format (type, formstr, format);

    if ((type == TYPE_VARBYTE) ||
        (type == TYPE_VARWORD) ||
        (type == TYPE_VARLONG) ||
        (type == TYPE_PICTURE))
      if (*str == EOS) set_null (type, str);

    result1 = format2str (type, str, format);
    printf ("nach format2str(%d): %s\n", result1, str);

    result1 = str2bin (type, str, bin);
    printf ("str2bin (result1 = %s): s = %s\n", result1 ? "TRUE" : "FALSE", str);

    strcpy (s, str);
    *str    = EOS;
    result2 = bin2str (type, bin, str);
    printf ("bin2str (result2 = %s): s = %s\n", result2 ? "TRUE" : "FALSE", str);
    printf ("is_null (result3 = %s): s = %s\n", is_null (type, bin) ? "TRUE" : "FALSE", str);

    str2format (type, str, format);
    printf ("str2format: s = %s\n", str);

    printf ("\n");
    strcpy (s, str);
    str_upper (s);
    printf ("str_upper (\"%s\"): %s\n", str, s);

    strcpy (s, str);
    str_lower (s);
    printf ("str_lower (\"%s\"): %s\n", str, s);

    strcpy (s, str);
    str_ascii (s);
    printf ("str_ascii (\"%s\"): %s\n", str, s);

    strcpy (s, str);
    str_ascii (s);
    str_upr (s);
    printf ("str_ascii, str_upr (\"%s\"): %s\n", str, s);

    get_date (&date);
    get_time (&time);
    get_tstamp (&timestamp);

    printf ("\n");
    printf ("DATE     : %2d/%02d/%04d\n", date.day, date.month, date.year);
    printf ("TIME     : %2d:%02d:%02d.%06ld\n", time.hour, time.minute, time.second, time.micro);
    printf ("TIMESTAMP: %2d/%02d/%04d %2d:%02d:%02d.%06ld\n",
                        date.day, date.month, date.year,
                        time.hour, time.minute, time.second, time.micro);

    printf ("\nTest more? [y/n]:");
    gets (str);
    printf ("\n\n");
  } /* while */
} /* test_conv */

#endif

/*****************************************************************************/

#if DBCALL

LOCAL SHORT std_width [NUM_TYPES] = {0, 6, 11, 14, 0, 8, 12, 20, -1, -1, -1, -1, 0, 11, -1, -1, -1, -1, -1, -1};
LOCAL BOOL  get_line _((BASE *base, SHORT table, CHAR *buffer, CHAR *s));

/*****************************************************************************/

LOCAL BOOL get_line (base, table, buffer, s)
BASE  *base;
SHORT table;
CHAR  *buffer;
CHAR  *s;

{
  BOOL       ok;
  SHORT      field;
  SHORT      size;
  SHORT      type;
  CHAR       str1 [128];
  CHAR       str2 [128];
  TABLE_INFO table_info;
  FIELD_INFO field_info;

  *s = EOS;
  ok = db_tableinfo (base, table, &table_info) != FAILURE;

  if (ok)
    for (field = 1; field < table_info.cols; field++)
    {
      db_fieldinfo (base, table, field, &field_info);
      type = field_info.type;

      if (is_null (type, buffer + field_info.addr))
        strcpy (str2, "NULL");
      else
        bin2str (type, buffer + field_info.addr, str2);

      size = std_width [field_info.type];
      if (size == 0) size = min (128, (SHORT)field_info.size);

      if (size > 0)
      {
        if (HASWILD (type))
          sprintf (str1, "%-*s", size, str2);
        else
          sprintf (str1, "%*s", size, str2);

        strcat (s, str1);
      } /* if */
      else
        strcat (s, "NOT PRINTABLE");

      strcat (s, "|");
    } /* for, if */

  return (ok);
} /* get_line */

/*****************************************************************************/

LOCAL VOID test_dbcall ()

{
  BASE       *base;
  BASENAME   basename;
  BASEPATH   basepath;
  USERNAME   username;
  PASSWORD   password;
  BASE_INFO  base_info;
  TABLE_INFO table_info;
  FIELD_INFO field_info;
  INDEX_INFO index_info;
  CHAR       filename [80];
  CHAR       *buffer, *s;
  CHAR       t [512];
  CHAR       type_str [20];
  SHORT      menu, num_cursors, i, j, k;
  LONG       datasize, treesize, cachesize, num;
  USHORT     flags;
  SHORT      table, field, inx;
  INXCOLS    inxcols;
  BOOL       ok, more;
  FILE       *f;
  SHORT      dir, status;
  LONG       address, steps;
  CURSOR     *cursor;

#if MSDOS
#if TURBO_C
  LONG mem;

  mem = mem_avail ();
  while (mem == mem_avail ()) malloc (10); /* must not be a multiple of 1024 */
#endif
#endif

  init_conv (moshort, molong, ',', '.');

  base   = NULL;
  table  = 0;
  inx    = 1;
  buffer = (CHAR *)mem_alloc (32768L);
  s      = (CHAR *)mem_alloc (32768L);

  if ((buffer == NULL) || (s == NULL)) return;

#if UNIX
  strcpy (basename, "test");
#else
  strcpy (basename, "TEST");
#endif

  strcpy (basepath, "");
  strcpy (username, "");
  strcpy (password, "");
  strcpy (filename, "TEST.ASC");
  strcpy (buffer,   "Test der L„nge 18");

  cachesize   = 63;
  num_cursors = 20;

  do
  {
    printf ("\n*** Base-Test ***\n\n");

#if GEMDOS | MSDOS | FLEXOS
    printf ("Speicher: %ld bytes\n\n", mem_avail ());
#endif

    if (base != NULL)
    {
      if (db_indexinfo (base, table, inx, &index_info) != FAILURE)
      {
        printf ("base : %s%s, cache: %ld\n", basepath, basename, cachesize);
        table = db_tableinfo (base, table, &table_info);
        str_type (index_info.type, type_str);
        printf ("table: %d = %s, indexes:%d\n", table, table_info.name, table_info.indexes);
        printf ("index: %d = %s, type: %s, num_keys: %ld\n\n", inx, index_info.indexname, type_str, index_info.num_keys);
      } /* if */
      else
        printf ("Fehler db_indexinfo\n");
    } /* if */

    printf (" 0 - Verlassen          6 - Einfgen aus Datei  12 - Liste ausgeben\n");
    printf (" 1 - Kreieren           7 - Satz einfgen       13 - Masken ausgeben\n");
    printf (" 2 - ™ffnen             8 - Satz l”schen        14 - Table/Field-Info\n");
    printf (" 3 - Schliessen         9 - Satz „ndern         15 - Puffer ausgeben\n");
    printf (" 4 - DB-Info           10 - Satz suchen         16 - Schlsselseiten schreiben\n");
    printf (" 5 - Parameter setzen  11 - Satz zurckholen    17 - Indexbaum l”schen\n");

    menu = base == NULL ? 2 : 0;

    sprintf (s, "\nWahl [%d]: ", menu);
    printf ("%s", s);
    gets (s);
    if (*s) menu = atoi (s);
    printf ("\n");

    switch (menu)
    {
      case 1 : if (base == NULL)
               {
                 printf ("Name [%s]: ", basename);
                 gets (s);
                 if (*s) strcpy (basename, s);

                 printf ("Pfad [%s]: ", basepath);
                 gets (s);
                 if (*s) strcpy (basepath, s);

                 printf ("Datendatei-Gr”sse (kb) [8]: ");
                 gets (s);
                 if (*s)
                   sscanf (s, "%ld", &datasize);
                 else
                   datasize = 8;

                 printf ("Schlsseldatei-Gr”sse (kb) [8]: ");
                 gets (s);
                 if (*s)
                   sscanf (s, "%ld", &treesize);
                 else
                   treesize = 8;

                 flags = 0;
                 printf ("Flags (hex) [%x]: ", flags);
                 gets (s);
                 if (*s) sscanf (s, "%x", &flags);

                 base = db_create (basename, basepath, flags, datasize, treesize);
               } /* if */
               break;
      case 2 : if (base == NULL)
               {
                 printf ("Name [%s]: ", basename);
                 gets (s);
                 if (*s) strcpy (basename, s);

                 printf ("Pfad [%s]: ", basepath);
                 gets (s);
                 if (*s) strcpy (basepath, s);

#if UNIX
                 flags = BASE_MULUSER;
#else
                 flags = 0;
#endif
                 printf ("flags (hex) [%x]: ", flags);
                 gets (s);
                 if (*s) sscanf (s, "%x", &flags);

                 printf ("Cache [%ld]: ", cachesize);
                 gets (s);
                 if (*s) sscanf (s, "%ld", &cachesize);

                 printf ("Anzahl der Cursoren [%d]: ", num_cursors);
                 gets (s);
                 if (*s) num_cursors = atoi (s);

                 printf ("Benutzer [%s]: ", username);
                 gets (s);
                 if (*s) strcpy (username, s);

                 printf ("Kennwort [%s]: ", password);
                 gets (s);
                 if (*s) strcpy (password, s);

                 base = db_open (basename, basepath, flags, cachesize, num_cursors, username, password);

                 if (db_status (base) != 0) printf ("error %d !!!\n", db_status (base));
                 if (base != NULL) db_fillnull (base, table, buffer);
               } /* if */
               break;
      case 3 : if (base != NULL)
               {
                 db_close (base);
                 base = NULL;
               } /* if */
               break;
      case 4 : if (base != NULL)
                 if (db_baseinfo (base, &base_info))
                 {
                   printf ("baseinfo:\n");
                   printf ("username: %s\n", base_info.username);
                   printf ("basename: %s\n", base_info.basename);
                   printf ("basepath: %s\n", base_info.basepath);

                   printf ("\ndatainfo:\n");
                   printf ("version   : %04x\n", base_info.data_info.version);
                   printf ("fieldsize : %u\n",   base_info.data_info.fieldsize);
                   printf ("file_size : %ld\n",  base_info.data_info.file_size);
                   printf ("next_rec  : %ld\n",  base_info.data_info.next_rec);
                   printf ("released  : %ld\n",  base_info.data_info.size_released);

                   bin2str (TYPE_TIMESTAMP, &base_info.data_info.created, s);
                   printf ("created   : %s\n", s);
                   bin2str (TYPE_TIMESTAMP, &base_info.data_info.lastuse, s);
                   printf ("lastuse   : %s\n", s);

                   printf ("\ntreeinfo:\n");
                   printf ("num_pages  : %ld\n", base_info.tree_info.num_pages);
                   printf ("used_pages : %ld\n", base_info.tree_info.used_pages);
                   printf ("free_page  : %ld\n", base_info.tree_info.free_page);
                   printf ("num_vpages : %ld\n", base_info.tree_info.num_vpages);
                   printf ("num_cursors: %d\n",  base_info.tree_info.num_cursors);

                   if ((base_info.tree_info.num_vpages > 0) &&
                       (base_info.tree_info.num_calls > 0))
                   {
                     printf ("\nCalls: %ld, Hits: %ld, not found: %ld -> %ld%% hit ratio\n",
                             base_info.tree_info.num_calls,
                             base_info.tree_info.num_hits,
                             base_info.tree_info.num_calls - base_info.tree_info.num_hits, base_info.tree_info.num_hits * 100 / base_info.tree_info.num_calls);
                   } /* if */
                 } /* if, if */
               break;
      case 5 : printf ("Nummer der Tabelle [%d]: ", table);
               gets (s);
               if (*s) table = atoi (s);

               printf ("Nummer des Index [%d]: ", inx);
               gets (s);
               if (*s) inx = atoi (s);

               if (base != NULL) db_fillnull (base, table, buffer);
               break;
      case 6 : if (base != NULL)
               {
                 db_tableinfo (base, table, &table_info);

                 printf ("Dateiname [%s]: ", filename);
                 gets (s);
                 if (*s) strcpy (filename, s);
                 f = fopen (filename, "r");

                 if (f != NULL)
                 {
                   num    = 0;
                   ok     = TRUE;
                   more   = TRUE;

                   while (ok && more)
                   {
                     mem_lset (buffer, 0, table_info.size);

                     for (i = 1; more && (i < table_info.cols); i++)
                     {
                       db_fieldinfo (base, table, i, &field_info);

                       more = text_rdln (f, s, 512);
                       if (HASWILD (field_info.type)) s [field_info.size] = EOS;

                       if (more)
                         if ((table == SYS_INDEX) && (i == 7)) /* inxcols */
                         {
                           str2bin (TYPE_WORD, s, &j);
                           inxcols.size = j * sizeof (INXCOL);

                           for (k = 0; k < j; k++)
                           {
                             more = text_rdln (f, s, 512);
                             sscanf (s, "%d %d", &inxcols.cols [k].col, &inxcols.cols [k].len);
                           } /* for */

                           mem_move (buffer + field_info.addr, &inxcols, (USHORT)typesize (field_info.type, &inxcols));
                         } /* if */
                         else
                           str2bin (field_info.type, s, buffer + field_info.addr);
                     } /* for */

                     if (more)
                     {
                       ok = db_insert (base, table, buffer, &status);
                       printf ("%ld\r", ++num);
                       fflush (stdout);
                     } /* if */
                   } /* while */

                   printf ("\n");
                   fclose (f);
                 } /* if */
                 else
                   printf ("Datei konnte nicht ge”ffnet werden!\n");
               } /* if */
               break;
      case  7: if (base != NULL)
                 if (db_tableinfo (base, table, &table_info) != FAILURE)
                 {
                   printf ("Satz eingeben:\n");

                   for (field = 1; field < table_info.cols; field++)
                     if (db_fieldinfo (base, table, field, &field_info) != FAILURE)
                     {
                       printf ("%-20s: ", field_info.name);
                       gets (s);

                       if (HASWILD (field_info.type)) s [field_info.size - 1] = EOS;

                       str2bin (field_info.type, s, &buffer [field_info.addr]);
                     } /* if, for */

                   db_insert (base, table, buffer, &status);
                 } /* if, if */
               break;
      case  8: if (base != NULL)
                 if (db_indexinfo (base, table, inx, &index_info) != FAILURE)
                 {
                   printf ("Satz l”schen, zun„chst suchen\n");

                   for (i = 0; i < index_info.inxcols.size / sizeof (INXCOL); i++)
                   {
                     field = index_info.inxcols.cols [i].col;
                     db_fieldinfo (base, table, field, &field_info);
                     printf ("%-20s: ", field_info.name);
                     gets (s);
                     if (HASWILD (field_info.type)) s [index_info.inxcols.cols [i].len] = EOS;
                     str2bin (field_info.type, s, &buffer [field_info.addr]);
                   } /* for */

                   cursor = db_newcursor (base);
                   if (cursor != NULL)
                   {
                     if (! db_search (base, table, inx, ASCENDING, cursor, buffer, 0L))
                       printf ("Datensatz nicht gefunden!\n");
                     else
                     {
                       address = db_readcursor (base, cursor, NULL);

                       if (address != 0)
                         if (db_reclock (base, address)) db_delete (base, table, address, &status);

                       if (db_status (base) != 0) printf ("error %d !!!\n", db_status (base));
                     } /* else */

                     db_freecursor (base, cursor);
                   } /* if */
                 } /* if, if */
               break;
      case  9: if (base != NULL)
                 if ((db_tableinfo (base, table, &table_info) != FAILURE) &&
                     (db_indexinfo (base, table, inx, &index_info) != FAILURE))
                 {
                   printf ("Satz „ndern, zun„chst suchen\n");

                   for (i = 0; i < index_info.inxcols.size / sizeof (INXCOL); i++)
                   {
                     field = index_info.inxcols.cols [i].col;
                     db_fieldinfo (base, table, field, &field_info);
                     printf ("%-20s: ", field_info.name);
                     gets (s);
                     if (HASWILD (field_info.type)) s [index_info.inxcols.cols [i].len] = EOS;
                     str2bin (field_info.type, s, &buffer [field_info.addr]);
                   } /* for */

                   cursor = db_newcursor (base);
                   if (cursor != NULL)
                   {
                     if (! db_search (base, table, inx, ASCENDING, cursor, buffer, 0L))
                       printf ("Datensatz nicht gefunden!\n");
                     else
                       if (db_read (base, table, buffer, cursor, 0L, TRUE))
                       {
                         printf ("\n");

                         for (field = 1; field < table_info.cols; field++)
                           if (db_fieldinfo (base, table, field, &field_info) != FAILURE)
                           {
                             bin2str (field_info.type, &buffer [field_info.addr], s);
                             printf ("%-20s [%s]: ", field_info.name, s);
                             gets (t);

                             if (*t)
                             {
                               strcpy (s, t);
                               if (HASWILD (field_info.type)) s [field_info.size] = EOS;
                               str2bin (field_info.type, s, &buffer [field_info.addr]);
                             } /* if */
                           } /* if, for */

                         db_update (base, table, buffer, &status);
                       } /* if, else */

                       if (db_status (base) != 0) printf ("error %d !!!\n", db_status (base));

                     db_freecursor (base, cursor);
                   } /* if */
                 } /* if, if */
               break;
      case 10: if (base != NULL)
                 if ((db_tableinfo (base, table, &table_info) != FAILURE) &&
                     (db_indexinfo (base, table, inx, &index_info) != FAILURE))
                 {
                   printf ("Satz suchen\n");

                   for (i = 0; i < index_info.inxcols.size / sizeof (INXCOL); i++)
                   {
                     field = index_info.inxcols.cols [i].col;
                     db_fieldinfo (base, table, field, &field_info);
                     printf ("%s: ", field_info.name);
                     gets (s);
                     if (HASWILD (field_info.type)) s [index_info.inxcols.cols [i].len] = EOS;
                     str2bin (field_info.type, s, &buffer [field_info.addr]);
                   } /* for */

                   cursor = db_newcursor (base);
                   if (cursor != NULL)
                   {
                     if (! db_search (base, table, inx, ASCENDING, cursor, buffer, 0L))
                       printf ("Datensatz nicht gefunden!\n");
                     else
                       if (db_read (base, table, buffer, cursor, 0L, FALSE))
                       {
                         printf ("\n");

                         for (field = 0; field < table_info.cols; field++)
                           if (db_fieldinfo (base, table, field, &field_info) != FAILURE)
                           {
                             bin2str (field_info.type, &buffer [field_info.addr], s);
                             printf ("%-20s: %s\n", field_info.name, s);
                           } /* if, for */
                       } /* if, else */

                     db_freecursor (base, cursor);
                   } /* if */
                 } /* if, if */
               break;
      case 11: if (base != NULL)
               {
                 printf ("Adresse des zurckzuholenden Datensatzes: ");
                 gets (s);
                 if (*s)
                 {
                   sscanf (s, "%ld", &address);
                   db_undelete (base, address, &status);
                 } /* if */
               } /* if */
               break;
      case 12: if (base != NULL)
                 if ((db_tableinfo (base, table, &table_info) != FAILURE) &&
                     (db_indexinfo (base, table, inx, &index_info) != FAILURE))
                 {
                   printf ("Richtung (1 = aufsteigend, -1 = absteigend) [1]: ");
                   gets (s);
                   if (*s)
                     dir = atoi (s);
                   else
                     dir = 1;

                   printf ("Sprungweite [%d]: ", dir);
                   gets (s);
                   if (*s)
                     sscanf (s, "%ld", &steps);
                   else
                     steps = dir;

                   cursor = db_newcursor (base);
                   if (cursor != NULL)
                   {
                     if (db_initcursor (base, table, inx, dir, cursor))
                       while (db_movecursor (base, cursor, steps))
                         if (db_read (base, table, buffer, cursor, 0L, FALSE))
                         {
                           get_line (base, table, buffer, s);
                           printf ("%s\n", s);
                         } /* if, while, if */

                     if (db_status (base) != 0) printf ("error %d !!!\n", db_status (base));
                     db_freecursor (base, cursor);
                   } /* if */
                 } /* if, if */
               break;
      case 13: if (base != NULL)
                 if ((db_tableinfo (base, table, &table_info) != FAILURE) &&
                     (db_indexinfo (base, table, inx, &index_info) != FAILURE))
                 {
                   printf ("Richtung (1 = aufsteigend, -1 = absteigend) [1]: ");
                   gets (s);
                   if (*s)
                     dir = atoi (s);
                   else
                     dir = 1;

                   printf ("Sprungweite [%d]: ", dir);
                   gets (s);
                   if (*s)
                     sscanf (s, "%ld", &steps);
                   else
                     steps = dir;

                   cursor = db_newcursor (base);
                   if (cursor != NULL)
                   {
                     if (db_initcursor (base, table, inx, dir, cursor))
                       while (db_movecursor (base, cursor, steps))
                         if (db_read (base, table, buffer, cursor, 0L, FALSE))
                         {
                           for (field = 0; field < table_info.cols; field++)
                             if (db_fieldinfo (base, table, field, &field_info) != FAILURE)
                             {
                               bin2str (field_info.type, &buffer [field_info.addr], s);
                               printf ("%-20s: %s\n", field_info.name, s);
                             } /* if, for */

                           printf ("------------------------------------------------------------\n");
                         } /* if, while, if */

                     if (db_status (base) != 0) printf ("error %d !!!\n", db_status (base));
                     db_freecursor (base, cursor);
                   } /* if */
                 } /* if, if */
               break;
      case 14: if (base != NULL)
               {
                 if (table == FAILURE)
                 {
                   printf ("Name der Tabelle: ");
                   gets (s);
                   if (*s)
                     strcpy (table_info.name, s);
                   else
                     strcpy (table_info.name, "SYS_TABLE");
                 } /* if */

                 table = db_tableinfo (base, table, &table_info);

                 if (table != FAILURE)
                 {
                   printf ("TABLE INFO:\n");
                   printf ("table   : %d\n",   table);
                   printf ("name    : %s\n",   table_info.name);
                   printf ("recs    : %ld\n",  table_info.recs);
                   printf ("cols    : %d\n",   table_info.cols);
                   printf ("indexes : %d\n",   table_info.indexes);
                   printf ("size    : %ld\n",  table_info.size);
                   printf ("color   : %d\n",   table_info.color);
                   printf ("icon    : %d\n",   table_info.icon);
                   printf ("children: %d\n",   table_info.children);
                   printf ("parents : %d\n",   table_info.parents);
                   printf ("flags   : %04x\n", table_info.flags);
                   printf ("\n");
                   printf ("FIELD INFO:\n");
                   printf ("name                             type      addr     size flags\n");
                   printf ("==============================================================\n");

                   for (field = 0; field < table_info.cols; field++)
                     if (db_fieldinfo (base, table, field, &field_info) != FAILURE)
                     {
                       str_type (field_info.type, type_str);
                       printf ("%-32s %-5s %8ld %8ld  %04x\n",
                               field_info.name,
                               type_str,
                               field_info.addr,
                               field_info.size,
                               field_info.flags);
                     } /* if, for */
                 } /* if */
                 else
                   printf ("FAILURE db_table/field info\n");
               } /* if */
               break;
      case 15: if (base != NULL)
                 if (db_tableinfo (base, table, &table_info) != FAILURE)
                 {
                   for (field = 0; field < table_info.cols; field++)
                     if (db_fieldinfo (base, table, field, &field_info) != FAILURE)
                     {
                       bin2str (field_info.type, &buffer [field_info.addr], s);
                       printf ("%-20s: %s\n", field_info.name, s);
                     } /* if, for */
                 } /* if, if */
               break;
      case 16: if (base != NULL) db_flush (base, TRUE, TRUE);
               break;
      case 17: if (base != NULL)
                 if (db_indexinfo (base, table, inx, &index_info) != FAILURE)
                   db_killtree (base, table, inx);
    } /* switch */

    if (db_status (base) != 0) printf ("error %d !!!\n", db_status (base));
  } while (menu != 0);

  if (base != NULL)
  {
    db_close (base);
    if (db_status (NULL) != 0) printf ("error %d !!!\n", db_status (NULL));
  } /* if */

  mem_free (s);
  mem_free (buffer);
} /* test_dbcall */

#endif

