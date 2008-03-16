/*****************************************************************************
 *
 * Module : UTILITY.C
 * Author : Dieter Gei
 *
 * Creation date    : 06.03.89
 * Last modification: $Id$
 *
 *
 * Description: This module implements the utility routines.
 *
 * History:
 * 04.01.96: Call to GlobalMemoryStatus used in mem_avail under WIN32
 * 01.01.96: Warnings of C/C++ 10.0 removed
 * 26.05.95: Function mem_realloc added (right now, works only for GEMDOS, WINDOWS or DOS (TURBO_C))
 * 30.09.93: TURBO_C relases os memory when calling free so malloc is used in mem_alloc
 * 01.08.93: Functions oem_to_ansi, ansi_to_oem, ansi_upper, and ansi_strcmp added
 * 12.07.93: Using malloc and free with Borland C++ because of incompatibilities of _dos_allocmem and fopen
 * 30.06.93: Problems with Intel chip (64K segments) fixed in mem_set and mem_move
 * 24.06.93: Modifications for Borland C++ added
 * 21.11.92: Modifications for MSDOS added
 * 03.11.92: Modifications for NT added
 * 31.10.92: Far pointer definitions for DLL implementation added
 * 25.10.92: Module header added
 * 06.03.89: Creation of body
 *****************************************************************************/

#include <ctype.h>
#include <time.h>

#include "import.h"

#if MSDOS
#ifndef _WINDOWS
#include <dos.h>
#if TURBO_C
#include <alloc.h>
#endif
#endif
#endif

#if UNIX
#if PCC
#include <malloc.h>
#endif
#endif

#include "export.h"
#include "utility.h"

/****** DEFINES **************************************************************/

#define GEMDOS_IKBD 0    /* if TRUE, use intelligent keboard */

#if GEMDOS_IKBD
#define IK_CLOCKSET 0x1B /* intelligent keyboard code for set clock */
#define IK_CLOCKGET 0x1C /* intelligent keyboard code for get clock */
#endif

#if GEMDOS
#define TO_UPPER "€šA¶€EEEIII’’O™OUUY™š›œŸAIOU¥¥AO¨©ª«¬­®¯·¸²²µµ¶·¸¹º»¼½¾¿ÁÁÂÃÄÅÆÇÈÉÊËÌÍÎÏĞÑÒÓÔÕÖ×ØÙÚÛÜİŞßàáâãäåæçèéêëìíîïğñòóôõö÷øùúûüış"
#define TO_LOWER "‡‚ƒ„…†‡ˆ‰Š‹Œ„†‚‘‘“”•–—˜”›œŸ ¡¢£¤¤¦§¨©ª«¬­®¯°±³³´´…°±¹º»¼½¾¿ÀÀÂÃÄÅÆÇÈÉÊËÌÍÎÏĞÑÒÓÔÕÖ×ØÙÚÛÜİŞßàáâãäåæçèéêëìíîïğñòóôõö÷øùúûüış"
#define TO_ASCII "CueaaaaceeeiiiAAEaAooouuyOUc$YsfaiounNao?[]24i<>aoOooOAAO~'+qCRT~~~~~~~~~~~~~~~~~~~~~~~~~~~~~@^~~s~~~~~~~~~~~~~~=+><~~/~''.~n23"
#endif

/* Ext. charset  "€‚ƒ„…†‡ˆ‰Š‹Œ‘’“”•–—˜™š›œŸ ¡¢£¤¥¦§¨©ª«¬­®¯°±²³´µ¶·¸¹º»¼½¾¿ÀÁÂÃÄÅÆÇÈÉÊËÌÍÎÏĞÑÒÓÔÕÖ×ØÙÚÛÜİŞßàáâãäåæçèéêëìíîïğñòóôõö÷øùúûüış" */

