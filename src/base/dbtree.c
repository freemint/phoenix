/*****************************************************************************
 *
 * Module : DBTREE.C
 * Author : Dieter Geiž
 *
 * Creation date    : 01.07.89
 * Last modification: $Id$
 *
 *
 * Description: This module implements the database tree routines.
 *
 * History:
 * 12.10.93: Warning with Visual C++ for Windows NT removed
 * 26.07.93: Problem with inserting keys fixed in function split
 * 27.05.93: Index pages are better packed when inserting alphabetically
 * 18.05.93: Pages are filled with zeros in write_page
 * 03.05.93: Cursor is zeroed in init_cursor if no keys exist
 * 31.10.92: Far pointer definitions for DLL implementation added
 * 30.10.92: Problem in too_much solved
 * 29.10.92: Trailing spaces removed in wildcard search
 * 26.10.92: Problem in compensate solved when two pages are merged
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
#include "dbtree.h"

/****** DEFINES **************************************************************/

#define WH_LT         0         /* search leftmost */
#define WH_RT         1         /* search rightmost */
#define WH_INS        2         /* search for insert */

#define INC_PAGES    16         /* increment in pages */

#define ENCODE(a, b)  (UCHAR)(((a) + (b)) & 0xFFL)

#define OFF(type, ident)        ((SIZE_T)(LONG)(((type *)0)->ident))
#define SIZE_KEY_ENTRY          OFF (KEY_ENTRY, key)
#define SIZE_KEY_PAGE           OFF (KEY_PAGE, key_entry)

/****** TYPES ****************************************************************/

typedef struct
{
  LONG pointer;                 /* number of left/right page */
  LONG data;                    /* address of data */
  CHAR key [MAX_KEYSIZE];       /* actual key */
} ENTRY;

typedef struct
{
  LONG pointer;                 /* number of left/right page */
  LONG data;                    /* address of data */
  CHAR key [1];                 /* address of actual key */
} KEY_ENTRY;

typedef struct
{
  LONG      next;               /* next page number for link */
  LONG      prev;               /* previous page number for link */
  SHORT     size;               /* size of used area of page */
  SHORT     num_keys;           /* number of keys in page */
  KEY_ENTRY key_entry [1];      /* address of first key_entry */
} KEY_PAGE;

typedef struct
{
  LPTREEINF tree;                /* tree of key */
  SHORT     type;                /* type of key */
  HPVOID    keyval;              /* value of key */
  LONG      data;                /* data of key */
  SHORT     dir;                 /* search direction */
  LONG      pagenum;             /* page number where key has been found */
  SHORT     inx;                 /* index of found key in page */
  SHORT     offset;              /* offset of found key in page */
  LONG      corr;                /* correction factor (only for delete) */
} KEY_POS;

/****** VARIABLES ************************************************************/

/****** FUNCTIONS ************************************************************/

LOCAL BOOL      new_root     _((LPTREEINF tree, HPKEY_DFN kd, ENTRY *entry));
LOCAL BOOL      to_insert    _((LPTREEINF tree, LONG pagenum, ENTRY *entry, SHORT type, HPVOID keyval, LONG data));
LOCAL BOOL      split        _((LPTREEINF tree, LONG pagenum, KEY_PAGE *kp, ENTRY *entry, KEY_POS *pos));

LOCAL BOOL      to_delete    _((LPTREEINF tree, LONG pagenum, SHORT type, HPVOID keyval, LONG data, ENTRY *entry, ENTRY *oe, BOOL *over, LONG *corr));
LOCAL BOOL      compensate   _((LPTREEINF tree, KEY_PAGE *kp, LONG pointer, KEY_POS *pos, ENTRY *entry, ENTRY *oe, BOOL *over));
LOCAL BOOL      fits         _((KEY_PAGE *kp, ENTRY *entry, SHORT type));
LOCAL BOOL      underflow    _((KEY_PAGE *kp));
LOCAL SHORT     too_much     _((KEY_PAGE *kp, SHORT type));
LOCAL VOID      corr_cursors _((LPTREEINF tree, KEY_PAGE *kp, LONG pagenum, LONG corrnum, SHORT inx, SHORT which));

LOCAL BOOL      is_wild      _((SHORT dir, CHAR *saveval, CHAR *shortval));
LOCAL BOOL      search_tree  _((LONG pagenum, KEY_POS *pos));
LOCAL BOOL      search_page  _((KEY_PAGE *kp, KEY_POS *pos));
LOCAL BOOL      search_lin   _((KEY_PAGE *kp, KEY_POS *pos));
LOCAL BOOL      search_bin   _((KEY_PAGE *kp, KEY_POS *pos));

LOCAL VOID      copy_entry   _((ENTRY *dst, KEY_ENTRY *src, SHORT type));
LOCAL VOID      xchng_entry  _((KEY_PAGE *kp, SHORT inx, ENTRY *entry, SHORT type));
LOCAL VOID      ins_entry    _((KEY_PAGE *kp, ENTRY *entry, SHORT type, SHORT offset));
LOCAL VOID      del_entry    _((KEY_PAGE *kp, SHORT type, SHORT offset));
LOCAL KEY_ENTRY *next_entry  _((KEY_ENTRY *ke, SHORT type));
LOCAL KEY_ENTRY *calc_entry  _((KEY_PAGE *kp, SHORT type, SHORT inx));
LOCAL SHORT     calc_offset  _((KEY_PAGE *kp, SHORT type, SHORT inx));

LOCAL LONG      new_page     _((LPTREEINF tree));
LOCAL BOOL      release_page _((LPTREEINF tree, LONG pagenum));
LOCAL BOOL      read_pg0     _((LPTREEINF tree));
LOCAL BOOL      write_pg0    _((LPTREEINF tree));
LOCAL BOOL      read_vpage   _((LPTREEINF tree, LONG pagenum, VOID *page, SHORT start, SHORT size));
LOCAL BOOL      write_vpage  _((LPTREEINF tree, LONG pagenum, VOID *page, SHORT start, SHORT size));
LOCAL VPAGEINFP in_memory    _((LPTREEINF tree, LONG pagenum));
LOCAL VPAGEINFP new_vpage    _((LPTREEINF tree, LONG pagenum));
LOCAL BOOL      find_vpage   _((LPTREEINF tree, LONG pagenum, LONG *inx));
LOCAL VOID      move_vpage   _((LPTREEINF tree, LONG oldinx, LONG newinx));
LOCAL BOOL      read_page    _((LPTREEINF tree, LONG pagenum, VOID *page, SHORT start, SHORT size));
LOCAL BOOL      write_page   _((LPTREEINF tree, LONG pagenum, VOID *page, SHORT start, SHORT size));

/*****************************************************************************/

GLOBAL LPTREEINF WINAPI create_tree (filename, flags, size)
LPCHAR filename;
USHORT flags;
LONG   size;

{
  LPTREEINF tree;
  PAGE      page;
  LONG      pages;
  PAGE0     *p0;

  tree = (LPTREEINF)mem_alloc ((LONG)sizeof (TREEINF));

  if (tree == NULL)
    set_dberror (DB_NOMEMORY);
  else
  {
    mem_set (tree, 0, sizeof (TREEINF));

    tree->handle = file_create (filename);

    if (tree->handle < 0)
    {
      set_dberror (DB_TNOCREATE);
      mem_free (tree);
      tree = NULL;
    } /* if */
    else
    {
      if (size < 1) size = 1;

      pages          = size * 1024L / PAGESIZE;
      p0             = &tree->page0;
      p0->flags      = flags;
      p0->num_pages  = pages;
      p0->used_pages = 1;
      p0->free_page  = 1;
      p0->rlsd_page  = 0;

      mem_set (page, 0, sizeof (page));
      mem_move (page, p0, sizeof (PAGE0));

      if (! write_page (tree, 0L, page, 0, PAGESIZE))   /* write full page zero */
      {
        file_close (tree->handle);
        mem_free (tree);
        tree = NULL;
      } /* if */
      else
      {
        if (--pages > 0)                                /* page zero already written */
        {
          mem_set (page, 0, sizeof (page));

          if (! write_page (tree, 1L, page, 0, PAGESIZE)) /* write page 1 */
          {
            file_close (tree->handle);
            mem_free (tree);
            tree = NULL;
          } /* if */
          else
            if (--pages > 0) expand_file (tree->handle, (pages * PAGESIZE) / 1024L);
        } /* if */
      } /* else */
    } /* else */
  } /* else */

  return (tree);
} /* create_tree */

/*****************************************************************************/

GLOBAL LPTREEINF WINAPI open_tree (filename, flags, cache_size, num_cursors)
LPCHAR filename;
USHORT flags;
LONG   cache_size;
SHORT  num_cursors;

