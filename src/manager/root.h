/*****************************************************************************
 *
 * Module : ROOT.H
 * Author : Dieter Geiž
 *
 * Creation date    : 01.07.89
 * Last modification: $Id$
 *
 *
 * Description: This module defines the root definitions.
 *
 * History:
 * 21.02.11: DEV_GDOS added
 * 18.01.04: Function user_help added
 * 04.01.04: enum FREETEXTS auskommentiert
 * 21.03.03: classes for selected objects for clipboard
 * 21.12.02: minimize_process added
 * 21.03.97: FTXTSUFF added
 * 03.03.97: Function appl_help is global
 * 01.03.97: Variables bUseDesktopWindow, bTopDesktopWindow, and bUseDesktopWindowSliders moved to global.h
 * 28.02.97: ERR_REC_NOT_FOUND added
 * 26.02.97: FSAVEFIL added
 * 25.02.97: ACCOUNT_PROP_ACC_PRINTER and ACCOUNT_PROP_LINK_ORDER added
 * 24.02.97: Variable bUseDesktopWindowSliders added
 * 18.02.97: ERR_ACC_NOT_FOUND added
 * 16.02.97: ACCOUNT_PROP_LINK_CONDITION added
 * 15.02.97: Link master and child swapped
 * 14.02.97: Variables bUseDesktopWindow and bTopDesktopWindow added
 * 03.01.97: ERR_BAT_NOBATCH and ERR_BAT_NOACCOUNT added
 * 01.01.97: FPPAGENR added
 * 27.12.96: ACCOUNT_PROP_ACC_PAGE_FORMAT and ACCOUNT_PROP_ACC_PAGE_ORIENTATION added
 * 14.12.96: FPAGE added
 * 11.10.95: MAX_RECORDS increased
 * 27.11.95: ACCOUNT_PROP_ACC_IS_TABLE definition added
 * 26.11.95: ACCOUNT_PROP_REG_... definitions added
 * 25.11.95: ACCOUNT_PROP_ACC_... definitions added
 * 21.10.95: ACCOUNT_PROP_VAL_... definitions added
 * 15.10.95: FPROPERTIES added
 * 14.10.95: ACCOUNT_PROP_FONT_FACENAME added
 * 08.10.95: ERR_IMPEXPBLOB added
 * 01.10.95: ACCOUNT_SIZE_LINES added
 * 30.09.95: ACCOUNT_PROP_IS_SELECTED added
 * 24.09.95: ACCOUNT_PROP... added
 * 14.09.95: ACCOUNT_GROUPBOX added
 * 18.07.95: Variables show_raster and use_raster added
 * 17.07.95: ACCOUNT_... definitions added
 * 09.07.95: FDEFACCOUNT added
 * 04.07.95; Parameter retval added in v_execute
 * 31.01.95; Variable warn_table added
 * 08.12.94: FGETPARM added
 * 01.12.94: ERR_SM_NO_FIELD added
 * 16.10.94: ERR_SM_* added
 * 09.07.94: Visible table capabilities added
 * 20.03.94: ERR_REPDEVICE removed
 * 19.03.94: FCOPIES added
 * 12.03.94: Variable show_info added
 * 05.03.94: Variable show_pattern added
 * 23.02.94: Variables show_top and show_left added
 * 22.02.94: COLUMNSEP added
 * 12.02.94: Variable use_std_fs moved to global.h
 * 16.11.93: Variable use_std_fs added
 * 04.11.93: String definitions for commdlg strings added
 * 03.11.93: Functions load_fonts and unload_fonts removed
 * 30.10.93: ERR_INFVERSION added
 * 07.10.93: Variables max_datacache and max_treecache added
 * 05.10.93: FDLG_CHECK, FDLG_RADIO, and FDLG_ARROW added
 * 03.10.93: Variable use_3d moved to global.h
 * 02.10.93: Functions text_export and text_import added
 * 01.10.93: Bitmaps for multikeys added
 * 30.09.93: Variables color_desktop and pattern_desktop moved to global.h
 * 27.09.93: Bitmaps for db's tables, cols, and indices added
 * 09.09.93: Listbox components removed from DB
 * 27.08.93: Variable hide_iconbar added
 * 20.08.93: Modifications for user defined buttons added
 * 01.07.89: Creation of body
 *****************************************************************************/