#if MSDOS | NT | OS2 | FLEXOS
#ifdef _WINDOWS
#define TO_UPPER "~~~~~~~~~~~~~~~~~‘’~~~~~~~~~~~~~ ¡¢£¤¥¦§¨©ª«¬­®¯°±²³´µ¶·¸¹º»¼½¾¿ÀÁÂÃÄÅÆÇÈÉÊËÌÍÎÏĞÑÒÓÔÕÖ×ØÙÚÛÜİŞßÀÁÂÃÄÅÆÇÈÉÊËÌÍÎÏĞÑÒÓÔÕÖ×ØÙÚÛÜİŞ"
#define TO_LOWER "~~~~~~~~~~~~~~~~~‘’~~~~~~~~~~~~~ ¡¢£¤¥¦§¨©ª«¬­®¯°±²³´µ¶·¸¹º»¼½¾¿àáâãäåæçèéêëìíîïğñòóôõö÷øùúûüışßàáâãäåæçèéêëìíîïğñòóôõö÷øùúûüış"
#define TO_ASCII "~~~~~~~~~~~~~~~~~‘’~~~~~~~~~~~~~ !c$oY|$\"ca<]-R-0+23´mq..10>423?AAAAAAACEEEEIIIIDNOOOOOXOUUUUYPsaaaaaaaceeeeiiiidnooooo-ouuuuyp"
#else
#define TO_UPPER "€šAA€EEEIII’’O™OUUY™š›œPŸAIOU¥¥AO¨©ª«¬­®¯·¸²²µµ¶·¸¹º»¼½¾¿ÀÁÂÃÄÅÆÇÈÉÊËÌÍÎÏĞÑÒÓÔÕÖ×ØÙÚÛÜİŞßàáâãäåæçèéêëìíîïğñòóôõö÷øùúûüış"
#define TO_LOWER "‡‚ƒ„…†‡ˆ‰Š‹Œ„†‚‘‘“”•–—˜”›œpŸ ¡¢£¤¤¦§¨©ª«¬­®¯°±³³´´¶°±¹º»¼½¾¿ÀÁÂÃÄÅÆÇÈÉÊËÌÍÎÏĞÑÒÓÔÕÖ×ØÙÚÛÜİŞßàáâãäåæçèéêëìíîïğñòóôõö÷øùúûüış"
#define TO_ASCII "CueaaaaceeeiiiAAEaAooouuyOUc$YPfaiounNao?[]24!<>~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~s~~~~~~~~~~~~~~=+><~~/~~~.~n2~"
#endif
#endif

#if UNIX
#define TO_UPPER "CUEAAAACEEEIIIAAEAAOOOUUYOUC$YSFAIOUNNAO?[]24I<>~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~s~~~~~~~~~~~~~~=+><~~/~~~.~n2~"
#define TO_LOWER "cueaaaaceeeiiiaaeaaooouuyouc$ypfaiounnao?[]24i<>~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~s~~~~~~~~~~~~~~=+><~~/~~~.~n2~"
#define TO_ASCII "CueaaaaceeeiiiAAEaAooouuyOUc$YsfaiounNao?[]24i<>aoOooOAAO~'+qCRT~~~~~~~~~~~~~~~~~~~~~~~~~~~~~@^~~s~~~~~~~~~~~~~~=+><~~/~''.~n23"
#endif

#define TO_OEM       "€,Ÿ,_ıüˆ%S<O`'''ù-_˜™s>oY ­›œİ'c~~~-r_~~~3'æú,1§¯¬«_¨AAAA’€EEEIIIID¥OOOO™xOUUUšY_á… ƒa„†‘‡Š‚ˆ‰¡Œ‹d¤•¢“o”öo—£–y_˜"
#define TO_ANSI      "ÇüéâäàåçêëèïîìÄÅÉæÆôöòûùÿÖÜ¢£¥PƒáíóúñÑªº¿_¬½¼¡«»___¦¦¦¦++¦¦++++++--+-+¦¦++--¦-+----++++++++__¦___ß_¶__µ__________±____÷_°•·_n²__"
#define TO_ANSIUPPER "€‚ƒ„…†‡ˆ‰Š‹Œ‘’“”•–—˜™Š›ŒŸ ¡¢£¤¥¦§¨©ª«¬­®¯°±²³´µ¶·¸¹º»¼½¾¿ÀÁÂÃÄÅÆÇÈÉÊËÌÍÎÏĞÑÒÓÔÕÖ×ØÙÚÛÜİŞßÀÁÂÃÄÅÆÇÈÉÊËÌÍÎÏĞÑÒÓÔÕÖ÷ØÙÚÛÜİŞY"

/****** TYPES ****************************************************************/

#if GEMDOS_IKBD
#if DR_C | LASER_C | LATTICE_C | MW_C
typedef struct kbdvbase
{
  VOID (*kb_midivec) ();
  VOID (*kb_vkbderr) ();
  VOID (*kb_vmiderr) ();
  VOID (*kb_statvec) ();
  VOID (*kb_mousevec) ();
  VOID (*kb_clockvec) ();
  VOID (*kb_joyvec) ();
  VOID (*kb_midisys) ();
  VOID (*kb_kbdsys) ();
} KBDVBASE;
#endif

typedef struct tm TM;

#endif /* GEMDOS */

/****** VARIABLES ************************************************************/

#if GEMDOS_IKBD
LOCAL  KBDVBASE *kbdvp;
LOCAL  VOID     (*old_clockvec) ();
LOCAL  TM       ktm;
LOCAL  BOOL     must_init = TRUE;
#endif /* GEMDOS_IKBD */

#if GEMDOS
EXTERN SHORT _app;
#endif

LOCAL UCHAR tbl_upper [128] = {TO_UPPER};
LOCAL UCHAR tbl_lower [128] = {TO_LOWER};
LOCAL UCHAR tbl_ascii [128] = {TO_ASCII};

LOCAL UCHAR tbl_oem [128]       = {TO_OEM};
LOCAL UCHAR tbl_ansi [128]      = {TO_ANSI};
LOCAL UCHAR tbl_ansiupper [128] = {TO_ANSIUPPER};