{
  LPTREEINF tree;
  SHORT     level, j;
  INT       mode;
  LONG      i, num_vpages, num_pages, size, bytes;
  VPAGEINFP vi;
  PAGEP     vp;

  level = 0;
  tree  = (LPTREEINF)mem_alloc ((LONG)sizeof (TREEINF));

  if (tree == NULL)
  {
    level = 1;
    set_dberror (DB_NOMEMORY);
  } /* if */
  else
  {
    mem_set (tree, 0, sizeof (TREEINF));

    mode = flags & TREE_RDONLY ? O_RDONLY : O_RDWR;

#if GEMDOS
    if (flags & TREE_MULUSER) mode |= O_SHARED; /* GEMDOS has no sharing mechanism without newtware support */
#else
    if (flags & (TREE_MULUSER | TREE_MULTASK)) mode |= O_SHARED;
#endif

    num_vpages        = cache_size * 1024L / PAGESIZE;
    tree->flags       = flags;
    tree->num_vpages  = num_vpages;
    tree->num_cursors = num_cursors;
    tree->handle      = file_open (filename, mode);

    if (tree->handle < 0)
    {
      level = 2;
      set_dberror (DB_TNOOPEN);
    } /* if */
    else
    {
      if (num_vpages > 0)
      {
        tree->vpages = (PAGE *)mem_alloc (num_vpages * sizeof (PAGE));

        if (tree->vpages == NULL)
        {
          level = 3;
          set_dberror (DB_NOMEMORY);
        } /* if */
        else
        {
          tree->vpageinf = (VPAGEINFP)mem_alloc (num_vpages * sizeof (VPAGEINF));

          if (tree->vpageinf == NULL)
          {
            level = 4;
            set_dberror (DB_NOMEMORY);
          } /* if */
          else
          {
            mem_lset (tree->vpageinf, 0, num_vpages * sizeof (VPAGEINF));
            vi = tree->vpageinf;
	    vp = tree->vpages;

            for (i = 0; i < num_vpages; i++, vi++, vp++)
            {
              vi->pagenum = FAILURE;
              vi->vpage   = (UCHAR *)vp;
            } /* for */
          } /* else */
        } /* else */
      } /* if */

      if (level == 0)
        if (! read_pg0 (tree))
          level = 5;
        else
        {
          if (num_vpages > 0)
          {
            num_pages = min (tree->page0.free_page, num_vpages);        /* minimum to read */
            size      = num_pages * PAGESIZE;                           /* bytes to read */
            bytes     = file_seek (tree->handle, 0L, SEEK_SET);         /* seek to page 0 */
            bytes     = file_read (tree->handle, size, tree->vpages);   /* read it */

            if (bytes < size)
            {
              level = 5;
              set_dberror (DB_TRDPAGE);
            } /* if */
            else
            {
              vi = tree->vpageinf;

              for (i = 0; i < num_vpages; i++, vi++)
              {
                vi->pagenum = i;

                if (i < num_pages)
                {
                  vi->time = tree->vtime++;

                  if ((i > 0) && (tree->page0.flags & TREE_ENCODE))
                    for (j = 0; j < PAGESIZE; j++) vi->vpage [j] ^= ENCODE (i, j);
                } /* if */
              } /* for */

              tree->vpageinf->time = tree->vtime++;     /* force page 0 to stay resident */
            } /* else */
          } /* if */

          if (level == 0)
            if (num_cursors > 0)
            {
              tree->cursors = (LPCURSOR)mem_alloc ((LONG)num_cursors * sizeof (CURSOR));

              if (tree->cursors == NULL)
              {
                level = 5;
                set_dberror (DB_NOMEMORY);
              } /* if */
              else
                mem_set (tree->cursors, 0, (USHORT)(num_cursors * sizeof (CURSOR)));
            } /* if, if */
        } /* else, if */
    } /* else */
  } /* else */

  switch (level)
  {
    case 6 : mem_free (tree->cursors);
    case 5 : mem_free (tree->vpageinf);
    case 4 : mem_free (tree->vpages);
    case 3 : file_close (tree->handle);
    case 2 : mem_free (tree);
    case 1 : tree = NULL;
    case 0 : break;
  } /* switch */

  return (tree);
} /* open_tree */

/*****************************************************************************/

GLOBAL BOOL WINAPI close_tree (tree)
LPTREEINF tree;

{
  BOOL ok;

  ok = (tree->flags & TREE_RDONLY) || write_pg0 (tree);
  ok = flush_virtual (tree) && (file_close (tree->handle) == SUCCESS) && ok;

  mem_free (tree->vpageinf);
  mem_free (tree->vpages);
  mem_free (tree->cursors);
  mem_free (tree);

  if (! ok) set_dberror (DB_TNOCLOSE);

  return (ok);
} /* close_tree */

/*****************************************************************************/

GLOBAL BOOL WINAPI insert_key (tree, keyval, data, kd)
LPTREEINF tree;
HPVOID    keyval;
LONG      data;
HPKEY_DFN kd;

{
  BOOL  ok;
  ENTRY entry;

  ok = TRUE;

  if (to_insert (tree, kd->root, &entry, kd->type, keyval, data)) ok = new_root (tree, kd, &entry);

  if (ok && (get_dberror () == SUCCESS)) kd->num_keys++;

  ok = get_dberror () == SUCCESS;

  return (ok);
} /* insert_key */

/*****************************************************************************/

GLOBAL BOOL WINAPI delete_key (tree, keyval, data, kd)
LPTREEINF tree;
HPVOID    keyval;
LONG      data;
HPKEY_DFN kd;

{
  BOOL     ok, to_del, over;
  PAGE     page;
  KEY_PAGE *kp;
  LONG     pagenum, corr;
  ENTRY    entry, oe;
  CHAR     saveval [MAX_KEYSIZE];
  SHORT    len;
  CURSOR   cursor;

  ok = TRUE;

  mem_move (saveval, keyval, keysize (kd->type, keyval));

  to_del = to_delete (tree, kd->root, kd->type, saveval, data, &entry, &oe, &over, &corr);

  if (to_del)
  {
    ok = read_vpage (tree, kd->root, page, 0, PAGESIZE);
    kp = (KEY_PAGE *)page;

    if (ok && (kp->num_keys == 0))              /* delete old root */
    {
      pagenum  = kd->root;
      kd->root = kp->key_entry->pointer;
      ok       = release_page (tree, pagenum);
    } /* if */
  } /* if */

  if (over) ok = new_root (tree, kd, &oe);

  if (ok && (get_dberror () == SUCCESS))
    kd->num_keys--;
  else
    if (HASWILD (kd->type) && (get_dberror () == DB_TNOKEY)) /* key probably to small */
    {
      strcpy (saveval, (CHAR *)keyval);
      len = (SHORT)strlen (saveval);

      if (len < MAX_KEYSIZE - 1)
      {
        saveval [len]     = WILD_CHARS;                 /* search for complete key */
        saveval [len + 1] = EOS;

        if (search_key (tree, ASCENDING, &cursor, saveval, data, kd, TRUE))
          if (read_cursor (&cursor, saveval) != 0)      /* now we got the full key */
          {
            set_dberror (SUCCESS);
            ok = delete_key (tree, saveval, data, kd);
          } /* if, if */
      } /* if */
    } /* if, else */

  ok = get_dberror () == SUCCESS;

  return (ok);
} /* delete_key */

/*****************************************************************************/

GLOBAL BOOL WINAPI search_key (tree, dir, cursor, keyval, data, kd, use_wildcards)
LPTREEINF tree;
SHORT     dir;
LPCURSOR  cursor;
HPVOID    keyval;
LONG      data;
HPKEY_DFN kd;
BOOL      use_wildcards;

{
  BOOL    found, more, wild;
  LONG    root, found_data;
  SHORT   type, cmp, len;
  INT     i;
  KEY_POS pos;
  CHAR    saveval [MAX_KEYSIZE];
  CHAR    cursval [MAX_KEYSIZE];
  CHAR    shortval [MAX_KEYSIZE];
  CHAR    key [MAX_KEYSIZE];

  root = kd->root;
  type = kd->type;
  wild = FALSE;

  mem_move (saveval, keyval, keysize (type, keyval));

  if (HASWILD (type) && use_wildcards)
  {
    i = strlen (saveval) - 1;

    while ((i >= 0) && (saveval [i] == ' ')) i--;

    if ((i > 0) && (saveval [i] == WILD_CHARS))         /* cut off spaces */
      saveval [i + 1] = EOS;

    strcpy (key, saveval);                              /* only for wildcard search */

    wild = is_wild (dir, saveval, shortval);
  } /* if */

  pos.tree   = tree;
  pos.type   = type;
  pos.keyval = (VOID *)saveval;
  pos.data   = data;
  pos.dir    = (SHORT)(((dir == ASCENDING) || (data != 0)) ? WH_LT : WH_RT);

  found = search_tree (root, &pos);                     /* search for key */

  cursor->tree     = tree;
  cursor->type     = type;
  cursor->pagenum  = pos.pagenum;                       /* save values in cursor */
  cursor->inx      = pos.inx;
  cursor->flags   &= ~ CURS_DELETED;

  if (wild)
  {
    more  = TRUE;
    len   = (SHORT)strlen (shortval);
    type |= USE_WILDCARD;                               /* use wildcards for string compare */

    while (more)
    {
      found_data = read_cursor (cursor, cursval);
      more       = found_data != 0;

      if (more)
      {
        cmp   = cmp_vals (type, key, cursval);
        found = cmp == 0;

        if (found && (data != 0)) found = data == found_data;

        more = ! found;

        if (more)
        {
          cursval [len] = EOS;                                  /* first 'len' digits */
          cmp           = cmp_vals (type, shortval, cursval);
          more          = (cmp == 0) && move_cursor (cursor, (LONG)dir);
        } /* if */
      } /* if */
    } /* while */
  } /* if */

  return (found);
} /* search_key */

/*****************************************************************************/

GLOBAL BOOL WINAPI update_key (tree, keyval, data, newdata, kd)
LPTREEINF tree;
HPVOID    keyval;
LONG      data;
LONG      newdata;
HPKEY_DFN kd;

{
  BOOL      found;
  SHORT     type;
  CURSOR    cursor;
  PAGE      page;
  KEY_PAGE  *kp;
  KEY_ENTRY *ke;

  found = search_key (tree, ASCENDING, &cursor, keyval, data, kd, FALSE);

  if (found)
    if (read_vpage (tree, cursor.pagenum, page, 0, PAGESIZE))
    {
      type     = kd->type;
      kp       = (KEY_PAGE *)page;
      ke       = calc_entry (kp, type, cursor.inx);
      ke->data = newdata;

      write_vpage (tree, cursor.pagenum, page, 0, PAGESIZE);
    } /* if, if */

  return (found);
} /* update_key */

/*****************************************************************************/

GLOBAL LPCURSOR WINAPI new_cursor (tree)
LPTREEINF tree;

{
  LPCURSOR cursor;
  SHORT    i;

  cursor = tree->cursors;

  for (i = 0; (i < tree->num_cursors) && (cursor->flags & CURS_USED); i++, cursor++);

  if (i == tree->num_cursors)           /* no cursor found */
    cursor = NULL;
  else
  {
    cursor->tree   = tree;
    cursor->flags |= CURS_USED;
  } /* else */

  return (cursor);
} /* new_cursor */

/*****************************************************************************/

GLOBAL VOID WINAPI free_cursor (cursor)
LPCURSOR cursor;

{
  if (cursor != NULL) mem_set (cursor, 0, sizeof (CURSOR));
} /* free_cursor */