#ifndef __ROOT__
#define __ROOT__

/****** DEFINES **************************************************************/


#define SEL_NONE          0             /* classes for selected objects */
#define SEL_TABLE         1             /* object in base window */
#define SEL_FIELD         2             /* object in base window */
#define SEL_KEY           3             /* object in base window */
#define SEL_MOBJ          4             /* object in mask window */

#define MAX_DB            8             /* max number of open databases */
#define MAX_DATACACHE  1000             /* max data cache */
#define MAX_TREECACHE  1000             /* max tree cache */
#define MAX_RECORDS    5000             /* max records for virtual table */

#define MAX_TBLICONS      (IPROCESS - ITABLE) /* max number of tables on desktop */
#define MAX_PROCESS       6             /* max number of simultaneous processes */

#define MAX_VTABLE        (MAX_TBLICONS + MAX_PROCESS + MAX_GEMWIND + 2) /* max virtual tables (0 & -1 not used) */
#define VTBL(vtable)      (vtable < FAILURE)
#define VTABLEP(vtable)   (&vtables [-vtable])

#define MAX_VINDEX        (MAX_TBLICONS + MAX_PROCESS + MAX_GEMWIND + 2) /* max virtual tables (0 & -1 not used) */
#define VINX(vindex)      (vindex < FAILURE)
#define VINDEXP(vindex)   (&vindexes [-vindex])

#define EVENTS_PS       100             /* default number of events per second */
#define RECS_PE           1             /* default number of records per event */

#define CLIPBRD           "CLIPBRD "    /* name of clipboard */

#define SCRP_OK        1000             /* message from scrap: ok */
#define SCRP_NOTOK     1001             /* message from scrap: not ok */
#define SCRP_UPDT      1002             /* message to scrap: update window */
#define SCRP_OPEN      1003             /* message to scrap: open window */

#define DEV_SCREEN        0             /* output on screen */
#define DEV_DISK          1             /* output to disk */
#define DEV_PRINTER       2             /* output to printer */
#define DEV_GDOS          3             /* output to GDOS */

#define SIGN(x)           ((x < 0) ? (-1) : (x == 0) ? 0 : 1)
#define LEN(ucsd)         (WORD)(UBYTE)(ucsd [0]) /* length of UCSD string */

#define DESC_ORDER        0x8000        /* sort order for column is descending */

#define FREETXT(index)    (freetxt [index])

#define COLUMNSEP         FS            /* separator of columns in a line */

/****** TYPES ****************************************************************/

typedef struct
{
  BYTE *indexname;                      /* pointer to index name */
  WORD indexnum;                        /* real index number */
} I_INFO;                               /* index info for given index */

typedef struct
{
  BYTE      *tablename;                 /* pointer to table name */
  WORD      tablenum;                   /* table number */
  WORD      indexes;                    /* number of indexes in table */
  WORD      index;                      /* actual index number */
  WORD      i_inx;                      /* actual index index */
  WORD      first;                      /* first index in listbox */
  FIELDNAME calcname;                   /* name of calculation */
  CALCCODE  calccode;                   /* code of calculation for table */
  I_INFO    *i_info;                    /* pointer to index information */
} T_INFO;                               /* table info for given table */

typedef struct
{
  BASE      *base;                      /* database */
  PASSWORD  password;                   /* password for checking with lockscreen */
  UWORD     flags;                      /* open flags for database */
  WORD      pos;                        /* position in combobox */
  WORD      tables;                     /* number of tables in base */
  WORD      table;                      /* actual table number */
  WORD      t_inx;                      /* actual table index */
  WORD      num_vis_tables;             /* number of visible tables */
  WORD      *vis_tables;                /* indexes into t_info of visible tables */
  BOOLEAN   sort_by_name;               /* TRUE, if tables are sorted by name */
  BYTE      *mem;                       /* pointer to dynamic memory */
  T_INFO    *t_info;                    /* pointer to table information */
  VOID      *buffer;                    /* data base buffer */
  SYSICON   *sysicon;                   /* data and mask of all icons */
  SYSLOOKUP *syslookup;                 /* lookup info */
  FORMATSTR *formatstr;                 /* the plain text of the formats */
  FORMAT    *format;                    /* the coded formats */
} DB;                                   /* database definition */