LOCAL UCHAR tbl_ansisort [256] =
{
  0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
  0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f,
  0x3e, 0x3f, 0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d,
  0x7f, 0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x4e, 0x4f, 0x50, 0x51, 0x52, 0x53,
  0x54, 0x89, 0x99, 0x9b, 0x9f, 0xa3, 0xad, 0xaf, 0xb1, 0xb3, 0xbd, 0xbf, 0xc1, 0xc3, 0xc5, 0xc9,
  0xd9, 0xdb, 0xdd, 0xdf, 0xe4, 0xe6, 0xf0, 0xf2, 0xf4, 0xf6, 0xfb, 0x55, 0x56, 0x57, 0x58, 0x59,
  0x5a, 0x91, 0x9a, 0x9d, 0xa1, 0xa8, 0xae, 0xb0, 0xb2, 0xb8, 0xbe, 0xc0, 0xc2, 0xc4, 0xc7, 0xd0,
  0xda, 0xdc, 0xde, 0xe1, 0xe5, 0xeb, 0xf1, 0xf3, 0xf5, 0xf8, 0xfd, 0x5b, 0x5c, 0x5d, 0x5e, 0x20,
  0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2a, 0xe0, 0x2b, 0xd7, 0x2c, 0x2d, 0x2e,
  0x2f, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0xe2, 0x39, 0xd8, 0x3a, 0x3b, 0xfc,
  0x5f, 0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e,
  0x6f, 0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7a, 0x7b, 0x7c, 0x7d, 0x7e,
  0x8a, 0x8b, 0x8c, 0x8d, 0x8e, 0x8f, 0x90, 0x9c, 0xa4, 0xa5, 0xa6, 0xa7, 0xb4, 0xb5, 0xb6, 0xb7,
  0xa0, 0xc6, 0xca, 0xcb, 0xcc, 0xcd, 0xce, 0x3c, 0xcf, 0xe7, 0xe8, 0xe9, 0xea, 0xf7, 0xfe, 0xe3,
  0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x9e, 0xa9, 0xaa, 0xab, 0xac, 0xb9, 0xba, 0xbb, 0xbc,
  0xa2, 0xc8, 0xd1, 0xd2, 0xd3, 0xd4, 0xd5, 0x3d, 0xd6, 0xec, 0xed, 0xee, 0xef, 0xf9, 0xff, 0xfa
};

LOCAL UCHAR tbl_ansisame [256] =
{
  0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
  0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f,
  0x3e, 0x3f, 0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d,
  0x7f, 0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x4e, 0x4f, 0x50, 0x51, 0x52, 0x53,
  0x54, 0x89, 0x99, 0x9b, 0x9f, 0xa3, 0xad, 0xaf, 0xb1, 0xb3, 0xbd, 0xbf, 0xc1, 0xc3, 0xc5, 0xc9,
  0xd9, 0xdb, 0xdd, 0xdf, 0xe4, 0xe6, 0xf0, 0xf2, 0xf4, 0xf6, 0xfb, 0x55, 0x56, 0x57, 0x58, 0x59,
  0x5a, 0x89, 0x99, 0x9b, 0x9f, 0xa3, 0xad, 0xaf, 0xb1, 0xb3, 0xbd, 0xbf, 0xc1, 0xc3, 0xc5, 0xc9,
  0xd9, 0xdb, 0xdd, 0xdf, 0xe4, 0xe6, 0xf0, 0xf2, 0xf4, 0xf6, 0xfb, 0x5b, 0x5c, 0x5d, 0x5e, 0x20,
  0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2a, 0xdf, 0x2b, 0xd7, 0x2c, 0x2d, 0x2e,
  0x2f, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0xdf, 0x39, 0xd7, 0x3a, 0x3b, 0xfb,
  0x5f, 0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e,
  0x6f, 0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7a, 0x7b, 0x7c, 0x7d, 0x7e,
  0x89, 0x89, 0x89, 0x89, 0x89, 0x89, 0x89, 0x9b, 0xa3, 0xa3, 0xa3, 0xa3, 0xb3, 0xb3, 0xb3, 0xb3,
  0x9f, 0xc5, 0xc9, 0xc9, 0xc9, 0xc9, 0xc9, 0x3c, 0xc9, 0xe6, 0xe6, 0xe6, 0xe6, 0xf6, 0xfe, 0xe3,
  0x89, 0x89, 0x89, 0x89, 0x89, 0x89, 0x89, 0x9b, 0xa3, 0xa3, 0xa3, 0xa3, 0xb3, 0xb3, 0xb3, 0xb3,
  0x9f, 0xc5, 0xc9, 0xc9, 0xc9, 0xc9, 0xc9, 0x3d, 0xc9, 0xe6, 0xe6, 0xe6, 0xe6, 0xf6, 0xfe, 0xf6
};

/****** FUNCTIONS ************************************************************/

