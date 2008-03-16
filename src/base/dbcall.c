/*****************************************************************************
 *
 * Module : DBCALL.C
 * Author : Dieter Geiž
 *
 * Creation date    : 01.07.89
 * Last modification: $Id$
 *
 *
 * Description: This module implements the database call routines.
 *
 * History:
 * 22.03.98: File locking is no longer called for GEMDOS in db_delete and db_update
 * 03.11.96: SYS_PICTURE added
 * 16.06.96: Function str_upper no longer called for GEMDOS in db_create and db_open
 * 26.02.96: Function str_upper no longer called for NT in db_create and db_open
 * 24.07.95: SYS_ACCOUNT table will also be shown in SYS_DELETED table
 * 19.07.95: SYS_ACCOUNT added
 * 19.06.94: Call to do_recunlock in db_undelete added
 * 16.06.94: Problem with file_locked in db_delete and db_update fixed
 * 21.12.93: Callback function in db_reorg and db_reorg_cache added
 * 01.08.93: DOS version uses ANSI charset for compatibility with Windows version
 * 18.07.93: db_open_cache and db_reorg_cache added
 * 03.07.93: Functions db_getstr and db_setstr are removing trailing spaces
 * 24.06.93: Modifications for Borland C++ added
 * 24.05.93: Log file writing added
 * 30.04.93: Random generator initialized in db_init
 * 25.04.93: Data dictionary updated from old versions in test_dic
 * 08.04.93: Field name "Address" renamed to "DbAddress"
 * 01.04.93: Endless loop for data dictionary reading in damaged databases in do_reorg removed
 * 07.03.93: Function update_root added
 * 15.02.93: Functions db_getstr and db_setstr added
 * 19.11.92: String in db_setfield terminated
 * 18.11.92: Function db_reorg added
 * 15.11.92: Funtions db_acc_... returning USHORT
 * 14.11.92: SHORTFUNC renamed to LOCKFUNC
 * 11.11.92: Optimistic locking added
 * 03.11.92: Modifications for NT added
 * 31.10.92: Far pointer definitions for DLL implementation added
 * 26.10.92: Warnings at level 4 removed
 * 25.10.92: Module header added
 * 01.07.89: Creation of body
 *****************************************************************************/

#include <ctype.h>

#include "redefs.h"

#include "import.h"
#include "conv.h"
#include "files.h"
#include "utility.h"

#include "dbdata.h"
#include "dbtree.h"
#include "dbroot.h"

#include "export.h"
#include "dbcall.h"

/****** DEFINES **************************************************************/

#define TREE_CACHE	16L	/* KBs of cache to open tree */
#define TREE_CURSORS	10	/* number of cursors to open tree */
#define DATA_CACHE	100L	/* KBs of data cache in multiuser mode */

#define DATASIZE	8L	/* KBs to create datafile */
#define TREESIZE	8L	/* KBs to create treefile */

#define VISITED		0x8000	/* table already visited in recurse */

/****** TYPES ****************************************************************/

/****** VARIABLES ************************************************************/

LOCAL SYSCOLUMN syscolumn [] =
{
  {0L, SYS_TABLE,   0, "DbAddress", TYPE_DBADDRESS, LOFFSET (SYSTABLE, address),    sizeof (LONG),      TYPE_DBADDRESS, GRANT_ALL | COL_ISINDEX},
  {0L, SYS_TABLE,   1, "Table",     TYPE_WORD,      LOFFSET (SYSTABLE, table),      sizeof (SHORT),     TYPE_WORD,      GRANT_ALL | COL_ISINDEX},
  {0L, SYS_TABLE,   2, "Name",      TYPE_CHAR,      LCHROFF (SYSTABLE, name),       sizeof (TABLENAME), TYPE_CHAR,      GRANT_ALL},
  {0L, SYS_TABLE,   3, "Recs",      TYPE_LONG,      LOFFSET (SYSTABLE, recs),       sizeof (LONG),      TYPE_LONG,      GRANT_ALL},
  {0L, SYS_TABLE,   4, "Cols",      TYPE_WORD,      LOFFSET (SYSTABLE, cols),       sizeof (SHORT),     TYPE_WORD,      GRANT_ALL},
  {0L, SYS_TABLE,   5, "Indexes",   TYPE_WORD,      LOFFSET (SYSTABLE, indexes),    sizeof (SHORT),     TYPE_WORD,      GRANT_ALL},
  {0L, SYS_TABLE,   6, "Size",      TYPE_LONG,      LOFFSET (SYSTABLE, size),       sizeof (LONG),      TYPE_LONG,      GRANT_ALL},
  {0L, SYS_TABLE,   7, "Color",     TYPE_WORD,      LOFFSET (SYSTABLE, color),      sizeof (SHORT),     TYPE_WORD,      GRANT_ALL},
  {0L, SYS_TABLE,   8, "Icon",      TYPE_WORD,      LOFFSET (SYSTABLE, icon),       sizeof (SHORT),     TYPE_WORD,      GRANT_ALL},
  {0L, SYS_TABLE,   9, "Children",  TYPE_WORD,      LOFFSET (SYSTABLE, children),   sizeof (SHORT),     TYPE_WORD,      GRANT_ALL},
  {0L, SYS_TABLE,  10, "Parents",   TYPE_WORD,      LOFFSET (SYSTABLE, parents),    sizeof (SHORT),     TYPE_WORD,      GRANT_ALL},
  {0L, SYS_TABLE,  11, "Flags",     TYPE_WORD,      LOFFSET (SYSTABLE, flags),      sizeof (USHORT),    TYPE_WORD,      GRANT_ALL},

  {0L, SYS_COLUMN,  0, "DbAddress", TYPE_DBADDRESS, LOFFSET (SYSCOLUMN, address),   sizeof (LONG),      TYPE_DBADDRESS, GRANT_ALL | COL_ISINDEX},
  {0L, SYS_COLUMN,  1, "Table",     TYPE_WORD,      LOFFSET (SYSCOLUMN, table),     sizeof (SHORT),     TYPE_WORD,      GRANT_ALL},
  {0L, SYS_COLUMN,  2, "Number",    TYPE_WORD,      LOFFSET (SYSCOLUMN, number),    sizeof (SHORT),     TYPE_WORD,      GRANT_ALL},
  {0L, SYS_COLUMN,  3, "Name",      TYPE_CHAR,      LCHROFF (SYSCOLUMN, name),      sizeof (FIELDNAME), TYPE_CHAR,      GRANT_ALL},
  {0L, SYS_COLUMN,  4, "Type",      TYPE_WORD,      LOFFSET (SYSCOLUMN, type),      sizeof (SHORT),     TYPE_WORD,      GRANT_ALL},
  {0L, SYS_COLUMN,  5, "Addr",      TYPE_LONG,      LOFFSET (SYSCOLUMN, addr),      sizeof (LONG),      TYPE_LONG,      GRANT_ALL},
  {0L, SYS_COLUMN,  6, "Size",      TYPE_LONG,      LOFFSET (SYSCOLUMN, size),      sizeof (LONG),      TYPE_LONG,      GRANT_ALL},
  {0L, SYS_COLUMN,  7, "Format",    TYPE_WORD,      LOFFSET (SYSCOLUMN, format),    sizeof (SHORT),     TYPE_WORD,      GRANT_ALL},
  {0L, SYS_COLUMN,  8, "Flags",     TYPE_WORD,      LOFFSET (SYSCOLUMN, flags),     sizeof (USHORT),    TYPE_WORD,      GRANT_ALL},

  {0L, SYS_INDEX,   0, "DbAddress", TYPE_DBADDRESS, LOFFSET (SYSINDEX, address),    sizeof (LONG),      TYPE_DBADDRESS, GRANT_ALL | COL_ISINDEX},
  {0L, SYS_INDEX,   1, "Table",     TYPE_WORD,      LOFFSET (SYSINDEX, table),      sizeof (SHORT),     TYPE_WORD,      GRANT_ALL},
  {0L, SYS_INDEX,   2, "Number",    TYPE_WORD,      LOFFSET (SYSINDEX, number),     sizeof (SHORT),     TYPE_WORD,      GRANT_ALL},
  {0L, SYS_INDEX,   3, "Name",      TYPE_CHAR,      LCHROFF (SYSINDEX, name),       sizeof (FIELDNAME), TYPE_CHAR,      GRANT_ALL},
  {0L, SYS_INDEX,   4, "Type",      TYPE_WORD,      LOFFSET (SYSINDEX, type),       sizeof (SHORT),     TYPE_WORD,      GRANT_ALL},
  {0L, SYS_INDEX,   5, "Root",      TYPE_LONG,      LOFFSET (SYSINDEX, root),       sizeof (LONG),      TYPE_LONG,      GRANT_ALL},
  {0L, SYS_INDEX,   6, "NumKeys",   TYPE_LONG,      LOFFSET (SYSINDEX, num_keys),   sizeof (LONG),      TYPE_LONG,      GRANT_ALL},
  {0L, SYS_INDEX,   7, "InxCols",   TYPE_VARWORD,   LOFFSET (SYSINDEX, inxcols),    sizeof (INXCOLS),   TYPE_VARWORD,   GRANT_ALL},
  {0L, SYS_INDEX,   8, "Flags",     TYPE_WORD,      LOFFSET (SYSINDEX, flags),      sizeof (USHORT),    TYPE_WORD,      GRANT_ALL},

  {0L, SYS_REL,     0, "DbAddress", TYPE_DBADDRESS, LOFFSET (SYSREL, address),      sizeof (LONG),      TYPE_DBADDRESS, GRANT_ALL | COL_ISINDEX},
  {0L, SYS_REL,     1, "RefTable",  TYPE_WORD,      LOFFSET (SYSREL, reftable),     sizeof (SHORT),     TYPE_WORD,      GRANT_ALL | COL_ISINDEX},
  {0L, SYS_REL,     2, "RefIndex",  TYPE_WORD,      LOFFSET (SYSREL, refindex),     sizeof (SHORT),     TYPE_WORD,      GRANT_ALL},
  {0L, SYS_REL,     3, "RelTable",  TYPE_WORD,      LOFFSET (SYSREL, reltable),     sizeof (SHORT),     TYPE_WORD,      GRANT_ALL},
  {0L, SYS_REL,     4, "RelIndex",  TYPE_WORD,      LOFFSET (SYSREL, relindex),     sizeof (SHORT),     TYPE_WORD,      GRANT_ALL},
  {0L, SYS_REL,     5, "InsRule",   TYPE_WORD,      LOFFSET (SYSREL, insrule),      sizeof (SHORT),     TYPE_WORD,      GRANT_ALL},
  {0L, SYS_REL,     6, "DelRule",   TYPE_WORD,      LOFFSET (SYSREL, delrule),      sizeof (SHORT),     TYPE_WORD,      GRANT_ALL},
  {0L, SYS_REL,     7, "UpdRule",   TYPE_WORD,      LOFFSET (SYSREL, updrule),      sizeof (SHORT),     TYPE_WORD,      GRANT_ALL},
  {0L, SYS_REL,     8, "Flags",     TYPE_WORD,      LOFFSET (SYSREL, flags),        sizeof (USHORT),    TYPE_WORD,      GRANT_ALL},

  {0L, SYS_USER,    0, "DbAddress", TYPE_DBADDRESS, LOFFSET (SYSUSER, address),     sizeof (LONG),      TYPE_DBADDRESS, GRANT_ALL | COL_ISINDEX},
  {0L, SYS_USER,    1, "Name",      TYPE_CHAR,      LCHROFF (SYSUSER, name),        sizeof (USERNAME),  TYPE_CHAR,      GRANT_ALL | COL_ISINDEX},
  {0L, SYS_USER,    2, "Password",  TYPE_CHAR,      LCHROFF (SYSUSER, pass),        sizeof (PASSWORD),  TYPE_CHAR,      GRANT_ALL},
  {0L, SYS_USER,    3, "Count",     TYPE_LONG,      LOFFSET (SYSUSER, count),       sizeof (LONG),      TYPE_LONG,      GRANT_ALL},
  {0L, SYS_USER,    4, "LastUsed",  TYPE_TIMESTAMP, LOFFSET (SYSUSER, lastuse),     sizeof (TIMESTAMP), TYPE_TIMESTAMP, GRANT_ALL},
  {0L, SYS_USER,    5, "Access",    TYPE_VARWORD,   LOFFSET (SYSUSER, access),      sizeof (ACCESSDEF), TYPE_VARWORD,   GRANT_ALL},
  {0L, SYS_USER,    6, "Flags",     TYPE_WORD,      LOFFSET (SYSUSER, flags),       sizeof (USHORT),    TYPE_WORD,      GRANT_ALL},

  {0L, SYS_DELETED, 0, "DbAddress", TYPE_DBADDRESS, LOFFSET (SYSDELETED, address),  sizeof (LONG),      TYPE_DBADDRESS, GRANT_ALL | COL_ISINDEX},
  {0L, SYS_DELETED, 1, "DelAddr",   TYPE_DBADDRESS, LOFFSET (SYSDELETED, del_addr), sizeof (LONG),      TYPE_DBADDRESS, GRANT_ALL | COL_ISINDEX},
  {0L, SYS_DELETED, 2, "Table",     TYPE_WORD,      LOFFSET (SYSDELETED, table),    sizeof (SHORT),     TYPE_WORD,      GRANT_ALL},

  {0L, SYS_ICON,    0, "DbAddress", TYPE_DBADDRESS, LOFFSET (SYSICON, address),     sizeof (LONG),      TYPE_DBADDRESS, GRANT_ALL | COL_ISINDEX},
  {0L, SYS_ICON,    1, "Number",    TYPE_WORD,      LOFFSET (SYSICON, number),      sizeof (SHORT),     TYPE_WORD,      GRANT_ALL | COL_ISINDEX},
  {0L, SYS_ICON,    2, "Width",     TYPE_WORD,      LOFFSET (SYSICON, width),       sizeof (SHORT),     TYPE_WORD,      GRANT_ALL},
  {0L, SYS_ICON,    3, "Height",    TYPE_WORD,      LOFFSET (SYSICON, height),      sizeof (SHORT),     TYPE_WORD,      GRANT_ALL},
  {0L, SYS_ICON,    4, "Icon",      TYPE_VARWORD,   LOFFSET (SYSICON, icon),        sizeof (ICONDEF),   TYPE_VARWORD,   GRANT_ALL},
  {0L, SYS_ICON,    5, "Bitmap",    TYPE_VARBYTE,   LOFFSET (SYSICON, bitmap),      sizeof (BITMAPDEF), TYPE_VARBYTE,   GRANT_ALL},
  {0L, SYS_ICON,    6, "Flags",     TYPE_WORD,      LOFFSET (SYSICON, flags),       sizeof (USHORT),    TYPE_WORD,      GRANT_ALL},

  {0L, SYS_TATTR,   0, "DbAddress", TYPE_DBADDRESS, LOFFSET (SYSTATTR, address),    sizeof (LONG),      TYPE_DBADDRESS, GRANT_ALL | COL_ISINDEX},
  {0L, SYS_TATTR,   1, "Table",     TYPE_WORD,      LOFFSET (SYSTATTR, table),      sizeof (SHORT),     TYPE_WORD,      GRANT_ALL | COL_ISINDEX},
  {0L, SYS_TATTR,   2, "X",         TYPE_WORD,      LOFFSET (SYSTATTR, x),          sizeof (SHORT),     TYPE_WORD,      GRANT_ALL},
  {0L, SYS_TATTR,   3, "Y",         TYPE_WORD,      LOFFSET (SYSTATTR, y),          sizeof (SHORT),     TYPE_WORD,      GRANT_ALL},
  {0L, SYS_TATTR,   4, "Width",     TYPE_WORD,      LOFFSET (SYSTATTR, width),      sizeof (SHORT),     TYPE_WORD,      GRANT_ALL},
  {0L, SYS_TATTR,   5, "Height",    TYPE_WORD,      LOFFSET (SYSTATTR, height),     sizeof (SHORT),     TYPE_WORD,      GRANT_ALL},
  {0L, SYS_TATTR,   6, "Flags",     TYPE_WORD,      LOFFSET (SYSTATTR, flags),      sizeof (USHORT),    TYPE_WORD,      GRANT_ALL},

  {0L, SYS_FORMAT,  0, "DbAddress", TYPE_DBADDRESS, LOFFSET (SYSFORMAT, address),   sizeof (LONG),      TYPE_DBADDRESS, GRANT_ALL | COL_ISINDEX},
  {0L, SYS_FORMAT,  1, "Number",    TYPE_WORD,      LOFFSET (SYSFORMAT, number),    sizeof (SHORT),     TYPE_WORD,      GRANT_ALL | COL_ISINDEX},
  {0L, SYS_FORMAT,  2, "Type",      TYPE_WORD,      LOFFSET (SYSFORMAT, type),      sizeof (SHORT),     TYPE_WORD,      GRANT_ALL},
  {0L, SYS_FORMAT,  3, "Format",    TYPE_CHAR,      LCHROFF (SYSFORMAT, format),    sizeof (FORMATSTR), TYPE_CHAR,      GRANT_ALL},
  {0L, SYS_FORMAT,  4, "Flags",     TYPE_WORD,      LOFFSET (SYSFORMAT, flags),     sizeof (USHORT),    TYPE_WORD,      GRANT_ALL},

  {0L, SYS_LOOKUP,  0, "DbAddress", TYPE_DBADDRESS, LOFFSET (SYSLOOKUP, address),   sizeof (LONG),      TYPE_DBADDRESS, GRANT_ALL | COL_ISINDEX},
  {0L, SYS_LOOKUP,  1, "Table",     TYPE_WORD,      LOFFSET (SYSLOOKUP, table),     sizeof (SHORT),     TYPE_WORD,      GRANT_ALL},
  {0L, SYS_LOOKUP,  2, "Column",    TYPE_WORD,      LOFFSET (SYSLOOKUP, column),    sizeof (SHORT),     TYPE_WORD,      GRANT_ALL},
  {0L, SYS_LOOKUP,  3, "RefTable",  TYPE_WORD,      LOFFSET (SYSLOOKUP, reftable),  sizeof (SHORT),     TYPE_WORD,      GRANT_ALL},
  {0L, SYS_LOOKUP,  4, "RefColumn", TYPE_WORD,      LOFFSET (SYSLOOKUP, refcolumn), sizeof (SHORT),     TYPE_WORD,      GRANT_ALL},
  {0L, SYS_LOOKUP,  5, "Flags",     TYPE_WORD,      LOFFSET (SYSLOOKUP, flags),     sizeof (USHORT),    TYPE_WORD,      GRANT_ALL},

  {0L, SYS_CALC,    0, "DbAddress", TYPE_DBADDRESS, LOFFSET (SYSCALC, address),     sizeof (LONG),      TYPE_DBADDRESS, GRANT_ALL | COL_ISINDEX},
  {0L, SYS_CALC,    1, "Tablename", TYPE_CHAR,      LCHROFF (SYSCALC, tablename),   sizeof (TABLENAME), TYPE_CHAR,      GRANT_ALL},
  {0L, SYS_CALC,    2, "Name",      TYPE_CHAR,      LCHROFF (SYSCALC, name),        sizeof (FIELDNAME), TYPE_CHAR,      GRANT_ALL},
  {0L, SYS_CALC,    3, "Text",      TYPE_CHAR,      LCHROFF (SYSCALC, text),        sizeof (CALCTEXT),  TYPE_CHAR,      GRANT_ALL},
  {0L, SYS_CALC,    4, "Code",      TYPE_VARBYTE,   LOFFSET (SYSCALC, code),        sizeof (CALCCODE),  TYPE_VARBYTE,   GRANT_ALL},
  {0L, SYS_CALC,    5, "Flags",     TYPE_WORD,      LOFFSET (SYSCALC, flags),       sizeof (USHORT),    TYPE_WORD,      GRANT_ALL},

  {0L, SYS_MASK,    0, "DbAddress", TYPE_DBADDRESS, LOFFSET (SYSMASK, address),     sizeof (LONG),      TYPE_DBADDRESS, GRANT_ALL | COL_ISINDEX},
  {0L, SYS_MASK,    1, "Device",    TYPE_CHAR,      LCHROFF (SYSMASK, device),      sizeof (DEVSIZE),   TYPE_CHAR,      GRANT_ALL},
  {0L, SYS_MASK,    2, "Name",      TYPE_CHAR,      LCHROFF (SYSMASK, name),        sizeof (FIELDNAME), TYPE_CHAR,      GRANT_ALL},
  {0L, SYS_MASK,    3, "Tablename", TYPE_CHAR,      LCHROFF (SYSMASK, tablename),   sizeof (TABLENAME), TYPE_CHAR,      GRANT_ALL | COL_ISINDEX},
  {0L, SYS_MASK,    4, "Username",  TYPE_CHAR,      LCHROFF (SYSMASK, username),    sizeof (USERNAME),  TYPE_CHAR,      GRANT_ALL},
  {0L, SYS_MASK,    5, "Flags",     TYPE_WORD,      LOFFSET (SYSMASK, flags),       sizeof (USHORT),    TYPE_WORD,      GRANT_ALL},
  {0L, SYS_MASK,    6, "Mask",      TYPE_VARBYTE,   LOFFSET (SYSMASK, mask),        sizeof (MASKDEF),   TYPE_VARBYTE,   GRANT_ALL},
  {0L, SYS_MASK,    7, "Version",   TYPE_WORD,      LOFFSET (SYSMASK, version),     sizeof (SHORT),     TYPE_WORD,      GRANT_ALL},
  {0L, SYS_MASK,    8, "CalcEntry", TYPE_CHAR,      LCHROFF (SYSMASK, calcentry),   sizeof (FIELDNAME), TYPE_CHAR,      GRANT_ALL},
  {0L, SYS_MASK,    9, "CalcExit",  TYPE_CHAR,      LCHROFF (SYSMASK, calcexit),    sizeof (FIELDNAME), TYPE_CHAR,      GRANT_ALL},
  {0L, SYS_MASK,   10, "X",         TYPE_WORD,      LOFFSET (SYSMASK, x),           sizeof (SHORT),     TYPE_WORD,      GRANT_ALL},
  {0L, SYS_MASK,   11, "Y",         TYPE_WORD,      LOFFSET (SYSMASK, y),           sizeof (SHORT),     TYPE_WORD,      GRANT_ALL},
  {0L, SYS_MASK,   12, "W",         TYPE_WORD,      LOFFSET (SYSMASK, w),           sizeof (SHORT),     TYPE_WORD,      GRANT_ALL},
  {0L, SYS_MASK,   13, "H",         TYPE_WORD,      LOFFSET (SYSMASK, h),           sizeof (SHORT),     TYPE_WORD,      GRANT_ALL},
  {0L, SYS_MASK,   14, "BkColor",   TYPE_LONG,      LOFFSET (SYSMASK, bkcolor),     sizeof (LONG),      TYPE_LONG,      GRANT_ALL},

  {0L, SYS_QUERY,   0, "DbAddress", TYPE_DBADDRESS, LOFFSET (SYSQUERY, address),    sizeof (LONG),      TYPE_DBADDRESS, GRANT_ALL | COL_ISINDEX},
  {0L, SYS_QUERY,   1, "Name",      TYPE_CHAR,      LCHROFF (SYSQUERY, name),       sizeof (FIELDNAME), TYPE_CHAR,      GRANT_ALL | COL_ISINDEX},
  {0L, SYS_QUERY,   2, "Query",     TYPE_CHAR,      LCHROFF (SYSQUERY, query),      sizeof (QUERYDEF),  TYPE_CHAR,      GRANT_ALL},

  {0L, SYS_REPORT,  0, "DbAddress", TYPE_DBADDRESS, LOFFSET (SYSREPORT, address),   sizeof (LONG),      TYPE_DBADDRESS, GRANT_ALL | COL_ISINDEX},
  {0L, SYS_REPORT,  1, "Name",      TYPE_CHAR,      LCHROFF (SYSREPORT, name),      sizeof (FIELDNAME), TYPE_CHAR,      GRANT_ALL | COL_ISINDEX},
  {0L, SYS_REPORT,  2, "Report",    TYPE_CHAR,      LCHROFF (SYSREPORT, report),    sizeof (REPORTDEF), TYPE_CHAR,      GRANT_ALL},

  {0L, SYS_BATCH,   0, "DbAddress", TYPE_DBADDRESS, LOFFSET (SYSBATCH, address),    sizeof (LONG),      TYPE_DBADDRESS, GRANT_ALL | COL_ISINDEX},
  {0L, SYS_BATCH,   1, "Name",      TYPE_CHAR,      LCHROFF (SYSBATCH, name),       sizeof (FIELDNAME), TYPE_CHAR,      GRANT_ALL | COL_ISINDEX},
  {0L, SYS_BATCH,   2, "Batch",     TYPE_CHAR,      LCHROFF (SYSBATCH, batch),      sizeof (BATCHDEF),  TYPE_CHAR,      GRANT_ALL},

  {0L, SYS_BLOB,    0, "DbAddress", TYPE_DBADDRESS, LOFFSET (SYSBLOB, address),     sizeof (LONG),      TYPE_DBADDRESS, GRANT_ALL | COL_ISINDEX},
  {0L, SYS_BLOB,    1, "Size",      TYPE_LONG,      LOFFSET (SYSBLOB, size),        sizeof (LONG),      TYPE_LONG,      GRANT_ALL},
  {0L, SYS_BLOB,    2, "Ext",       TYPE_CHAR,      LCHROFF (SYSBLOB, ext),         sizeof (BLOBEXT),   TYPE_CHAR,      GRANT_ALL},
  {0L, SYS_BLOB,    3, "Name",      TYPE_CHAR,      LCHROFF (SYSBLOB, name),        sizeof (BLOBNAME),  TYPE_CHAR,      GRANT_ALL},
  {0L, SYS_BLOB,    4, "Flags",     TYPE_WORD,      LOFFSET (SYSBLOB, flags),       sizeof (USHORT),    TYPE_WORD,      GRANT_ALL},
  {0L, SYS_BLOB,    5, "Blob",      TYPE_CHAR,      LCHROFF (SYSBLOB, blob),        sizeof (CHAR) * 2,  TYPE_CHAR,      GRANT_ALL},

  {0L, SYS_ACCOUNT, 0, "DbAddress", TYPE_DBADDRESS, LOFFSET (SYSACCOUNT, address),  sizeof (LONG),      TYPE_DBADDRESS, GRANT_ALL | COL_ISINDEX},
  {0L, SYS_ACCOUNT, 1, "Name",      TYPE_CHAR,      LCHROFF (SYSACCOUNT, name),     sizeof (FIELDNAME), TYPE_CHAR,      GRANT_ALL | COL_ISINDEX},
  {0L, SYS_ACCOUNT, 2, "Account",   TYPE_BLOB,      LOFFSET (SYSACCOUNT, account),  sizeof (BLOB),      TYPE_BLOB,      GRANT_ALL},

  {0L, SYS_PICTURE, 0, "DbAddress", TYPE_DBADDRESS, LOFFSET (SYSPICTURE, address),  sizeof (LONG),      TYPE_DBADDRESS, GRANT_ALL | COL_ISINDEX},
  {0L, SYS_PICTURE, 1, "Name",      TYPE_CHAR,      LCHROFF (SYSPICTURE, name),     sizeof (FIELDNAME), TYPE_CHAR,      GRANT_ALL | COL_ISINDEX},
  {0L, SYS_PICTURE, 2, "Picture",   TYPE_BLOB,      LOFFSET (SYSPICTURE, picture),  sizeof (BLOB),      TYPE_BLOB,      GRANT_ALL},
  {0L, SYS_PICTURE, 3, "Flags",     TYPE_WORD,      LOFFSET (SYSPICTURE, flags),    sizeof (USHORT),    TYPE_WORD,      GRANT_ALL}
}; /* SYSCOLUMN */