typedef struct
{
  FULLNAME filename;                    /* full name of database */
  UWORD    flags;                       /* open flags for database */
  LONG     treecache;                   /* size of tree cache */
  LONG     datacache;                   /* size of data cache */
  WORD     num_cursors;                 /* number of cursors */
  USERNAME username;                    /* name of user */
  PASSWORD password;                    /* password of user */
  BOOLEAN  sort_by_name;                /* TRUE, if tables are sorted by name */
} DB_SPEC;                              /* database specification for opening */

typedef struct
{
  DB   *db;                             /* database of table */
  WORD table;                           /* physical table */
  LONG recs;                            /* number of actual records */
  WORD cols;                            /* number of actual columns */
  WORD used;                            /* used counter */
  LONG maxrecs;                         /* maximum number of records */
  LONG *recaddr;                        /* addresses of records */
} VTABLE;                               /* virtual table structure */

typedef struct
{
  DB      *db;                          /* database of table */
  WORD    vtable;                       /* virtual table */
  WORD    used;                         /* used counter */
  INXCOLS inxcols;                      /* column numbers of (multiple) indexes */
} VINDEX;                               /* virtual table structure */

typedef struct
{
  DB      *db;                          /* database of icon */
  WORD    table;                        /* table of icon */
  WINDOWP window;                       /* window of icon */
} ICON_INFO;                            /* icon information */

typedef struct
{
  WORD x;                               /* x-position */
  WORD y;                               /* y-position */
} ICON_POS;                             /* icon position */

typedef struct
{
  STRING  recsep;                       /* record separator */
  STRING  colsep;                       /* column separator */
  STRING  txtsep;                       /* text separator */
  BYTE    decimal [2];                  /* decimal period */
  WORD    asciiformat;                  /* ascii or binary format */
  BOOLEAN colname;                      /* print column names before first record */
  WORD    dateformat;                   /* date format */
  WORD    mode;                         /* insert or update mode */
  WORD    primary;                      /* primary index */
  FORMAT  format;                       /* format for date */
  WORD    exportfilter;                 /* KEYTAB filter for the export */
} IMPEXPCFG;

typedef struct
{
  LONGSTR lheader;                      /* left header */
  LONGSTR cheader;                      /* center header */
  LONGSTR rheader;                      /* right header */
  LONGSTR lfooter;                      /* left footer */
  LONGSTR cfooter;                      /* center footer */
  LONGSTR rfooter;                      /* right footer */
  WORD    width;                        /* width of paper */
  WORD    length;                       /* length of paper */
  WORD    mtop;                         /* top margin */
  WORD    mbottom;                      /* bottom margin */
  WORD    mheader;                      /* header margin */
  WORD    mfooter;                      /* footer margin */
  WORD    mleft;                        /* left margin */
  BOOLEAN formadvance;                  /* advance form after every page */
  BOOLEAN colheader;                    /* add column header */
  BOOLEAN sum;                          /* add groups */
  WORD    group;                        /* number of cols for different of group */
  WORD    result;                       /* resulting number of records per page */
} PAGE_FORMAT;

typedef struct
{
  STRING  name;                         /* name of printer */
  STRING  control;                      /* control code sequences */
  STRING  filename;                     /* filename */
  WORD    face;                         /* face */
  BOOLEAN condensed;                    /* condensed letters */
  BOOLEAN micro;                        /* micro letters */
  BOOLEAN expanded;                     /* expanded letters */
  BOOLEAN italic;                       /* italic letters */
  BOOLEAN emphasized;                   /* empahsized letters */
  BOOLEAN nlq;                          /* near letter quality */
  BOOLEAN spool;                        /* spool file */
  WORD    port;                         /* port number */
  BYTE    *port_name [5];               /* port name */
  WORD    lspace;                       /* line spacing */
  LONG    events_ps;                    /* events per second for spooler */
  LONG    bytes_pe;                     /* bytes per event for spooler */
} PRNCFG;