/*****************************************************************************/

GLOBAL BOOL WINAPI init_cursor (tree, dir, kd, cursor)
LPTREEINF tree;
SHORT     dir;
HPKEY_DFN kd;
LPCURSOR  cursor;

{
  BOOL      ok, more;
  LONG      pagenum, last;
  SHORT     type;
  PAGE      page;
  KEY_PAGE  *kp;
  KEY_ENTRY *ke;

  cursor->tree   = tree;
  cursor->type   = kd->type;
  cursor->flags &= ~ CURS_DELETED;
  pagenum        = kd->root;
  type           = kd->type;
  last           = 0;
  ok             = pagenum != 0;
  kp             = (KEY_PAGE *)page;

  if (ok)                                               /* root wasn't zero */
  {
    more = TRUE;

    while (ok && more)                                  /* iteration will work */
    {
      ok   = read_vpage (tree, pagenum, page, 0, PAGESIZE);
      last = pagenum;

      if (ok)
      {
        ke      = (dir == ASCENDING) ? kp->key_entry : calc_entry (kp, type, kp->num_keys);
        pagenum = ke->pointer;
        more    = pagenum != 0;                         /* no leaf */
      } /* if */
    } /* while */
  } /* if */

  if (ok)
  {
    cursor->pagenum = last;
    cursor->inx     = (SHORT)((dir == ASCENDING) ? 0 : kp->num_keys + 1);
  } /* if */
  else
  {
    cursor->pagenum = 0;
    cursor->inx     = 0;
  } /* else */

  return (ok);
} /* init_cursor */

/*****************************************************************************/

GLOBAL BOOL WINAPI move_cursor (cursor, steps)
LPCURSOR cursor;
LONG     steps;

{
  BOOL     ok;
  PAGE     page;
  KEY_PAGE *kp;
  SHORT    max_steps, add;

  ok = cursor->pagenum != 0;
  kp = (KEY_PAGE *)page;

  while (ok && (steps != 0))
  {
    ok = read_vpage (cursor->tree, cursor->pagenum, page, 0, PAGESIZE);

    if (steps > 0)
    {
      if (cursor->flags & CURS_DELETED) steps--;

      max_steps    = (SHORT)(kp->num_keys - cursor->inx + 1);
      add          = (SHORT)min ((LONG)max_steps, steps);
      steps       -= add;
      cursor->inx += add;

      if (cursor->inx > kp->num_keys)           /* read next page */
        if (kp->next == 0)                      /* page doesn't exist */
        {
          ok          = FALSE;
          cursor->inx = kp->num_keys;
        } /* if */
        else
        {
          cursor->pagenum = kp->next;
	  cursor->inx     = 1;
        } /* else, if */
    } /* if */
    else
    {
      if (cursor->inx == -1) cursor->inx = kp->num_keys;

      max_steps    = (SHORT)(-cursor->inx);
      add          = (SHORT)max ((LONG)max_steps, steps);
      steps       -= add;
      cursor->inx += add;

      if (cursor->inx == 0)                     /* read prev page */
	if (kp->prev == 0)                      /* page doesn't exist */
        {
          ok          = FALSE;
          cursor->inx = 1;
        } /* if */
        else
        {
          cursor->pagenum = kp->prev;
	  cursor->inx     = -1;                 /* correct value, see later */
        } /* else, if */
    } /* else */

    cursor->flags &= ~ CURS_DELETED;
  } /* while */

  if (cursor->inx == -1)
  {
    ok          = read_vpage (cursor->tree, cursor->pagenum, page, 0, PAGESIZE);
    cursor->inx = kp->num_keys;
  } /* if */

  return (ok);
} /* move_cursor */

/*****************************************************************************/

GLOBAL LONG WINAPI read_cursor (cursor, keyval)
LPCURSOR cursor;
HPVOID   keyval;

{
  LONG      data;
  SHORT     type;
  PAGE      page;
  KEY_PAGE  *kp;
  KEY_ENTRY *ke;

  data = 0;
  type = cursor->type;

  if (cursor->pagenum != 0)
    if (read_vpage (cursor->tree, cursor->pagenum, page, 0, PAGESIZE))
    {
      kp = (KEY_PAGE *)page;

      if (cursor->inx > kp->num_keys) cursor->inx = kp->num_keys; /* overflow caused by another cursor */
      if (cursor->inx < 1) cursor->inx = 1;                       /* underflow caused by access after init_cursor */

      ke   = calc_entry (kp, type, cursor->inx);
      data = ke->data;

      if (keyval != NULL) mem_move (keyval, ke->key, keysize (type, ke->key));
    } /* if, if */

  return (data);
} /* read_cursor */

/*****************************************************************************/

GLOBAL BOOL WINAPI test_cursor (cursor, dir, keyval)
LPCURSOR cursor;
SHORT    dir;
HPVOID   keyval;

{
  BOOL  found, more, wild;
  SHORT type, cmp, len;
  INT   i;
  CHAR  saveval [MAX_KEYSIZE];
  CHAR  cursval [MAX_KEYSIZE];
  CHAR  shortval [MAX_KEYSIZE];
  CHAR  key [MAX_KEYSIZE];

  found = FALSE;
  wild  = FALSE;
  type  = cursor->type;

  if (cursor->pagenum != 0)
  {
    mem_move (saveval, keyval, keysize (type, keyval));

    if (HASWILD (type))
    {
      i = strlen (saveval) - 1;

      while ((i >= 0) && (saveval [i] == ' ')) i--;

      if ((i > 0) && (saveval [i] == WILD_CHARS))       /* cut off spaces */
        saveval [i + 1] = EOS;

      strcpy (key, saveval);                            /* only for wildcard search */

      wild = is_wild (dir, saveval, shortval);
    } /* if */

    more = move_cursor (cursor, (LONG)dir);
    if (more) more = read_cursor (cursor, cursval) != 0;

    if (more)
      if (! wild)                                       /* regular compare */
	found = cmp_vals (type, keyval, cursval) == 0;
      else
      {
        len   = (SHORT)strlen (shortval);
        type |= USE_WILDCARD;                           /* use wildcards for string compare */

        while (more)
        {
          cmp   = cmp_vals (type, key, cursval);
          found = cmp == 0;
          more  = ! found;

          if (more)
	  {
            cursval [len] = EOS;                        /* first 'len' digits */
            cmp           = cmp_vals (type, shortval, cursval);
            more          = (cmp == 0) && move_cursor (cursor, (LONG)dir) && (read_cursor (cursor, cursval) != 0);
          } /* if */
        } /* while */
      } /* else, if */
  } /* if */

  return (found);
} /* test_cursor */

/*****************************************************************************/

GLOBAL LONG WINAPI locate_addr (cursor, dir, addr)
LPCURSOR cursor;
SHORT    dir;
LONG     addr;

{
  LONG      moved;
  BOOL      ok;
  PAGE      page;
  KEY_PAGE  *kp;
  KEY_ENTRY *ke;

  moved = 0;
  ok    = cursor->pagenum != 0;
  kp    = (KEY_PAGE *)page;

  if (ok) ok = read_vpage (cursor->tree, cursor->pagenum, page, 0, PAGESIZE);

  while (ok)
  {
    ke = calc_entry (kp, cursor->type, cursor->inx);
    ok = ke->data != addr;

    if (ok)
      if (dir == ASCENDING)
      {
        cursor->inx++;
        moved++;

        if (cursor->inx > kp->num_keys)         /* read next page */
          if (kp->next == 0)                    /* page doesn't exist */
          {
            ok          = FALSE;
            cursor->inx = kp->num_keys;
          } /* if */
          else
          {
            cursor->pagenum = kp->next;
	    ok              = read_vpage (cursor->tree, cursor->pagenum, page, 0, PAGESIZE);
            cursor->inx     = 1;
          } /* else, if */
      } /* if */
      else
      {
        cursor->inx--;
        moved--;

        if (cursor->inx == 0)                   /* read prev page */
          if (kp->prev == 0)                    /* page doesn't exist */
          {
            ok          = FALSE;
	    cursor->inx = 1;
          } /* if */
          else
          {
            cursor->pagenum = kp->prev;
            ok              = read_vpage (cursor->tree, cursor->pagenum, page, 0, PAGESIZE);
            cursor->inx     = kp->num_keys;
          } /* else, if */
      } /* else, if */
  } /* while */

  return (moved);
} /* locate_addr */

/*****************************************************************************/

GLOBAL BOOL WINAPI is_firstlast (cursor, first)
LPCURSOR cursor;
BOOL     first;

{
  BOOL     ok, ret;
  PAGE     page;
  KEY_PAGE *kp;

  ret = TRUE;
  ok  = cursor->pagenum != 0;
  kp  = (KEY_PAGE *)page;

  if (ok) ok = read_vpage (cursor->tree, cursor->pagenum, page, 0, PAGESIZE);

  if (ok)
    if (first)
      ret = (cursor->inx <= 1) && (kp->prev == 0);
    else
      ret = (cursor->inx >= kp->num_keys) && (kp->next == 0);

  return (ret);
} /* is_firstlast */

/*****************************************************************************/

GLOBAL BOOL WINAPI kill_virtual (tree)
LPTREEINF tree;

{
  REG VPAGEINFP v;
  REG LONG      num_vpages, i;

  num_vpages  = tree->num_vpages;
  v           = tree->vpageinf;
  tree->vtime = 0;

  for (i = 0; i < num_vpages; i++, v++)
  {
    v->time    = 0;
    v->dirty   = FALSE;
    v->pagenum = FAILURE;
  } /* for */

  return (read_pg0 (tree));             /* else inconsistence */
} /* kill_virtual */

/*****************************************************************************/

GLOBAL BOOL WINAPI flush_virtual (tree)
LPTREEINF tree;

{
  REG VPAGEINFP v;
  REG LONG      num_vpages, i;
  REG BOOL      ok;
  REG KEY_PAGE *kp;

  num_vpages = tree->num_vpages;
  v          = tree->vpageinf;
  ok         = TRUE;

  for (i = 0; ok && (i < num_vpages); i++, v++)
    if (v->dirty)                       /* need to save modified pages only */
    {
      kp       = (KEY_PAGE *)v->vpage;
      ok       = write_page (tree, v->pagenum, v->vpage, 0, (SHORT)((v->pagenum == 0) ? (SHORT)PAGESIZE : kp->size));
      v->dirty = FALSE;
    } /* if, for */

  return (ok);
} /* flush_virtual */

