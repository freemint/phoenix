/*****************************************************************************
 *
 * Module : MIMPEXP.C
 * Author : Dieter Geiž
 *
 * Creation date    : 01.07.89
 * Last modification: $Id$
 *
 *
 * Description: This module implements the import/export dialog box.
 *
 * History:
 * 04.04.94: Using new syntax for inf files
 * 16.11.93: Using new file selector
 * 22.08.93: Modifications for user defined buttons added
 * 01.07.89: Creation of body
 *****************************************************************************/

#include <ctype.h>

#include "import.h"
#include "global.h"
#include "windows.h"

#include "manager.h"

#include "database.h"
#include "root.h"

#include "desktop.h"
#include "controls.h"
#include "dialog.h"
#include "impexp.h"
#include "resource.h"

#include <keytab.h>

#include "export.h"
#include "mimpexp.h"

/****** DEFINES **************************************************************/

#define DEF_RECSEP      "13,10"
#define DEF_COLSEP      "13,10"
#define DEF_TXTSEP      ""

/****** TYPES ****************************************************************/

/****** VARIABLES ************************************************************/

LOCAL BYTE *szImportExport = "ImportExport";

/****** FUNCTIONS ************************************************************/

LOCAL VOID    click_imexparm _((WINDOWP window, MKINFO *mk));
LOCAL BOOLEAN key_imexparm   _((WINDOWP window, MKINFO *mk));
LOCAL LONG    KeyTabExport_callback _((WORD wh, OBJECT *tree, WORD obj, WORD msg, LONG index, VOID *p));

LOCAL BYTE    KeyTabExport[40][38];          /* Pointer to the export filter */
LOCAL WORD    KeyTabExportMax;

/*****************************************************************************/

GLOBAL BOOLEAN init_mimpexp ()

{
	WORD i;
	
	KeyTabExportMax = Akt_getExpMaxNr ();

  if ( KeyTabExportMax != -1 )
	{
		for ( i = 0; i < KeyTabExportMax; ++i )
		{
			strcpy ( &KeyTabExport[i][38], Akt_getExpNameFromNr( i ));
    }

 		ListBoxSetCallback (imexparm, EXEXPORTFILTER, KeyTabExport_callback);
	  ListBoxSetStyle (imexparm, EXEXPORTFILTER, LBS_VREALTIME, TRUE);
	  ListBoxSetCount (imexparm, EXEXPORTFILTER, KeyTabExportMax, NULL);
	  ListBoxSetCurSel (imexparm, EXEXPORTFILTER, 0);
	  impexpcfg.exportfilter = 0;
	}
	else
	{
		strncpy (get_str (imexparm, EXEXPORTFILTER), "KeyTab not present", 38);
/*	  set_str ( imexparm, EXEXPORTFILTER, "KeyTab not present" ); */
	  do_state (imexparm, EXEXPORTFILTER, DISABLED);
	  do_state (imexparm, EXEXPORTFILTER - 1, DISABLED);
	  impexpcfg.exportfilter = -1000;
	}

  set_ptext (imexparm, EXRECSEP, DEF_RECSEP);
  set_ptext (imexparm, EXCOLSEP, DEF_COLSEP);
  set_ptext (imexparm, EXTXTSEP, DEF_TXTSEP);

  get_impexp (&impexpcfg);

  if (is_state (imexparm, EXBINARY, SELECTED))
    do_state (imexparm, EXCOLNAM, DISABLED);


  strcpy (exp_path, app_path);

  return (TRUE);
} /* init_mimpexp */

/*****************************************************************************/

GLOBAL BOOLEAN term_mimpexp ()

{
  return (TRUE);
} /* term_mimpexp */

/*****************************************************************************/

GLOBAL VOID get_impexp (impexpcfg)
IMPEXPCFG *impexpcfg;

{
  impexpcfg->exportfilter = ListBoxGetCurSel (imexparm, EXEXPORTFILTER );

  get_ptext (imexparm, EXRECSEP, impexpcfg->recsep);
  get_ptext (imexparm, EXCOLSEP, impexpcfg->colsep);
  get_ptext (imexparm, EXTXTSEP, impexpcfg->txtsep);
  get_ptext (imexparm, EXDECIMA, impexpcfg->decimal);

  impexpcfg->asciiformat = get_rbutton (imexparm, EXBINARY);
  impexpcfg->colname     = get_checkbox (imexparm, EXCOLNAM);
  impexpcfg->dateformat  = get_rbutton (imexparm, EXDDMMYY);
  impexpcfg->mode        = get_rbutton (imexparm, EXINSERT);
} /* get_impexp */

/*****************************************************************************/