LOCAL SYSINDEX sysindex [] =
{
  {0L, SYS_TABLE,   0, "", TYPE_DBADDRESS, 0L, 0L, {1 * sizeof (INXCOL), {{0, 0}        }}, 0},
  {0L, SYS_TABLE,   1, "", TYPE_WORD,      0L, 0L, {1 * sizeof (INXCOL), {{1, 0}        }}, INX_PRIMARY},

  {0L, SYS_COLUMN,  0, "", TYPE_DBADDRESS, 0L, 0L, {1 * sizeof (INXCOL), {{0, 0}        }}, 0},
  {0L, SYS_COLUMN,  1, "", TYPE_LONG,      0L, 0L, {2 * sizeof (INXCOL), {{1, 0}, {2, 0}}}, INX_PRIMARY},

  {0L, SYS_INDEX,   0, "", TYPE_DBADDRESS, 0L, 0L, {1 * sizeof (INXCOL), {{0, 0}        }}, 0},
  {0L, SYS_INDEX,   1, "", TYPE_LONG,      0L, 0L, {2 * sizeof (INXCOL), {{1, 0}, {2, 0}}}, INX_PRIMARY},

  {0L, SYS_REL,     0, "", TYPE_DBADDRESS, 0L, 0L, {1 * sizeof (INXCOL), {{0, 0}        }}, 0},
  {0L, SYS_REL,     1, "", TYPE_WORD,      0L, 0L, {1 * sizeof (INXCOL), {{1, 0}        }}, 0},

  {0L, SYS_USER,    0, "", TYPE_DBADDRESS, 0L, 0L, {1 * sizeof (INXCOL), {{0, 0}        }}, 0},
  {0L, SYS_USER,    1, "", TYPE_CHAR,      0L, 0L, {1 * sizeof (INXCOL), {{1, MAX_USERNAME}}}, INX_PRIMARY},

  {0L, SYS_DELETED, 0, "", TYPE_DBADDRESS, 0L, 0L, {1 * sizeof (INXCOL), {{0, 0}        }}, 0},
  {0L, SYS_DELETED, 1, "", TYPE_DBADDRESS, 0L, 0L, {1 * sizeof (INXCOL), {{1, 0}        }}, 0},

  {0L, SYS_ICON,    0, "", TYPE_DBADDRESS, 0L, 0L, {1 * sizeof (INXCOL), {{0, 0}        }}, 0},
  {0L, SYS_ICON,    1, "", TYPE_WORD,      0L, 0L, {1 * sizeof (INXCOL), {{1, 0}        }}, INX_PRIMARY},

  {0L, SYS_TATTR,   0, "", TYPE_DBADDRESS, 0L, 0L, {1 * sizeof (INXCOL), {{0, 0}        }}, 0},
  {0L, SYS_TATTR,   1, "", TYPE_WORD,      0L, 0L, {1 * sizeof (INXCOL), {{1, 0}        }}, INX_PRIMARY},

  {0L, SYS_FORMAT,  0, "", TYPE_DBADDRESS, 0L, 0L, {1 * sizeof (INXCOL), {{0, 0}        }}, 0},
  {0L, SYS_FORMAT,  1, "", TYPE_WORD,      0L, 0L, {1 * sizeof (INXCOL), {{1, 0}        }}, INX_PRIMARY},

  {0L, SYS_LOOKUP,  0, "", TYPE_DBADDRESS, 0L, 0L, {1 * sizeof (INXCOL), {{0, 0}        }}, 0},
  {0L, SYS_LOOKUP,  1, "", TYPE_LONG,      0L, 0L, {2 * sizeof (INXCOL), {{1, 0}, {2, 0}}}, INX_PRIMARY},

  {0L, SYS_CALC,    0, "", TYPE_DBADDRESS, 0L, 0L, {1 * sizeof (INXCOL), {{0, 0}        }}, 0},
  {0L, SYS_CALC,    1, "", TYPE_CHAR,      0L, 0L, {2 * sizeof (INXCOL), {{1, MAX_TABLENAME}, {2, MAX_FIELDNAME}}}, INX_PRIMARY},

  {0L, SYS_MASK,    0, "", TYPE_DBADDRESS, 0L, 0L, {1 * sizeof (INXCOL), {{0, 0}        }}, 0},
  {0L, SYS_MASK,    1, "", TYPE_CHAR,      0L, 0L, {2 * sizeof (INXCOL), {{1, 1}, {2, MAX_FIELDNAME}}}, INX_PRIMARY},
  {0L, SYS_MASK,    2, "", TYPE_CHAR,      0L, 0L, {1 * sizeof (INXCOL), {{3, MAX_TABLENAME}}}, 0},

  {0L, SYS_QUERY,   0, "", TYPE_DBADDRESS, 0L, 0L, {1 * sizeof (INXCOL), {{0, 0}        }}, 0},
  {0L, SYS_QUERY,   1, "", TYPE_CHAR,      0L, 0L, {1 * sizeof (INXCOL), {{1, MAX_FIELDNAME}}}, INX_PRIMARY},

  {0L, SYS_REPORT,  0, "", TYPE_DBADDRESS, 0L, 0L, {1 * sizeof (INXCOL), {{0, 0}        }}, 0},
  {0L, SYS_REPORT,  1, "", TYPE_CHAR,      0L, 0L, {1 * sizeof (INXCOL), {{1, MAX_FIELDNAME}}}, INX_PRIMARY},

  {0L, SYS_BATCH,   0, "", TYPE_DBADDRESS, 0L, 0L, {1 * sizeof (INXCOL), {{0, 0}        }}, 0},
  {0L, SYS_BATCH,   1, "", TYPE_CHAR,      0L, 0L, {1 * sizeof (INXCOL), {{1, MAX_FIELDNAME}}}, INX_PRIMARY},

  {0L, SYS_BLOB,    0, "", TYPE_DBADDRESS, 0L, 0L, {1 * sizeof (INXCOL), {{0, 0}        }}, 0},
  {0L, SYS_BLOB,    1, "", TYPE_CHAR,      0L, 0L, {1 * sizeof (INXCOL), {{3, MAX_BLOBNAME}}}, 0},

  {0L, SYS_ACCOUNT, 0, "", TYPE_DBADDRESS, 0L, 0L, {1 * sizeof (INXCOL), {{0, 0}        }}, 0},
  {0L, SYS_ACCOUNT, 1, "", TYPE_CHAR,      0L, 0L, {1 * sizeof (INXCOL), {{1, MAX_FIELDNAME}}}, INX_PRIMARY},

  {0L, SYS_PICTURE, 0, "", TYPE_DBADDRESS, 0L, 0L, {1 * sizeof (INXCOL), {{0, 0}        }}, 0},
  {0L, SYS_PICTURE, 1, "", TYPE_CHAR,      0L, 0L, {1 * sizeof (INXCOL), {{1, MAX_FIELDNAME}}}, INX_PRIMARY}
}; /* SYSINDEX */

LOCAL SYSTABLE systable [NUM_SYSTABLES] =
{
  {0L, SYS_TABLE,   "SYS_TABLE",   NUM_SYSTABLES,                           0, 0, sizeof (SYSTABLE),   1, 0, 0, 0, GRANT_ALL},
  {0L, SYS_COLUMN,  "SYS_COLUMN",  sizeof (syscolumn) / sizeof (SYSCOLUMN), 0, 0, sizeof (SYSCOLUMN),  1, 0, 0, 0, GRANT_ALL},
  {0L, SYS_INDEX,   "SYS_INDEX",   sizeof (sysindex)  / sizeof (SYSINDEX),  0, 0, sizeof (SYSINDEX),   1, 0, 0, 0, GRANT_ALL},
  {0L, SYS_REL,     "SYS_REL",     0,                                       0, 0, sizeof (SYSREL),     1, 0, 0, 0, GRANT_ALL},
  {0L, SYS_USER,    "SYS_USER",    0,                                       0, 0, sizeof (SYSUSER),    1, 0, 0, 0, GRANT_ALL},
  {0L, SYS_DELETED, "SYS_DELETED", 0,                                       0, 0, sizeof (SYSDELETED), 1, 0, 0, 0, GRANT_ALL},
  {0L, SYS_ICON,    "SYS_ICON",    0,                                       0, 0, sizeof (SYSICON),    1, 0, 0, 0, GRANT_ALL},
  {0L, SYS_TATTR,   "SYS_TATTR",   0,                                       0, 0, sizeof (SYSTATTR),   1, 0, 0, 0, GRANT_ALL},
  {0L, SYS_FORMAT,  "SYS_FORMAT",  0,                                       0, 0, sizeof (SYSFORMAT),  1, 0, 0, 0, GRANT_ALL},
  {0L, SYS_LOOKUP,  "SYS_LOOKUP",  0,                                       0, 0, sizeof (SYSLOOKUP),  1, 0, 0, 0, GRANT_ALL},
  {0L, SYS_CALC,    "SYS_CALC",    0,                                       0, 0, sizeof (SYSCALC),    1, 0, 0, 0, GRANT_ALL | TBL_VISIBLE | TBL_LOGGED},
  {0L, SYS_MASK,    "SYS_MASK",    0,                                       0, 0, sizeof (SYSMASK),    1, 0, 0, 0, GRANT_ALL},
  {0L, SYS_QUERY,   "SYS_QUERY",   0,                                       0, 0, sizeof (SYSQUERY),   1, 0, 0, 0, GRANT_ALL | TBL_VISIBLE | TBL_LOGGED},
  {0L, SYS_REPORT,  "SYS_REPORT",  0,                                       0, 0, sizeof (SYSREPORT),  1, 0, 0, 0, GRANT_ALL | TBL_VISIBLE | TBL_LOGGED},
  {0L, SYS_BATCH,   "SYS_BATCH",   0,                                       0, 0, sizeof (SYSBATCH),   1, 0, 0, 0, GRANT_ALL | TBL_VISIBLE | TBL_LOGGED},
  {0L, SYS_BLOB,    "SYS_BLOB",    0,                                       0, 0, sizeof (SYSBLOB),    1, 0, 0, 0, GRANT_ALL | TBL_LOGGED},
  {0L, SYS_ACCOUNT, "SYS_ACCOUNT", 0,                                       0, 0, sizeof (SYSACCOUNT), 1, 0, 0, 0, GRANT_ALL | TBL_VISIBLE | TBL_LOGGED},
  {0L, SYS_PICTURE, "SYS_PICTURE", 0,                                       0, 0, sizeof (SYSPICTURE), 1, 0, 0, 0, GRANT_ALL | TBL_LOGGED},
  {0L, SYS_DUMMY18, "SYS_DUMMY18", 0,                                       0, 0, 0,                   1, 0, 0, 0, GRANT_ALL},
  {0L, SYS_DUMMY19, "SYS_DUMMY19", 0,                                       0, 0, 0,                   1, 0, 0, 0, GRANT_ALL}
}; /* SYSTABLE */

LOCAL  BOOL    initialized = FALSE;
LOCAL  CHAR    logid []    = "PHOENIX LOG";
GLOBAL CHAR    serial []   = "Peter ist doof.."; /* will be serial number */

/****** FUNCTIONS ************************************************************/

LOCAL VOID      set_systable   _((VOID));
LOCAL VOID      build_name     _((LPSTR basename, LPSTR basepath, LPSTR dataname, LPSTR treename));
LOCAL BOOL      check_multi    _((LPBASE base, BOOL do_write));
LOCAL BOOL      test_dic       _((LPBASE base));
LOCAL BOOL      load_dic       _((LPBASE base));
LOCAL BOOL      create_dic     _((LPBASE base));
LOCAL BOOL      set_access     _((LPBASE base, LPSTR username, LPSTR password));
LOCAL BOOL      do_reorg       _((LPBASE base, LPDATAINF data, LPSTR basename, LPSTR basepath, LPSTR tmpname, LONG datacache, LONG treecache, LONG length, REORGFUNC reorgfunc, LPREORG_RESULT reorg_result));
LOCAL BOOL      read_reorg     _((LPBASE base, LPDATAINF data, SHORT table, LONG address, HPVOID buffer, LONG size));
LOCAL BOOL      insert_blobs   _((LPBASE base, LPDATAINF data, SHORT table, HPVOID buffer));
LOCAL BOOL      is_logtable    _((LPBASE base, SHORT table));

LOCAL BOOL      read_systable  _((LPBASE base, SYSTABLE *systblp, LONG recs));
LOCAL BOOL      read_syscolumn _((LPBASE base, SYSCOLUMN *syscolp, LONG recs));
LOCAL BOOL      read_sysindex  _((LPBASE base, SYSINDEX *sysinxp, LONG recs));
LOCAL BOOL      read_sysrel    _((LPBASE base, SYSREL *sysrelp, LONG recs));

LOCAL BOOL      do_insrec      _((LPBASE base, SHORT table, HPVOID buffer, SHORT *status));
LOCAL BOOL      do_delrec      _((LPBASE base, SHORT table, LONG address, SHORT *status));
LOCAL BOOL      do_updtrec     _((LPBASE base, SHORT table, HPVOID buffer, LONG version, SHORT *status, BOOL testins));

LOCAL BOOL      do_insert      _((LPBASE base, SHORT table, HPVOID buffer));
LOCAL BOOL      do_delete      _((LPBASE base, SHORT table, LONG address));
LOCAL BOOL      do_update      _((LPBASE base, SHORT table, HPVOID buffer, LONG version, BOOL key_updt));
LOCAL BOOL      do_read        _((LPBASE base, SHORT table, HPVOID buffer, LPCURSOR cursor, LONG address, BOOL modify, LPLONG version));
LOCAL LPSYSBLOB do_readblob    _((LPBASE base, LPSYSBLOB buffer, LPCURSOR cursor, LONG address, BOOL modify));
LOCAL BOOL      do_reclock     _((LPBASE base, LONG address));
LOCAL BOOL      do_recunlock   _((LPBASE base, LONG address));

LOCAL BOOL      ins_keys       _((LPBASE base, SHORT table, HPVOID buffer, LONG addr));
LOCAL BOOL      del_keys       _((LPBASE base, SHORT table, HPVOID buffer, LONG addr));
LOCAL BOOL      updt_keys      _((LPBASE base, SHORT table, HPVOID buffer, HPVOID oldbuf, LONG addr));
LOCAL BOOL      test_keys      _((LPBASE base, SHORT table, HPVOID buffer, LONG addr, SHORT *status));
LOCAL BOOL      test_cols      _((LPBASE base, SHORT table, HPVOID buffer, SHORT *status));
LOCAL BOOL      test_ins       _((LPBASE base, SHORT table, HPVOID buffer, SHORT *status));
LOCAL BOOL      test_del       _((LPBASE base, SHORT table, LONG address, SHORT *status));
LOCAL BOOL      test_updt      _((LPBASE base, SHORT table, HPVOID buffer, SHORT *status));

LOCAL LONG      do_pack        _((LPBASE base, SHORT table, HPVOID buffer, HPVOID packbuf));
LOCAL LONG      do_unpack      _((LPBASE base, SHORT table, HPVOID buffer, HPVOID packbuf, LONG packsize));
LOCAL BOOL      do_buildkey    _((LPBASE base, SHORT table, SHORT inx, HPVOID buffer, HPVOID keyval));
LOCAL VOID      do_convstr     _((LPBASE base, HPCHAR s));
LOCAL VOID      set_keyvalue   _((LPBASE base, SHORT table, SHORT inx, HPVOID buffer, SHORT srctable, SHORT srcinx, HPVOID srcbuf));

LOCAL SHORT     abscol         _((LPBASE base, SHORT table, SHORT col));
LOCAL SHORT     absinx         _((LPBASE base, SHORT table, SHORT inx));
LOCAL VOID      keydfn         _((LPBASE base, SHORT table, SHORT inx, KEY_DFN *kd));
LOCAL BOOL      table_ok       _((LPBASE base, SHORT table));
LOCAL BOOL      column_ok      _((LPBASE base, SHORT table, SHORT col));
LOCAL BOOL      index_ok       _((LPBASE base, SHORT table, SHORT inx));
LOCAL VOID      update_root    _((LPBASE base, SHORT table, SHORT inx));
LOCAL BOOL      write_loghead  _((LPBASE base, SHORT table, LPLOGHEADER lplh));
LOCAL BOOL      write_logbuf   _((LPBASE base, SHORT table, HPVOID buffer));