/*****************************************************************************/

GLOBAL BOOL WINAPI kill_tree (tree, root)
LPTREEINF tree;
LONG      root;

{
  BOOL     ok;
  PAGE     page;
  KEY_PAGE *kp;

  if (root == 0)
    ok = TRUE;                                          /* yes, I'm ready */
  else
  {
    kp = (KEY_PAGE *)page;
    ok = read_vpage (tree, root, page, 0, PAGESIZE);

    if (ok)
    {
      ok = kill_tree (tree, kp->key_entry->pointer);    /* kill left subtree */

      while (ok && (root != 0))                         /* kill all pages to the right */
      {
        release_page (tree, root);
        root = kp->next;
        if (root != 0) ok = read_vpage (tree, root, page, 0, PAGESIZE);
      } /* while */
    } /* if */
  } /* if */

  return (ok);
} /* kill_tree */

/*****************************************************************************/

GLOBAL BOOL WINAPI expand_tree (tree, size)
LPTREEINF tree;
LONG      size;

{
  BOOL ok;
  LONG pages;

  ok    = TRUE;
  pages = (size * 1024L) / PAGESIZE - tree->page0.num_pages;

  if (pages > 0)
  {
    ok                    = expand_file (tree->handle, (pages * PAGESIZE) / 1024L);
    tree->page0.num_pages = file_seek (tree->handle, 0L, SEEK_END) / PAGESIZE;
    ok                    = write_pg0 (tree) && ok;
  } /* if */

  return (ok);
} /* expand_tree */

/*****************************************************************************/

LOCAL BOOL new_root (tree, kd, entry)
LPTREEINF tree;
HPKEY_DFN kd;
ENTRY     *entry;

{
  BOOL      ok;
  KEY_PAGE  *kp;
  KEY_ENTRY *ke;
  LONG      pagenum;
  PAGE      page;

  pagenum = new_page (tree);
  ok      = pagenum != FAILURE;

  if (ok)
  {
    mem_set (page, 0, PAGESIZE);

    kp = (KEY_PAGE *)page;
    ke = kp->key_entry;

    kp->num_keys = 0;
    kp->size     = (SHORT)(SIZE_KEY_PAGE + SIZE_KEY_ENTRY + keysize (kd->type, ke->key));
    ke->pointer  = kd->root;

    ins_entry (kp, entry, kd->type, kp->size);

    kd->root = pagenum;
    ok       = write_vpage (tree, pagenum, kp, 0, kp->size);
  } /* if */

  return (ok);
} /* new_root */

/*****************************************************************************/

LOCAL BOOL to_insert (tree, pagenum, entry, type, keyval, data)
LPTREEINF tree;
LONG      pagenum;
ENTRY     *entry;
SHORT     type;
HPVOID    keyval;
LONG      data;

{
  BOOL      to_ins, ok;
  PAGE      page;
  KEY_PAGE  *kp;
  KEY_ENTRY *ke;
  KEY_POS   pos;

  ok = to_ins = TRUE;

  if (pagenum == 0)                             /* no root or leaf */
  {
    entry->pointer = 0;
    entry->data    = data;
    mem_move (entry->key, keyval, keysize (type, keyval));
  } /* if */
  else
  {
    ok = read_vpage (tree, pagenum, page, 0, PAGESIZE);

    if (ok)
    {
      kp         = (KEY_PAGE *)page;
      pos.tree   = tree;
      pos.type   = type;
      pos.keyval = keyval;
      pos.data   = data;
      pos.dir    = WH_INS;

      search_page (kp, &pos);                   /* where in page to place */

      ke     = (KEY_ENTRY *)(page + pos.offset);
      to_ins = to_insert (tree, ke->pointer, entry, type, keyval, data);

      if (to_ins)
      {
        if (ke->pointer == 0) corr_cursors (tree, kp, pagenum, 0L, (SHORT)(pos.inx + 1), 1);

        if (fits (kp, entry, type))             /* into same page */
        {
          pos.offset += (SHORT)(SIZE_KEY_ENTRY + keysize (type, ke->key));  /* to the right */
          ins_entry (kp, entry, type, pos.offset);
          to_ins = FALSE;
	} /* if */
        else
          to_ins = split (tree, pagenum, kp, entry, &pos);

        ok = write_vpage (tree, pagenum, kp, 0, kp->size);
      } /* if */
    } /* if */
  } /* else */

  return (ok && to_ins);
} /* to_insert */

/*****************************************************************************/

LOCAL BOOL split (tree, pagenum, kp, entry, pos)
LPTREEINF tree;
LONG      pagenum;
KEY_PAGE  *kp;
ENTRY     *entry;
KEY_POS   *pos;

{
  BOOL      ok, is_leaf;
  PAGE      page;
  LONG      splitnum;
  KEY_PAGE  *kpr;
  KEY_ENTRY *ke;
  SHORT     type, offset, inx;
  SHORT     half, sizel, sizer;

  splitnum = new_page (tree);
  ok       = splitnum != FAILURE;

  if (ok)
  {
    mem_set (page, 0, PAGESIZE);

    type     = pos->type;
    offset   = pos->offset;
    inx      = pos->inx;
    kpr      = (KEY_PAGE *)page;
    ke       = (KEY_ENTRY *)((CHAR *)kp + offset);
    offset  += (SHORT)(SIZE_KEY_ENTRY + keysize (type, ke->key));   /* to the right */
    ke       = kp->key_entry;
    is_leaf  = ke->pointer == 0;
    half     = 0;

    if ((kp->prev == 0) && (inx == 0))                  /* insert leftmost key */
    {
      half = (SHORT)(1 + (is_leaf ? 0 : 1));            /* key will be inserted into left page later, greatest key however will be deleted in non-leafs */
      ke   = calc_entry (kp, type, half);
    } /* if */
    else
      if ((kp->next == 0) && (inx == kp->num_keys))     /* insert rightmost key */
      {
        half = kp->num_keys;                            /* key will be inserted into right page later */
        ke   = calc_entry (kp, type, half);
      } /* if */
      else
        while ((LONG)ke - (LONG)kp->key_entry < (LONG)(kp->size - SIZE_KEY_PAGE) / 2)
        {
          half++;
          ke = next_entry (ke, type);
        } /* while, else, else */

    if (inx >= half)
    {
      half++;                                   /* entry to raise always in left page */
      inx++;                                    /* cheat condition "inx < half" below */
      ke = next_entry (ke, type);
    } /* while */

    sizel = (SHORT)((LONG)ke - (LONG)kp);
    sizer = (SHORT)(kp->size - sizel);

    kpr->size     = (SHORT)(SIZE_KEY_PAGE + SIZE_KEY_ENTRY + keysize (type, kpr->key_entry->key));
    kpr->num_keys = (SHORT)(kp->num_keys - half + 1);
    kp->size      = sizel;
    kp->num_keys  = (SHORT)(half - 1);

    mem_move ((CHAR *)kpr + kpr->size, ke, sizer);
    mem_set ((CHAR *)kp + kp->size, 0, (USHORT)(PAGESIZE - kp->size));

    kpr->size += sizer;

    if (inx < half)
      ins_entry (kp, entry, type, offset);              /* insert into left page */
    else
    {
      offset = (SHORT)(offset - sizel + SIZE_KEY_PAGE + SIZE_KEY_ENTRY + keysize (type, kpr->key_entry->key));
      ins_entry (kpr, entry, type, offset);             /* insert into right page */
    } /* else */

    ke = calc_entry (kp, type, kp->num_keys);           /* last entry */

    mem_move (entry, ke, (SHORT)(SIZE_KEY_ENTRY + keysize (type, ke->key)));  /* entry to raise */

    if (! is_leaf)
      del_entry (kp, type, calc_offset (kp, type, kp->num_keys));        /* delete largest entry */
    else
      corr_cursors (tree, kp, pagenum, splitnum, kp->num_keys, 2);

    kpr->key_entry->pointer = entry->pointer;
    entry->pointer          = splitnum;

    kpr->next = kp->next;
    kpr->prev = pagenum;
    kp->next  = splitnum;

    ok = write_vpage (tree, splitnum, kpr, 0, kpr->size);

    if (ok && (kpr->next != 0))
    {
      offset = (SHORT)OFFSET (KEY_PAGE, prev);
      ok     = write_vpage (tree, kpr->next, &splitnum, offset, sizeof (LONG));
    } /* if */
  } /* if */

  return (ok);
} /* split */

/*****************************************************************************/

LOCAL BOOL to_delete (tree, pagenum, type, keyval, data, entry, oe, over, corr)
LPTREEINF tree;
LONG      pagenum;
SHORT     type;
HPVOID    keyval;
LONG      data;
ENTRY     *entry, *oe;
BOOL      *over;
LONG      *corr;