typedef union
{
  WORD   c_word;                        /* one word */
  LONG   c_long;                        /* one long */
  DOUBLE c_float;                       /* one double */
  STRING c_cfloat;                      /* one character float */
} CALC_TYPE;

/****** VARIABLES ************************************************************/

GLOBAL DB        db [MAX_DB];           /* all databases */
GLOBAL DB        *actdb;                /* actual database */
GLOBAL WORD      num_opendb;            /* number of open db's */
GLOBAL VTABLE    vtables [MAX_VTABLE];  /* virtual tables */
GLOBAL VINDEX    vindexes [MAX_VINDEX]; /* virtual indexes */
GLOBAL ICON_INFO icon_info [MAX_TBLICONS];  /* icon information */
GLOBAL ICON_POS  icon_pos [FKEYS + 1 - DTABLES]; /* icon positions */
GLOBAL WORD      sort_order;            /* ascending, descending */
GLOBAL BOOLEAN   sort_by_name;          /* tables and indexes sorted by name */
GLOBAL BOOLEAN   show_raster;           /* show raster in account */
GLOBAL BOOLEAN   use_raster;            /* use raster in account */
GLOBAL BOOLEAN   lockable;              /* screen is lockable */

GLOBAL FULLNAME  tmpdir;                /* temporary directory */
GLOBAL BOOLEAN   proc_beep;             /* beep at finishing of processes */
GLOBAL BOOLEAN   autosave;              /* save config on exit */
GLOBAL BOOLEAN   use_fonts;             /* use external fonts */
GLOBAL BOOLEAN   hide_func;             /* hide function keys */
GLOBAL BOOLEAN   hide_iconbar;          /* hide iconbar keys */
GLOBAL BOOLEAN   show_queried;          /* show number of queried recs */
GLOBAL BOOLEAN   show_grid;             /* show grid in list windows */
GLOBAL BOOLEAN   show_info;             /* show info line in list windows */
GLOBAL BOOLEAN   show_top;              /* show topmost line */
GLOBAL BOOLEAN   show_left;             /* show leftmost column */
GLOBAL BOOLEAN   show_pattern;          /* show pattern in topmost line and leftmost column (only in b/w) */
GLOBAL BOOLEAN   use_calc;              /* use calculation in every window */

GLOBAL BOOLEAN   minimize_process; 			/* minimize processes */
GLOBAL BOOLEAN   minimize;	            /* minimize rest */
GLOBAL BOOLEAN   exclusiv_process;      /* start process always exklusiv */

GLOBAL BOOLEAN   warn_table;            /* warning of using empty tables */
GLOBAL LONG      max_datacache;         /* max. cache for data file */
GLOBAL LONG      max_treecache;         /* max. cache for tree file */
GLOBAL LONG      max_records;           /* max. records for query tables */
GLOBAL LONG      events_ps;             /* number of events per second */
GLOBAL LONG      recs_pe;               /* number of records per event */

GLOBAL FULLNAME  cfg_path;              /* path of configuration */
GLOBAL FILENAME  cfg_name;              /* name of configuration */
GLOBAL FULLNAME  drv_path;              /* path of printer driver */
GLOBAL FILENAME  drv_name;              /* name of printer driver */

GLOBAL FONTDESC  fontdesc;              /* font for next window */

GLOBAL UWORD     wi_modeless;           /* WI_MODELESS or WI_MODAL */

GLOBAL BYTE      **freetxt;             /* array of all free text elements */

GLOBAL ICON      tooldb_icon;		/* toolbar db icon */
GLOBAL ICON      tooltbl_icon;		/* toolbar table icon */
GLOBAL ICON      toolcol_icon;		/* toolbar column icon */
GLOBAL ICON      toolinx_icon;		/* toolbar index icon */
GLOBAL ICON      toolmul_icon;		/* toolbar multi index icon */
GLOBAL ICON      toolcal_icon;		/* toolbar calc operator icon */
GLOBAL ICON      toolbat_icon;		/* toolbar batch command icon */