LOCAL VOID      fill_sysptr    _((SYSTABLE *systable, SYSPTR *sysptr, SHORT tables));
LOCAL VOID      str_rem_trail  _((HPCHAR s));

/*****************************************************************************/

GLOBAL VOID WINAPI db_init (lock)
LOCKFUNC lock;

{
  TIME time;

  init_data (lock);

  get_time (&time);
  srand (time.minute * time.second);	/* initialize random generator */
} /* db_init */

/*****************************************************************************/

GLOBAL LPBASE WINAPI db_create (basename, basepath, flags, datasize, treesize)
LPSTR  basename, basepath;
USHORT flags;
LONG   datasize, treesize;

{
  LPBASE   base;
  FULLNAME dataname, treename;
  SHORT    level;

  set_systable ();
  set_dberror (SUCCESS);

  level = 0;                            /* used for error exit */
  base  = (LPBASE)mem_alloc ((LONG)sizeof (BASE));

  if (base == NULL)
  {
    level = 1;
    set_dberror (DB_NOMEMORY);
  } /* if */
  else
  {
    mem_set (base, 0, sizeof (BASE));

    base->hFileLog = FAILURE;

#if MSDOS | FLEXOS
    str_upper (basename);
    str_upper (basepath);
#endif

    build_name (basename, basepath, dataname, treename);

    base->datainf = create_data (dataname, (USHORT)(flags & 0xFF00 & ~ BASE_SUPER), datasize);

    if (base->datainf == NULL)
      level = 2;
    else
    {
      base->datainf->flags |= flags & 0xFF00 & BASE_SUPER;

      base->treeinf = create_tree (treename, (USHORT)(flags & 0x00FF), treesize);

      if (base->treeinf == NULL)
        level = 3;
      else
      {
        base->username [0] = EOS;
        strcpy (base->basename, basename);
        strcpy (base->basepath, basepath);

        base->systbls   = NUM_SYSTABLES;
        base->systable  = systable;
        base->syscolumn = syscolumn;
        base->sysindex  = sysindex;
        base->sysptr    = (SYSPTR *)mem_alloc ((LONG)NUM_SYSTABLES * sizeof (SYSPTR));

        if (base->sysptr == NULL)
        {
          level = 4;
          set_dberror (DB_NOMEMORY);
        } /* if */
        else
        {
          fill_sysptr (base->systable, base->sysptr, NUM_SYSTABLES);
          close_tree (base->treeinf);
          base->treeinf = open_tree (treename, 0, TREE_CACHE, TREE_CURSORS);

          if (base->treeinf == NULL)
            level = 4;
          else
            if (! create_dic (base))
              level = 5;
            else
            {
              mem_free (base->sysptr);

              base->systable  = NULL;
              base->syscolumn = NULL;
              base->sysindex  = NULL;
              base->sysrel    = NULL;
              base->sysptr    = NULL;

              if (! load_dic (base))
                level = 5;
              else
              {
		base->systbls = (SHORT)base->systable [SYS_TABLE].recs;
		base->syscols = (SHORT)base->systable [SYS_COLUMN].recs;
		base->sysinxs = (SHORT)base->systable [SYS_INDEX].recs;
              } /* else */
            } /* else, else */
        } /* else */
      } /* else */
    } /* else */
  } /* else */

  switch (level)
  {
    case 5 : if (base->treeinf != NULL) close_tree (base->treeinf);
    case 4 : mem_free (base->sysptr);
    case 3 : close_data (base->datainf);
    case 2 : mem_free (base);
    case 1 : base = NULL;
    case 0 : break;
  } /* switch */

  return (base);
} /* db_create */

/*****************************************************************************/

GLOBAL LPBASE WINAPI db_open (basename, basepath, flags, cachesize, num_cursors, username, password)
LPSTR  basename, basepath;
USHORT flags;
LONG   cachesize;
SHORT  num_cursors;
LPSTR  username, password;

{
  return (db_open_cache (basename, basepath, flags, 0L, cachesize, num_cursors, username, password));
} /* db_open */

/*****************************************************************************/

GLOBAL LPBASE WINAPI db_open_cache (basename, basepath, flags, datacache, treecache, num_cursors, username, password)
LPSTR  basename, basepath;
USHORT flags;
LONG   datacache, treecache;
SHORT  num_cursors;
LPSTR  username, password;

{
  LPBASE   base;
  FULLNAME dataname, treename, logname;
  SHORT    level;  
  INT      mode;

  set_systable ();
  set_dberror (SUCCESS);

  level = 0;                            /* used for error exit */
  base  = (LPBASE)mem_alloc ((LONG)sizeof (BASE));

  if (base == NULL)
  {
    level = 1;
    set_dberror (DB_NOMEMORY);
  } /* if */
  else
  {
    mem_set (base, 0, sizeof (BASE));
    
    base->hFileLog = FAILURE;

#if MSDOS | FLEXOS
    str_upper (basename);
    str_upper (basepath);
#endif

    build_name (basename, basepath, dataname, treename);

    base->datainf = open_data (dataname, (USHORT)(flags & 0xFF00), datacache * 1024L);

    if ((get_dberror () != SUCCESS) && (get_dberror () != DB_DNOTCLOSED))
      level = 2;
    else
    {
      base->treeinf = open_tree (treename, (USHORT)(flags & 0x00FF), treecache, num_cursors);

      if ((get_dberror () != SUCCESS) && (get_dberror () != DB_DNOTCLOSED))
        level = 3;
      else
      {
        strcpy (base->username, (username == NULL) ? "" : username);
        strcpy (base->basename, basename);
        strcpy (base->basepath, basepath);

        if (! db_lock (base->datainf))
          level = 4;
        else
        {
          if (! test_dic (base))
            level = 4;
          else
          {
            if (! set_access (base, (username == NULL) ? "" : username, (password == NULL) ? "" : password))
            {
              level = 5;
              set_dberror (DB_CPASSWORD);
            } /* if */
            else
            {
	      base->systbls = (SHORT)base->systable [SYS_TABLE].recs;
	      base->syscols = (SHORT)base->systable [SYS_COLUMN].recs;
	      base->sysinxs = (SHORT)base->systable [SYS_INDEX].recs;

              if ((base->datainf->page0.flags & DATA_USELOG) && ! (flags & (DATA_RDONLY | DATA_NOLOG)))
              {
                strcpy (logname, dataname);
                strcpy (strrchr (logname, SUFFSEP) + 1, LOG_SUFF);

                mode = O_RDWR;
#if GEMDOS
                if (flags & DATA_MULUSER) mode |= O_SHARED;     /* GEMDOS has no sharing mechanism without nework support */
#else
                if (flags & (DATA_MULUSER | DATA_MULTASK)) mode |= O_SHARED;
#endif
                if (! file_exist (logname))
                  if ((base->hFileLog = file_create (logname)) >= 0)
                  {
                    file_write (base->hFileLog, sizeof (logid), logid);
                    file_close (base->hFileLog);
                  } /* if, if */

                base->hFileLog = file_open (logname, mode);
              } /* if */
            } /* else */
          } /* else */

          db_unlock (base->datainf);
        } /* else */
      } /* else */
    } /* else */
  } /* else */

  switch (level)
  {
    case 5 : mem_free (base->systable);
             mem_free (base->syscolumn);
             mem_free (base->sysindex);
             mem_free (base->sysrel);
             mem_free (base->sysptr);
    case 4 : if (base->treeinf != NULL) close_tree (base->treeinf);
    case 3 : if (base->datainf != NULL) close_data (base->datainf);
    case 2 : mem_free (base);
    case 1 : base = NULL;
    case 0 : if (level == 0) base->status = get_dberror (); break;
  } /* switch */

  return (base);
} /* db_open_cache */

/*****************************************************************************/

GLOBAL BOOL WINAPI db_close (base)
LPBASE base;

{
  BOOL  ok;
  SHORT i;

  ok = FALSE;

  set_dberror (SUCCESS);

  if (db_lock (base->datainf) && check_multi (base, FALSE))
  {
    if (! (base->treeinf->flags & (TREE_FLUSH | TREE_MULUSER | TREE_MULTASK))) /* write key information */
      for (i = 0, ok = TRUE; (i < base->sysinxs) && ok; i++)
        if (base->sysindex [i].flags & INX_DIRTY)
          ok = do_update (base, SYS_INDEX, &base->sysindex [i], 0L, FALSE);

    ok = close_tree (base->treeinf) && ok;

    if (base->hFileLog >= 0)
      file_close (base->hFileLog);

    db_unlock (base->datainf);
    ok = close_data (base->datainf) && ok;

    mem_free (base->systable);
    mem_free (base->syscolumn);
    mem_free (base->sysindex);
    mem_free (base->sysrel);
    mem_free (base->sysptr);
    mem_free (base);
  } /* if */

  return (ok);
} /* db_close */

/*****************************************************************************/

GLOBAL BOOL WINAPI db_reorg (basename, basepath, cachesize, crypt, reorgfunc, reorg_result)
LPSTR          basename, basepath;
LONG           cachesize;
BOOL           crypt;
REORGFUNC      reorgfunc;
LPREORG_RESULT reorg_result;

{
  return (db_reorg_cache (basename, basepath, 0L, cachesize, crypt, reorgfunc, reorg_result));
} /* db_reorg */

/*****************************************************************************/

GLOBAL BOOL WINAPI db_reorg_cache (basename, basepath, datacache, treecache, crypt, reorgfunc, reorg_result)
LPSTR          basename, basepath;
LONG           datacache, treecache;
BOOL           crypt;
REORGFUNC      reorgfunc;
LPREORG_RESULT reorg_result;

{
  BOOL     ok;
  DATAINF  *data;
  BASE     *base;
  FULLNAME datname, indname, tmpdat, tmpind;
  FILENAME tmpname;
  USHORT   flags;
  UINT     i;

  strcpy (datname, basepath);
  strcat (datname, basename);
  chrcat (datname, SUFFSEP);
  strcpy (indname, datname);
  strcat (datname, DATA_SUFF);
  strcat (indname, TREE_SUFF);

  data = open_data (datname, 0, 0L);
  ok   = data != NULL;

  if (ok)
  {
    for (i = 0; i < 0xFFFF; i++)
    {
      sprintf (tmpname, "~TMP%04x", i);		/* make a temporary file on basepath */
      strcpy (tmpdat, basepath);
      strcat (tmpdat, tmpname);
      chrcat (tmpdat, SUFFSEP);
      strcat (tmpdat, DATA_SUFF);
      if (! file_exist (tmpdat)) break;
    } /* for */

    flags = (USHORT)(data->page0.flags & ~ BASE_ENCODE);
    if (crypt) flags |= BASE_ENCODE;

    base = db_create (tmpname, basepath, (USHORT)(flags | BASE_SUPER), DATASIZE, TREESIZE);
    ok   = base != NULL;

    if (ok)
    {
      db_close (base);

      base = db_open_cache (tmpname, basepath, BASE_NOLOG | BASE_SUPER, datacache, treecache, TREE_CURSORS, NULL, NULL);
      ok   = base != NULL;

      if (ok) ok = do_reorg (base, data, basename, basepath, tmpname, datacache, treecache, file_length (datname), reorgfunc, reorg_result);
    } /* if */

    close_data (data);

    strcpy (tmpind, basepath);
    strcat (tmpind, tmpname);
    chrcat (tmpind, SUFFSEP);
    strcpy (tmpdat, tmpind);
    strcat (tmpind, TREE_SUFF);
    strcat (tmpdat, DATA_SUFF);

    if (ok)
    {
      file_remove (datname);
      file_remove (indname);

      file_rename (tmpdat, datname);
      file_rename (tmpind, indname);
    } /* if */
    else
    {
      file_remove (tmpdat);
      file_remove (tmpind);
    } /* else */
  } /* if */

  return (ok);
} /* db_reorg_cache */

/*****************************************************************************/

GLOBAL BOOL WINAPI db_flush (base, flushdata, flushtree)
LPBASE base;
BOOL   flushdata, flushtree;

{
  BOOL  ok;
  SHORT i;

  ok = FALSE;

  set_dberror (SUCCESS);

  if (db_lock (base->datainf) && check_multi (base, FALSE))
  {
    ok = TRUE;

    if (flushdata)
      ok = ok && flush_data (base->datainf);

    if (flushtree)
      ok = ok && flush_virtual (base->treeinf);

    ok = ok && write_dpg0 (base->datainf);

    if (flushtree)
      if (! (base->treeinf->flags & (TREE_FLUSH | TREE_MULUSER | TREE_MULTASK))) /* write key information */
        for (i = 0; (i < (SHORT)base->systable [SYS_INDEX].recs) && ok; i++)
          if (base->sysindex [i].flags & INX_DIRTY)
            ok = do_update (base, SYS_INDEX, &base->sysindex [i], 0L, FALSE);

    db_unlock (base->datainf);
  } /* if */

  base->status = get_dberror ();

  return (ok);
} /* db_flush */

/*****************************************************************************/

GLOBAL BOOL WINAPI db_beg_trans (base, write)
LPBASE base;
BOOL   write;

{
  BOOL ok;

  set_dberror (SUCCESS);

  ok           = db_lock (base->datainf) && check_multi (base, write);
  base->status = get_dberror ();

  return (ok);
} /* db_beg_trans */

/*****************************************************************************/

GLOBAL BOOL WINAPI db_end_trans (base)
LPBASE base;

{
  BOOL ok;

  set_dberror (SUCCESS);

  ok           = db_unlock (base->datainf);
  base->status = get_dberror ();

  return (ok);
} /* db_end_trans */

/*****************************************************************************/

GLOBAL USHORT WINAPI db_version (base)
LPBASE base;

{
  return (base->datainf->page0.version);
} /* db_version */

/*****************************************************************************/

GLOBAL USHORT WINAPI db_lib_version ()

{
  return (DB_VERSION);
} /* db_lib_version */

/*****************************************************************************/

GLOBAL SHORT WINAPI db_status (base)
LPBASE base;

{
  return ((SHORT)((base == NULL) ? get_dberror () : base->status));
} /* db_status */

/*****************************************************************************/

GLOBAL BOOL WINAPI db_baseinfo (base, base_info)
LPBASE      base;
LPBASE_INFO base_info;

{
  BOOL      ok;
  DATAINF   *data;
  DATA_INFO *data_info;
  TREEINF   *tree;
  TREE_INFO *tree_info;

  set_dberror (SUCCESS);

  ok = db_lock (base->datainf) && check_multi (base, FALSE);

  if (ok)
  {
    strcpy (base_info->username, base->username);
    strcpy (base_info->basename, base->basename);
    strcpy (base_info->basepath, base->basepath);

    data      = base->datainf;
    data_info = &base_info->data_info;

    data_info->version       = data->page0.version;
    data_info->fieldsize     = data->page0.fieldsize;
    data_info->file_size     = data->page0.file_size;
    data_info->next_rec      = data->page0.next_rec;
    data_info->size_released = data->page0.size_released;
    data_info->reorg         = data->page0.reorg;
    data_info->created       = data->page0.created;
    data_info->lastuse       = data->page0.lastuse;
    data_info->cache_size    = data->cache_size;
    data_info->num_calls     = data->num_calls;
    data_info->num_hits      = data->num_hits;

    mem_move (data_info->tbl_ascii, data->page0.tbl_ascii, 256);

    tree      = base->treeinf;
    tree_info = &base_info->tree_info;

    tree_info->num_pages   = tree->page0.num_pages;
    tree_info->used_pages  = tree->page0.used_pages;
    tree_info->free_page   = tree->page0.free_page;
    tree_info->num_vpages  = tree->num_vpages;
    tree_info->num_cursors = tree->num_cursors;
    tree_info->num_calls   = tree->num_calls;
    tree_info->num_hits    = tree->num_hits;

    db_unlock (base->datainf);
  } /* if */

  base->status = get_dberror ();

  return (ok);
} /* db_baseinfo */

/*****************************************************************************/

GLOBAL SHORT WINAPI db_tableinfo (base, table, table_info)
LPBASE       base;
SHORT        table;
LPTABLE_INFO table_info;

{
  BOOL      ok;
  BOOL      found;
  SYSTABLE  *tablep;
  LONG      recs;
  TABLENAME tablename;
  TABLENAME sysname;

  set_dberror (SUCCESS);

  ok = TRUE;
#if 0
  ok = db_lock (base->datainf) && check_multi (base, FALSE);
#endif

  if (ok)
  {
    recs = base->systable [SYS_TABLE].recs;

    if (table == FAILURE)                       /* get table name */
    {
      found  = FALSE;
      table  = SYS_TABLE;
      tablep = base->systable;
      strcpy (tablename, table_info->name);
      str_upper (tablename);
      str_rem_trail (tablename);

      while (! found && (table < (SHORT)recs))
      {
        strcpy (sysname, tablep->name);
        str_upper (sysname);
        found = strcmp (sysname, tablename) == 0;

        if (! found)
        {
          table++;
          tablep++;
        } /* if */
      } /* while */

      if (! found) table = FAILURE;
    } /* if */

    if (table_ok (base, table))
    {
      tablep = &base->systable [table];

      strcpy (table_info->name, tablep->name);
      table_info->recs     = tablep->recs;
      table_info->cols     = tablep->cols;
      table_info->indexes  = tablep->indexes;
      table_info->size     = tablep->size;
      table_info->color    = tablep->color;
      table_info->icon     = tablep->icon;
      table_info->children = tablep->children;
      table_info->parents  = tablep->parents;
      table_info->flags    = tablep->flags;
    } /* if */
    else
      table = FAILURE;

    /*db_unlock (base->datainf);*/
  } /* if */
#if 0
  else
    table = FAILURE;
#endif

  base->status = get_dberror ();

  return (table);
} /* db_tableinfo */

/*****************************************************************************/

GLOBAL SHORT WINAPI db_fieldinfo (base, table, field, field_info)
LPBASE       base;
SHORT        table;
SHORT        field;
LPFIELD_INFO field_info;

{
  BOOL      found;
  SHORT     abs_col;
  SYSTABLE  *tablep;
  SYSCOLUMN *colp;
  FIELDNAME fieldname;
  FIELDNAME sysname;

  set_dberror (SUCCESS);

  if (table_ok (base, table))
  {
    tablep = &base->systable [table];

    if (field == FAILURE)                       /* get field name */
    {
      found   = FALSE;
      field   = 0;
      abs_col = abscol (base, table, field);
      colp    = &base->syscolumn [abs_col];
      strcpy (fieldname, field_info->name);
      str_upper (fieldname);
      str_rem_trail (fieldname);

      while (! found && (field < tablep->cols))
      {
        strcpy (sysname, colp->name);
        str_upper (sysname);
        found = strcmp (sysname, fieldname) == 0;

        if (! found)
        {
          field++;
          colp++;
        } /* if */
      } /* while */

      if (! found) field = FAILURE;
    } /* if */

    if (column_ok (base, table, field))
    {
      abs_col = abscol (base, table, field);
      colp    = &base->syscolumn [abs_col];

      strcpy (field_info->name, colp->name);
      field_info->type   = colp->type;
      field_info->addr   = colp->addr;
      field_info->size   = colp->size;
      field_info->format = colp->format;
      field_info->flags  = colp->flags;
    } /* if */
    else
      field = FAILURE;
  } /* if */
  else
    field = FAILURE;

  base->status = get_dberror ();

  return (field);
} /* db_fieldinfo */

/*****************************************************************************/

GLOBAL SHORT WINAPI db_indexinfo (base, table, inx, index_info)
LPBASE       base;
SHORT        table;
SHORT        inx;
LPINDEX_INFO index_info;

{
  BOOL      ok, found;
  SHORT     abs_inx, num_cols, i;
  SYSTABLE  *tablep;
  SYSCOLUMN *colp;
  SYSINDEX  *inxp;
  INXCOL    *inxcolp;
  CHAR      *p;
  FIELDNAME sysname;
  FIELDNAME indexname;

  set_dberror (SUCCESS);

  ok = TRUE;
#if 0
  ok = db_lock (base->datainf) && check_multi (base, FALSE);
#endif

  if (ok)
  {
    ok = table_ok (base, table);

    if (! ok) inx = FAILURE;

    if (ok && (inx == FAILURE))                 /* get index name */
    {
      found   = FALSE;
      inx     = 0;
      abs_inx = absinx (base, table, inx);
      inxp    = &base->sysindex [abs_inx];
      tablep  = &base->systable [table];
      strcpy (indexname, index_info->name);
      str_upper (indexname);
      str_rem_trail (indexname);

      while (! found && (inx < tablep->indexes))
      {
        strcpy (sysname, inxp->name);
        str_upper (sysname);
        found = strcmp (sysname, indexname) == 0;

        if (! found)
        {
          inx++;
          inxp++;
        } /* if */
      } /* while */

      if (! found) inx = FAILURE;
    } /* if */

    if (index_ok (base, table, inx))
    {
      abs_inx  = absinx (base, table, inx);
      inxp     = &base->sysindex [abs_inx];

      strcpy (index_info->name, inxp->name);
      index_info->indexname [0] = EOS;
      index_info->type          = inxp->type;
      index_info->root          = inxp->root;
      index_info->num_keys      = inxp->num_keys;
      index_info->flags         = inxp->flags;
      mem_move (&index_info->inxcols, &inxp->inxcols, sizeof (INXCOLS));

      num_cols = (SHORT)(inxp->inxcols.size / sizeof (INXCOL));
      inxcolp  = inxp->inxcols.cols;
      p        = index_info->indexname;

      for (i = 0; i < num_cols; i++, inxcolp++)
      {
        colp = &base->syscolumn [abscol (base, table, inxcolp->col)];

        if (strlen (p) + strlen (colp->name) <= MAX_INDEXNAME)
        {
          strcat (p, colp->name);
          strcat (p, ",");
        } /* if */
      } /* for */

      p [strlen (p) - 1] = EOS;         /* delete trailing ',' */
    } /* if */
    else
      inx = FAILURE;

#if 0
    db_unlock (base->datainf);
#endif
  } /* if */

  base->status = get_dberror ();

  return (inx);
} /* db_indexinfo */