LOCAL INT  str_scan     _((INT c1, INT c2, INT l1, INT l2, CHAR *s1, CHAR *s2));

#if GEMDOS
LOCAL LONG *get_actpd   _((VOID));
#endif

#if GEMDOS_IKBD
LOCAL VOID new_clockvec _((CHAR *p));
LOCAL TM   *k_gettime   _((VOID));
LOCAL VOID init_kbclock _((VOID));
#endif /* GEMDOS */

/*****************************************************************************/
/* Memory functions                                                          */
/*****************************************************************************/

#if GEMDOS
LOCAL LONG *get_actpd ()

{
  LONG  *ret;
  SHORT tos;
  LONG  stack;

  if (_app) return (NULL);                              /* don't need to set basepage */

  stack = Super (NULL);
  tos   = *(USHORT *)(*(LONG *)0x4F2 + 0x02);           /* get TOS version */

  if (tos >= 0x0102)
    ret = (LONG *)(*(LONG *)(*(LONG *)0x4F2 + 0x28));   /* get pointer to basepage */
  else
    ret = (LONG *)0x602C;

  Super ((VOID *)stack);
  return (ret);
} /* get_actpd */
#endif

/*****************************************************************************/

GLOBAL HPVOID WINAPI mem_alloc (mem)
LONG mem;

{
  HPVOID ret;
#if GEMDOS
  LONG *pdp, pd;
#endif

  if (mem == 0) return ((VOID *)0xFFFFFFFFL);   /* need no memory to allocate */

#if GEMDOS
  ret = NULL;
  pdp = get_actpd ();

  if (pdp != NULL) pd = *pdp;

#if TURBO_C
  if (pdp != NULL) *pdp = (LONG)_BasPag;
#endif

#if TURBO_C
  ret = (HPVOID)malloc (mem);			/* releases memory when free'd */
#else
  ret = (HPVOID)Malloc (mem);
#endif

  if (pdp != NULL) *pdp = pd;
#endif

#if FLEXOS
  ret = (HPVOID)Malloc (mem);
#endif

#if MSDOS | NT
#ifdef _WINDOWS
  ret = (HPVOID)GlobalAllocPtr (GMEM_MOVEABLE, mem);
#else
  {
#if MS_C
    UINT res, size, seg;

    size = (UINT)((mem + 15) >> 4);		/* bytes to paragraphs */
    res  = _dos_allocmem (size, &seg);
    ret  = (res != 0) ? 0L : (HPVOID)((ULONG)seg << 16);
#else
    ret = (mem < 0x00010000L) ? (HPVOID)malloc ((size_t)mem) : NULL;
#endif
  } /* MSDOS without WINDOWS */
#endif
#endif

#if OS2 | UNIX
  ret = (HPVOID)malloc (mem);
#endif

  return (ret);
} /* mem_alloc */

/*****************************************************************************/

GLOBAL HPVOID WINAPI mem_realloc (memptr, mem)
HPVOID memptr;
LONG   mem;

{
  HPVOID ret;

  if (memptr == (VOID *)0xFFFFFFFFL) memptr = NULL;	/* no memory allocated previously  */

#if GEMDOS
#if TURBO_C
  ret = (HPVOID)realloc (memptr, mem);
#else
  ret = NULL;
#endif
#endif

#if FLEXOS
  ret = NULL;
#endif

#if MSDOS | NT
#ifdef _WINDOWS
  ret = (HPVOID)GlobalReAllocPtr (memptr, mem, GMEM_MOVEABLE);
#else
  {
#if MS_C
    ret = NULL;
#else
    ret = (mem < 0x00010000L) ? (HPVOID)malloc ((size_t)mem) : NULL;
#endif
  } /* MSDOS without WINDOWS */
#endif
#endif

#if OS2 | UNIX
  ret = (HPVOID)realloc (memptr, mem);
#endif

  return (ret);
} /* mem_realloc */

/*****************************************************************************/

GLOBAL VOID WINAPI mem_free (memptr)
HPVOID memptr;

{
  BOOL ok;

#if GEMDOS
  LONG *pdp, pd;
#endif

  ok = (memptr != NULL) && (memptr != (HPVOID)0xFFFFFFFFL);

#if GEMDOS
  if (ok)
  {
    pdp = get_actpd ();
    if (pdp != NULL) pd  = *pdp;

#if TURBO_C
    if (pdp != NULL) *pdp = (LONG)_BasPag;
#endif

#if TURBO_C
    free (memptr);
#else
    Mfree (memptr);
#endif

    if (pdp != NULL) *pdp = pd;
  } /* if */
#endif

#if FLEXOS
  if (ok) Mfree ((VOID *)memptr);
#endif

#if MSDOS | NT
#ifdef _WINDOWS
  if (ok) GlobalFreePtr (memptr);
#else
  {
#if MS_C
    UINT seg;

    if (ok)
    {
      seg = (UINT)((ULONG)memptr >> 16);
      _dos_freemem (seg);
    } /* if */
#else
    if (ok) free ((VOID *)memptr);
#endif
  } /* MSDOS without WINDOWS */
#endif
#endif

#if OS2 | UNIX
  if (ok) free (memptr);
#endif
} /* mem_free */