{
  BOOL      to_del, ok, found;
  PAGE      page;
  KEY_PAGE  *kp;
  KEY_ENTRY *ke;
  KEY_POS   pos;
  LONG      pointer, i;

  ok     = TRUE;
  to_del = FALSE;
  *over  = FALSE;
  *corr  = 0;

  if (pagenum == 0)
    set_dberror (DB_TNOKEY);
  else
  {
    ok = read_vpage (tree, pagenum, page, 0, PAGESIZE);

    if (ok)
    {
      kp          = (KEY_PAGE *)page;
      pos.tree    = tree;
      pos.type    = type;
      pos.keyval  = keyval;
      pos.data    = data;                       /* specific data to search */
      pos.dir     = WH_LT;
      pos.pagenum = pagenum;

      found = search_page (kp, &pos);           /* where in page */

      pagenum = pos.pagenum;                    /* could have been changed by search_page */
      ke      = (KEY_ENTRY *)(page + pos.offset);
      pointer = ke->pointer;

      if (found && (pointer == 0))              /* it's a leaf */
      {
        corr_cursors (tree, kp, pagenum, kp->next, pos.inx, 3);

        *corr = pos.corr;

        del_entry (kp, type, pos.offset);

        ke = calc_entry (kp, type, kp->num_keys);       /* get greatest key */
        copy_entry (entry, ke, type);                   /* save greatest key */

        to_del = underflow (kp);
        ok     = write_vpage (tree, pagenum, kp, 0, kp->size);
      } /* if */
      else
      {
        to_del   = to_delete (tree, pointer, type, keyval, data, entry, oe, over, corr);
        i        = *corr;
        pos.corr = 0;

        while (ok && (i-- > 0))                         /* corrections to be made */
        {
          pos.inx++;

          if ((pos.inx > kp->num_keys) && (kp->next != 0)) /* read next page */
          {
            pos.pagenum = pagenum = kp->next;
            pos.inx     = 0;
            pos.corr++;                                 /* one leaf to the right */

            ok = read_vpage (tree, pagenum, kp, 0, PAGESIZE);
          } /* if */
        } /* while */

        *corr      = pos.corr;
        pos.offset = calc_offset (kp, type, pos.inx);
        ke         = (KEY_ENTRY *)(page + pos.offset);
        pointer    = ke->pointer;

        if (to_del)
        {
          to_del = compensate (tree, kp, pointer, &pos, entry, oe, over);
          ok     = write_vpage (tree, pagenum, kp, 0, kp->size);
        } /* if */
        else
          if (found && (get_dberror () == SUCCESS) && (pos.inx < kp->num_keys)) /* replace greatest key */
          {
            pos.offset = calc_offset (kp, type, ++pos.inx); /* was at prev inx */
            ke         = next_entry (ke, type);

            if (ke->data == data)                       /* exchange only one time */
            {
              entry->pointer = ke->pointer;             /* save old pointer */

              if (*over)                                /* save overflow entry from recurse */
              {
                xchng_entry (kp, pos.inx, entry, type);
                ok = write_vpage (tree, pagenum, kp, 0, kp->size);
              } /* if */
              else
              {
                del_entry (kp, type, pos.offset);
                copy_entry (oe, (KEY_ENTRY *)entry, type);
                *over = TRUE;
              } /* else */
            } /* if */

            pos.offset = calc_offset (kp, type, --pos.inx);     /* original inx */
          } /* if, else */

        if (*over)
        {
          *over = ! fits (kp, oe, type);

          if (*over)
            ok = split (tree, pagenum, kp, oe, &pos);
          else
          {
            pos.offset = calc_offset (kp, type, ++pos.inx);     /* insert key one pos to the right */
            ins_entry (kp, oe, type, pos.offset);
          } /* else */

          ok = write_vpage (tree, pagenum, kp, 0, kp->size);
        } /* if */
      } /* else */
    } /* if */
  } /* else */

  return (ok && to_del);
} /* to_delete */

/*****************************************************************************/

LOCAL BOOL compensate (tree, kp, pointer, pos, entry, oe, over)
LPTREEINF tree;
KEY_PAGE  *kp;
LONG      pointer;
ENTRY     *entry;
KEY_POS   *pos;
ENTRY     *oe;
BOOL      *over;

{
  BOOL      under, ok, is_leaf;
  PAGE      page1, page2;
  KEY_PAGE  *kpl, *kpr;
  KEY_ENTRY *kel, *ker, *ke;
  ENTRY     save;
  LONG      neighbour;
  SHORT     offset, size, more, i;
  SHORT     type, inx;

  under  = FALSE;
  *over  = FALSE;
  ok     = read_vpage (tree, pointer, page1, 0, PAGESIZE);
  offset = (SHORT)SIZE_KEY_PAGE;                       /* offset zero key */
  type   = pos->type;
  inx    = pos->inx;
  size   = 0;

  if (ok)
    if (inx < kp->num_keys)                     /* choose right neighbour */
    {
      inx++;
      kpl       = (KEY_PAGE *)page1;
      kpr       = (KEY_PAGE *)page2;
      neighbour = kpl->next;
      is_leaf   = kpl->key_entry->pointer == 0;
      ke        = calc_entry (kp, type, inx);

      if (read_vpage (tree, neighbour, page2, 0, PAGESIZE))
      {
        more = (SHORT)((too_much (kpr, type) + 1) / 2);  /* round */

        if (more > 0)                           /* merge with right */
        {
          offset += (SHORT)(SIZE_KEY_ENTRY + keysize (type, kpr->key_entry->key)); /* offset first key */
          ker     = calc_entry (kpr, type, 1);

          if (is_leaf)
          {
            corr_cursors (tree, kpl, neighbour, pointer, more, 5);

            for (i = 0; (i < more) && (fits (kpl, (ENTRY *)ker, type)); i++)
            {
              ins_entry (kpl, (ENTRY *)ker, type, kpl->size);
              del_entry (kpr, type, offset);
            } /* for */

            kel = calc_entry (kpl, type, kpl->num_keys); /* get greatest key */
            copy_entry (&save, kel, type);
            save.pointer = ke->pointer;                 /* save pointer */
          }
          else
          {
            copy_entry (&save, ker, type);              /* save first entry */

            ke->pointer = kpr->key_entry->pointer;
            ins_entry (kpl, (ENTRY *)ke, type, kpl->size); /* move father */

            for (i = 1; (i <= more - 1) && (fits (kpl, (ENTRY *)ker, type)); i++)
            {
              ins_entry (kpl, (ENTRY *)ker, type, kpl->size);
              del_entry (kpr, type, offset);
              copy_entry (&save, ker, type);            /* save first entry */
            } /* for */

            del_entry (kpr, type, offset);              /* delete entry 1 of right page */
            kpr->key_entry->pointer = save.pointer;     /* restore zero ptr of right page */
            save.pointer            = neighbour;
          } /* else */

          offset = calc_offset (kp, type, inx);
          del_entry (kp, type, offset);                 /* delete father */
          copy_entry (oe, (KEY_ENTRY *)&save, type);    /* save new father */
          *over = TRUE;                                 /* restore via overflow */

          ok = write_vpage (tree, pointer, kpl, 0, kpl->size);
          ok = ok && write_vpage (tree, neighbour, kpr, 0, kpr->size);
        } /* if */
        else                                    /* unite with right */
        {
          if (is_leaf)
          {
            i       = 1;                        /* start from first key */
            offset += (SHORT)(SIZE_KEY_ENTRY + keysize (type, kpr->key_entry->key)); /* offset first key */
          } /* if */
          else
          {
            i = 0;                              /* start from leftmost key */

            if (pos->data == ke->data)          /* greatest key deletetd */
              copy_entry (&save, (KEY_ENTRY *)entry, type);
            else
              copy_entry (&save, ke, type);

            save.pointer = kpr->key_entry->pointer;     /* save pointer */
            xchng_entry (kpr, 0, &save, type);          /* copy to inx 0 */
            if (VARLEN (type)) size = MAX_KEYSIZE;      /* space for at least one large key */
          } /* else */

          if (kpl->size + kpr->size - offset <= PAGESIZE - size) /* no large father */
          {
            if (is_leaf) corr_cursors (tree, kpl, neighbour, pointer, kpl->num_keys, 4);

            kel  = calc_entry (kpl, type, (SHORT)(kpl->num_keys + 1));
            ker  = calc_entry (kpr, type, i);
            size = (SHORT)(kpr->size - offset);

            mem_move (kel, ker, size);
            kpl->num_keys += (SHORT)(kpr->num_keys + 1 - i);
            kpl->size     += size;
            kpl->next      = kpr->next;         /* link pages */

            if (kpr->next != 0)
              ok = write_vpage (tree, kpr->next, &pointer, OFFSET (KEY_PAGE, prev), sizeof (LONG));

            del_entry (kp, type, calc_offset (kp, type, inx));

            ok = ok && write_vpage (tree, pointer, kpl, 0, kpl->size);
            release_page (tree, neighbour);
            under = underflow (kp);
          } /* if */
        } /* else */
      } /* if */
    } /* if */
    else                                        /* choose left neighbour */
    {
      kpr       = (KEY_PAGE *)page1;
      kpl       = (KEY_PAGE *)page2;
      neighbour = kpr->prev;
      is_leaf   = kpr->key_entry->pointer == 0;
      ke        = calc_entry (kp, type, inx);

      if (read_vpage (tree, neighbour, page2, 0, PAGESIZE))
      {
        more = (SHORT)((too_much (kpl, type) + 1) / 2);  /* round */

        if (more > 0)                           /* merge with left */
        {
          offset += (SHORT)(SIZE_KEY_ENTRY + keysize (type, kpr->key_entry->key)); /* offset first key */
          kel     = calc_entry (kpl, type, kpl->num_keys);

          if (is_leaf)
          {
            corr_cursors (tree, kpl, neighbour, pointer, more, 6);

            for (i = 0; (i < more) && (fits (kpr, (ENTRY *)kel, type)); i++)
            {
              ins_entry (kpr, (ENTRY *)kel, type, offset);
              del_entry (kpl, type, calc_offset (kpl, type, kpl->num_keys));
              kel = calc_entry (kpl, type, kpl->num_keys);
            } /* for */

            kel = calc_entry (kpl, type, kpl->num_keys); /* get greatest key */
            copy_entry (&save, kel, type);
            save.pointer = ke->pointer;                  /* save pointer */
          }
          else
          {
            ke->pointer = kpr->key_entry->pointer;
            ins_entry (kpr, (ENTRY *)ke, type, offset);  /* move father */
            kel = calc_entry (kpl, type, kpl->num_keys); /* get greatest key */
            copy_entry (&save, kel, type);

            for (i = 1; (i <= more - 1) && (fits (kpr, (ENTRY *)kel, type)); i++)
            {
              ins_entry (kpr, (ENTRY *)kel, type, offset);
              del_entry (kpl, type, calc_offset (kpl, type, kpl->num_keys));
              kel = calc_entry (kpl, type, kpl->num_keys); /* get greatest key */
              copy_entry (&save, kel, type);
            } /* for */

            kpr->key_entry->pointer = save.pointer;     /* restore zero ptr of right page */
            del_entry (kpl, type, calc_offset (kpl, type, kpl->num_keys));
            save.pointer = pointer;
          } /* else */

          offset = calc_offset (kp, type, inx);
          del_entry (kp, type, offset);                 /* delete father */
          copy_entry (oe, (KEY_ENTRY *)&save, type);    /* save new father */
          pos->inx--;
          pos->offset = calc_offset (kp, type, pos->inx);
          *over = TRUE;                                 /* restore via overflow */

          ok = write_vpage (tree, neighbour, kpl, 0, kpl->size);
          ok = ok && write_vpage (tree, pointer, kpr, 0, kpr->size);
        } /* if */
        else                                    /* unite with left */
        {
          if (is_leaf)
          {
            i       = 1;                        /* start from first cey */
            offset += (SHORT)(SIZE_KEY_ENTRY + keysize (type, kpr->key_entry->key)); /* offset first key */
          } /* if */
          else
          {
            i = 0;                              /* start from leftmost key */
            copy_entry (&save, ke, type);
            save.pointer = kpr->key_entry->pointer;     /* save pointer */
            xchng_entry (kpr, 0, &save, type);          /* copy to inx 0 */
            if (VARLEN (type)) size = MAX_KEYSIZE;      /* space for at least one large key */
          } /* else */

          if (kpl->size + kpr->size - offset <= PAGESIZE - size) /* no large father */
          {
            if (is_leaf) corr_cursors (tree, kpl, pointer, neighbour, kpl->num_keys, 4);

            kel  = calc_entry (kpl, type, (SHORT)(kpl->num_keys + 1));
            ker  = calc_entry (kpr, type, i);
            size = (SHORT)(kpr->size - offset);

            mem_move (kel, ker, size);
            kpl->num_keys += (SHORT)(kpr->num_keys + 1 - i);
            kpl->size     += size;
            kpl->next      = kpr->next;         /* link pages */

            if (kpr->next != 0)
              ok = write_vpage (tree, kpr->next, &neighbour, OFFSET (KEY_PAGE, prev), sizeof (LONG));

            del_entry (kp, type, calc_offset (kp, type, inx));

            ok = ok && write_vpage (tree, neighbour, kpl, 0, kpl->size);
            release_page (tree, pointer);
            under = underflow (kp);
          } /* if */
        } /* else */
      } /* if */
    } /* else, if */

  return (ok && under);
} /* compensate */

