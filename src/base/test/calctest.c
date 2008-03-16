/*****************************************************************************
 *
 * Module : CALCTEST.C
 * Author : Jrgen Geiž
 *
 * Creation date    : 11.09.91
 * Last modification: $Id$
 *
 *
 * Description: This module test the calc functions db_compile & db_execute
 *
 * History:
 * 26.04.93: Adapted for new compiler by Purex
 * 11.09.91: Creation of body
 *****************************************************************************/

#include "import.h"
#include "conv.h"
#include "files.h"
#include "utility.h"

#include "dbroot.h"
#include "dbdata.h"
#include "dbtree.h"
#include "dbcall.h"
#include "dbcalc.h"

#include "export.h"

/****** DEFINES **************************************************************/

/****** TYPES ****************************************************************/

/****** VARIABLES ************************************************************/

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

LOCAL VOID test_calc (VOID);

GLOBAL VOID graf_mkstate (WORD FAR *pmx, WORD FAR *gpmy, WORD FAR *pmstate, WORD FAR *pkstate);
GLOBAL WORD open_alert   (BYTE *alertmsg);

/*****************************************************************************/

GLOBAL INT main ()
{
  test_calc ();

  return (0);
} /* main */

/*****************************************************************************/

LOCAL VOID test_calc ()

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
  SYSCALC    *syscalc;
  CHAR       filename [80];
  CHAR       *buffer, *s;
  CHAR       t [512];
  CHAR       type_str [20];
  SHORT      menu, num_cursors, i;
  LONG       cachesize;
  USHORT     flags;
  SHORT      table, field, inx;
  SHORT      err, line, col;
  SHORT      dir, status;
  LONG       address, steps;
  CURSOR     *cursor;

  init_conv (moshort, molong, ',', '.');

  base    = NULL;
  table   = NUM_SYSTABLES;
  inx     = 1;
  buffer  = (CHAR *)mem_alloc (32768L);
  s       = (CHAR *)mem_alloc (32768L);
  syscalc = (SYSCALC *)mem_alloc (sizeof (SYSCALC));

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
  strcpy (buffer,   "");
  strcpy (syscalc->name, "Rechnung 1");
  strcpy (syscalc->text, "Testtext = 'Hallo';");
  set_null (TYPE_LONG,    &syscalc->address);
  set_null (TYPE_VARBYTE, &syscalc->code);
  syscalc->flags = 0;

  cachesize   = 63;
  num_cursors = 20;

  do
  {
    printf ("\n*** Calc-Test ***\n\n");

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

    printf (" 0 - Verlassen          6 - Compile             12 - Liste ausgeben\n");
    printf (" 1 - Calc Parameter     7 - Execute             13 - Masken ausgeben\n");
    printf (" 2 - ™ffnen             8 - SYSCALC sichern     14 - Table/Field-Info\n");
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
      case 1 : table = db_tableinfo (base, table, &table_info);
               strcpy (syscalc->tablename, table_info.name);
               printf ("Table: %s\n", syscalc->tablename);

               printf ("Name [%s]: ", syscalc->name);
               gets (s);
               if (*s) strcpy (syscalc->name, s);

               printf ("Calctext [%s]: ", syscalc->text);
               gets (s);
               if (*s) strcpy (syscalc->text, s);
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
                 flags = BASE_MULTI;
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
                 err = db_compile (base, table, syscalc, &line, &col, NULL);
                 printf ("\nerr: %d, line: %d, col: %d\n", err, line, col);
               } /* if */
               break;
      case  7: if (base != NULL)
                 db_execute (base, table, &syscalc->code, buffer, 1L, NULL);
               break;
      case  8: if (base != NULL) db_insert (base, SYS_CALC, syscalc, &status);
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
                         if (db_status (base) != 0) printf ("error %d !!!\n", db_status (base));
                       } /* if, else */

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
                           /*get_line (base, table, buffer, s);*/
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
      case 16: if (base != NULL) db_flush (base);
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

/*****************************************************************************/

GLOBAL VOID graf_mkstate _((WORD FAR *pmx, WORD FAR *gpmy, WORD FAR *pmstate, WORD FAR *pkstate))
{
} /* graf_mkstate */

/*****************************************************************************/

GLOBAL WORD open_alert (BYTE *alertmsg)
{
  puts (alertmsg);
  
  return (0);
} /* open_alert */

/*****************************************************************************/