/*****************************************************************************/

GLOBAL USHORT WINAPI db_acc_table (base, table)
LPBASE base;
SHORT  table;

{
  USHORT access;

  if (table_ok (base, table))
    access = (USHORT)(base->systable [table].flags & GRANT_ALL);
  else
    access = GRANT_NOTHING;

  return (access);
} /* db_acc_table */

/*****************************************************************************/

GLOBAL USHORT WINAPI db_acc_column (base, table, col)
LPBASE base;
SHORT  table, col;

{
  USHORT access;
  SHORT  abs_col;

  if (column_ok (base, table, col))
  {
    abs_col = abscol (base, table, col);
    access  = (USHORT)(base->syscolumn [abs_col].flags & GRANT_ALL);
  } /* if */
  else
    access = GRANT_NOTHING;

  return (access);
} /* db_acc_column */

/*****************************************************************************/

GLOBAL USHORT WINAPI db_acc_index (base, table, inx)
LPBASE base;
SHORT  table, inx;

{
  USHORT   access, flags;
  SHORT    abs_inx, i, col;
  SHORT    num_cols;
  SYSINDEX *inxp;
  INXCOL   *inxcolp;

  if (index_ok (base, table, inx))
  {
    access   = GRANT_ALL;
    abs_inx  = absinx (base, table, inx);
    inxp     = &base->sysindex [abs_inx];
    num_cols = (SHORT)(inxp->inxcols.size / sizeof (INXCOL));
    inxcolp  = inxp->inxcols.cols;

    for (i = 0; i < num_cols; i++, inxcolp++)
    {
      col     = inxcolp->col;
      flags   = base->syscolumn [abscol (base, table, col)].flags;
      access &= flags;
    } /* for */
  } /* if */
  else
    access = GRANT_NOTHING;

  return (access);
} /* db_acc_index */

/*****************************************************************************/

GLOBAL LPSTR WINAPI db_tablename (base, table)
LPBASE base;
SHORT  table;

{
  return (base->systable [table].name);
} /* db_tablename */

/*****************************************************************************/

GLOBAL LPSTR WINAPI db_fieldname (base, table, field)
LPBASE base;
SHORT  table, field;

{
  return (base->syscolumn [abscol (base, table, field)].name);
} /* db_fieldname */

/*****************************************************************************/

GLOBAL LPSTR WINAPI db_indexname (base, table, inx)
LPBASE base;
SHORT  table, inx;

{
  return (base->sysindex [absinx (base, table, inx)].name);
} /* db_indexname */

/*****************************************************************************/

GLOBAL LPCURSOR WINAPI db_newcursor (base)
LPBASE base;

{
  LPCURSOR cursor;

  set_dberror (SUCCESS);

  cursor = new_cursor (base->treeinf);

  base->status = get_dberror ();

  return (cursor);
} /* db_newcursor */

/*****************************************************************************/

GLOBAL VOID WINAPI db_freecursor (base, cursor)
LPBASE   base;
LPCURSOR cursor;

{
  set_dberror (SUCCESS);

  free_cursor (cursor);

  base->status = get_dberror ();
} /* db_freecursor */

/*****************************************************************************/

GLOBAL BOOL WINAPI db_initcursor (base, table, inx, dir, cursor)
LPBASE   base;
SHORT    table;
SHORT    inx;
SHORT    dir;
LPCURSOR cursor;

{
  BOOL    ok;
  KEY_DFN kd;

  set_dberror (SUCCESS);

  ok = db_lock (base->datainf) && check_multi (base, FALSE);

  if (ok)
  {
    keydfn (base, table, inx, &kd);
    ok = init_cursor (base->treeinf, dir, &kd, cursor);

    db_unlock (base->datainf);
  } /* if */

  base->status = get_dberror ();

  return (ok);
} /* db_initcursor */

/*****************************************************************************/

GLOBAL BOOL WINAPI db_movecursor (base, cursor, steps)
LPBASE   base;
LPCURSOR cursor;
LONG     steps;

{
  BOOL ok;

  set_dberror (SUCCESS);

  ok = db_lock (base->datainf) && check_multi (base, FALSE);

  if (ok)
  {
    ok = move_cursor (cursor, steps);
    db_unlock (base->datainf);
  } /* if */

  base->status = get_dberror ();

  return (ok);
} /* db_movecursor */

/*****************************************************************************/

GLOBAL LONG WINAPI db_readcursor (base, cursor, keyval)
LPBASE   base;
LPCURSOR cursor;
HPVOID   keyval;

{
  LONG addr;
  BOOL ok;

  set_dberror (SUCCESS);

  addr = 0;
  ok   = db_lock (base->datainf) && check_multi (base, FALSE);

  if (ok)
  {
    addr = read_cursor (cursor, keyval);
    db_unlock (base->datainf);
  } /* if */

  base->status = get_dberror ();

  return (addr);
} /* db_readcursor */

/*****************************************************************************/

GLOBAL BOOL WINAPI db_testcursor (base, cursor, dir, keyval)
LPBASE   base;
LPCURSOR cursor;
SHORT    dir;
HPVOID   keyval;

{
  BOOL ok;

  set_dberror (SUCCESS);

  ok = db_lock (base->datainf) && check_multi (base, FALSE);

  if (ok)
  {
#ifdef NONULLKEYS
    ok = ! is_null (cursor->type, keyval);
#endif

    if (! ok)
      set_dberror (DB_CNULLKEY);
    else
      ok = test_cursor (cursor, dir, keyval);

    db_unlock (base->datainf);
  } /* if */

  base->status = get_dberror ();

  return (ok);
} /* db_testcursor */

/*****************************************************************************/

GLOBAL LONG WINAPI db_locateaddr (base, cursor, dir, addr)
LPBASE   base;
LPCURSOR cursor;
SHORT    dir;
LONG     addr;

{
  LONG    moved;
  BOOL ok;

  set_dberror (SUCCESS);

  moved = 0;
  ok    = db_lock (base->datainf) && check_multi (base, FALSE);

  if (ok)
  {
    moved = locate_addr (cursor, dir, addr);
    db_unlock (base->datainf);
  } /* if */

  base->status = get_dberror ();

  return (moved);
} /* db_locateaddr */

/*****************************************************************************/

GLOBAL BOOL WINAPI db_isfirst (base, cursor)
LPBASE   base;
LPCURSOR cursor;

{
  BOOL ok, is_first;

  set_dberror (SUCCESS);

  is_first = FALSE;
  ok       = TRUE; /*db_lock (base->datainf) && check_multi (base, FALSE);*/

  if (ok)
  {
    is_first = is_firstlast (cursor, TRUE);
    /*db_unlock (base->datainf);*/
  } /* if */

  base->status = get_dberror ();

  return (is_first);
} /* db_isfirst */

/*****************************************************************************/

GLOBAL BOOL WINAPI db_islast (base, cursor)
LPBASE   base;
LPCURSOR cursor;

{
  BOOL ok, is_last;

  set_dberror (SUCCESS);

  is_last = FALSE;
  ok      = TRUE; /*db_lock (base->datainf) && check_multi (base, FALSE);*/

  if (ok)
  {
    is_last = is_firstlast (cursor, FALSE);
    /*db_unlock (base->datainf);*/
  } /* if */

  base->status = get_dberror ();

  return (is_last);
} /* db_islast */

/*****************************************************************************/

GLOBAL BOOL WINAPI db_insert (base, table, buffer, status)
LPBASE base;
SHORT  table;
HPVOID buffer;
SHORT  *status;

{
  BOOL      ok, written;
  LOGHEADER lh;

  set_dberror (SUCCESS);
  *status = 0;
  ok      = (base->datainf->flags & DATA_RDONLY) == 0;

  if (ok) ok = (table != SYS_USER) || (base->datainf->flags & BASE_SUPER);

  if (! ok)
    set_dberror (DB_CNOACCESS);
  else
    ok = db_lock (base->datainf) && check_multi (base, TRUE);

  if (ok)
  {
    ok = do_insrec (base, table, buffer, status);

    if (ok && (base->hFileLog >= 0) && is_logtable (base, table))
    {
      lh.size      = sizeof (lh);
      lh.version   = LOG_VERSION;
      lh.operation = LOG_INSERT;
      lh.flags     = LOG_FLAG_NUMBER | LOG_FLAG_USER | LOG_FLAG_TIME;

      written = write_loghead (base, table, &lh);
      written = written && write_logbuf (base, table, buffer);
    } /* if */

    db_unlock (base->datainf);
  } /* if */

  base->status = get_dberror ();

  return (ok);
} /* db_insert */

/*****************************************************************************/

GLOBAL BOOL WINAPI db_delete (base, table, address, status)
LPBASE  base;
SHORT   table;
LONG    address;
LPSHORT status;

{
  BOOL      ok, written;
  LOGHEADER lh;

  set_dberror (SUCCESS);
  *status = 0;
  ok      = (base->datainf->flags & DATA_RDONLY) == 0;

  if (ok) ok = (table != SYS_USER) || (base->datainf->flags & BASE_SUPER);

  if (! ok)
    set_dberror (DB_CNOACCESS);
  else
    ok = db_lock (base->datainf) && check_multi (base, TRUE);

  if (ok)
  {
    ok = do_delrec (base, table, address, status);

#if GEMDOS
    if (base->datainf->flags & DATA_MULUSER)            /* GEMDOS has no locking mechanism without netware support */
#else
    if (base->datainf->flags & (DATA_MULUSER | DATA_MULTASK))
#endif
      if (file_locked (base->datainf->handle, address, 1L))
        do_recunlock (base, address);

    if (ok && (base->hFileLog >= 0) && is_logtable (base, table))
    {
      lh.size      = sizeof (lh);
      lh.version   = LOG_VERSION;
      lh.operation = LOG_DELETE;
      lh.flags     = LOG_FLAG_NUMBER | LOG_FLAG_USER | LOG_FLAG_TIME;

      written = write_loghead (base, table, &lh);
      written = written && file_write (base->hFileLog, sizeof (LONG), &address) == sizeof (LONG);
    } /* if */

    db_unlock (base->datainf);
  } /* if */

  base->status = get_dberror ();

  return (ok);
} /* db_delete */

/*****************************************************************************/

GLOBAL BOOL WINAPI db_undelete (base, address, status)
LPBASE  base;
LONG    address;
LPSHORT status;

{
  BOOL       ok, written;
  SHORT      table;
  SYSTABLE   *tablep;
  SYSDELETED sysdel;
  CURSOR     cursor;
  HPCHAR     buffer;
  KEY_DFN    kd;
  LOGHEADER  lh;

  set_dberror (SUCCESS);
  *status = 0;
  ok      = (base->datainf->flags & DATA_RDONLY) == 0;

  if (! ok)
    set_dberror (DB_CNOACCESS);
  else
    ok = db_lock (base->datainf) && check_multi (base, TRUE);

  if (ok)
  {
    ok = undelete_data (base->datainf, address);

    if (ok)
    {
      keydfn (base, SYS_DELETED, 1, &kd);
      ok = search_key (base->treeinf, ASCENDING, &cursor, &address, 0L, &kd, FALSE);

      if (ok)
      {
        ok = do_read (base, SYS_DELETED, &sysdel, &cursor, 0L, TRUE, NULL);

        if (ok) ok = (db_acc_table (base, sysdel.table) & GRANT_INSERT) != 0;

        if (ok) ok = (sysdel.table != SYS_USER) || (base->datainf->flags & BASE_SUPER);

        if (ok)
        {
          table  = sysdel.table;
          tablep = &base->systable [table];
	  buffer = (HPCHAR)mem_alloc (tablep->size);
          ok     = buffer != NULL;

          if (! ok)
            set_dberror (DB_NOMEMORY);
          else
          {
            ok = do_read (base, table, buffer, NULL, address, FALSE, NULL);

            if (ok)
            {
              ok = test_keys (base, table, buffer, 0L, status);
              if (ok) ok = test_cols (base, table, buffer, status);
              if (ok) ok = test_ins (base, table, buffer, status);

              if (ok)
              {
                ok = ins_keys (base, table, buffer, address);

                if (ok)
                {
                  base->systable [table].recs++;        /* increase number of recs */
                  ok = do_update (base, SYS_TABLE, &base->systable [table], 0L, FALSE);

                  if (ok)
                  {
                    ok = do_recunlock (base, sysdel.address);
                    ok = do_delete (base, SYS_DELETED, sysdel.address);

                    if (ok)
                    {
                      base->systable [SYS_DELETED].recs--;  /* decrease number of recs */
                      ok = do_update (base, SYS_TABLE, &base->systable [SYS_DELETED], 0L, FALSE);
                    } /* if */
                  } /* if */
                } /* if */
              } /* if */
              else
                delete_data (base->datainf, address);   /* ok remains FALSE */
            } /* if */

            mem_free (buffer);
          } /* else */
        } /* if */
        else
        {
          if (get_dberror == SUCCESS) set_dberror (DB_CNOACCESS);
          delete_data (base->datainf, address);         /* ok remains FALSE */
        } /* else */
      } /* if */
      else
      {
        set_dberror (DB_TNOKEY);
        delete_data (base->datainf, address);           /* ok remains FALSE */
      } /* else */
    } /* if */

    if (ok && (base->hFileLog >= 0))
    {
      lh.size      = sizeof (lh);
      lh.version   = LOG_VERSION;
      lh.operation = LOG_UNDELETE;
      lh.flags     = LOG_FLAG_USER | LOG_FLAG_TIME;

      written = write_loghead (base, SYS_DELETED, &lh);
      written = written && file_write (base->hFileLog, sizeof (LONG), &address) == sizeof (LONG);
    } /* if */

    db_unlock (base->datainf);
  } /* if */

  base->status = get_dberror ();

  return (ok);
} /* db_undelete */

/*****************************************************************************/

GLOBAL BOOL WINAPI db_update (base, table, buffer, status)
LPBASE base;
SHORT  table;
HPVOID buffer;
SHORT  *status;

{
  BOOL      ok, written;
  LOGHEADER lh;

  set_dberror (SUCCESS);
  *status = 0;
  ok      = (base->datainf->flags & DATA_RDONLY) == 0;

  if (ok) ok = (table != SYS_USER) || (base->datainf->flags & BASE_SUPER);

  if (ok)
    if (*(LONG *)buffer == base->systable [SYS_USER].address)
      ok = (base->datainf->flags & BASE_SUPER) != 0;

  if (! ok)
    set_dberror (DB_CNOACCESS);
  else
    ok = db_lock (base->datainf) && check_multi (base, TRUE);

  if (ok)
  {
    ok = do_updtrec (base, table, buffer, 0L, status, TRUE);

#if GEMDOS
    if (base->datainf->flags & DATA_MULUSER)            /* GEMDOS has no locking mechanism without netware support */
#else
    if (base->datainf->flags & (DATA_MULUSER | DATA_MULTASK))
#endif
      if (file_locked (base->datainf->handle, *(LONG *)buffer, 1L))
        do_recunlock (base, *(LONG *)buffer);

    if (ok && (base->hFileLog >= 0) && is_logtable (base, table))
    {
      lh.size      = sizeof (lh);
      lh.version   = LOG_VERSION;
      lh.operation = LOG_UPDATE;
      lh.flags     = LOG_FLAG_NUMBER | LOG_FLAG_USER | LOG_FLAG_TIME;

      written = write_loghead (base, table, &lh);
      written = written && file_write (base->hFileLog, sizeof (LONG), buffer) == sizeof (LONG);
      written = written && write_logbuf (base, table, buffer);
    } /* if */

    db_unlock (base->datainf);
  } /* if */

  base->status = get_dberror ();

  return (ok);
} /* db_update */

/*****************************************************************************/

GLOBAL BOOL WINAPI db_update_opt (base, table, buffer, old_version, new_version, status)
LPBASE base;
SHORT  table;
HPVOID buffer;
LONG   old_version;
LPLONG new_version;
SHORT  *status;

{
  BOOL      ok, written;
  LOGHEADER lh;

  set_dberror (SUCCESS);
  *status = 0;
  ok      = (base->datainf->flags & DATA_RDONLY) == 0;

  if (ok) ok = (table != SYS_USER) || (base->datainf->flags & BASE_SUPER);

  if (ok)
    if (*(LONG *)buffer == base->systable [SYS_USER].address)
      ok = (base->datainf->flags & BASE_SUPER) != 0;

  if (! ok)
    set_dberror (DB_CNOACCESS);
  else
    ok = db_lock (base->datainf) && check_multi (base, TRUE);

  if (ok)
  {
    ok = do_updtrec (base, table, buffer, old_version, status, TRUE);

    if (ok && (new_version != NULL))
      read_data (base->datainf, *(LONG *)buffer, NULL, NULL, NULL, new_version, NULL, NULL);

    if (ok && (base->hFileLog >= 0) && is_logtable (base, table))
    {
      lh.size      = sizeof (lh);
      lh.version   = LOG_VERSION;
      lh.operation = LOG_UPDATE;
      lh.flags     = LOG_FLAG_NUMBER | LOG_FLAG_USER | LOG_FLAG_TIME;

      written = write_loghead (base, table, &lh);
      written = written && file_write (base->hFileLog, sizeof (LONG), buffer) == sizeof (LONG);
      written = written && write_logbuf (base, table, buffer);
    } /* if */

    db_unlock (base->datainf);
  } /* if */

  base->status = get_dberror ();

  return (ok);
} /* db_update_opt */

/*****************************************************************************/

GLOBAL BOOL WINAPI db_read (base, table, buffer, cursor, address, modify)
LPBASE   base;
SHORT    table;
HPVOID   buffer;
LPCURSOR cursor;
LONG     address;
BOOL     modify;

{
  BOOL ok;

  set_dberror (SUCCESS);

  ok = db_lock (base->datainf) && check_multi (base, FALSE);

  if (ok)
  {
    ok = (db_acc_table (base, table) & GRANT_SELECT) != 0;

    if (! ok)
      set_dberror (DB_CNOACCESS);
    else
      ok = do_read (base, table, buffer, cursor, address, modify, NULL);

    db_unlock (base->datainf);
  } /* if */

  base->status = get_dberror ();

  return (ok);
} /* db_read */

/*****************************************************************************/

GLOBAL BOOL WINAPI db_read_opt (base, table, buffer, cursor, address, version)
LPBASE   base;
SHORT    table;
HPVOID   buffer;
LPCURSOR cursor;
LONG     address;
LPLONG   version;

{
  BOOL ok;

  set_dberror (SUCCESS);

  ok = db_lock (base->datainf) && check_multi (base, FALSE);

  if (ok)
  {
    ok = (db_acc_table (base, table) & GRANT_SELECT) != 0;

    if (! ok)
      set_dberror (DB_CNOACCESS);
    else
      ok = do_read (base, table, buffer, cursor, address, FALSE, version);

    db_unlock (base->datainf);
  } /* if */

  base->status = get_dberror ();

  return (ok);
} /* db_read_opt */

/*****************************************************************************/

GLOBAL LPSYSBLOB WINAPI db_readblob (base, buffer, cursor, address, modify)
LPBASE    base;
LPSYSBLOB buffer;
LPCURSOR  cursor;
LONG      address;
BOOL      modify;

{
  SYSBLOB *blob;
  BOOL    ok;

  set_dberror (SUCCESS);

  blob = NULL;
  ok   = db_lock (base->datainf) && check_multi (base, FALSE);

  if (ok)
  {
    ok = (db_acc_table (base, SYS_BLOB) & GRANT_SELECT) != 0;

    if (! ok)
      set_dberror (DB_CNOACCESS);
    else
      blob = do_readblob (base, buffer, cursor, address, modify);

    db_unlock (base->datainf);
  } /* if */

  base->status = get_dberror ();

  return (blob);
} /* db_readblob */

/*****************************************************************************/

GLOBAL BOOL WINAPI db_search (base, table, inx, dir, cursor, buffer, address)
LPBASE   base;
SHORT    table;
SHORT    inx;
SHORT    dir;
LPCURSOR cursor;
HPVOID   buffer;
LONG     address;

{
  BOOL    ok;
  KEY     keyval;
  KEY_DFN kd;

  set_dberror (SUCCESS);

  ok = db_lock (base->datainf) && check_multi (base, FALSE);

  if (ok)
  {
    ok = do_buildkey (base, table, inx, buffer, keyval);

    if (! ok)
      set_dberror (DB_CNULLKEY);
    else
    {
      keydfn (base, table, inx, &kd);
      ok = search_key (base->treeinf, dir, cursor, keyval, address, &kd, TRUE);
    } /* else */

    db_unlock (base->datainf);
  } /* if */

  base->status = get_dberror ();

  return (ok);
} /* db_search */

/*****************************************************************************/

GLOBAL BOOL WINAPI db_keysearch (base, table, inx, dir, cursor, keyval, address)
LPBASE   base;
SHORT    table;
SHORT    inx;
SHORT    dir;
LPCURSOR cursor;
HPVOID   keyval;
LONG     address;

{
  BOOL    ok;
  KEY_DFN kd;

  set_dberror (SUCCESS);

  ok = db_lock (base->datainf) && check_multi (base, FALSE);

  if (ok)
  {
    keydfn (base, table, inx, &kd);
    ok = search_key (base->treeinf, dir, cursor, keyval, address, &kd, TRUE);
    db_unlock (base->datainf);
  } /* if */

  base->status = get_dberror ();

  return (ok);
} /* db_keysearch */

/*****************************************************************************/

GLOBAL BOOL WINAPI db_reclock (base, address)
LPBASE base;
LONG   address;

{
  BOOL ok;

  set_dberror (SUCCESS);

  ok = db_lock (base->datainf) && check_multi (base, FALSE);

  if (ok)
  {
    ok = do_reclock (base, address);
    db_unlock (base->datainf);
  } /* if */

  base->status = get_dberror ();

  return (ok);
} /* db_reclock */