/*****************************************************************************/

GLOBAL LONG WINAPI mem_avail ()

{
#ifdef WIN32
  MEMORYSTATUS ms;
#endif

#ifdef _WINDOWS
#ifdef WIN32
  ms.dwLength = sizeof (ms);
  GlobalMemoryStatus (&ms);
  return (ms.dwTotalPhys);
#else
  return (GetFreeSpace (0));
#endif
#else
#if MSDOS
#if MS_C
  {
    UINT seg;

    _dos_allocmem (0xFFFFU, &seg);

    return (16L * (LONG)seg);
  } /* MSDOS without WINDOWS */
#endif

#if TURBO_C
  return (coreleft ());
#endif

#else
  return (Mavail ());
#endif
#endif
} /* mem_avail */

/*****************************************************************************/

GLOBAL HPVOID WINAPI mem_set (dest, val, len)
HPVOID dest;
INT    val;
USHORT len;

{
#if I8086 | DR_C | LASER_C | LATTICE_C
  REG HPUCHAR d;

  for (d = (HPUCHAR)dest; len > 0; len--) *d++ = (UCHAR)val;

#else
  if (len != 0) memset ((VOID *)dest, val, len);        /* less than 64 K */
#endif

  return (dest);
} /* mem_set */

/*****************************************************************************/

GLOBAL HPVOID WINAPI mem_move (dest, src, len)
HPVOID       dest;
CONST HPVOID src;
USHORT       len;

{
#if I8086 | DR_C | LASER_C | LATTICE_C | HIGH_C | PCC
  REG HPUCHAR s, d;
  REG USHORT  l;

  s = (HPUCHAR)src;
  d = (HPUCHAR)dest;
  l = len;

  if ((s < d) && (s + l > d))
    for (d += l, s += l; l > 0; l--) *(--d) = *(--s);
  else
    for (; l > 0; l--) *d++ = *s++;

#else
  if (len != 0) memmove ((VOID *)dest, (VOID *)src, len);       /* less than 64 K */
#endif

  return (dest);
} /* mem_move */

/*****************************************************************************/

GLOBAL HPVOID WINAPI mem_lset (dest, val, len)
HPVOID dest;
INT    val;
ULONG  len;

{
#if MSDOS | OS2 | FLEXOS | DR_C | LASER_C | LATTICE_C | MW_C
  REG HPUCHAR d;

  if (len < 0x00010000L)
    mem_set (dest, val, (USHORT)len);
  else
    for (d = (HPUCHAR)dest; len > 0; len--) *d++ = (UCHAR)val;

#else
  if (len != 0) memset (dest, val, len);
#endif

  return (dest);
} /* mem_lset */

/*****************************************************************************/

GLOBAL HPVOID WINAPI mem_lmove (dest, src, len)
HPVOID       dest;
CONST HPVOID src;
ULONG        len;

{
#if MSDOS | OS2 | FLEXOS | DR_C | LASER_C | LATTICE_C | MW_C | PCC
  REG HPUCHAR s, d;
  REG ULONG   l;

  if (len < 0x00010000L)
    mem_move (dest, src, (USHORT)len);
  else
  {
    s = (HPUCHAR)src;
    d = (HPUCHAR)dest;
    l = len;

    if ((s < d) && (s + l > d))
      for (d += l, s += l; l > 0; l--) *(--d) = *(--s);
    else
      for (; l > 0; l--) *d++ = *s++;
  } /* else */
#else
  if (len != 0) memmove (dest, src, len);
#endif

  return (dest);
} /* mem_lmove */

/*****************************************************************************/
/* String functions                                                          */
/*****************************************************************************/

GLOBAL INT WINAPI str_match (s1, s2)
LPSTR s1, s2;

{
  return (str_scan (0, 0, (INT)(strlen (s1) - 1), (INT)(strlen (s2) - 1), s1, s2));
} /* str_match */

/*****************************************************************************/

GLOBAL LPSTR WINAPI str_upr (s)
LPSTR s;

{
#ifdef _WINDOWS
  AnsiUpper (s);
#else
#if MS_C | TURBO_C
  strupr (s);
#else
  REG LPSTR str;

  str = s;
  while (*str) *str++ = toupper (*str);
#endif
#endif

  return (s);
} /* str_upr */

/*****************************************************************************/

GLOBAL LPSTR WINAPI str_lwr (s)
LPSTR s;

{
#ifdef _WINDOWS
  AnsiLower (s);
#else
#if MS_C | TURBO_C
  strlwr (s);
#else
  REG LPSTR str;

  str = s;
  while (*str) *str++ = tolower (*str);
#endif
#endif

  return (s);
} /* str_lwr */

/******************************************************************************/

GLOBAL LPSTR WINAPI str_upper (s)
LPSTR s;