/*****************************************************************************/

LOCAL BOOL fits (kp, entry, type)
KEY_PAGE *kp;
ENTRY    *entry;
SHORT    type;

{
  SHORT size;

  if (VARLEN (type) && (kp->key_entry->pointer != 0))
    size = MAX_KEYSIZE;         /* space for at least one large key */
  else
    size = 0;

  return ((SIZE_T)(kp->size + SIZE_KEY_ENTRY + keysize (type, entry->key)) <= (SIZE_T)(PAGESIZE - size));
} /* fits */

/*****************************************************************************/

LOCAL BOOL underflow (kp)
KEY_PAGE *kp;

{
  return (kp->size - SIZE_KEY_PAGE < (PAGESIZE - SIZE_KEY_PAGE) / 2);
} /* underflow */

/*****************************************************************************/

LOCAL SHORT too_much (kp, type)
KEY_PAGE *kp;
SHORT    type;

{
  REG SHORT      i;
  REG KEY_ENTRY *ke;

  i  = 0;
  ke = kp->key_entry;

  while ((i < kp->num_keys) && ((LONG)ke - (LONG)kp->key_entry + keysize (type, ke->key) < (LONG)(PAGESIZE - SIZE_KEY_PAGE) / 2))
  {
    i++;
    ke = next_entry (ke, type);
  } /* while */

  return ((SHORT)(kp->num_keys - i));
} /* too_much */

/*****************************************************************************/

LOCAL VOID corr_cursors (tree, kp, pagenum, corrnum, inx, which)
LPTREEINF tree;
KEY_PAGE  *kp;
LONG      pagenum;
LONG      corrnum;
SHORT     inx;
SHORT     which;

{
  SHORT    i;
  LPCURSOR cursor;

  for (i = 0, cursor = tree->cursors; i < tree->num_cursors; i++, cursor++)
    if ((cursor->tree != NULL) && (cursor->pagenum != 0) &&
        ((cursor->pagenum == pagenum) || (cursor->pagenum == corrnum) && (which == 6)))
      switch (which)
      {
        case 1 : if (cursor->inx >= inx) cursor->inx++;         /* cursor right of insertion point */
                 break;
        case 2 : if (cursor->inx > inx)                         /* cursor in right (splitted) page */
                 {
                   cursor->pagenum  = corrnum;
                   cursor->inx     -= inx;
                 } /* if */
                 break;
        case 3 : if (cursor->inx > inx)
                   cursor->inx--;
                 else
                   if (cursor->inx == inx)              /* key of cursor deleted */
                   {
                     cursor->flags |= CURS_DELETED;

                     if (inx >= kp->num_keys)           /* last key of page */
                       if (corrnum == 0)                /* last page */
                       {
                         cursor->inx = kp->num_keys;

                         if (cursor->inx == 1) cursor->pagenum = 0; /* last key of last page */
                       } /* if */
                       else                             /* next key on next page */
                       {
                         cursor->pagenum = corrnum;
                         cursor->inx     = 1;
                       } /* else, if */
                   } /* if, else */
                 break;
        case 4 : cursor->pagenum  = corrnum;            /* unite with right or left page */
                 cursor->inx     += inx;
                 break;
        case 5 : if (cursor->inx <= inx)                /* merge with right page */
                 {
                   cursor->pagenum  = corrnum;
                   cursor->inx     += kp->num_keys;
                 } /* if */
                 else
                   cursor->inx -= inx;
                 break;
        case 6 : if (cursor->pagenum == corrnum)        /* merge with left page */
                   cursor->inx += inx;
                 else
                   if (cursor->inx > kp->num_keys - inx)
                   {
                     cursor->pagenum  = corrnum;
                     cursor->inx     -= (SHORT)(kp->num_keys - inx);
                   } /* if, else */
                 break;
      } /* switch, if, for */
} /* corr_cursors */

/*****************************************************************************/

LOCAL BOOL is_wild (dir, saveval, shortval)
SHORT dir;
CHAR  *saveval, *shortval;

{
  BOOL wild;
  CHAR *p;
  CHAR s [MAX_KEYSIZE];

  wild = FALSE;

#if ANSI
  strcpy (s, "   ");
  s [0] = WILD_CHARS;
  s [1] = WILD_CHAR;
  s [2] = WILD_DIGIT;

  p = strpbrk (saveval, s);
  if (p != NULL)
  {
    *p   = EOS;
    wild = TRUE;
  } /* if */
#else
  p = strchr (saveval, WILD_CHARS);
  if (p != NULL)
  {
    *p   = EOS;
    wild = TRUE;
  } /* if */

  p = strchr (saveval, WILD_CHAR);
  if (p != NULL)
  {
    *p   = EOS;
    wild = TRUE;
  } /* if */

  p = strchr (saveval, WILD_DIGIT);
  if (p != NULL)
  {
    *p   = EOS;
    wild = TRUE;
  } /* if */
#endif

  if (wild)
  {
    strcpy (shortval, saveval);

    if (dir == DESCENDING)
    {
      strcat (saveval, " ");
      saveval [strlen (saveval) - 1] = (CHAR)(UCHAR)0xFF;    /* highest char */
    } /* if */
  } /* if */

  return (wild);
} /* is_wild */

/*****************************************************************************/

LOCAL BOOL search_tree (pagenum, pos)
LONG    pagenum;
KEY_POS *pos;

{
  BOOL      found, more;
  PAGE      page;
  KEY_ENTRY *entry;

  found        = FALSE;
  pos->pagenum = pagenum;
  pos->inx     = 0;
  pos->offset  = 0;

  if (pagenum != 0)                                     /* root wasn't zero */
  {
    more = TRUE;

    while (more)                                        /* iteration will work also */
    {
      more = read_vpage (pos->tree, pos->pagenum, page, 0, PAGESIZE);

      if (! more) found = FALSE;                        /* error in reading */

      if (more)
      {
        found = search_page ((KEY_PAGE *)page, pos);
        entry = (KEY_ENTRY *)(page + pos->offset);
        more  = entry->pointer != 0;                    /* no leaf */

        if (more) pos->pagenum = entry->pointer;        /* save last pagenum */
      } /* if */
    } /* while */
  } /* if */

  return (found);
} /* search_tree */

/*****************************************************************************/

LOCAL BOOL search_page (kp, pos)
KEY_PAGE *kp;
KEY_POS  *pos;

{
  if (VARLEN (pos->type))
    return (search_lin (kp, pos));
  else
    return (search_bin (kp, pos));
} /* search_page */

/*****************************************************************************/

LOCAL BOOL search_lin (kp, pos)
KEY_PAGE *kp;
KEY_POS  *pos;