/*****************************************************************************/

GLOBAL BOOL WINAPI db_recunlock (base, address)
LPBASE base;
LONG   address;

{
  BOOL ok;

  set_dberror (SUCCESS);

  ok = db_lock (base->datainf) && check_multi (base, FALSE);

  if (ok)
  {
    ok = do_recunlock (base, address);
    db_unlock (base->datainf);
  } /* if */

  base->status = get_dberror ();

  return (ok);
} /* db_recunlock */

/*****************************************************************************/

GLOBAL LONG WINAPI db_pack (base, table, buffer, packbuf)
LPBASE base;
SHORT  table;
HPVOID buffer;
HPVOID packbuf;

{
  LONG size;

  set_dberror (SUCCESS);

  size = do_pack (base, table, buffer, packbuf);

  base->status = get_dberror ();

  return (size);
} /* db_pack */

/*****************************************************************************/

GLOBAL LONG WINAPI db_unpack (base, table, buffer, packbuf, packsize)
LPBASE base;
SHORT  table;
HPVOID buffer;
HPVOID packbuf;
LONG   packsize;

{
  LONG size;

  set_dberror (SUCCESS);

  size = do_unpack (base, table, buffer, packbuf, packsize);

  base->status = get_dberror ();

  return (size);
} /* db_unpack */

/*****************************************************************************/

GLOBAL BOOL WINAPI db_killtree (base, table, inx)
LPBASE base;
SHORT  table;
SHORT  inx;

{
  BOOL     ok;
  SYSINDEX *inxp;

  set_dberror (SUCCESS);

  ok = db_lock (base->datainf) && check_multi (base, TRUE);

  if (ok)
  {
    inxp = &base->sysindex [absinx (base, table, inx)];
    ok   = kill_tree (base->treeinf, inxp->root);

    if (ok)
    {
      inxp->root      = 0;
      inxp->num_keys  = 0;
      inxp->flags    |= INX_DIRTY;
    } /* if */

    db_unlock (base->datainf);
  } /* if */

  base->status = get_dberror ();

  return (ok);
} /* db_killtree */

/*****************************************************************************/

GLOBAL BOOL WINAPI db_reorgtree (base, table, buffer)
LPBASE base;
SHORT  table;
HPVOID buffer;

{
  BOOL     ok;
  SHORT    i;
  SYSTABLE *tablep;
  SYSINDEX *inxp;
  SYSPTR   *sysp;
  KEY      keyval;
  KEY_DFN  kd;
  LONG     addr;

  set_dberror (SUCCESS);

  ok     = TRUE;
  tablep = &base->systable [table];
  sysp   = &base->sysptr [table];
  inxp   = &base->sysindex [sysp->index];
  addr   = *(LONG *)buffer;

  for (i = 0; ok && (i < tablep->indexes); i++, inxp++)
    if (inxp->flags & INX_REORG)
      if (do_buildkey (base, table, i, buffer, keyval)) /* key is not null */
      {
        kd.type     = inxp->type;
        kd.root     = inxp->root;
        kd.num_keys = inxp->num_keys;

        ok = insert_key (base->treeinf, keyval, addr, &kd);

        inxp->root     = kd.root;
        inxp->num_keys = kd.num_keys;

        update_root (base, table, i);

        if (ok && (base->treeinf->flags & (TREE_FLUSH | TREE_MULUSER | TREE_MULTASK)))
          ok = do_update (base, SYS_INDEX, inxp, 0L, FALSE);
        else
          inxp->flags |= INX_DIRTY;
      } /* if, if, for */

  base->status = get_dberror ();

  return (ok);
} /* db_reorgtree */

/*****************************************************************************/

GLOBAL BOOL WINAPI db_buildkey (base, table, inx, buffer, keyval)
LPBASE base;
SHORT  table;
SHORT  inx;
HPVOID buffer;
HPVOID keyval;

{
  BOOL ok;

  set_dberror (SUCCESS);

  ok = do_buildkey (base, table, inx, buffer, keyval);

  base->status = get_dberror ();

  return (ok);
} /* db_buildkey */

/*****************************************************************************/

GLOBAL BOOL WINAPI db_fillnull (base, table, buffer)
LPBASE base;
SHORT  table;
HPVOID buffer;

{
  BOOL      ok;
  SHORT     abs_col;
  SHORT     field;
  SYSTABLE  *tablep;
  SYSCOLUMN *colp;
  HPCHAR    bufp;

  set_dberror (SUCCESS);

  ok = table_ok (base, table);

  if (ok)
  {
    tablep  = &base->systable [table];
    abs_col = abscol (base, table, 0);
    colp    = &base->syscolumn [abs_col];
    bufp    = (HPCHAR)buffer;

    for (field = 0; field < tablep->cols; field++, colp++)
      set_null (colp->type, bufp + colp->addr);
  } /* if */

  base->status = get_dberror ();

  return (ok);
} /* db_fillnull */

/*****************************************************************************/

GLOBAL BOOL WINAPI db_getfield (base, table, field, buffer, value)
LPBASE base;
SHORT  table;
SHORT  field;
HPVOID buffer;
HPVOID value;

{
  BOOL      ok;
  LONG      size;
  SHORT     abs_col;
  SYSCOLUMN *colp;
  HPCHAR    bufp;

  set_dberror (SUCCESS);

  ok = column_ok (base, table, field);

  if (ok)
  {
    abs_col = abscol (base, table, field);
    colp    = &base->syscolumn [abs_col];
    bufp    = (HPCHAR)buffer + colp->addr;
    size    = typesize (colp->type, bufp);

    mem_lmove (value, bufp, size);
  } /* if */

  base->status = get_dberror ();

  return (ok);
} /* db_getfield */

/*****************************************************************************/

GLOBAL BOOL WINAPI db_setfield (base, table, field, buffer, value)
LPBASE base;
SHORT  table;
SHORT  field;
HPVOID buffer;
HPVOID value;

{
  BOOL      ok;
  LONG      size;
  SHORT     abs_col;
  SYSCOLUMN *colp;
  HPCHAR    bufp;

  set_dberror (SUCCESS);

  ok = column_ok (base, table, field);

  if (ok)
  {
    abs_col = abscol (base, table, field);
    colp    = &base->syscolumn [abs_col];
    bufp    = (HPCHAR)buffer + colp->addr;
    size    = min (typesize (colp->type, value), colp->size);

    mem_lmove (bufp, value, size);

    if (VARLEN (colp->type))                    /* any string type */
      bufp [size - 1] = EOS;                    /* terminate string */
  } /* if */

  base->status = get_dberror ();

  return (ok);
} /* db_setfield */

/*****************************************************************************/

GLOBAL SHORT WINAPI db_cmpfield (base, table, field, buffer1, buffer2)
LPBASE base;
SHORT  table;
SHORT  field;
HPVOID buffer1;
HPVOID buffer2;

{
  BOOL      ok;
  SHORT     value;
  SHORT     abs_col;
  SYSCOLUMN *colp;
  HPCHAR    bufp1;
  HPCHAR    bufp2;

  set_dberror (SUCCESS);

  value = 0;
  ok    = column_ok (base, table, field);

  if (ok)
  {
    abs_col = abscol (base, table, field);
    colp    = &base->syscolumn [abs_col];
    bufp1   = (HPCHAR)buffer1 + colp->addr;
    bufp2   = (HPCHAR)buffer2 + colp->addr;

    value = cmp_vals (colp->type, bufp1, bufp2);
  } /* if */

  base->status = get_dberror ();

  return (value);
} /* db_cmpfield */

/*****************************************************************************/

GLOBAL SHORT WINAPI db_expand (base, datasize, treesize)
LPBASE base;
LONG   datasize;
LONG   treesize;

{
  SHORT result;

  set_dberror (SUCCESS);

  result = SUCCESS;

  if (! expand_data (base->datainf, datasize))
    result = -1;
  else
    if (! expand_tree (base->treeinf, treesize)) result = -2;

  base->status = get_dberror ();

  return (result);
} /* db_expand */

/*****************************************************************************/

GLOBAL VOID WINAPI db_convstr (base, s)
LPBASE base;
HPCHAR s;

{
  set_dberror (SUCCESS);

  do_convstr (base, s);

  base->status = get_dberror ();
} /* db_convstr */

/*****************************************************************************/

GLOBAL BOOL WINAPI db_getstr (base, table, field, buffer, s)
LPBASE base;
SHORT  table;
SHORT  field;
HPVOID buffer;
HPCHAR s;

{
  BOOL      ok;
  SHORT     abs_col;
  SYSCOLUMN *colp;
  HPCHAR    bufp;

  set_dberror (SUCCESS);

  ok = column_ok (base, table, field);

  if (ok)
  {
    abs_col = abscol (base, table, field);
    colp    = &base->syscolumn [abs_col];
    bufp    = (HPCHAR)buffer + colp->addr;

    if (HASWILD (colp->type))                   /* a character string */
      str_rem_trail (bufp);

    bin2str (colp->type, bufp, s);
  } /* if */

  base->status = get_dberror ();

  return (ok);
} /* db_getstr */

/*****************************************************************************/

GLOBAL BOOL WINAPI db_setstr (base, table, field, buffer, s)
LPBASE base;
SHORT  table;
SHORT  field;
HPVOID buffer;
HPCHAR s;

{
  BOOL      ok;
  SHORT     abs_col;
  SYSCOLUMN *colp;
  HPCHAR    bufp;

  set_dberror (SUCCESS);

  ok = column_ok (base, table, field);

  if (ok)
  {
    abs_col = abscol (base, table, field);
    colp    = &base->syscolumn [abs_col];
    bufp    = (HPCHAR)buffer + colp->addr;

    str2bin (colp->type, s, bufp);

    if (HASWILD (colp->type))                   /* a character string */
      str_rem_trail (bufp);
  } /* if */

  base->status = get_dberror ();

  return (ok);
} /* db_setstr */

/*****************************************************************************/

LOCAL VOID set_systable ()
{
  SHORT     sTable, sCols, sInxs, sColPos, sInxPos, sNumTables, sNumColumns, sNumIndexes;
  SYSTABLE  *pTable;
  SYSCOLUMN *pColumn;
  SYSINDEX  *pIndex;

  if (! initialized)
  {
    initialized = TRUE;
    sNumTables  = (SHORT)(sizeof (systable) / sizeof (SYSTABLE));
    sNumColumns = (SHORT)(sizeof (syscolumn) / sizeof (SYSCOLUMN));
    sNumIndexes = (SHORT)(sizeof (sysindex) / sizeof (SYSINDEX));

    for (sTable = sColPos = sInxPos = 0, pTable = systable, pColumn = syscolumn, pIndex = sysindex; sTable < sNumTables; sTable++, pTable++)
    {
      sCols = 0;
      sInxs = 0;

      while ((sColPos < sNumColumns) && (pColumn->table == sTable)) sCols++, sColPos++, pColumn++;
      while ((sInxPos < sNumIndexes) && (pIndex->table == sTable)) sInxs++, sInxPos++, pIndex++;

      pTable->cols    = sCols;
      pTable->indexes = sInxs;
    } /* for */
  } /* if */
} /* set_systable */

/*****************************************************************************/

LOCAL VOID build_name (basename, basepath, dataname, treename)
LPSTR basename, basepath, dataname, treename;

{
  CHAR sep [2];

  strcpy (dataname, basepath);
  strcpy (treename, basepath);

  sep [0] = SUFFSEP;
  sep [1] = EOS;

  strcat (dataname, basename);          /* build dataname */
  strcat (dataname, sep);
  strcat (dataname, DATA_SUFF);

  strcat (treename, basename);          /* build treename */
  strcat (treename, sep);
  strcat (treename, TREE_SUFF);
} /* build_name */

/*****************************************************************************/

LOCAL BOOL check_multi (base, do_write)
LPBASE base;
BOOL   do_write;

{
  BOOL ok;

  ok = TRUE;

  if (test_multi (base->datainf, do_write))     /* something changed */
  {
    ok = kill_virtual (base->treeinf);
    if (ok) ok = read_dpg0 (base->datainf);
    if (ok) ok = read_systable (base, base->systable, (LONG)base->systbls);
    if (ok) ok = read_sysindex (base, base->sysindex, (LONG)base->sysinxs);
  } /* if */

  return (ok);
} /* check_multi */

/*****************************************************************************/

LOCAL BOOL test_dic (base)
LPBASE base;

{
  BOOL      ok, modify, mod;
  SHORT     table, diff, status;
  SYSCOLUMN *syscolp;
  SYSINDEX  *sysinxp;
  SYSPTR    sp [NUM_SYSTABLES];

  ok = load_dic (base);

  if (ok)
  {
    fill_sysptr (systable, sp, NUM_SYSTABLES);  /* fill only NUM_SYSTABLES tables (systables) */

    for (table = SYS_TABLE, modify = FALSE; (table < NUM_SYSTABLES) && ok; table++)
    {
      mod  = FALSE;
      diff = (SHORT)(systable [table].cols - base->systable [table].cols);

      if (diff > 0)                             /* there are new columns in this version */
      {
        modify  = mod = TRUE;
        syscolp = &syscolumn [sp [table].column + systable [table].cols - diff];

        while (ok && (diff > 0))
        {
          ok = do_insrec (base, SYS_COLUMN, syscolp, &status);
          diff--;
          syscolp++;
        } /* while */
      } /* if */

      diff = (SHORT)(systable [table].indexes - base->systable [table].indexes);

      if (diff > 0)                             /* there are new indexes in this version */
      {
        modify  = mod = TRUE;
        sysinxp = &sysindex [sp [table].index + systable [table].indexes - diff];

        while (ok && (diff > 0))
        {
          ok = do_insrec (base, SYS_INDEX, sysinxp, &status);
          diff--;
          sysinxp++;
        } /* while */
      } /* if */

      if (ok && mod)
      {
        base->systable [table].cols    = systable [table].cols;		/* update number of cols */
        base->systable [table].indexes = systable [table].indexes;	/* update number of indexes */
        base->systable [table].size    = systable [table].size;		/* update size */
        base->systable [table].flags   = systable [table].flags;	/* update flags */
        strcpy (base->systable [table].name, systable [table].name);	/* in case name has been changed */
        ok = do_update (base, SYS_TABLE, &base->systable [table], 0L, FALSE);
      } /* if */
    } /* for */

    if (ok && modify)
    {
#if 0
      base->datainf->page0.version = DB_VERSION;   		/* db is now updated to new version */
#endif

      mem_free (base->systable);
      mem_free (base->syscolumn);
      mem_free (base->sysindex);
      mem_free (base->sysrel);
      mem_free (base->sysptr);

      ok = load_dic (base);
    } /* if */
  } /* if */

  return (ok);
} /* test_dic */

/*****************************************************************************/

LOCAL BOOL load_dic (base)
LPBASE base;

{
  BOOL      ok;
  SHORT     level;
  LONG      tables, recs;
  SYSTABLE  *systblp, systbl0;
  SYSCOLUMN *syscolp;
  SYSINDEX  *sysinxp;
  SYSREL    *sysrelp;
  SYSPTR    sp [NUM_SYSTABLES];

  level           = 0;
  base->systable  = systable;                   /* set only while systables are read */
  base->syscolumn = syscolumn;
  base->sysindex  = sysindex;
  base->sysptr    = sp;

  fill_sysptr (base->systable, base->sysptr, NUM_SYSTABLES); /* fill only NUM_SYSTABLES tables (systables) */

  ok = do_read (base, SYS_TABLE, &systbl0, NULL, base->datainf->page0.addr_table, FALSE, NULL);

  if (ok)
  {
    tables  = systbl0.recs;
    systblp = (SYSTABLE *)mem_alloc (tables * sizeof (SYSTABLE));
    ok      = systblp != NULL;

    if (! ok)
    {
      level = 1;
      set_dberror (DB_NOMEMORY);
    } /* if */
    else
    {
      ok = read_systable (base, systblp, tables);

      if (! ok)
        level = 2;
      else
      {
        base->systable = systblp;                       /* correct pointer */
        base->sysptr   = (SYSPTR *)mem_alloc (tables * sizeof (SYSPTR));
        ok             = base->sysptr != NULL;

        if (! ok)
        {
          level = 2;
          set_dberror (DB_NOMEMORY);
        } /* if */
        else
        {
          fill_sysptr (base->systable, base->sysptr, (SHORT)tables);

          recs    = base->systable [SYS_COLUMN].recs;
          syscolp = (SYSCOLUMN *)mem_alloc (recs * sizeof (SYSCOLUMN));
          ok      = syscolp != NULL;

          if (! ok)
          {
            level = 3;
            set_dberror (DB_NOMEMORY);
          } /* if */
          else
          {
            ok = read_syscolumn (base, syscolp, recs);

            if (! ok)
              level = 4;
            else
            {
              base->syscolumn = syscolp;                /* correct pointer */

              recs    = base->systable [SYS_INDEX].recs;
              sysinxp = (SYSINDEX *)mem_alloc (recs * sizeof (SYSINDEX));
              ok      = sysinxp != NULL;

              if (! ok)
              {
                level = 4;
                set_dberror (DB_NOMEMORY);
              } /* if */
              else
              {
                ok = read_sysindex (base, sysinxp, recs);

                if (! ok)
                  level = 5;
                else
                {
                  base->sysindex = sysinxp;             /* correct the pointer */

                  recs = base->systable [SYS_REL].recs;

                  if (recs > 0)
                  {
                    sysrelp = (SYSREL *)mem_alloc (recs * sizeof (SYSREL));
                    ok      = sysrelp != NULL;

                    if (! ok)
                    {
                      level = 5;
                      set_dberror (DB_NOMEMORY);
                    } /* if */
                    else
                    {
                      ok = read_sysrel (base, sysrelp, recs);

                      if (! ok)
                        level = 6;
                      else
                        base->sysrel = sysrelp;         /* correct the pointer */
                    } /* else */
                  } /* if */
                } /* else */
              } /* else */
            } /* else */
          } /* else */
        } /* else */
      } /* else */
    } /* else */
  } /* if */

  switch (level)
  {
    case 6 : mem_free (base->sysrel);
    case 5 : mem_free (base->sysindex);
    case 4 : mem_free (base->syscolumn);
    case 3 : mem_free (base->sysptr);
    case 2 : mem_free (base->systable);
    case 1 : break;
    case 0 : break;
  } /* switch */

  if (! ok) set_dberror (DB_CREADDD);

  return (ok);
} /* load_dic */

/*****************************************************************************/

LOCAL BOOL create_dic (base)
LPBASE base;

{
  BOOL   ok;
  SHORT  i;
  LONG   tbl_recs, col_recs, inx_recs;
  DPAGE0 *p;

  ok       = TRUE;
  tbl_recs = base->systable [SYS_TABLE].recs;

  for (i = 0; ok && (i < (SHORT)tbl_recs); i++)
    ok = do_insert (base, SYS_TABLE, &base->systable [i]);

  col_recs = base->systable [SYS_COLUMN].recs;

  for (i = 0; ok && (i < (SHORT)col_recs); i++)
    ok = do_insert (base, SYS_COLUMN, &base->syscolumn [i]);

  inx_recs = base->systable [SYS_INDEX].recs;

  for (i = 0; ok && (i < (SHORT)inx_recs); i++)
    ok = do_insert (base, SYS_INDEX, &base->sysindex [i]);

  p              = &base->datainf->page0;
  p->addr_table  = base->systable [0].address;
  p->addr_column = base->syscolumn [0].address;
  p->addr_index  = base->sysindex [0].address;

  p->root_table  = base->sysindex [absinx (base, SYS_TABLE,  1)].root;
  p->root_column = base->sysindex [absinx (base, SYS_COLUMN, 1)].root;
  p->root_index  = base->sysindex [absinx (base, SYS_INDEX,  1)].root;

  for (i = 0; ok && (i < systable [SYS_INDEX].indexes); i++)                      /* update indexes of sysindex on disk */
    ok = do_update (base, SYS_INDEX, &base->sysindex [absinx (base, SYS_INDEX, i)], 0L, FALSE);

  for (i = 0; i < (SHORT)inx_recs; i++) /* for creating a second db */
  {
    sysindex [i].root      = 0;
    sysindex [i].num_keys  = 0;
    sysindex [i].flags    &= ~ INX_DIRTY;
  } /* for */

  if (! ok) set_dberror (DB_CCREATEDD);

  return (ok);
} /* create_dic */

/*****************************************************************************/

LOCAL BOOL set_access (base, username, password)
LPBASE base;
LPSTR  username, password;