{
#ifdef _WINDOWS
  AnsiUpper (s);
#else
  REG UCHAR *str;

  str = (UCHAR *)s;
  while (*str) *str++ = (UCHAR)ch_upper (*str);
#endif

  return (s);
} /* str_upper */

/******************************************************************************/

GLOBAL LPSTR WINAPI str_lower (s)
LPSTR s;

{
#ifdef _WINDOWS
  AnsiLower (s);
#else
  REG UCHAR *str;

  str = (UCHAR *)s;
  while (*str) *str++ = (UCHAR)ch_lower (*str);
#endif

  return (s);
} /* str_lower */

/******************************************************************************/

GLOBAL LPSTR WINAPI str_ascii (s)
LPSTR s;

{
  REG UCHAR *str;

  str = (UCHAR *)s;
  while (*str) *str++ = (UCHAR)ch_ascii (*str);

  return (s);
} /* str_ascii */

/******************************************************************************/

GLOBAL LPSTR WINAPI str_rmchar (s, c)
REG LPSTR s;
REG INT   c;

{
  REG UCHAR *src, *dst;

  src = dst = (UCHAR *)s;

  while (*src)
  {
    if (*src != (UCHAR)c)
      *dst++ = *src++;
    else
      src++;
  } /* while */

  *dst = EOS;

  return (s);
} /* str_rmchar */

/******************************************************************************/

GLOBAL LPSTR WINAPI chrcat (s, c)
LPSTR s;
CHAR  c;

{
  CHAR ch [2];

  ch [0] = c;
  ch [1] = EOS;

  return (strcat (s, ch));
} /* chrcat */

/*****************************************************************************/

GLOBAL USHORT WINAPI ch_upper (c)
USHORT c;

{
  c &= 0x00FF;

#ifdef _WINDOWS
  return (LOBYTE (LOWORD ((ULONG)AnsiUpper ((LPSTR)(ULONG)c))));
#else
  return ((USHORT)((c >= 128) ? tbl_upper [c - 128] : toupper (c)));
#endif
} /* ch_upper */

/*****************************************************************************/

GLOBAL USHORT WINAPI ch_lower (c)
USHORT c;

{
  c &= 0x00FF;

#ifdef _WINDOWS
  return (LOBYTE (LOWORD ((ULONG)AnsiLower ((LPSTR)(ULONG)c))));
#else
  return ((USHORT)((c >= 128) ? tbl_lower [c - 128] : tolower (c)));
#endif
} /* ch_lower */

/*****************************************************************************/

GLOBAL USHORT WINAPI ch_ascii (c)
USHORT c;

{
  c &= 0x00FF;

#ifdef _WINDOWS
  return (c);           /* all chars can be sorted */
#else
  return ((USHORT)((c >= 128) ? tbl_ascii [c - 128] : c));
#endif
} /* ch_ascii */

/*****************************************************************************/

GLOBAL VOID WINAPI oem_to_ansi (oem, ansi)
HPCHAR oem, ansi;

{
  REG HPCHAR src, dst;
  REG INT    c;

  src = oem;
  dst = ansi;

  while (*src)
  {
    c      = *src++ & 0x00FF;
    *dst++ = (CHAR)((c >= 128) ? tbl_ansi [c - 128] : c);
  } /* while */
} /* oem_to_ansi */

/*****************************************************************************/

GLOBAL VOID WINAPI ansi_to_oem (ansi, oem)
HPCHAR ansi, oem;

{
  REG HPCHAR src, dst;
  REG INT    c;

  src = ansi;
  dst = oem;

  while (*src)
  {
    c      = *src++ & 0x00FF;
    *dst++ = (CHAR)((c >= 128) ? tbl_oem [c - 128] : c);
  } /* while */
} /* ansi_to_oem */

/*****************************************************************************/

GLOBAL HPCHAR WINAPI ansi_upper (s)
HPCHAR s;

{
  REG INT c;

  while (*s)
  {
    c    = *s & 0x00FF;
    *s++ = (CHAR)((c >= 128) ? tbl_ansiupper [c - 128] : toupper (c));
  } /* while */

  return (s);
} /* ansi_upper */

/*****************************************************************************/

GLOBAL INT WINAPI ansi_strcmp (s1, s2)
HPCHAR s1, s2;