{
  CHAR  *entry;
  SHORT num_keys, i, offset, cmp, size;
  BOOL  is_leaf;

  entry     = (CHAR *)kp->key_entry;
  num_keys  = kp->num_keys;
  offset    = (SHORT)SIZE_KEY_PAGE;
  i         = 0;
  cmp       = 0;
  is_leaf   = (kp->key_entry->pointer == 0);
  pos->corr = 0;

  do
  {
    i++;
    size    = (SHORT)(SIZE_KEY_ENTRY + keysize (pos->type, ((KEY_ENTRY *)entry)->key));
    offset += size;                                     /* increase offset */
    entry  += size;                                     /* increase entry */
    if (i <= num_keys) cmp = cmp_vals (pos->type, pos->keyval, ((KEY_ENTRY *)entry)->key);
  } while ((i <= num_keys) && (cmp > 0));               /* still greater */

  if (cmp == 0)                                         /* probably more than one key */
  {
    if ((pos->dir == WH_RT) || (pos->dir == WH_INS))
    {
      while ((i <= num_keys) && (cmp == 0) && ((pos->data == 0) || (pos->data > ((KEY_ENTRY *)entry)->data)))
      {
        i++;
        size    = (SHORT)(SIZE_KEY_ENTRY + keysize (pos->type, ((KEY_ENTRY *)entry)->key));
        offset += size;                                 /* increase offset */
        entry  += size;                                 /* increase entry */
        if (i <= num_keys) cmp = cmp_vals (pos->type, pos->keyval, ((KEY_ENTRY *)entry)->key);
      } /* while */
    } /* if */

    cmp = 0;                                            /* correct cmp */
  } /* if */

  if (is_leaf && (pos->dir != WH_INS) && (pos->data != 0) && (cmp == 0))
  {
    while ((i <= num_keys) && (cmp == 0) && (pos->data != ((KEY_ENTRY *)entry)->data))
    {
      i++;
      size    = (SHORT)(SIZE_KEY_ENTRY + keysize (pos->type, ((KEY_ENTRY *)entry)->key));
      offset += size;                                   /* increase offset */
      entry  += size;                                   /* increase entry */
      if (i <= num_keys) cmp = cmp_vals (pos->type, pos->keyval, ((KEY_ENTRY *)entry)->key);

      if ((i > num_keys) && (kp->next != 0))            /* read next page */
      {
        pos->pagenum = kp->next;
        pos->corr++;                                    /* one leaf to the right */

        if (read_vpage (pos->tree, kp->next, kp, 0, PAGESIZE))
        {
          entry    = (CHAR *)kp->key_entry;
          num_keys = kp->num_keys;
          offset   = (SHORT)SIZE_KEY_PAGE;
          i        = 0;
        } /* if */
      } /* if */
    } /* while */

    if (i > num_keys) cmp = 1;                          /* specific data not found */
  } /* if */
  else
    if (! is_leaf ||
        (pos->dir == WH_RT) ||
        (pos->dir == WH_INS) ||
        (pos->dir == WH_LT) && (cmp != 0) && (i > num_keys))    /* behind last */
    {
      i--;
      offset -= size;
    } /* if, else */

  if (is_leaf && (pos->dir == WH_RT) && (i == 0))
    if (kp->prev == 0)
    {
      i++;
      offset += size;
    } /* if */
    else
    {
      pos->pagenum = kp->prev;

      if (read_vpage (pos->tree, kp->prev, kp, 0, PAGESIZE))    /* read last item of prev page */
      {
        i      = kp->num_keys;
        offset = calc_offset (kp, pos->type, i);
        entry  = (CHAR *)kp + offset;
        cmp    = cmp_vals (pos->type, pos->keyval, ((KEY_ENTRY *)entry)->key);
      } /* if */
    } /* else, if */

  pos->inx    = i;
  pos->offset = offset;

  return (cmp == 0);
} /* search_lin */

/*****************************************************************************/

LOCAL BOOL search_bin (kp, pos)
KEY_PAGE *kp;
KEY_POS  *pos;

{
  CHAR  *entry;
  SHORT num_keys, l, r, i, cmp, size;
  BOOL  is_leaf;

  num_keys  = kp->num_keys;
  l         = 0;
  r         = (SHORT)(num_keys + 1);
  size      = (SHORT)(SIZE_KEY_ENTRY + keysize (pos->type, pos->keyval));
  is_leaf   = (kp->key_entry->pointer == 0);
  pos->corr = 0;

  do
  {
    i     = (SHORT)((l + r) / 2);
    entry = (CHAR *)kp->key_entry + i * size;
    cmp   = cmp_vals (pos->type, pos->keyval, ((KEY_ENTRY *)entry)->key);

    if (cmp < 0)
      r = i;
    else
      l = i;
  } while ((l + 1 < r) && (cmp != 0));

  if (cmp != 0)
  {
    i      = (SHORT)(l + 1);                            /* one position to the right like linear search */
    entry += size;
  } /* if */

  if (cmp == 0)                                         /* probably more than one key */
  {
    if ((pos->dir == WH_RT) || (pos->dir == WH_INS))
    {
      if ((pos->dir == WH_INS) && (pos->data < ((KEY_ENTRY *)entry)->data))
      {
        while ((i > 0) && (cmp == 0) && ((pos->data == 0) || (pos->data < ((KEY_ENTRY *)entry)->data)))
        {
          i--;
          entry -= size;                                /* decrease entry */
          if (i > 0) cmp = cmp_vals (pos->type, pos->keyval, ((KEY_ENTRY *)entry)->key);
        } /* while */

        i++;                                            /* correct i */
        entry += size;
      } /* if */
      else
        while ((i <= num_keys) && (cmp == 0) && ((pos->data == 0) || (pos->data > ((KEY_ENTRY *)entry)->data)))
        {
          i++;
          entry += size;                                /* increase entry */
          if (i <= num_keys) cmp = cmp_vals (pos->type, pos->keyval, ((KEY_ENTRY *)entry)->key);
        } /* while, else */
    } /* if */
    else
    {
      while ((i > 0) && (cmp == 0))
      {
        i--;
        entry -= size;                                  /* decrease entry */
        if (i > 0) cmp = cmp_vals (pos->type, pos->keyval, ((KEY_ENTRY *)entry)->key);
      } /* while */

      i++;                                              /* correct i */
      entry += size;
    } /* else */

    cmp = 0;                                            /* correct cmp */
  } /* if */

  if (is_leaf && (pos->dir != WH_INS) && (pos->data != 0) && (cmp == 0))
  {
    while ((i <= num_keys) && (cmp == 0) && (pos->data != ((KEY_ENTRY *)entry)->data))
    {
      i++;
      entry += size;                                    /* increase entry */
      if (i <= num_keys) cmp = cmp_vals (pos->type, pos->keyval, ((KEY_ENTRY *)entry)->key);

      if ((i > num_keys) && (kp->next != 0))            /* read next page */
      {
        pos->pagenum = kp->next;
        pos->corr++;                                    /* one leaf to the right */

        if (read_vpage (pos->tree, kp->next, kp, 0, PAGESIZE))
        {
          entry    = (CHAR *)kp->key_entry;
          num_keys = kp->num_keys;
          i        = 0;
        } /* if */
      } /* if */
    } /* while */

    if (i > num_keys) cmp = 1;                          /* specific data not found */
  } /* if */
  else
    if (! is_leaf ||
        (pos->dir == WH_RT) ||
        (pos->dir == WH_INS) ||
        (pos->dir == WH_LT) && (cmp != 0) && (i > num_keys)) i--; /* behind last */

  if (is_leaf && (pos->dir == WH_RT) && (i == 0))
    if (kp->prev == 0)
      i++;
    else
    {
      pos->pagenum = kp->prev;

      if (read_vpage (pos->tree, kp->prev, kp, 0, PAGESIZE))    /* read last item of prev page */
      {
        i     = kp->num_keys;
        entry = (CHAR *)kp + SIZE_KEY_PAGE + i * size;
        cmp   = cmp_vals (pos->type, pos->keyval, ((KEY_ENTRY *)entry)->key);
      } /* if */
    } /* else, if */

  pos->inx    = i;
  pos->offset = (SHORT)(SIZE_KEY_PAGE + i * size);

  return (cmp == 0);
} /* search_bin */

/*****************************************************************************/

LOCAL VOID copy_entry (dst, src, type)
ENTRY     *dst;
KEY_ENTRY *src;
SHORT     type;

{
  mem_move (dst, src, (USHORT)(SIZE_KEY_ENTRY + keysize (type, src->key)));
} /* copy_entry */

/*****************************************************************************/

LOCAL VOID xchng_entry (kp, inx, entry, type)
KEY_PAGE *kp;
SHORT    inx;
ENTRY    *entry;
SHORT    type;

{
  SHORT offset;

  offset = calc_offset (kp, type, inx);

  del_entry (kp, type, offset);
  ins_entry (kp, entry, type, offset);
} /* xchng_entry */

/*****************************************************************************/

LOCAL VOID ins_entry (kp, entry, type, offset)
KEY_PAGE *kp;
ENTRY    *entry;
SHORT    type, offset;

{
  SHORT size;

  size = (SHORT)(SIZE_KEY_ENTRY + keysize (type, entry->key));

  mem_move ((CHAR *)kp + offset + size, (CHAR *)kp + offset, (USHORT)(kp->size - offset));
  mem_move ((CHAR *)kp + offset, entry, size);

  kp->num_keys++;
  kp->size += size;
} /* ins_entry */

/*****************************************************************************/

LOCAL VOID del_entry (kp, type, offset)
KEY_PAGE *kp;
SHORT    type, offset;

{
  SHORT     size;
  KEY_ENTRY *ke;

  ke   = (KEY_ENTRY *)((CHAR *)kp + offset);
  size = (SHORT)(SIZE_KEY_ENTRY + keysize (type, ke->key));

  mem_move (ke, (CHAR *)ke + size, (USHORT)(kp->size - offset - size));

  kp->num_keys--;
  kp->size -= size;

  mem_set ((CHAR *)kp + kp->size, 0, size);
} /* del_entry */

/*****************************************************************************/

LOCAL KEY_ENTRY *next_entry (ke, type)
KEY_ENTRY *ke;
SHORT     type;

{
  return ((KEY_ENTRY *)((CHAR *)ke + SIZE_KEY_ENTRY + keysize (type, ke->key)));
} /* next_entry */

/*****************************************************************************/

LOCAL KEY_ENTRY *calc_entry (kp, type, inx)
KEY_PAGE *kp;
SHORT    type, inx;

{
  REG KEY_ENTRY *ke;

  inx = (SHORT)(min (inx, kp->num_keys + 1));

  if (VARLEN (type))                                    /* var length */
  {
    ke = kp->key_entry;
    while (inx-- > 0) ke = next_entry (ke, type);
  } /* if */
  else                                                  /* fixed length */
    ke = (KEY_ENTRY *)((CHAR *)kp + SIZE_KEY_PAGE + inx * (SIZE_KEY_ENTRY + keysize (type, NULL)));

  return (ke);
} /* calc_entry */

/*****************************************************************************/

LOCAL SHORT calc_offset (kp, type, inx)
KEY_PAGE *kp;
SHORT    type, inx;

{
  KEY_ENTRY *ke;

  ke = calc_entry (kp, type, inx);
  return ((SHORT)((LONG)ke - (LONG)kp));
} /* calc_offset */

/*****************************************************************************/