{
  BOOL      ok;
  CURSOR    cursor;
  SYSUSER   sys_user;
  SHORT     *listp;
  LONG      size;
  SHORT     table;
  SHORT     column;
  SHORT     abs_col;
  SYSTABLE  *tablep;
  SYSCOLUMN *colp;
  KEY_DFN   kd;
  USERNAME  user, name;
  PASSWORD  pass;
#ifdef PHOENIX
  CHAR      *p;
#endif

  strcpy (user, username);
  strcpy (pass, password);
  do_convstr (base, user);
  do_convstr (base, pass);

#ifdef PHOENIX
  for (p = user; *p; p++) *p += 1;
  for (p = pass; *p; p++) *p += 1;

  if ((strcmp (user, "EH0KH") == 0) &&                  /* DG/JG */
      (strcmp (pass, "%QIPFOJY%") == 0)) return (TRUE); /* $PHOENIX$ */
#endif

  if (base->systable [SYS_USER].recs == 0) return (TRUE);

  strcpy (user, username);
  do_convstr (base, user);
  keydfn (base, SYS_USER, 1, &kd);      /* key #1 is username */
  ok = search_key (base->treeinf, ASCENDING, &cursor, user, 0L, &kd, FALSE);

  if (ok)
  {
    ok = do_read (base, SYS_USER, &sys_user, &cursor, 0L, TRUE, NULL);

    if (ok)
    {
      sys_user.count++;
      get_tstamp (&sys_user.lastuse);
      if (! (base->datainf->flags & DATA_RDONLY)) ok = do_update (base, SYS_USER, &sys_user, 0L, FALSE);
      do_recunlock (base, sys_user.address);
    } /* if */

    if (ok)
    {
      strcpy (name, sys_user.name);
      do_convstr (base, name);
      strcpy (pass, password);
      do_convstr (base, pass);
      build_pass (pass, pass);
      ok = (strcmp (pass, sys_user.pass) == 0) && (strcmp (user, name) == 0);
    } /* if */

    if (ok)
    {
      if (sys_user.flags & USER_READONLY)
        for (table = SYS_TABLE; table < (SHORT)base->systable [SYS_TABLE].recs; table++)
        {
          tablep        = &base->systable [table];
          tablep->flags = (USHORT)((tablep->flags & ~ GRANT_ALL) | GRANT_SELECT);
        } /* for, if */

      strcpy (username, sys_user.name);         /* get real user name */
      strcpy (base->username, username);

      size  = sys_user.access.size / sizeof (SHORT);
      listp = sys_user.access.list;

      while ((size > 0) && ok)
      {
        table = *listp++;                       /* get table access flags */
        size--;

        if (table_ok (base, table))
	{
          tablep        = &base->systable [table];
          tablep->flags = (USHORT)((tablep->flags & ~ GRANT_ALL) | *listp);
          listp++;
          size--;
        } /* if */
        else
          ok = FALSE;

        while ((*listp != -1) && (size > 0))    /* get column access flags */
        {
          column = *listp++;
          size--;

          abs_col     = abscol (base, table, column);
          colp        = &base->syscolumn [abs_col];
          colp->flags = (USHORT)((colp->flags & ~ GRANT_ALL) | *listp);
          listp++;
          size--;
        } /* while */

        listp++;
        size--;
      } /* while */
    } /* if */
  } /* if */

  return (ok);
} /* set_access */

/*****************************************************************************/

LOCAL BOOL do_reorg (base, data, basename, basepath, tmpname, datacache, treecache, length, reorgfunc, reorg_result)
LPBASE         base;
LPDATAINF      data;
LPSTR          basename, basepath, tmpname;
LONG           datacache, treecache, length;
REORGFUNC      reorgfunc;
LPREORG_RESULT reorg_result;

{
  BOOL       ok, ready, user, superuser, deleted, moved;
  SHORT      table, i, status;
  SHORT      tbl, col, inx, rel, form;
  LONG       address, size, inc, maxrecs, relrecs;
  LONG       lAllRecs, lLostRecs, lDelRecs, lSizeBefore, lSizeAfter, lActRec, lMaxRecs;
  SYSTABLE   systable [SYS_FORMAT + 1];
  SYSTABLE   *tblp;
  SYSCOLUMN  *colp;
  SYSINDEX   *inxp;
  SYSREL     *relp;
  SYSUSER    sysuser;
  SYSDELETED sysdeleted;
  SYSFORMAT  *formp;
  FORMAT     *formatp;
  HPVOID     buffer;

  address                      = FIRST_REC;
  base->datainf->page0.created = data->page0.created;
  base->datainf->page0.lastuse = data->page0.lastuse;

  mem_move (base->datainf->page0.tbl_ascii, data->page0.tbl_ascii, (USHORT)sizeof (data->page0.tbl_ascii));
  mem_set (&sysuser, 0, sizeof (SYSUSER));      /* delete username and password */

  lAllRecs = lLostRecs = lDelRecs = lSizeBefore = lSizeAfter = 0;
  lActRec  = 0;
  lMaxRecs = SYS_FORMAT + 1;

  for (i = SYS_TABLE, ok = TRUE; (i <= SYS_FORMAT) && ok; i++)
  {
    size = read_data (data, address, NULL, &table, &inc, NULL, NULL, NULL);
    ok   = size != 0;
    lActRec++;

    if (ok)
    {
      systable [i].address = address;

      ok       = read_reorg (base, data, table, address, &systable [i], size);
      address += inc + data->page0.dfn_offset;
    } /* if */
  } /* for */

  if (ok)
  {
    tblp    = (SYSTABLE *)mem_alloc (systable [SYS_TABLE].recs * sizeof (SYSTABLE));
    colp    = (SYSCOLUMN *)mem_alloc (systable [SYS_COLUMN].recs * sizeof (SYSCOLUMN));
    inxp    = (SYSINDEX *)mem_alloc (systable [SYS_INDEX].recs * sizeof (SYSINDEX));
    relp    = (SYSREL *)mem_alloc (systable [SYS_REL].recs * sizeof (SYSREL));
    formp   = (SYSFORMAT *)mem_alloc (systable [SYS_FORMAT].recs * sizeof (SYSFORMAT));
    formatp = (FORMAT *)mem_alloc (systable [SYS_FORMAT].recs * sizeof (FORMAT));
    ok      = (tblp != NULL) && (colp != NULL) && (inxp != NULL) && (relp != NULL) && (formp != NULL) && (formatp != NULL);

    if (! ok)
      set_dberror (DB_NOMEMORY);
    else
    {
      address  = FIRST_REC;
      tbl      = col = inx = rel = form = 0;
      ready    = superuser = FALSE;
      user     = systable [SYS_USER].recs == 0;
      lActRec  = 0;
      lMaxRecs = systable [SYS_TABLE].recs +
                 systable [SYS_COLUMN].recs +
                 systable [SYS_INDEX].recs +
                 systable [SYS_REL].recs +
                 systable [SYS_FORMAT].recs + (user ? 0 : 1);

      while (ok && ! ready) /* read important part of data dictionary */
      {
        size = read_data (data, address, NULL, &table, &inc, NULL, &deleted, &moved);
        ok   = size != 0;

        if (! ok)
        {
          if (get_dberror () == SUCCESS)  /* only zero's read */
            ok = ready = TRUE;
        } /* if */
        else
        {
          if (! (deleted || moved))
            switch (table)
            {
              case SYS_TABLE  : lActRec++;
                                ok = read_reorg (base, data, table, address, &tblp [tbl++], size);
                                break;
              case SYS_COLUMN : lActRec++;
                                ok = read_reorg (base, data, table, address, &colp [col++], size);
                                break;
              case SYS_INDEX  : lActRec++;
                                ok = read_reorg (base, data, table, address, &inxp [inx++], size);
                                break;
              case SYS_REL    : lActRec++;
                                ok = read_reorg (base, data, table, address, &relp [rel++], size);
                                break;
              case SYS_USER   : if (! user)
                                {
                                  lActRec++;

                                  user = superuser = TRUE;
                                  ok   = read_reorg (base, data, table, address, &sysuser, size);
                                } /* if */
                                break;
              case SYS_FORMAT : lActRec++;
                                ok = read_reorg (base, data, table, address, &formp [form], size);
                                build_format (formp [form].type, formp [form].format, formatp [form]);
                                form++;
                                break;
            } /* switch, if */

          address += inc + data->page0.dfn_offset;

          if ((address >= length) || (lActRec == lMaxRecs)) ready = TRUE;
        } /* else */
      } /* while */

      if (ok)
      {
        lActRec  = 0;
        lMaxRecs = systable [SYS_TABLE ].recs - base->systable [SYS_TABLE ].recs +
                   systable [SYS_COLUMN].recs - base->systable [SYS_COLUMN].recs +
                   systable [SYS_INDEX ].recs - base->systable [SYS_INDEX ].recs +
                   systable [SYS_REL   ].recs - base->systable [SYS_REL   ].recs;
        lAllRecs = lMaxRecs;

        for (i = SYS_USER, maxrecs = 0; i < (SHORT)systable [SYS_TABLE].recs; i++)
          if ((i != SYS_DELETED) && (i != SYS_BLOB))
            maxrecs += tblp [i].recs;

        for (i = 0; ok && (i < (SHORT)systable [SYS_TABLE].recs); i++)
          if (tblp [i].table >= NUM_SYSTABLES) /* insert user tables only */
          {
            lActRec++;
            tblp [i].recs = 0;
            ok = db_insert (base, SYS_TABLE, &tblp [i], &status);
          } /* if, for */

        for (i = 0; ok && (i < (SHORT)systable [SYS_COLUMN].recs); i++)
          if (colp [i].table >= NUM_SYSTABLES) /* insert user columns only */
          {
            lActRec++;
            ok = db_insert (base, SYS_COLUMN, &colp [i], &status);
          } /* if, for */

        for (i = 0; ok && (i < (SHORT)systable [SYS_INDEX].recs); i++)
          if (inxp [i].table >= NUM_SYSTABLES) /* insert user indexes only */
          {
            lActRec++;
            inxp [i].root     = 0;
            inxp [i].num_keys = 0;
            ok = db_insert (base, SYS_INDEX, &inxp [i], &status);
          } /* if, for */

        for (i = (SHORT)base->systable [SYS_REL].recs; ok && (i < (SHORT)systable [SYS_REL].recs); i++)
        {
          lActRec++;
          ok = db_insert (base, SYS_REL, &relp [i], &status);
        } /* for */

        if (ok)
        {
          mem_free (tblp);      /* don't need it anymore */
          mem_free (colp);
          mem_free (inxp);
          mem_free (relp);
          mem_free (formp);

          tblp      = NULL;
          colp      = NULL;
          inxp      = NULL;
          relp      = NULL;
          formp     = NULL;
          address   = FIRST_REC;
          ready     = FALSE;
          table     = FAILURE;
          lLostRecs = lDelRecs = 0;
          lActRec   = 0;
          lMaxRecs  = maxrecs;

          db_close (base);

          if (reorgfunc != NULL)
            reorgfunc (REORG_MAXRECS, lMaxRecs);

          if (reorgfunc != NULL)
            reorgfunc (REORG_ACTREC, 0L);

          base  = db_open_cache (tmpname, basepath, BASE_NOLOG | BASE_SUPER, datacache, treecache, TREE_CURSORS, NULL, NULL);
          ok    = base != NULL;

          if (ok)
          {
            relrecs = base->systable [SYS_REL].recs;
            base->systable [SYS_REL].recs = 0; /* no insert rules please */

            if (superuser)                     /* insert superuser as first user */
            {
              table = SYS_USER;
              lActRec++;
              ok = db_insert (base, SYS_USER, &sysuser, &status);
              if (reorgfunc != NULL)
                ok = reorgfunc (REORG_ACTREC, lActRec);
            } /* for */

            while (ok && ! ready)
            {
              size = read_data (data, address, NULL, &table, &inc, NULL, &deleted, &moved);
              ok   = size != 0;

              if (! ok)
              {
                if (get_dberror () == SUCCESS)  /* only zero's read */
                  ready = TRUE;
                else
                  lLostRecs++;

                ok = TRUE;      /* please continue */
              } /* if */
              else
              {
                if (table == SYS_DELETED)
                  if (read_reorg (base, data, table, address, &sysdeleted, size))
                    if ((sysdeleted.table >= NUM_SYSTABLES) || (sysdeleted.table == SYS_CALC) || (sysdeleted.table == SYS_QUERY) || (sysdeleted.table == SYS_REPORT) || (sysdeleted.table == SYS_BATCH) || (sysdeleted.table == SYS_ACCOUNT) || (sysdeleted.table == SYS_PICTURE))
                      lDelRecs++;

                if (! (deleted || moved) && (table > SYS_REL) && (table != SYS_DELETED) && ((table != SYS_USER) || ! superuser) && (table != SYS_BLOB))
                {
                  lActRec++;

                  buffer = mem_alloc (base->systable [table].size);
                  ok     = buffer != NULL;

                  if (! ok)
                    set_dberror (DB_NOMEMORY);
                  else
                  {
                    ok = read_reorg (base, data, table, address, buffer, size);

                    if (! ok)
                    {
                      if (get_dberror () != DB_NOMEMORY)
                      {
                        lLostRecs++;
                        ok = TRUE;      /* please continue */
                      } /* if */
                    } /* if */
                    else
                    {
                      lAllRecs++;
                      ok = insert_blobs (base, data, table, buffer);

                      if (ok) ok = db_insert (base, table, buffer, &status);
                    } /* else */

                    mem_free (buffer);
                  } /* else */
                } /* if */

                if ((table == SYS_USER) && ! deleted)
                  superuser = FALSE;       /* superuser has already been inserted */
              } /* else */

              address += inc + data->page0.dfn_offset;
              if (address >= length) ready = TRUE;

              if (reorgfunc != NULL)
                ok = reorgfunc (REORG_ACTREC, lActRec);
            } /* while */

            base->systable [SYS_REL].recs = relrecs;
            lSizeBefore  = address;
            lSizeAfter   = base->datainf->page0.next_rec;
            lLostRecs   += lMaxRecs - lActRec;
          } /* if */
        } /* if */
      } /* if */
    } /* if */

    mem_free (tblp);
    mem_free (colp);
    mem_free (inxp);
    mem_free (relp);
    mem_free (formp);
    mem_free (formatp);
  } /* if */

  if (base != NULL) db_close (base);

  if (reorg_result != NULL)
  {
    reorg_result->all_recs    = lAllRecs;
    reorg_result->lost_recs   = lLostRecs;
    reorg_result->del_recs    = lDelRecs;
    reorg_result->size_before = lSizeBefore;
    reorg_result->size_after  = lSizeAfter;
    reorg_result->status      = get_dberror ();
  } /* if */

  return (ok);
} /* do_reorg */

/*****************************************************************************/

LOCAL BOOL read_reorg (base, data, table, address, buffer, size)
LPBASE    base;
LPDATAINF data;
SHORT     table;
LONG      address;
HPVOID    buffer;
LONG      size;

{
  BOOL   ok;
  HPVOID packbuf;

  packbuf = mem_alloc (size);
  ok      = packbuf != NULL;

  if (! ok)
    set_dberror (DB_NOMEMORY);  /* force caller to think it's a db error */
  else
  {
    size = read_data (data, address, packbuf, NULL, NULL, NULL, NULL, NULL);
    ok   = size != 0;

    if (ok) db_unpack (base, table, buffer, packbuf, size);

    mem_free (packbuf);
  } /* else */

  return (ok);
} /* read_reorg */

/*****************************************************************************/

LOCAL BOOL insert_blobs (base, data, table, buffer)
LPBASE    base;
LPDATAINF data;
SHORT     table;
HPVOID    buffer;

{
  BOOL       bOk;
  SHORT      i, sStatus;
  TABLE_INFO ti;
  FIELD_INFO fi;
  HPBLOB     hpBlob;
  SYSBLOB    *pSysBlob;
  LONG       lSize;

  db_tableinfo (base, table, &ti);

  for (i = 0, bOk = TRUE; (i < ti.cols) && bOk; i++)
  {
    db_fieldinfo (base, table, i, &fi);

    if (fi.type == TYPE_BLOB)
    {
      hpBlob = (HPBLOB)((HPCHAR)buffer + fi.addr);

      if (hpBlob->address != 0)		/* blob assigned at least once */
      {
        lSize  = read_data (data, hpBlob->address, NULL, NULL, NULL, NULL, NULL, NULL);
        bOk    = lSize != 0;

        if (bOk)
        {
	  pSysBlob = (SYSBLOB *)mem_alloc (lSize + sizeof (LONG));	/* LONG for the address */
          bOk      = pSysBlob != NULL;

          if (! bOk)
            set_dberror (DB_NOMEMORY);
          else
          {
	    lSize = read_data (data, hpBlob->address, &pSysBlob->size, NULL, NULL, NULL, NULL, NULL);
            bOk   = lSize != 0;

            if (bOk) bOk = db_insert (base, SYS_BLOB, pSysBlob, &sStatus);

            hpBlob->address = bOk ? pSysBlob->address : 0;

            mem_free (pSysBlob);
          } /* else */
        } /* if */
      } /* if */
    } /* if */
  } /* for */

  return (bOk);
} /* insert_blobs */

/*****************************************************************************/

LOCAL BOOL is_logtable (base, table)
LPBASE base;                       
SHORT  table;

{
  return ((table >= NUM_SYSTABLES) || (systable [table].flags & TBL_LOGGED));
} /* is_logtable */

/*****************************************************************************/

LOCAL BOOL read_systable (base, systblp, recs)
LPBASE   base;
SYSTABLE *systblp;
LONG     recs;

{
  BOOL    ok;
  LONG    i;
  KEY_DFN kd;
  CURSOR  cursor;

  ok = FALSE;

  keydfn (base, SYS_TABLE, 1, &kd);

  kd.root = base->datainf->page0.root_table;

  if (init_cursor (base->treeinf, ASCENDING, &kd, &cursor))
    for (i = 0, ok = TRUE; (i < recs) && ok; i++)
    {
      ok = move_cursor (&cursor, 1L);

      if (ok)
      {
        ok = do_read (base, SYS_TABLE, systblp, &cursor, 0L, FALSE, NULL);

        if (i < NUM_SYSTABLES)
	  systblp->size = systable [(SHORT)i].size;     /* use correct size of static data */

        systblp++;
      } /* if */
    } /* for, if */

  return (ok);
} /* read_systable */

/*****************************************************************************/

LOCAL BOOL read_syscolumn (base, syscolp, recs)
LPBASE    base;
SYSCOLUMN *syscolp;
LONG      recs;

{
  BOOL    ok;
  LONG    i;
  KEY_DFN kd;
  CURSOR  cursor;
  SYSPTR  sp [NUM_SYSTABLES];

  fill_sysptr (systable, sp, NUM_SYSTABLES); /* fill only NUM_SYSTABLES tables (systables) */

  ok = FALSE;

  keydfn (base, SYS_COLUMN, 1, &kd);

  kd.root = base->datainf->page0.root_column;

  if (init_cursor (base->treeinf, ASCENDING, &kd, &cursor))
    for (i = 0, ok = TRUE; (i < recs) && ok; i++)
    {
      ok = move_cursor (&cursor, 1L);

      if (ok)
      {
        ok = do_read (base, SYS_COLUMN, syscolp, &cursor, 0L, FALSE, NULL);

        if ((syscolp->table < NUM_SYSTABLES) && (base->systable [syscolp->table].cols <= systable [syscolp->table].cols))
        {
          syscolp->addr = syscolumn [sp [syscolp->table].column + syscolp->number].addr;  /* use correct size of static data */
          syscolp->size = syscolumn [sp [syscolp->table].column + syscolp->number].size;  /* use correct size of static data */
        } /* if */

        syscolp++;
      } /* if */
    } /* for, if */

  return (ok);
} /* read_syscolumn */

/*****************************************************************************/

LOCAL BOOL read_sysindex (base, sysinxp, recs)
LPBASE   base;
SYSINDEX *sysinxp;
LONG     recs;

{
  BOOL    ok;
  LONG    i;
  KEY_DFN kd;
  CURSOR  cursor;

  ok = FALSE;

  keydfn (base, SYS_INDEX, 1, &kd);

  kd.root = base->datainf->page0.root_index;

  if (init_cursor (base->treeinf, ASCENDING, &kd, &cursor))
    for (i = 0, ok = TRUE; (i < recs) && ok; i++)
    {
      ok = move_cursor (&cursor, 1L);
      if (ok) ok = do_read (base, SYS_INDEX, sysinxp++, &cursor, 0L, FALSE, NULL);
    } /* for, if */

  return (ok);
} /* read_sysindex */

/*****************************************************************************/

LOCAL BOOL read_sysrel (base, sysrelp, recs)
LPBASE base;
SYSREL *sysrelp;
LONG   recs;

{
  BOOL    ok;
  LONG    i;
  KEY_DFN kd;
  CURSOR  cursor;

  if (recs == 0) return (TRUE);

  ok = FALSE;

  keydfn (base, SYS_REL, 1, &kd);

  if (init_cursor (base->treeinf, ASCENDING, &kd, &cursor))
    for (i = 0, ok = TRUE; (i < recs) && ok; i++)
    {
      ok = move_cursor (&cursor, 1L);
      if (ok) ok = do_read (base, SYS_REL, sysrelp++, &cursor, 0L, FALSE, NULL);
    } /* for, if */

  return (ok);
} /* read_sysrel */

/*****************************************************************************/

LOCAL BOOL do_insrec (base, table, buffer, status)
LPBASE base;
SHORT  table;
HPVOID buffer;
SHORT  *status;

{
  BOOL ok;

  ok = (db_acc_table (base, table) & GRANT_INSERT) != 0;

  if (! ok)
    set_dberror (DB_CNOACCESS);
  else
  {
    ok = test_keys (base, table, buffer, 0L, status);
    if (ok) ok = test_cols (base, table, buffer, status);
    if (ok) ok = test_ins (base, table, buffer, status);

    if (ok)
    {
      ok = do_insert (base, table, buffer);

      if (ok)
      {
        base->systable [table].recs++;                  /* increase number of recs */
        ok = do_update (base, SYS_TABLE, &base->systable [table], 0L, FALSE);
      } /* if */
    } /* if */
  } /* else */

  return (ok);
} /* do_insrec */

/*****************************************************************************/

LOCAL BOOL do_delrec (base, table, address, status)
LPBASE base;
SHORT  table;
LONG   address;
SHORT  *status;

{
  BOOL       ok;
  SYSDELETED sysdel;

  ok = (db_acc_table (base, table) & GRANT_DELETE) != 0;

  if (! ok)
    set_dberror (DB_CNOACCESS);
  else
  {
    if (table == SYS_DELETED)   /* record will be lost forever */
    {
      ok = do_read (base, table, &sysdel, NULL, address, FALSE, NULL);
      if (ok) ok = inc_released (base->datainf, sysdel.del_addr);
    } /* if */

    if (ok) ok = test_del (base, table, address, status);
    if (ok) ok = do_delete (base, table, address);

    if (ok)
    {
      base->systable [table].recs--;                    /* decrease number of recs */
      ok = do_update (base, SYS_TABLE, &base->systable [table], 0L, FALSE);

      sysdel.table    = table;
      sysdel.del_addr = address;

      if (ok)
        if ((table >= NUM_SYSTABLES) || (table == SYS_CALC) || (table == SYS_QUERY) || (table == SYS_REPORT) || (table == SYS_BATCH) || (table == SYS_ACCOUNT) || (table == SYS_PICTURE))
        {
          ok = do_insert (base, SYS_DELETED, &sysdel);

          if (ok)
          {
            base->systable [SYS_DELETED].recs++;          /* increase number of recs */
            ok = do_update (base, SYS_TABLE, &base->systable [SYS_DELETED], 0L, FALSE);
          } /* if */
        } /* if */
        else
          ok = inc_released (base->datainf, address);
    } /* if */
  } /* else */

  return (ok);
} /* do_delrec */