{
  INT   diff, rest;
  UCHAR ch1, ch2, same1, same2;
  BOOL  ss1, ss2;

  while (*s1 == *s2)
  {
    if (*s1 == EOS)
      return (0);
    s1++;
    s2++;
  } /* while */

  ss1   = ss2 = FALSE;
  ch1   = tbl_ansisort [(UCHAR)(*s1)];
  ch2   = tbl_ansisort [(UCHAR)(*s2)];
  same1 = tbl_ansisame [(UCHAR)(*s1)];
  same2 = tbl_ansisame [(UCHAR)(*s2)];
  diff  = ch1 - ch2;

  if (((UCHAR)(*s1) == 0xDF) && ((UCHAR)(*s2) == 'S'))		/* the german sharp s */
  {
    ss1   = TRUE;
    same1 = tbl_ansisame ['S'];
    s1--;
  } /* if */
  else
    if (((UCHAR)(*s2) == 0xDF) && ((UCHAR)(*s1) == 'S'))	/* the german sharp s */
    {
      ss2   = TRUE;
      same2 = tbl_ansisame ['S'];
      s2--;
    } /* else, if */

  if (same1 == same2)
  {
    rest = 0;

    while (same1 == same2)
    {
      s1++;
      s2++;
      ch1 = (UCHAR)(*s1);
      ch2 = (UCHAR)(*s2);

      if (ss1) ch1 = 'S';
      if (ss2) ch2 = 'S';

      ss1   = ss2 = FALSE;
      rest  = tbl_ansisort [ch1] - tbl_ansisort [ch2];
      same1 = tbl_ansisame [ch1];
      same2 = tbl_ansisame [ch2];

      if ((*s1 == EOS) || (*s2 == EOS))
        break;

      if ((ch1 == 0xDF) && (ch2 == 'S'))	/* the german sharp s */
      {
        ss1   = TRUE;
        same1 = tbl_ansisame [ch2];
        s1--;
      } /* if */
      else
        if ((ch2 == 0xDF) && (ch1 == 'S'))	/* the german sharp s */
        {
          ss2   = TRUE;
          same2 = tbl_ansisame [ch1];
          s2--;
        } /* else, if */
    } /* while */

    if (same1 != same2)
      diff = rest;
  } /* if */

  return ((diff < 0) ? -1 : (diff > 0) ? 1 : 0);
} /* ansi_strcmp */

/*****************************************************************************/

GLOBAL HPCHAR WINAPI hstrcat (s1, s2)
HPCHAR s1, s2;

{
  mem_lmove (s1 + hstrlen (s1), s2, hstrlen (s2) + 1);

  return (s1);
} /* hstrcat */

/*****************************************************************************/

GLOBAL INT WINAPI hstrcmp (s1, s2)
HPCHAR s1, s2;

{
  for (; *s1 == *s2; s1++, s2++)
    if (*s1 == EOS) return (0);

  return (*s1 - *s2);
} /* hstrcmp */

/*****************************************************************************/

GLOBAL HPCHAR WINAPI hstrcpy (s1, s2)
HPCHAR s1, s2;

{
  mem_lmove (s1, s2, hstrlen (s2) + 1);

  return (s1);
} /* hstrcpy */

/*****************************************************************************/

GLOBAL ULONG WINAPI hstrlen (s)
HPCHAR s;

{
  HPCHAR t;

  for (t = s; *t != EOS; t++);

  return (t - s);
} /* hstrlen */

/*****************************************************************************/

GLOBAL HPCHAR WINAPI hstrncat (s1, s2, count)
HPCHAR s1, s2;
ULONG  count;

{
  ULONG l1, l2;

  l1 = hstrlen (s1);
  l2 = hstrlen (s2);
  if (count > l2) count = l2;

  mem_lmove (s1 + l1, s2, count);
  s1 [l1 + count] = EOS;

  return (s1);
} /* hstrncat */

/*****************************************************************************/

GLOBAL HPCHAR WINAPI hstrncpy (s1, s2, count)
HPCHAR s1, s2;
ULONG  count;

{
  ULONG l;

  l = hstrlen (s2);

  if (count <= l)
    mem_lmove (s1, s2, count);
  else
  {
    mem_lmove (s1, s2, l);
    s1 [l] = EOS;
  } /* else */

  return (s1);
} /* hstrncpy */

/*****************************************************************************/

LOCAL INT str_scan (c1, c2, l1, l2, s1, s2)
INT   c1, c2, l1, l2;
LPSTR s1, s2;

{
  REG INT  comp;
  REG CHAR ch1, ch2;
  REG BOOL stop;

  comp = 0;

  do
  {
    stop = TRUE;

    if (c1 > l1)
      comp = c2 - l2 - 1;
    else
    {
      ch1 = s1 [c1++];

      if (c2 > l2)
	comp = ! ((ch1 == WILD_CHARS) && (c1 > l1));
      else
        if (ch1 == WILD_CHARS)
          do
          {
            comp = str_scan (c1, c2++, l1, l2, s1, s2);
          } while (! ((comp == 0) || (c2 > l2 + 1)));
        else
        {
          ch2 = s2 [c2++];

          if (ch1 == WILD_CHAR)
	    ch1 = ch2;
	  else
	    if (ch1 == WILD_DIGIT)
	      ch1 = (CHAR)((isdigit (ch2) || (ch2 == WILD_DIGIT)) ? ch2 : ' ');

#ifdef _WINDOWS
{
          CHAR sz1 [2], sz2 [2];

          sz1 [0] = ch1;
          sz1 [1] = EOS;
          sz2 [0] = ch2;
          sz2 [1] = EOS;

          stop = (comp = lstrcmp (sz1, sz2)) != 0;
}
#elif MSDOS && ! defined (USE_DOS_CHARSET)
{
          CHAR sz1 [2], sz2 [2];

          sz1 [0] = ch1;
          sz1 [1] = EOS;
          sz2 [0] = ch2;
          sz2 [1] = EOS;

          stop = (comp = ansi_strcmp (sz1, sz2)) != 0;
}
#else
          stop = (comp = ch1 - ch2) != 0;
#endif
        } /* else */
    } /* else */
  } while (! stop);

  return (comp);
} /* str_scan */