/****** FUNCTIONS ************************************************************/

GLOBAL BOOLEAN init_root         _((VOID));
GLOBAL BOOLEAN term_root         _((VOID));

GLOBAL WORD    open_db           _((FULLNAME filename, UWORD flags, LONG datacache, LONG treecache, WORD num_cursors, USERNAME username, PASSWORD password, BOOLEAN sort_by_name));
GLOBAL WORD    close_db          _((VOID));

GLOBAL VOID    switch_base       _((WORD pos));
GLOBAL VOID    switch_table      _((DB *db, WORD table));
GLOBAL WORD    tablenum          _((DB *db, WORD t_inx));
GLOBAL WORD    indexnum          _((DB *db, WORD t_inx, WORD i_inx));
GLOBAL WORD    tableinx          _((DB *db, WORD table));
GLOBAL WORD    indexinx          _((DB *db, WORD table, WORD index));
GLOBAL WORD    sel_from_inx      _((DB *db, WORD t_inx));

GLOBAL WORD    rtable            _((WORD vtable));
GLOBAL WORD    new_vtable        _((DB *db, WORD table, LONG maxrecs, WORD cols));
GLOBAL VOID    free_vtable       _((WORD vtable));
GLOBAL VOID    del_vtable        _((WORD vtable, LONG index));
GLOBAL LONG    in_vtable         _((VTABLE *vtablep, LONG addr));
GLOBAL VOID    check_vtable      _((WORD vtable, LONG addr));
GLOBAL WORD    new_vindex        _((DB *db, WORD vtable, INXCOLS *inxcols));
GLOBAL VOID    free_vindex       _((WORD vindex));
GLOBAL WORD    v_tableinfo       _((DB *db, WORD vtable, TABLE_INFO *table_info));
GLOBAL WORD    v_indexinfo       _((DB *db, WORD vtable, WORD vindex, INDEX_INFO *index_info));
GLOBAL BOOLEAN v_initcursor      _((DB *db, WORD vtable, WORD inx, WORD dir, CURSOR *cursor));
GLOBAL BOOLEAN v_movecursor      _((DB *db, CURSOR *cursor, LONG steps));
GLOBAL LONG    v_readcursor      _((DB *db, CURSOR *cursor, VOID *keyval));
GLOBAL BOOLEAN v_isfirst         _((DB *db, CURSOR *cursor));
GLOBAL BOOLEAN v_islast          _((DB *db, CURSOR *cursor));
GLOBAL BOOLEAN v_read            _((DB *db, WORD vtable, VOID *buffer, CURSOR *cursor, LONG address, BOOLEAN modify));
GLOBAL WORD    v_execute         _((DB *db, WORD vtable, CALCCODE *calccode, VOID *buffer, LONG count, LPVALUEDESC retval));
GLOBAL LONG    num_keys          _((DB *db, WORD vtable, WORD inx));

GLOBAL BYTE    *dbtbl_name       _((DB *db, WORD vtable, BYTE *name));
GLOBAL BYTE    *table_name       _((DB *db, WORD vtable, BYTE *name));
GLOBAL BYTE    *column_name      _((DB *db, WORD vtable, WORD col, BYTE *name));
GLOBAL BYTE    *index_name       _((DB *db, WORD vtable, WORD inx, BYTE *name));
GLOBAL BYTE    *sort_name        _((DB *db, WORD vtable, WORD inx, WORD dir, BYTE *name));
GLOBAL BYTE    *temp_name        _((BYTE *s));