LOCAL LONG new_page (tree)
LPTREEINF tree;

{
  BOOL  ok;
  LONG  pagenum, pages, next;
  PAGE0 *p;
  PAGE  page;

  ok = TRUE;
  p  = &tree->page0;
  p->used_pages++;

  if (p->rlsd_page == 0)                        /* no released pages */
  {
    pagenum = p->free_page++;                   /* use next free page */

    if (p->free_page > p->num_pages)
    {
      pages = INC_PAGES;
      mem_set (page, 0, PAGESIZE);

      while (pages--)
        ok = write_page (tree, p->num_pages++, page, 0, PAGESIZE); /* expand tree */
    } /* if */
  } /* if */
  else
  {
    pagenum      = p->rlsd_page;
    ok           = read_vpage (tree, pagenum, &next, 0, sizeof (next));
    p->rlsd_page = next;
  } /* else */

  ok = ok && write_pg0 (tree);

  return (ok ? pagenum : FAILURE);
} /* new_page */

/*****************************************************************************/

LOCAL BOOL release_page (tree, pagenum)
LPTREEINF tree;
LONG      pagenum;

{
  BOOL ok;
  LONG next;

  next                  = tree->page0.rlsd_page;
  tree->page0.rlsd_page = pagenum;
  tree->page0.used_pages--;

  ok = write_vpage (tree, pagenum, &next, 0, sizeof (next));

  return (ok && write_pg0 (tree));
} /* release_page */

/*****************************************************************************/

LOCAL BOOL read_pg0 (tree)
LPTREEINF tree;

{
  return (read_page (tree, 0L, &tree->page0, 0, sizeof (PAGE0))); /* physical read */
} /* read_pg0 */

/*****************************************************************************/

LOCAL BOOL write_pg0 (tree)
LPTREEINF tree;

{
  return (write_vpage (tree, 0L, &tree->page0, 0, sizeof (PAGE0)));
} /* write_pg0 */

/*****************************************************************************/

LOCAL BOOL read_vpage (tree, pagenum, page, start, size)
LPTREEINF tree;
LONG      pagenum;
VOID      *page;
SHORT     start, size;

{
  BOOL      ok;
  VPAGEINFP v;

  ok = TRUE;

  if (tree->num_vpages == 0)
    ok = read_page (tree, pagenum, page, start, size);  /* read part of page only */
  else
  {
    v = in_memory (tree, pagenum);                      /* look for page */

    if (v == NULL) v = new_vpage (tree, pagenum);       /* there is at least one page */

    v->time = tree->vtime++;
    if (ok) mem_move (page, v->vpage + start, size);    /* into page */
  } /* else */

  return (ok);
} /* read_vpage */

/*****************************************************************************/

LOCAL BOOL write_vpage (tree, pagenum, page, start, size)
LPTREEINF tree;
LONG      pagenum;
VOID      *page;
SHORT     start, size;

{
  BOOL      ok;
  VPAGEINFP v;

  ok = TRUE;
  v  = in_memory (tree, pagenum);                       /* look for page */

  if (v != NULL)
  {
    v->time  = tree->vtime++;
    v->dirty = TRUE;
    mem_move (v->vpage + start, page, size);            /* into virtual page */
  } /* if */

  if ((v == NULL) || (tree->flags & (TREE_FLUSH | TREE_MULUSER | TREE_MULTASK)))
  {
    ok = write_page (tree, pagenum, page, start, size); /* write part of page only */
    if (v != NULL) v->dirty = FALSE;                    /* buffer is save */
  } /* if */

  return (ok);
} /* write_vpage */

/*****************************************************************************/

LOCAL VPAGEINFP in_memory (tree, pagenum)
LPTREEINF tree;
LONG      pagenum;

{
  BOOL found;
  LONG i;

  found = find_vpage (tree, pagenum, &i);

  tree->num_calls++;
  if (found) tree->num_hits++;

  return (found ? tree->vpageinf + i : NULL);
} /* in_memory */

/*****************************************************************************/

LOCAL VPAGEINFP new_vpage (tree, pagenum)
LPTREEINF tree;
LONG      pagenum;

{
  REG LONG      i, num_vpages, time;
  REG VPAGEINFP v, lru;
  LONG          oldinx, newinx;
  BOOL          ok;
  KEY_PAGE      *kp;

  num_vpages = tree->num_vpages;
  v          = lru = tree->vpageinf;
  time       = v->time;
  ok         = TRUE;

  for (i = 0; (i < num_vpages) && (v->pagenum != FAILURE); i++, v++)
  {
    if (v->time < time)                         /* LRU strategy */
    {
      lru  = v;
      time = v->time;
    } /* if */
  } /* for */

  if (i < num_vpages)
  {
    lru    = v;                                 /* first non-used page */
    oldinx = i;
  } /* if */
  else
    find_vpage (tree, lru->pagenum, &oldinx);

  find_vpage (tree, pagenum, &newinx);

  if (lru->dirty)
  {
    kp = (KEY_PAGE *)lru->vpage;
    ok = write_page (tree, lru->pagenum, lru->vpage, 0, (SHORT)((lru->pagenum == 0) ? (SHORT)PAGESIZE : kp->size));
  } /* if */

  if (ok)
    if ((pagenum == 0) || (pagenum < tree->page0.free_page)) /* page 0 must be filled */
      ok = read_page (tree, pagenum, lru->vpage, 0, PAGESIZE);
    else
      mem_set (lru->vpage, 0, PAGESIZE);

  lru->time    = 0;
  lru->dirty   = FALSE;
  lru->pagenum = pagenum;

  if (tree->page0.free_page > 0) move_vpage (tree, oldinx, newinx); /* page 0 already read */

  find_vpage (tree, pagenum, &newinx);                  /* index has moved */

  return (tree->vpageinf + newinx);
} /* new_vpage */

/*****************************************************************************/

LOCAL BOOL find_vpage (tree, pagenum, inx)
LPTREEINF tree;
LONG      pagenum;
LONG      *inx;

{
  REG BOOL      found;
  REG LONG      i, l, r;
  REG VPAGEINFP v;

  l     = i = 0;
  r     = min (tree->page0.free_page + 1, tree->num_vpages);    /* '1' because of writing next free page */
  found = FALSE;

  if (r > 0)
    if (tree->vpageinf->pagenum == FAILURE) r = tree->num_vpages; /* there exist empty pages */

  while ((l != r) && ! found)
  {
    i = (l + r) / 2;
    v = tree->vpageinf + i;

    if (pagenum == v->pagenum)
      found = TRUE;
    else
      if (pagenum < v->pagenum)
        r = i;
      else
        l = i + 1;
  } /* while */

  if (! found) i = l;
  if (inx != NULL) *inx = i;

  return (found);
} /* find_vpage */

/*****************************************************************************/

LOCAL VOID move_vpage (tree, oldinx, newinx)
LPTREEINF tree;
LONG      oldinx, newinx;

{
  LONG      size;
  VPAGEINF  save;
  VPAGEINFP src, dst;

  if (oldinx < newinx)
  {
    newinx--;
    src  = tree->vpageinf + oldinx + 1;
    dst  = tree->vpageinf + oldinx;
    size = (newinx - oldinx) * sizeof (VPAGEINF);

    if (size > 0)
    {
      mem_move (&save, dst, sizeof (VPAGEINF));
      mem_lmove (dst, src, size);
      mem_move (tree->vpageinf + newinx, &save, sizeof (VPAGEINF));
    } /* if */
  } /* if */
  else
    if (oldinx > newinx)
    {
      src  = tree->vpageinf + newinx;
      dst  = tree->vpageinf + newinx + 1;
      size = (oldinx - newinx) * (LONG)sizeof (VPAGEINF);

      if (size > 0)
      {
        mem_move (&save, tree->vpageinf + oldinx, sizeof (VPAGEINF));
        mem_lmove (dst, src, size);
        mem_move (tree->vpageinf + newinx, &save, sizeof (VPAGEINF));
      } /* if */
    } /* if, else */
} /* move_vpage */

/*****************************************************************************/

LOCAL BOOL read_page (tree, pagenum, page, start, size)
LPTREEINF tree;
LONG      pagenum;
VOID      *page;
SHORT     start, size;

{
  BOOL  ok;
  LONG  bytes;
  LONG  offset;
  SHORT i;
  PAGE  save;
  UCHAR *p;

  offset = pagenum * PAGESIZE + start;
  bytes  = file_seek (tree->handle, offset, SEEK_SET);
  ok     = bytes == offset;

  if (ok)
  {
    bytes = file_read (tree->handle, (LONG)size, save);
    ok    = bytes == size;

    if ((pagenum != 0) && (tree->page0.flags & TREE_ENCODE))
    {
      p = save;
      for (i = 0; i < size; i++) *p++ ^= ENCODE (pagenum, start + i);
    } /* if */

    mem_move (page, save, size);
  } /* if */

  if (! ok) set_dberror (DB_TRDPAGE);

  return (ok);
} /* read_page */

/*****************************************************************************/

LOCAL BOOL write_page (tree, pagenum, page, start, size)
LPTREEINF tree;
LONG      pagenum;
VOID      *page;
SHORT     start, size;

{
  BOOL  ok;
  LONG  bytes;
  LONG  offset;
  SHORT i;
  PAGE  save;
  UCHAR *p;

  offset = pagenum * PAGESIZE + start;
  bytes  = file_seek (tree->handle, offset, SEEK_SET);
  ok     = bytes == offset;

  if (ok)
  {
    mem_set (save, 0, PAGESIZE);
    mem_move (save, page, size);

    if ((pagenum != 0) && (tree->page0.flags & TREE_ENCODE))
    {
      p = save;
      for (i = 0; i < size; i++) *p++ ^= ENCODE (pagenum, start + i);
    } /* if */

    if ((pagenum != 0) && (start == 0) && (size > sizeof (LONG)))
      size  = PAGESIZE;         /* write whole page and fill with zeroes */

    bytes = file_write (tree->handle, (LONG)size, save);
    ok    = bytes == size;
  } /* if */

  if (! ok) set_dberror (DB_TWRPAGE);

  return (ok);
} /* write_page */

/*****************************************************************************/