/*****************************************************************************/
/* Date and time functions                                                   */
/*****************************************************************************/

GLOBAL VOID WINAPI get_date (dateptr)
HPDATE dateptr;

{
  time_t    tloc;
  struct tm *tptr;

  tloc = time ((time_t *)NULL);
  tptr = localtime (&tloc);

  dateptr->day   = (UCHAR)tptr->tm_mday;
  dateptr->month = (UCHAR)(tptr->tm_mon + 1);
  dateptr->year  = (USHORT)(tptr->tm_year + 1900);
} /* get_date */

/*****************************************************************************/

#if GEMDOS_IKBD
LOCAL VOID new_clockvec (p)
CHAR *p;

{
  REG SHORT *tp;
  REG SHORT i;

  if ((*p & 0xFF) == 0xFC) ++p;
  tp = &ktm.tm_year + 1;

  while (tp > &ktm.tm_sec)
  {
    i = *p++;
    *--tp = ((i & 0xF0) >> 1) + ((i & 0xF0) >> 3) + (i & 0x0F);
  } /* while */

  ktm.tm_mon--;
  kbdvp->kb_clockvec = old_clockvec;
} /* new_clockvec */

/*****************************************************************************/

LOCAL TM *k_gettime ()

{
  LOCAL CHAR kbdcmd [2] = {IK_CLOCKGET, 0};

  kbdvp              = Kbdvbase ();
  old_clockvec       = kbdvp->kb_clockvec;
  kbdvp->kb_clockvec = new_clockvec;
  Ikbdws (0, kbdcmd);

  while (kbdvp->kb_clockvec == new_clockvec);

  return (&ktm);
} /* k_gettime */

/*****************************************************************************/

LOCAL VOID init_kbclock ()

{
  SHORT date;
  SHORT day, month, year;
  SHORT time;
  SHORT hour, minute, second;
  CHAR  kbdcmd [7];

  date  = Tgetdate ();                  /* get GEMDOS date */
  day   = date & 0x1F;
  date  >>= 5;
  month = date & 0x0F;
  date  >>=4;
  year  = date;
  year += 80;

  time   = Tgettime () ;                /* get GEMDOS time */
  second = (time & 0x1F) * 2;
  time   >>= 5;
  minute = (time & 0x3F) ;
  time   >>= 6 ;
  hour   = (time & 0x1F) ;

  kbdcmd [0] = IK_CLOCKSET;

  kbdcmd [1] = 16 * (year / 10)   + (year % 10);   /* bcd year */
  kbdcmd [2] = 16 * (month / 10)  + (month % 10);  /* month    */
  kbdcmd [3] = 16 * (day / 10)    + (day % 10);    /* day      */
  kbdcmd [4] = 16 * (hour / 10)   + (hour % 10);   /* hour     */
  kbdcmd [5] = 16 * (minute / 10) + (minute % 10); /* minute   */
  kbdcmd [6] = 16 * (second /10)  + (second % 10); /* second   */

  Ikbdws (6, kbdcmd);
} /* init_kbclock */
#endif /* GEMDOS_IKBD */

/*****************************************************************************/

GLOBAL VOID WINAPI get_time (timeptr)
HPTIME timeptr;

{
#if GEMDOS_IKBD
  TM *tptr;

  if (must_init)
  {
    must_init = FALSE;
    init_kbclock ();     /* init intelligent keyboard clock */
  } /* if */

  tptr = k_gettime ();

  timeptr->hour   = (UCHAR)tptr->tm_hour;
  timeptr->minute = (UCHAR)tptr->tm_min;
  timeptr->second = (UCHAR)tptr->tm_sec;
  timeptr->micro  = 0;
#else
  time_t    tloc;
  struct tm *tptr;

  tloc = time ((time_t *)NULL);
  tptr = localtime (&tloc);

  timeptr->hour   = (UCHAR)tptr->tm_hour;
  timeptr->minute = (UCHAR)tptr->tm_min;
  timeptr->second = (UCHAR)tptr->tm_sec;
  timeptr->micro  = 0;
#endif
} /* get_time */

/*****************************************************************************/

GLOBAL VOID WINAPI get_tstamp (timestamp)
HPTIMESTAMP timestamp;

{
  get_date (&timestamp->date);
  get_time (&timestamp->time);
} /* get_tstamp */

/*****************************************************************************/