GLOBAL VOID set_impexp (impexpcfg)
IMPEXPCFG *impexpcfg;

{
  LOCAL STRING s;

  ListBoxSetCurSel (imexparm, EXEXPORTFILTER, impexpcfg->exportfilter);
  ListBoxGetText (imexparm, EXEXPORTFILTER, impexpcfg->exportfilter, s);
  strncpy (get_str (imexparm, EXEXPORTFILTER), s, 38);

  set_ptext (imexparm, EXRECSEP, impexpcfg->recsep);
  set_ptext (imexparm, EXCOLSEP, impexpcfg->colsep);
  set_ptext (imexparm, EXTXTSEP, impexpcfg->txtsep);
  set_ptext (imexparm, EXDECIMA, impexpcfg->decimal);

  set_rbutton (imexparm, impexpcfg->asciiformat, EXBINARY, EXASCII);
  set_checkbox (imexparm, EXCOLNAM, impexpcfg->colname);
  set_rbutton (imexparm, impexpcfg->dateformat, EXDDMMYY, EXNUMBER);
  set_rbutton (imexparm, impexpcfg->mode, EXINSERT, EXUPDATE);

  if (is_state (imexparm, EXBINARY, SELECTED) == ! is_state (imexparm, EXCOLNAM, DISABLED))
    flip_state (imexparm, EXCOLNAM, DISABLED);

  undo_state (imexparm, EXOK, DISABLED);
  undo_state (imexparm, EXSAVE, DISABLED);
} /* set_impexp */

/*****************************************************************************/

GLOBAL BOOLEAN load_impexp (loadinf, loadname, cfg, updt_dialog)
BYTE      *loadinf, *loadname;
IMPEXPCFG *cfg;
BOOLEAN   updt_dialog;

{
  LONGSTR  s;
  BYTE     *pInf;
  FILE     *file;
  FULLNAME filename;
  WINDOWP  window;

  filename [0] = EOS;

  if (loadinf != NULL)
    pInf = loadinf;
  else
  {
    if ((loadname != NULL) && (*loadname != EOS))
      strcpy (filename, loadname);
    else
      if (! get_open_filename (FLOADIMP, NULL, 0L, FFILTER_IMP, NULL, cfg_path, FIMPSUFF, filename, NULL))
        return (FALSE);

    pInf = ReadInfFile (filename);

    if (pInf == NULL)
    {
      file_error (ERR_FILEOPEN, filename);
      return (FALSE);
    } /* if */
  } /* else */

  busy_mouse ();

  if (FindSection (pInf, szImportExport) != NULL)
  {
    GetProfileString (pInf, szImportExport, "RecSep", cfg->recsep, cfg->recsep);
    GetProfileString (pInf, szImportExport, "ColSep", cfg->colsep, cfg->colsep);
    GetProfileString (pInf, szImportExport, "TxtSep", cfg->txtsep, cfg->txtsep);
    GetProfileString (pInf, szImportExport, "Decimal", cfg->decimal, cfg->decimal);
    cfg->asciiformat = GetProfileWord (pInf, szImportExport, "Format", cfg->asciiformat) + EXBINARY;
    cfg->colname     = GetProfileBool (pInf, szImportExport, "ColName", cfg->colname);
    cfg->dateformat  = GetProfileWord (pInf, szImportExport, "DateFormat", cfg->dateformat) + EXDDMMYY;
    cfg->mode        = GetProfileWord (pInf, szImportExport, "Mode", cfg->mode) + EXINSERT;
    cfg->exportfilter= GetProfileWord (pInf, szImportExport, "ExportFilter", cfg->exportfilter);
    if ( Akt_getKeyTab () == NULL )						/* KEYTAB not present */
    	cfg->exportfilter = -1000;
  } /* if */
  else
  {
    file = fopen (filename, READ_TXT);

    text_rdln (file, cfg->recsep, LONGSTRLEN);
    text_rdln (file, cfg->colsep, LONGSTRLEN);
    text_rdln (file, cfg->txtsep, LONGSTRLEN);
    text_rdln (file, cfg->decimal, LONGSTRLEN);
    text_rdln (file, s, LONGSTRLEN);
    sscanf (s, "%d %d %d %d\n", &cfg->asciiformat, &cfg->colname, &cfg->dateformat, &cfg->mode);
    cfg->asciiformat += EXBINARY;
    cfg->dateformat  += EXDDMMYY;
    cfg->mode        += EXINSERT;

    fclose (file);
   	cfg->exportfilter = -1000;
    save_impexp (NULL, filename, cfg);	/* save as new format */
  } /* else */

  if (updt_dialog)
  {
    set_impexp (cfg);

    window = search_window (CLASS_DIALOG, SRCH_OPENED, IMEXPARM);
    if (window != NULL) set_redraw (window, &window->scroll);
  } /* if */

  if (loadinf == NULL) mem_free (pInf);

  arrow_mouse ();
  return (TRUE);
} /* load_impexp */