/*****************************************************************************/

LOCAL BOOL do_updtrec (base, table, buffer, version, status, testins)
LPBASE base;
SHORT  table;
HPVOID buffer;
LONG   version;
SHORT  *status;
BOOL   testins;

{
  BOOL ok;

  ok = (db_acc_table (base, table) & GRANT_UPDATE) != 0;

  if (! ok)
    set_dberror (DB_CNOACCESS);
  else
  {
    ok = test_keys (base, table, buffer, *(LONG *)buffer, status);
    if (ok) ok = test_cols (base, table, buffer, status);
    if (ok) ok = test_updt (base, table, buffer, status);
    if (ok && testins) ok = test_ins (base, table, buffer, status); /* in case changing dependant value */
    if (ok) ok = do_update (base, table, buffer, version, TRUE);
  } /* else */

  return (ok);
} /* do_updtrec */

/*****************************************************************************/

LOCAL BOOL do_insert (base, table, buffer)
LPBASE base;
SHORT  table;
HPVOID buffer;

{
  BOOL      ok, encode;
  LONG      addr, size;
  LONG      *longp;
  HPVOID    packbuf;
  SHORT     col, abs_col;
  SYSTABLE  *tablep;
  SYSCOLUMN *colp;
  SYSINDEX  *inxp;
  HPCHAR    bufp;

  if (table == SYS_INDEX)
  {
    inxp         = (SYSINDEX *)buffer;
    inxp->flags &= ~ INX_DIRTY;         /* index is about to be written */
  } /* if */

  packbuf = mem_alloc (do_pack (base, table, buffer, NULL));
  ok      = packbuf != NULL;

  if (! ok)
    set_dberror (DB_NOMEMORY);
  else
  {
    tablep  = &base->systable [table];
    abs_col = abscol (base, table, 1);
    colp    = &base->syscolumn [abs_col];
    bufp    = (HPCHAR)buffer;

    for (col = 1; col < tablep->cols; col++, colp++)
      if (! (colp->flags & GRANT_INSERT)) set_null (colp->type, bufp + colp->addr);

    size   = do_pack (base, table, buffer, packbuf);
    encode = table == SYS_USER;
    addr   = insert_data (base->datainf, table, size, packbuf, encode);
    ok     = addr != 0;
    longp  = (LONG *)buffer;
    *longp = addr;

    mem_free (packbuf);

    if (ok) ok = ins_keys (base, table, buffer, addr);
  } /* else */

  return (ok);
} /* do_insert */

/*****************************************************************************/

LOCAL BOOL do_delete (base, table, address)
LPBASE base;
SHORT  table;
LONG   address;

{
  BOOL     ok;
  SYSTABLE *tablep;
  HPVOID   oldbuf;

  tablep = &base->systable [table];
  oldbuf = mem_alloc (tablep->size);
  ok     = oldbuf != NULL;

  if (! ok)
    set_dberror (DB_NOMEMORY);
  else
  {
    ok = do_read (base, table, oldbuf, NULL, address, FALSE, NULL);

    if (ok) ok = del_keys (base, table, oldbuf, address);
    if (ok) ok = delete_data (base->datainf, address);

    mem_free (oldbuf);
  } /* else */

  return (ok);
} /* do_delete */

/*****************************************************************************/

LOCAL BOOL do_update (base, table, buffer, version, key_updt)
LPBASE base;
SHORT  table;
HPVOID buffer;
LONG   version;
BOOL   key_updt;

{
  BOOL      ok, encode, deleted;
  LONG      addr, size, diskversion;
  HPVOID    packbuf, oldbuf;
  LONG      msize;
  SHORT     col, abs_col;
  SYSTABLE  *tablep;
  SYSCOLUMN *colp;
  SYSINDEX  *inxp;
  HPCHAR    oldbufp;
  HPCHAR    bufp;
  USHORT    flags;

  ok     = TRUE;
  flags  = GRANT_NOTHING;
  oldbuf = NULL;

  switch (table)                                        /* save old grant flags */
  {
    case SYS_TABLE  : tablep         = (SYSTABLE *)buffer;
                      flags          = tablep->flags;
                      tablep->flags |= GRANT_ALL;
                      break;
    case SYS_COLUMN : colp         = (SYSCOLUMN *)buffer;
                      flags        = colp->flags;
                      colp->flags |= GRANT_ALL;
                      break;
    case SYS_INDEX  : inxp         = (SYSINDEX *)buffer;
                      inxp->flags &= ~ INX_DIRTY;       /* index is about to be written */
                      break;
  } /* switch */

  addr   = *(LONG *)buffer;
  tablep = &base->systable [table];

  if (version != 0L)                                    /* optimistic locking */
  {
#if GEMDOS
    if (base->datainf->flags & DATA_MULUSER)            /* GEMDOS has no locking mechanism without netware support */
#else
    if (base->datainf->flags & (DATA_MULUSER | DATA_MULTASK))
#endif
    {
      ok = ! file_locked (base->datainf->handle, addr, 1L);

      if (! ok)
        set_dberror (DB_CRECLOCKED);
    } /* if */
  } /* if */

  if (ok)
  {
    size = read_data (base->datainf, addr, NULL, NULL, NULL, &diskversion, &deleted, NULL);
    ok   = ! deleted && ((version == 0L) || (diskversion == version));

    if (! ok)
      if (deleted)
        set_dberror (DB_CDELETEDUPDATE);
      else
        set_dberror (DB_CUPDATED);
  } /* if */

  if (ok)
  {
    if (table == SYS_BLOB)                              /* allocate memory and read blob in allocated */
      oldbuf = do_readblob (base, NULL, NULL, addr, FALSE);
    else
      oldbuf = mem_alloc (tablep->size);

    ok = oldbuf != NULL;

    if (! ok)
      if (table != SYS_BLOB)
        set_dberror (DB_NOMEMORY);                      /* do_readblob already set an error status */
  } /* if */

  if (ok)
  {
    packbuf = mem_alloc (do_pack (base, table, buffer, NULL));
    ok      = packbuf != NULL;

    if (! ok)
    {
      set_dberror (DB_NOMEMORY);
      mem_free (oldbuf);
    } /* if */
    else
    {
      if (table != SYS_BLOB) ok = do_read (base, table, oldbuf, NULL, addr, FALSE, NULL);

      if (ok)
      {
        abs_col = abscol (base, table, 1);
        colp    = &base->syscolumn [abs_col];
	oldbufp = (HPCHAR)oldbuf;
	bufp    = (HPCHAR)buffer;

        for (col = 1; col < tablep->cols; col++, colp++)
          if (! (colp->flags & GRANT_UPDATE))           /* use old values */
          {
	    msize = typesize (colp->type, oldbufp + colp->addr);
	    mem_lmove (bufp + colp->addr, oldbufp + colp->addr, msize);
          } /* if, for */

        size   = do_pack (base, table, buffer, packbuf);
        encode = table == SYS_USER;
        ok     = update_data (base->datainf, table, size, packbuf, encode, addr);
      } /* if */

      mem_free (packbuf);

      if (ok && key_updt) ok = updt_keys (base, table, buffer, oldbuf, addr);

      mem_free (oldbuf);
    } /* else */
  } /* else */

  switch (table)                                        /* restore old grant flags */
  {
    case SYS_TABLE  : tablep        = (SYSTABLE *)buffer;
                      tablep->flags = flags;
                      break;
    case SYS_COLUMN : colp        = (SYSCOLUMN *)buffer;
                      colp->flags = flags;
                      break;
  } /* switch */

  return (ok);
} /* do_update */

/*****************************************************************************/

LOCAL BOOL do_read (base, table, buffer, cursor, address, modify, version)
LPBASE   base;
SHORT    table;
HPVOID   buffer;
LPCURSOR cursor;
LONG     address;
BOOL     modify;
LPLONG   version;

{
  BOOL      ok, deleted;
  LONG      size;
  LONG      *longp;
  HPVOID    packbuf;
  SHORT     col, abs_col;
  SYSTABLE  *tablep;
  SYSCOLUMN *colp;
  HPCHAR    bufp;

  if ((address == 0) && (cursor != NULL)) address = read_cursor (cursor, NULL);
  ok   = address != 0;
  size = 0;

  if (! ok)
    set_dberror (DB_CINVALID);
  else
  {
    if (modify) ok = do_reclock (base, address);

    if (ok)
    {
      size = read_data (base->datainf, address, NULL, NULL, NULL, version, NULL, NULL);
      ok   = size != 0;
    } /* if */

    if (ok && (buffer != NULL))
    {
      packbuf = mem_alloc (size);
      ok      = packbuf != NULL;

      if (! ok)
        set_dberror (DB_NOMEMORY);
      else
      {
	size = read_data (base->datainf, address, packbuf, NULL, NULL, version, &deleted, NULL);
        ok   = size != 0;

        if (ok)
        {
          do_unpack (base, table, buffer, packbuf, size);
          longp  = (LONG *)buffer;
          *longp = address;

          tablep  = &base->systable [table];
          abs_col = abscol (base, table, 1);
          colp    = &base->syscolumn [abs_col];
	  bufp    = (HPCHAR)buffer;

          for (col = 1; col < tablep->cols; col++, colp++)
	    if (! (colp->flags & GRANT_SELECT)) set_null (colp->type, bufp + colp->addr);

          if (deleted) set_dberror (DB_CDELETED);
        } /* if */

        mem_free (packbuf);
      } /* else */
    } /* if */
  } /* else */

  return (ok);
} /* do_read */

/*****************************************************************************/

LOCAL LPSYSBLOB do_readblob (base, buffer, cursor, address, modify)
LPBASE    base;
LPSYSBLOB buffer;
LPCURSOR  cursor;
LONG      address;
BOOL      modify;

{
  HPCHAR    blob;
  BOOL      ok, deleted;
  LONG      size;
  LONG      *longp;
  SHORT     col, abs_col;
  SYSTABLE  *tablep;
  SYSCOLUMN *colp;

  blob = NULL;

  if ((address == 0) && (cursor != NULL)) address = read_cursor (cursor, NULL);
  ok   = address != 0;
  size = 0;

  if (! ok)
    set_dberror (DB_CINVALID);
  else
  {
    if (modify) ok = do_reclock (base, address);

    if (ok)
    {
      size = read_data (base->datainf, address, NULL, NULL, NULL, NULL, NULL, NULL);
      ok   = size != 0;
    } /* if */

    if (ok)
    {
      blob = mem_alloc (size + sizeof (LONG));          /* LONG for the address */
      ok   = blob != NULL;

      if (! ok)
        set_dberror (DB_NOMEMORY);
      else
      {
	size = read_data (base->datainf, address, blob + sizeof (LONG), NULL, NULL, NULL, &deleted, NULL);
        ok   = size != 0;

        if (ok)
        {
          longp  = (LONG *)blob;
          *longp = address;

          tablep  = &base->systable [SYS_BLOB];
          abs_col = abscol (base, SYS_BLOB, 1);
          colp    = &base->syscolumn [abs_col];

          for (col = 1; col < tablep->cols; col++, colp++)
	    if (! (colp->flags & GRANT_SELECT)) set_null (colp->type, blob + colp->addr);

          if (deleted) set_dberror (DB_CDELETED);
          if (buffer != NULL) mem_free (buffer);        /* free old dynamic buffer */
        } /* if */
        else
        {
          mem_free (blob);
          blob = NULL;
        } /* else */
      } /* else */
    } /* if */
  } /* else */

  return ((LPSYSBLOB)blob);
} /* do_readblob */

/*****************************************************************************/

LOCAL BOOL do_reclock (base, address)
LPBASE base;
LONG   address;

{
  BOOL ok;

  ok = TRUE;

  if (base->datainf->flags & DATA_RDONLY) return (ok);

#if GEMDOS
  if (base->datainf->flags & DATA_MULUSER) /* GEMDOS has no locking mechanism without netware support */
#else
  if (base->datainf->flags & (DATA_MULUSER | DATA_MULTASK))
#endif
  {
    ok = file_lock (base->datainf->handle, address, 1L, FALSE);
    if (! ok) set_dberror (DB_CLOCK_ERR);
  } /* if */

  return (ok);
} /* do_reclock */

/*****************************************************************************/

LOCAL BOOL do_recunlock (base, address)
LPBASE base;
LONG   address;

{
  BOOL ok;

  ok = TRUE;

  if (base->datainf->flags & DATA_RDONLY) return (ok);

#if GEMDOS
  if (base->datainf->flags & DATA_MULUSER) /* GEMDOS has no locking mechanism without netware support */
#else
  if (base->datainf->flags & (DATA_MULUSER | DATA_MULTASK))
#endif
  {
    ok = file_unlock (base->datainf->handle, address, 1L);
    if (! ok) set_dberror (DB_CFREE_ERR);
  } /* if */

  return (ok);
} /* do_recunlock */

/*****************************************************************************/

LOCAL BOOL ins_keys (base, table, buffer, addr)
LPBASE base;
SHORT  table;
HPVOID buffer;
LONG   addr;

{
  BOOL     ok;
  SHORT    i;
  SYSTABLE *tablep;
  SYSINDEX *inxp;
  SYSPTR   *sysp;
  KEY      keyval;
  KEY_DFN  kd;

  ok     = TRUE;
  tablep = &base->systable [table];
  sysp   = &base->sysptr [table];
  inxp   = &base->sysindex [sysp->index];

  for (i = 0; ok && (i < tablep->indexes); i++, inxp++)
    if (do_buildkey (base, table, i, buffer, keyval))   /* key is not null */
    {
      kd.type     = inxp->type;
      kd.root     = inxp->root;
      kd.num_keys = inxp->num_keys;

      ok = insert_key (base->treeinf, keyval, addr, &kd);

      inxp->root     = kd.root;
      inxp->num_keys = kd.num_keys;

      update_root (base, table, i);

      if (ok && (base->treeinf->flags & (TREE_FLUSH | TREE_MULUSER | TREE_MULTASK)))
        ok = do_update (base, SYS_INDEX, inxp, 0L, FALSE);
      else
        inxp->flags |= INX_DIRTY;
    } /* if, for */

  return (ok);
} /* ins_keys */

/*****************************************************************************/

LOCAL BOOL del_keys (base, table, buffer, addr)
LPBASE base;
SHORT  table;
HPVOID buffer;
LONG   addr;

{
  BOOL     ok;
  SHORT    i;
  SYSTABLE *tablep;
  SYSINDEX *inxp;
  SYSPTR   *sysp;
  KEY      keyval;
  KEY_DFN  kd;

  ok     = TRUE;
  tablep = &base->systable [table];
  sysp   = &base->sysptr [table];
  inxp   = &base->sysindex [sysp->index];

  for (i = 0; ok && (i < tablep->indexes); i++, inxp++)
    if (do_buildkey (base, table, i, buffer, keyval))   /* key is not null */
    {
      kd.type     = inxp->type;
      kd.root     = inxp->root;
      kd.num_keys = inxp->num_keys;

      ok = delete_key (base->treeinf, keyval, addr, &kd);

      inxp->root     = kd.root;
      inxp->num_keys = kd.num_keys;

      update_root (base, table, i);

      if (ok && (base->treeinf->flags & (TREE_FLUSH | TREE_MULUSER | TREE_MULTASK)))
        ok = do_update (base, SYS_INDEX, inxp, 0L, FALSE);
      else
        inxp->flags |= INX_DIRTY;
    } /* if */

  return (ok);
} /* del_keys */

/*****************************************************************************/

LOCAL BOOL updt_keys (base, table, buffer, oldbuf, addr)
LPBASE base;
SHORT  table;
HPVOID buffer, oldbuf;
LONG   addr;

{
  BOOL     ok, old_ok, new_ok;
  SHORT    i;
  SYSTABLE *tablep;
  SYSINDEX *inxp;
  SYSPTR   *sysp;
  KEY      oldval, newval;
  KEY_DFN  kd;

  ok     = TRUE;
  tablep = &base->systable [table];
  sysp   = &base->sysptr [table];
  inxp   = &base->sysindex [sysp->index];

  for (i = 0; ok && (i < tablep->indexes); i++, inxp++)
  {
    old_ok = do_buildkey (base, table, i, oldbuf, oldval);
    new_ok = do_buildkey (base, table, i, buffer, newval);

    if (cmp_vals (inxp->type, oldval, newval) != 0)
    {
      kd.type     = inxp->type;
      kd.root     = inxp->root;
      kd.num_keys = inxp->num_keys;

      if (old_ok) ok = delete_key (base->treeinf, oldval, addr, &kd);
      if (ok && new_ok) ok = insert_key (base->treeinf, newval, addr, &kd);

      inxp->root     = kd.root;
      inxp->num_keys = kd.num_keys;

      update_root (base, table, i);

      if (ok && (base->treeinf->flags & (TREE_FLUSH | TREE_MULUSER | TREE_MULTASK)))
        ok = do_update (base, SYS_INDEX, inxp, 0L, FALSE);
      else
        inxp->flags |= INX_DIRTY;
    } /* if */
  } /* for */

  return (ok);
} /* updt_keys */

/*****************************************************************************/

LOCAL BOOL test_keys (base, table, buffer, addr, status)
LPBASE base;
SHORT  table;
HPVOID buffer;
LONG   addr;
SHORT  *status;

{
  BOOL     ok, found;
  SHORT    i;
  SYSTABLE *tablep;
  SYSINDEX *inxp;
  SYSPTR   *sysp;
  KEY      keyval;
  CURSOR   cursor;
  KEY_DFN  kd;

  ok      = TRUE;
  tablep  = &base->systable [table];
  sysp    = &base->sysptr [table];
  inxp    = &base->sysindex [sysp->index + 1];          /* start with key #1 */
  *status = 0;

  for (i = 1; ok && (i < tablep->indexes); i++, inxp++) /* start with key #1 */
    if (inxp->flags & (INX_UNIQUE | INX_PRIMARY))
      if (do_buildkey (base, table, i, buffer, keyval)) /* key is not null */
      {
        keydfn (base, table, i, &kd);
        found = search_key (base->treeinf, ASCENDING, &cursor, keyval, 0L, &kd, FALSE);
	ok    = get_dberror () == SUCCESS;

        if (ok && found)                /* key has been found */
        {
          ok = (addr == 0) ? FALSE : read_cursor (&cursor, NULL) == addr;

          if (! ok)
          {
            *status = i;
            set_dberror (DB_CNOTUNIQUE);
          } /* if */
        } /* if */
      } /* if, if, for */

  return (ok);
} /* test_keys */

/*****************************************************************************/

LOCAL BOOL test_cols (base, table, buffer, status)
LPBASE base;
SHORT  table;
HPVOID buffer;
SHORT  *status;

{
  BOOL      ok;
  SHORT     i;
  SYSTABLE  *tablep;
  SYSCOLUMN *colp;
  SYSPTR    *sysp;

  ok      = TRUE;
  tablep  = &base->systable [table];
  sysp    = &base->sysptr [table];
  colp    = &base->syscolumn [sysp->column + 1];        /* start with col #1 */
  *status = 0;

  for (i = 1; ok && (i < tablep->cols); i++, colp++)    /* start with col #1 */
    if (is_null (colp->type, (HPCHAR)buffer + colp->addr))
    {
      if (colp->flags & COL_DEFAULT)
	set_default (colp->type, (HPCHAR)buffer + colp->addr);
      else
        if (colp->flags & COL_MANDATORY)
        {
          ok      = FALSE;
          *status = i;
          set_dberror (DB_CNULLCOL);
        } /* if */
    } /* if, for */

  return (ok);
} /* test_cols */

/*****************************************************************************/

LOCAL BOOL test_ins (base, table, buffer, status)
LPBASE base;
SHORT  table;
HPVOID buffer;
SHORT  *status;

{
  BOOL     ok, found;
  SHORT    i, recs;
  SYSTABLE *tablep;
  SYSREL   *sysrelp;
  KEY      keyval;
  CURSOR   cursor;
  KEY_DFN  kd;

  ok      = TRUE;
  tablep  = &base->systable [table];
  sysrelp = base->sysrel;
  recs    = (SHORT)base->systable [SYS_REL].recs;

  if (tablep->parents > 0)
    for (i = 0; ok && (i < recs); i++, sysrelp++)
      if ((sysrelp->reltable == table) && (sysrelp->insrule == REL_RESTRICTED))
        if (do_buildkey (base, table, sysrelp->relindex, buffer, keyval)) /* key is not null */
        {
          keydfn (base, sysrelp->reftable, sysrelp->refindex, &kd);
          found = search_key (base->treeinf, ASCENDING, &cursor, keyval, 0L, &kd, FALSE);
          ok    = get_dberror () == SUCCESS;

          if (! ok || ! found)
          {
            ok      = FALSE;
            *status = sysrelp->relindex;
            set_dberror (DB_CNOINSERT);
	  } /* if */
        } /* if, if, for, if */

  return (ok);
} /* test_ins */

/*****************************************************************************/

LOCAL BOOL test_del (base, table, address, status)
LPBASE base;
SHORT  table;
LONG   address;
SHORT  *status;