GLOBAL WORD    init_columns      _((DB *db, WORD table, WORD tblcols, WORD *columns, WORD *colwidth));
GLOBAL WORD    def_width         _((DB *db, WORD table, WORD col));
GLOBAL LONG    get_width         _((WORD cols, WORD *colwidth));
GLOBAL VOID    build_colheader   _((DB *db, WORD vtable, WORD cols, WORD *columns, WORD *colwidth, LONG max_cols, BYTE *header));
GLOBAL VOID    rec_to_line       _((DB *db, WORD vtable, VOID *buffer, WORD cols, WORD *columns, WORD *colwidth, LONG max_cols, BYTE *line));
GLOBAL VOID    col_to_str        _((DB *db, WORD table, VOID *buffer, WORD col, WORD colwidth, BYTE *s));
GLOBAL VOID    get_colheader     _((DB *db, WORD vtable, WORD cols, WORD *columns, WORD *colwidth, LONG max_cols, BYTE *header));
GLOBAL VOID    rec2line          _((DB *db, WORD vtable, VOID *buffer, WORD cols, WORD *columns, WORD *colwidth, LONG max_cols, BYTE *line));
GLOBAL VOID    col2str           _((DB *db, WORD table, VOID *buffer, WORD col, WORD colwidth, BYTE *s));
GLOBAL BOOLEAN str2col           _((DB *db, WORD table, VOID *buffer, WORD col, BYTE *s));
GLOBAL VOID    build_keystr      _((DB *db, WORD table, WORD inx, VOID *buffer, BYTE *keystr));
GLOBAL BOOLEAN printable         _((WORD type));
GLOBAL BOOLEAN str2ucsd          _((BYTE *s, BYTE *u));
GLOBAL BOOLEAN same_ucsd         _((BYTE *u1, BYTE *u2));
GLOBAL LONG    date2days         _((DATE *date));
GLOBAL VOID    days2date         _((LONG days, DATE *date));
GLOBAL LONG    time2secs         _((TIME *time));
GLOBAL VOID    secs2time         _((LONG seconds, TIME *time));

GLOBAL WORD    get_word          _((OBJECT *tree, WORD object));
GLOBAL LONG    get_long          _((OBJECT *tree, WORD object));
GLOBAL VOID    set_word          _((OBJECT *tree, WORD object, WORD value));
GLOBAL VOID    set_long          _((OBJECT *tree, WORD object, LONG value));

GLOBAL BOOLEAN hndl_help         _((WORD helpindex));
GLOBAL BOOLEAN appl_help         _((BYTE *helpmsg));
GLOBAL BOOLEAN user_help         _((BYTE *helpmsg));
GLOBAL WORD    dbtest            _((DB *db));
GLOBAL WORD    dberror           _((BYTE *basename, WORD errornum));
GLOBAL BOOLEAN integrity         _((BASE *base, WORD table, WORD status));
GLOBAL WORD    file_error        _((WORD errornum, BYTE *filename));
GLOBAL VOID    sort_tables       _((DB *db, BOOLEAN alphanum));
GLOBAL WORD    find_rel          _((DB *db, WORD src_table, WORD src_index, WORD dst_table));
GLOBAL BOOLEAN sel_file          _((BYTE *name, BYTE *path, WORD suffix, WORD label, BYTE *filename));
GLOBAL BOOLEAN get_open_filename _((WORD wTitle, BYTE *pszHelpID, ULONG ulFlags, WORD wFilter, LONG *plFilterIndex, BYTE *pszInitialDir, WORD wDefExt, BYTE *pszFullName, BYTE *pszFileName));
GLOBAL BOOLEAN get_save_filename _((WORD wTitle, BYTE *pszHelpID, ULONG ulFlags, WORD wFilter, LONG *plFilterIndex, BYTE *pszInitialDir, WORD wDefExt, BYTE *pszFullName, BYTE *pszFileName));
GLOBAL VOID    set_idata         _((OBJECT *tree, WORD object, WORD *mask, WORD *data, WORD width, WORD height));
GLOBAL VOID    set_greyline      _((VOID));
GLOBAL BOOLEAN esc_pressed       _((VOID));
GLOBAL BOOLEAN check_close       _((VOID));
GLOBAL VOID    set_period        _((MKINFO *mk));
GLOBAL VOID    scan_header       _((DB *db, WORD vtable, WORD inx, WORD dir, WORD pagenr, BYTE *src, BYTE *dst));

GLOBAL BOOLEAN text_export       _((BYTE *text, LONG size));
GLOBAL BOOLEAN text_import       _((BYTE *text, LONG size));

#endif /* __ROOT__ */