/*****************************************************************************/

GLOBAL BOOLEAN save_impexp (savefile, savename, cfg)
FILE      *savefile;
BYTE      *savename;
IMPEXPCFG *cfg;

{
  FILE     *file;
  FULLNAME filename;

  filename [0] = EOS;

  if (savefile != NULL)
    file = savefile;
  else
  {
    if (savename != NULL)
      strcpy (filename, savename);
    else
      if (! get_save_filename (FSAVEIMP, NULL, 0L, FFILTER_IMP, NULL, cfg_path, FIMPSUFF, filename, NULL))
        return (FALSE);

    file = fopen (filename, WRITE_TXT);

    if (file == NULL)
    {
      file_error (ERR_FILECREATE, filename);
      return (FALSE);
    } /* if */
  } /* else */

  busy_mouse ();

  fprintf (file, "[%s]\n", szImportExport);
  fprintf (file, "RecSep=%s\n", cfg->recsep);
  fprintf (file, "ColSep=%s\n", cfg->colsep);
  fprintf (file, "TxtSep=%s\n", cfg->txtsep);
  fprintf (file, "Decimal=%s\n", cfg->decimal);
  fprintf (file, "Format=%d\n", cfg->asciiformat - EXBINARY);
  fprintf (file, "ColName=%d\n", cfg->colname);
  fprintf (file, "DateFormat=%d\n", cfg->dateformat - EXDDMMYY);
  fprintf (file, "Mode=%d\n", cfg->mode - EXINSERT);
  fprintf (file, "ExportFilter=%d\n", cfg->exportfilter);

  if (savefile == NULL) fclose (file);

  arrow_mouse ();
  return (TRUE);
} /* save_impexp */

/*****************************************************************************/

GLOBAL VOID mimpexp ()

{
  WINDOWP window;
  WORD    ret;

  window = search_window (CLASS_DIALOG, SRCH_ANY, IMEXPARM);

  if (window == NULL)
  {
    form_center (imexparm, &ret, &ret, &ret, &ret);
    window = crt_dialog (imexparm, NULL, IMEXPARM, FREETXT (FIMEXPAR), wi_modeless);

    if (window != NULL)
    {
      window->click = click_imexparm;
      window->key   = key_imexparm;
    } /* if */
  } /* if */

  if (window != NULL)
  {
    if (window->opened == 0)
    {
      window->edit_obj = find_flags (imexparm, ROOT, EDITABLE);
      window->edit_inx = NIL;
      set_impexp (&impexpcfg);
    } /* if */

    if (! open_dialog (IMEXPARM)) hndl_alert (ERR_NOOPEN);
  } /* if */
} /* mimpexp */

/*****************************************************************************/

GLOBAL VOID mimport (db, table, filename, minimize)
DB      *db;
WORD    table;
BYTE    *filename;
BOOLEAN minimize;

{
  WORD       cols, *columns, *colwidth;
  TABLE_INFO tinf;

  if (v_tableinfo (db, table, &tinf) == FAILURE)
    dbtest (db);
  else
  {
    columns = (WORD *)mem_alloc (2L * tinf.cols * sizeof (WORD));  /* mem_free in module list */

    if (columns == NULL)
      hndl_alert (ERR_NOMEMORY);
    else
    {
      set_meminfo ();
      colwidth = columns + tinf.cols;
      cols     = init_columns (db, rtable (table), tinf.cols, columns, colwidth);

      if (! imp_list (db, rtable (table), cols, columns, colwidth, filename, minimize))
      {
        mem_free (columns);
        set_meminfo ();
      } /* if */
    } /* else */
  } /* else */
} /* mimport */

/*****************************************************************************/

GLOBAL VOID mexport (db, table, index, dir, filename, minimize)
DB      *db;
WORD    table, index, dir;
BYTE    *filename;
BOOLEAN minimize;
{
  WORD       cols, *columns, *colwidth;
  TABLE_INFO tinf;

  if (v_tableinfo (db, table, &tinf) == FAILURE)
    dbtest (db);
  else
  {
    columns = (WORD *)mem_alloc (2L * tinf.cols * sizeof (WORD));  /* mem_free in module list */

    if (columns == NULL)
      hndl_alert (ERR_NOMEMORY);
    else
    {
      set_meminfo ();
      colwidth = columns + tinf.cols;
      cols     = init_columns (db, rtable (table), tinf.cols, columns, colwidth);

      if (! exp_list (db, table, index, dir, cols, columns, colwidth, filename, minimize))
      {
        mem_free (columns);
        set_meminfo ();
      } /* if */
    } /* else */
  } /* else */
} /* mexport */