{
  BOOL     ok, found;
  SHORT    i, recs;
  SYSTABLE *tablep, *reltablep;
  SYSREL   *sysrelp;
  KEY      oldval;
  CURSOR   cursor;
  KEY_DFN  kd;
  HPVOID   oldbuf, relbuf;
  LONG     addr;

  ok      = TRUE;
  tablep  = &base->systable [table];
  sysrelp = base->sysrel;
  recs    = (SHORT)base->systable [SYS_REL].recs;

  if ((tablep->children > 0) && ! (tablep->flags & VISITED))
  {
    tablep->flags |= VISITED;
    oldbuf         = mem_alloc (tablep->size);
    ok             = oldbuf != NULL;

    if (! ok)
      set_dberror (DB_NOMEMORY);
    else
    {
      ok = do_read (base, table, oldbuf, NULL, address, FALSE, NULL); /* need old buffer */

      for (i = 0; ok && (i < recs); i++, sysrelp++)
        if ((sysrelp->reftable == table) && ! (base->systable [sysrelp->reltable].flags & VISITED))
        {
          do_buildkey (base, table, sysrelp->refindex, oldbuf, oldval);

          reltablep = &base->systable [sysrelp->reltable];
          keydfn (base, sysrelp->reltable, sysrelp->relindex, &kd);

          switch (sysrelp->delrule)
          {
	    case REL_RESTRICTED : found = search_key (base->treeinf, ASCENDING, &cursor, oldval, 0L, &kd, FALSE);
                                  ok    = get_dberror () == SUCCESS;

                                  if (ok && found)
                                  {
                                    ok      = FALSE;
                                    *status = sysrelp->refindex;
                                    set_dberror (DB_CNODELETE);
                                  } /* if */
                                  break;
            case REL_CASCADED   : while (ok && search_key (base->treeinf, ASCENDING, &cursor, oldval, 0L, &kd, FALSE))
                                  {
                                    addr = read_cursor (&cursor, NULL);
				    ok   = addr != 0;
                                    if (ok) ok = do_reclock (base, addr);

                                    if (ok)
                                    {
                                      ok = do_delrec (base, sysrelp->reltable, addr, status);
                                      if (*status != 0) *status = sysrelp->refindex; /* refer to original index */
                                      do_recunlock (base, addr);
                                    } /* if */
				  } /* while */
                                  break;
	    case REL_SETNULL    : relbuf = mem_alloc (reltablep->size);
                                  ok     = relbuf != NULL;

                                  if (! ok)
                                    set_dberror (DB_NOMEMORY);
                                  else
                                  {
                                    while (ok && search_key (base->treeinf, ASCENDING, &cursor, oldval, 0L, &kd, FALSE))
                                    {
                                      ok = do_read (base, sysrelp->reltable, relbuf, &cursor, 0L, TRUE, NULL);

                                      if (ok)
                                      {
					set_keyvalue (base, sysrelp->reltable, sysrelp->relindex, relbuf, sysrelp->reftable, sysrelp->refindex, NULL);
                                        ok = do_updtrec (base, sysrelp->reltable, relbuf, 0L, status, FALSE);
					if (*status != 0) *status = sysrelp->refindex; /* refer to original index */
                                      } /* if */

                                      do_recunlock (base, *(LONG *)relbuf);
                                    } /* while */

                                    mem_free (relbuf);
                                  } /* else */
                                  break;
          } /* switch */
        } /* if, for */

      mem_free (oldbuf);
    } /* else */

    tablep->flags &= ~VISITED;
  } /* if */

  return (ok);
} /* test_del */

/*****************************************************************************/

LOCAL BOOL test_updt (base, table, buffer, status)
LPBASE base;
SHORT  table;
HPVOID buffer;
SHORT  *status;

{
  BOOL     ok, found;
  SHORT    i, recs;
  SYSTABLE *tablep, *reltablep;
  SYSINDEX *inxp;
  SYSPTR   *sysp;
  SYSREL   *sysrelp;
  KEY      oldval, newval;
  CURSOR   cursor;
  KEY_DFN  kd;
  HPVOID   oldbuf, relbuf;
  LONG     addr;

  ok      = TRUE;
  tablep  = &base->systable [table];
  sysp    = &base->sysptr [table];
  inxp    = &base->sysindex [sysp->index];
  sysrelp = base->sysrel;
  recs    = (SHORT)base->systable [SYS_REL].recs;

  if ((tablep->children > 0) && ! (tablep->flags & VISITED))
  {
    tablep->flags |= VISITED;
    oldbuf         = mem_alloc (tablep->size);
    ok             = oldbuf != NULL;

    if (! ok)
      set_dberror (DB_NOMEMORY);
    else
    {
      addr = *(LONG *)buffer;
      ok   = do_read (base, table, oldbuf, NULL, addr, FALSE, NULL); /* need old buffer */

      for (i = 0; ok && (i < recs); i++, sysrelp++)
        if ((sysrelp->reftable == table) && ! (base->systable [sysrelp->reltable].flags & VISITED))
        {
	  do_buildkey (base, table, sysrelp->refindex, oldbuf, oldval);
          do_buildkey (base, table, sysrelp->refindex, buffer, newval);

          if (cmp_vals (inxp [sysrelp->refindex].type, oldval, newval) != 0)
          {
            reltablep = &base->systable [sysrelp->reltable];
            keydfn (base, sysrelp->reltable, sysrelp->relindex, &kd);

            switch (sysrelp->updrule)
            {
              case REL_RESTRICTED : found = search_key (base->treeinf, ASCENDING, &cursor, oldval, 0L, &kd, FALSE);
                                    ok    = get_dberror () == SUCCESS;

				    if (ok && found)
                                    {
                                      ok      = FALSE;
                                      *status = sysrelp->refindex;
                                      set_dberror (DB_CNOUPDATE);
                                    } /* if */
                                    break;
              case REL_CASCADED   : /* fall through */
	      case REL_SETNULL    : relbuf = mem_alloc (reltablep->size);
                                    ok     = relbuf != NULL;

                                    if (! ok)
                                      set_dberror (DB_NOMEMORY);
				    else
                                    {
                                      while (ok && search_key (base->treeinf, ASCENDING, &cursor, oldval, 0L, &kd, FALSE))
                                      {
                                        ok = do_read (base, sysrelp->reltable, relbuf, &cursor, 0L, TRUE, NULL);

                                        if (ok)
                                        {
                                          set_keyvalue (base, sysrelp->reltable, sysrelp->relindex, relbuf, sysrelp->reftable, sysrelp->refindex, (sysrelp->updrule == REL_CASCADED) ? buffer : NULL);
                                          ok = do_updtrec (base, sysrelp->reltable, relbuf, 0L, status, FALSE);
                                          if (*status != 0) *status = sysrelp->refindex; /* refer to original index */
                                        } /* if */

                                        do_recunlock (base, *(LONG *)relbuf);
                                      } /* while */

                                      mem_free (relbuf);
                                    } /* else */
                                    break;
            } /* switch */
          } /* if */
        } /* if, for */

      mem_free (oldbuf);
    } /* else */

    tablep->flags &= ~VISITED;
  } /* if */

  return (ok);
} /* test_updt */

/*****************************************************************************/

LOCAL LONG do_pack (base, table, buffer, packbuf)
LPBASE base;
SHORT  table;
HPVOID buffer;
HPVOID packbuf;

{
  LONG      size, msize;
  SHORT     i;
  SYSTABLE  *tablep;
  SYSCOLUMN *colp;
  SYSBLOB   *blobp;
  HPCHAR    srcp, dstp;

  size   = 0;
  tablep = &base->systable [table];
  colp   = &base->syscolumn [abscol (base, table, 1)];  /* don't use col zero */
  dstp   = (HPCHAR)packbuf;

  if (table == SYS_BLOB)                                /* unpacked variable size */
  {
    blobp = (SYSBLOB *)buffer;
    size  = LCHROFF (SYSBLOB, blob) - sizeof (LONG) + blobp->size;
    if (dstp != NULL) mem_lmove (dstp, (HPCHAR)buffer + colp->addr, size);
  } /* if */
  else
    for (i = 1; i < tablep->cols; i++, colp++)
    {
      srcp  = (HPCHAR)buffer + colp->addr;
      msize = typesize (colp->type, srcp);

      if (dstp != NULL)
      {
        mem_lmove (dstp, srcp, msize);

#if MSDOS && ! defined (_WINDOWS) && ! defined (USE_DOS_CHARSET)
        if (HASWILD (colp->type))
          oem_to_ansi (dstp, dstp);
#endif
      } /* if */

      size += msize;
      if (dstp != NULL) dstp += msize;
    } /* for, else */

  return (size);
} /* do_pack */

/*****************************************************************************/

LOCAL LONG do_unpack (base, table, buffer, packbuf, packsize)
LPBASE base;
SHORT  table;
HPVOID buffer;
HPVOID packbuf;
LONG   packsize;

{
  LONG      msize, size;
  SHORT     i;
  SYSTABLE  *tablep;
  SYSCOLUMN *colp;
  HPCHAR    srcp, dstp;

  tablep = &base->systable [table];
  colp   = &base->syscolumn [abscol (base, table, 1)];  /* don't use col zero */
  srcp   = (HPCHAR)packbuf;

  if (table == SYS_BLOB)                                /* unpacked variable size */
    mem_lmove ((HPCHAR)buffer + colp->addr, srcp, packsize);
  else
    for (i = 1; i < tablep->cols; i++, colp++)
    {
      dstp  = (HPCHAR)buffer + colp->addr;
      msize = (packsize <= 0) ? 0L : typesize (colp->type, srcp);
      size  = msize;

      if (VARLEN (colp->type))                 /* any string type */
        if (size > colp->size)                 /* string has been made shorter */
        {
          size        = colp->size - 1;        /* don't move too many bytes */
          dstp [size] = EOS;                   /* terminate string */
        } /* if, if */

      if (packsize > 0)                        /* valid buffer */
        mem_lmove (dstp, srcp, size);
      else                                     /* more fields than there are in buffer */
        set_null (colp->type, dstp);

#if MSDOS && ! defined (_WINDOWS) && ! defined (USE_DOS_CHARSET)
        if (HASWILD (colp->type))
          ansi_to_oem (dstp, dstp);
#endif

      srcp     += msize;
      packsize -= msize;
    } /* for, else */

  return (tablep->size);
} /* do_unpack */

/*****************************************************************************/

LOCAL BOOL do_buildkey (base, table, inx, buffer, keyval)
LPBASE base;
SHORT  table;
SHORT  inx;
HPVOID buffer;
HPVOID keyval;

{
  BOOL      ok, isnull;
  SHORT     col0, col1, word0, word1;
  SHORT     type, num_cols, i, size;
  LONG      addr;
  SYSCOLUMN *colp;
  SYSINDEX  *inxp;
  SYSPTR    *sysp;
  INXCOL    *inxcolp;
  HPCHAR    bufp, valp;
  KEY       s, t;

  ok       = TRUE;
  sysp     = &base->sysptr [table];
  colp     = &base->syscolumn [sysp->column];
  inxp     = &base->sysindex [sysp->index + inx];
  bufp     = (HPCHAR)buffer;
  valp     = (HPCHAR)keyval;
  num_cols = (SHORT)(inxp->inxcols.size / sizeof (INXCOL));
  inxcolp  = inxp->inxcols.cols;
  col0     = inxcolp->col;

  if (num_cols == 1)                            /* only one column for key */
  {
    bufp += colp [col0].addr;
    size  = keysize (inxp->type, bufp);
#ifdef NONULLKEYS
    ok    = ! is_null (inxp->type, bufp);
#endif

    if (ok)
    {
      if (HASWILD (inxp->type))                 /* a character string */
      {
	size        = (SHORT)min (inxcolp->len, (SHORT)strlen ((CHAR *)bufp));
        valp [size] = EOS;
      } /* if */

      mem_move (valp, bufp, size);

      if (HASWILD (inxp->type)) do_convstr (base, valp); /* a character string */
    } /* if */
  } /* if */
  else
  {
    col1 = inxcolp [1].col;

    if ((num_cols == 2) && (colp [col0].type == TYPE_WORD) && (colp [col1].type == TYPE_WORD))
    {
      word0 = *(SHORT *)(bufp + colp [col0].addr);       /* result is one long word */
      word1 = *(SHORT *)(bufp + colp [col1].addr);
#ifdef NONULLKEYS
      ok    = ! (is_null (TYPE_WORD, &word0) || is_null (TYPE_WORD, &word1));
#endif
      if (ok) *(LONG *)valp = ((ULONG)(USHORT)word0 << 16) + (USHORT)word1;
    } /* if */
    else
    {
      valp [0] = EOS;
      isnull   = FALSE;

      for (i = 0; ok && (i < num_cols); i++, inxcolp++)
      {
	col0   = inxcolp->col;
        addr   = colp [col0].addr;
        type   = colp [col0].type;
        s [0]  = EOS;
#ifdef NONULLKEYS
        isnull = isnull || is_null (type, bufp + addr);
#endif

        if (HASWILD (type))                     /* upper case etc. before building key */
        {
	  strncpy (t, (CHAR *)(bufp + addr), inxcolp->len); /* don't overwrite t */
          t [inxcolp->len] = EOS;
          do_convstr (base, t);
	  build_str (type, inxcolp->len, t, s);
        } /* if */
        else
	  build_str (type, inxcolp->len, bufp + addr, s);

	ok = strlen ((CHAR *)valp) + strlen (s) < MAX_KEYSIZE;
	if (ok) strcat ((CHAR *)valp, s);
      } /* for */

      if (ok) ok = ! isnull;
    } /* else */
  } /* else */

  return (ok);
} /* do_buildkey */

/*****************************************************************************/

LOCAL VOID do_convstr (base, s)
LPBASE base;
HPCHAR s;

{
#ifdef _WINDOWS
  AnsiUpper (s);
#elif MSDOS && ! defined (USE_DOS_CHARSET)
  oem_to_ansi (s, s);
  ansi_upper (s);
#else
  REG HPUCHAR str;
  REG UCHAR   c;

  str = (HPUCHAR)s;

  while (*str)
  {
    c = base->datainf->page0.tbl_ascii [*str];
    if (c == 0) c = (UCHAR)ch_ascii (*str); /* in case there's no table in data page zero */
    *str++ = (UCHAR)toupper (c);
  } /* while */
#endif
} /* do_convstr */

/*****************************************************************************/

LOCAL VOID set_keyvalue (base, table, inx, buffer, srctable, srcinx, srcbuf)
LPBASE base;
SHORT  table, inx;
HPVOID buffer;
SHORT  srctable, srcinx;
HPVOID srcbuf;

{
  SHORT     cols, srccols, col, srccol, len, srclen, type, srctype, i;
  LONG      addr, srcaddr, size, srcsize;
  SYSCOLUMN *colp, *srccolp;
  SYSINDEX  *inxp, *srcinxp;
  SYSPTR    *sysp, *srcsysp;
  INXCOL    *inxcolp, *srcinxcolp;
  HPCHAR    bufp, srcbufp;

  sysp       = &base->sysptr [table];
  colp       = &base->syscolumn [sysp->column];
  inxp       = &base->sysindex [sysp->index + inx];
  bufp       = (HPCHAR)buffer;
  inxcolp    = inxp->inxcols.cols;
  cols       = (SHORT)(inxp->inxcols.size / sizeof (INXCOL));

  srcsysp    = &base->sysptr [srctable];
  srccolp    = &base->syscolumn [srcsysp->column];
  srcinxp    = &base->sysindex [srcsysp->index + srcinx];
  srcbufp    = (HPCHAR)srcbuf;
  srcinxcolp = srcinxp->inxcols.cols;
  srccols    = (SHORT)(srcinxp->inxcols.size / sizeof (INXCOL));

  for (i = 0; i < min (cols, srccols); i++, inxcolp++, srcinxcolp++)
  {
    col  = inxcolp->col;
    len  = inxcolp->len;
    addr = colp [col].addr;
    type = colp [col].type;
    size = typesize (type, bufp + addr);

    if (srcbuf == NULL)
      set_null (type, bufp + addr);
    else
    {
      srccol  = srcinxcolp->col;
      srclen  = inxcolp->len;
      srcaddr = srccolp [srccol].addr;
      srctype = srccolp [srccol].type;
      srcsize = typesize (srctype, srcbufp + srcaddr);

      if (type == srctype)
        if (HASWILD (type))
	  mem_move (bufp + addr, srcbufp + srcaddr, (USHORT)min (len, srclen));
        else
	  mem_lmove (bufp + addr, srcbufp + srcaddr, min (size, srcsize));
    } /* else */
  } /* for */
} /* set_keyvalue */

/*****************************************************************************/

LOCAL SHORT abscol (base, table, col)
LPBASE base;
SHORT  table, col;

{
  return ((SHORT)(base->sysptr [table].column + col));
} /* abscol */

/*****************************************************************************/

LOCAL SHORT absinx (base, table, inx)
LPBASE base;
SHORT  table, inx;

{
  return ((SHORT)(base->sysptr [table].index + inx));
} /* absinx */

/*****************************************************************************/

LOCAL VOID keydfn (base, table, inx, kd)
LPBASE  base;
SHORT   table;
SHORT   inx;
KEY_DFN *kd;

{
  SYSINDEX *inxp;

  inxp         = &base->sysindex [absinx (base, table, inx)];
  kd->type     = inxp->type;
  kd->root     = inxp->root;
  kd->num_keys = inxp->num_keys;
} /* keydfn */

/*****************************************************************************/

LOCAL BOOL table_ok (base, table)
LPBASE base;
SHORT  table;

{
  BOOL ok;

  ok = (SYS_TABLE <= table) && (table < (SHORT)base->systable [SYS_TABLE].recs);

  if (! ok) set_dberror (DB_CNOTABLE);

  return (ok);
} /* table_ok */

/*****************************************************************************/

LOCAL BOOL column_ok (base, table, col)
LPBASE base;
SHORT  table;
SHORT  col;

{
  BOOL ok;

  ok = table_ok (base, table);

  if (ok)
  {
    ok = (0 <= col) && (col < base->systable [table].cols);

    if (! ok) set_dberror (DB_CNOCOLUMN);
  } /* if */

  return (ok);
} /* column_ok */

/*****************************************************************************/

LOCAL BOOL index_ok (base, table, inx)
LPBASE base;
SHORT  table;
SHORT  inx;

{
  BOOL ok;

  ok = table_ok (base, table);

  if (ok)
  {
    ok = (0 <= inx) && (inx < base->systable [table].indexes);

    if (! ok) set_dberror (DB_CNOINDEX);
  } /* if */

  return (ok);
} /* index_ok */

/*****************************************************************************/

LOCAL VOID update_root (base, table, inx)
LPBASE base;
SHORT  table;
SHORT  inx;

{
  DPAGE0 *p;

  p = &base->datainf->page0;

  if (inx == 1)
    switch (table)
    {
      case SYS_TABLE  : p->root_table  = base->sysindex [absinx (base, SYS_TABLE,  1)].root; break;
      case SYS_COLUMN : p->root_column = base->sysindex [absinx (base, SYS_COLUMN, 1)].root; break;
      case SYS_INDEX  : p->root_index  = base->sysindex [absinx (base, SYS_INDEX,  1)].root; break;
    } /* switch, if */
} /* update_root */

/*****************************************************************************/

LOCAL BOOL write_loghead (base, table, lplh)
LPBASE      base;
SHORT       table;
LPLOGHEADER lplh;

{
  BOOL      ok;
  TIMESTAMP ts;

  get_tstamp (&ts);

  file_seek (base->hFileLog, 0L, SEEK_END);                     /* append to the logfile */

  if (base->datainf->page0.flags & DATA_ENCODE)
    lplh->flags |= LOG_FLAG_ENCODE;

  ok = file_write (base->hFileLog, sizeof (LOGHEADER), lplh) == sizeof (LOGHEADER);

  if (lplh->flags & LOG_FLAG_NUMBER)
    ok = ok && file_write (base->hFileLog, sizeof (SHORT), &table) == sizeof (SHORT);

  if (lplh->flags & LOG_FLAG_NAME)
    ok = ok && file_write (base->hFileLog, sizeof (TABLENAME), base->systable [table].name) == sizeof (TABLENAME);

  if (lplh->flags & LOG_FLAG_USER)
    ok = ok && file_write (base->hFileLog, sizeof (USERNAME), base->username) == sizeof (USERNAME);

  if (lplh->flags & LOG_FLAG_TIME)
    ok = ok && file_write (base->hFileLog, sizeof (TIMESTAMP), &ts) == sizeof (TIMESTAMP);

  return (ok);
} /* write_loghead */

/*****************************************************************************/

LOCAL BOOL write_logbuf (base, table, buffer)
LPBASE base;
SHORT  table;
HPVOID buffer;

{
  BOOL   ok;
  LONG   size, l, *longp;
  HPCHAR packbuf, p;

  packbuf = mem_alloc (do_pack (base, table, buffer, NULL) + sizeof (LONG));
  ok      = packbuf != NULL;

  if (ok)
  {
    size   = do_pack (base, table, buffer, packbuf + sizeof (LONG));
    longp  = (LONG *)packbuf;
    *longp = size;

    if (base->datainf->page0.flags & DATA_ENCODE)
      for (l = size, p = packbuf + sizeof (LONG); l > 0; l--, p++)
        *p ^= 0x55;

    size += sizeof (LONG);
    ok    = file_write (base->hFileLog, size, packbuf) == size;

    mem_free (packbuf);
  } /* else */

  return (ok);
} /* write_logbuf */

/*****************************************************************************/

LOCAL VOID fill_sysptr (systable, sysptr, tables)
SYSTABLE *systable;
SYSPTR   *sysptr;
SHORT    tables;

{
  SHORT i, col, inx;

  for (i = col = inx = 0; i < tables; i++, systable++, sysptr++)
  {
    sysptr->column  = col;
    sysptr->index   = inx;
    col            += systable->cols;
    inx            += systable->indexes;
  } /* for */
} /* fill_sysptr */

/*****************************************************************************/

LOCAL VOID str_rem_trail (s)
HPCHAR s;

{
  REG LONG l;

  l = hstrlen (s) - 1;
  while ((l >= 0) && (s [l] == ' ')) l--;
  s [l + 1] = EOS;
} /* str_rem_trail */

/*****************************************************************************/