/*****************************************************************************/

LOCAL VOID click_imexparm (window, mk)
WINDOWP window;
MKINFO  *mk;

{
  IMPEXPCFG saveimpexp;

  switch (window->exit_obj)
  {
    case EXOK     : get_impexp (&impexpcfg);
                    break;
    case EXCANCEL : set_impexp (&impexpcfg);
                    break;
    case EXHELP   : hndl_help (HIMEXPAR);
                    undo_state (window->object, window->exit_obj, SELECTED);
                    draw_object (window, window->exit_obj);
                    break;
    case EXLOAD   : load_impexp (NULL, NULL, &saveimpexp, TRUE);
                    undo_state (window->object, window->exit_obj, SELECTED);
                    draw_object (window, window->exit_obj);
                    break;
    case EXSAVE   : get_impexp (&saveimpexp);
                    save_impexp (NULL, NULL, &saveimpexp);
                    undo_state (window->object, window->exit_obj, SELECTED);
                    draw_object (window, window->exit_obj);
                    break;
    case EXEXPORTFILTER :
                    ListBoxSetComboRect (window->object, window->exit_obj, NULL, KeyTabExportMax);
                    ListBoxSetSpec (window->object, window->exit_obj, (LONG)window);
                    ListBoxComboClick (window->object, window->exit_obj, mk);
                    undo_state (window->object, window->exit_obj, SELECTED);
                    draw_object (window, window->exit_obj);
                    break;
  } /* switch */

  if (is_state (imexparm, EXBINARY, SELECTED) == ! is_state (imexparm, EXCOLNAM, DISABLED))
  {
    flip_state (imexparm, EXCOLNAM, DISABLED);
    draw_object (window, EXCOLNAM);
  } /* if */
} /* click_imexparm */

/*****************************************************************************/

LOCAL BOOLEAN key_imexparm (window, mk)
WINDOWP window;
MKINFO  *mk;

{
  BYTE    *p1, *p2, *p3, *p4;
  STRING  u1, u2, u3;
  BOOLEAN ok;

  switch (window->edit_obj)
  {
    case EXRECSEP :
    case EXCOLSEP :
    case EXTXTSEP :
    case EXDECIMA : p1 = get_str (imexparm, EXRECSEP);
                    p2 = get_str (imexparm, EXCOLSEP);
                    p3 = get_str (imexparm, EXTXTSEP);
                    p4 = get_str (imexparm, EXDECIMA);

                    ok = str2ucsd (p1, u1);
                    ok = str2ucsd (p2, u2) && ok;
                    ok = str2ucsd (p3, u3) && ok;

                    if ((! ok ||
                         (*p1 == EOS) ||
                         (*p2 == EOS) ||
                         (*p4 == EOS) ||
                         (LEN (u3) != 0) && ((u3 [1] == *p4) || (u3 [1] == '-') || isdigit (u3 [1])) ||
                         same_ucsd (u2, u3)) == ! is_state (imexparm, EXOK, DISABLED))
                    {
                      flip_state (imexparm, EXOK, DISABLED);
                      draw_object (window, EXOK);
                      flip_state (imexparm, EXSAVE, DISABLED);
                      draw_object (window, EXSAVE);
                     } /* if */
                    break;
  } /* switch */

  return (FALSE);
} /* key_imexparm */

/*****************************************************************************/

LOCAL LONG KeyTabExport_callback (WORD wh, OBJECT *tree, WORD obj, WORD msg, LONG index, VOID *p)
{
  WINDOWP      window;
  LOCAL STRING s;

  window = (WINDOWP)ListBoxGetSpec (tree, obj);

  switch (msg)
  {
    case LBN_GETITEM    : return ((LONG)&KeyTabExport[index][0]);
    case LBN_DRAWITEM   : break;
    case LBN_SELCHANGE  : ListBoxGetText (tree, obj, index, s);
                          strncpy (get_str (tree, obj), s, 38);
                          ListBoxSetStyle (tree, obj, LBS_COMBOTEXTREDRAW, TRUE);
                          draw_win_obj (window, tree, obj);
                          ListBoxSetStyle (tree, obj, LBS_COMBOTEXTREDRAW, FALSE);
                          break;
    case LBN_DBLCLK     : break;
    case LBN_KEYPRESSED : break;
  } /* switch */

  return (0L);
} /* inx_callback */
