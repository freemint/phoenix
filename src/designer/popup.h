/*****************************************************************************/
/*                                                                           */
/* Modul: POPUP.H                                                            */
/* Datum: 23/04/93                                                           */
/*                                                                           */
/*****************************************************************************/

#ifndef __POPUP__
#define __POPUP__

/****** DEFINES **************************************************************/

#define POPUP_INIT   0x01                  /* Initialisiere Popup */
#define POPUP_DRAW   0x02                  /* Zeichne Popup */

#define POPUP_GETLINE   0                  /* Hole Datum */
#define POPUP_CHANGED   1                  /* Eintrag ge�ndert */
#define POPUP_KEYPRESS  2                  /* Taste gedr�ckt */

/****** TYPES ****************************************************************/

typedef struct popup_spec *POP; /* Zeiger f�r Parameter */

typedef LONG (* POPFUNC) _((POP pop, WORD message, LONG item, MKINFO *mk));

typedef struct popup_spec
{
  POPFUNC popfunc;              /* Funktion zum Besorgen der Eintr�ge */
  OBJECT  *tree;                /* Objektbaum des Popups */
  BYTE    *itemstr;             /* Zeiger auf angeforderte Zeichenkette  */
  BOOLEAN v_slider;             /* Vertikaler Schieber vorhanden ? */
  BOOLEAN realtime;             /* Vertikaler Schieber in Echtzeit ? */
  BOOLEAN mandatory;            /* Ein Feld mu� ausgew�hlt sein */
  WORD    bmsk;                 /* Knopf-Maske */
  WORD    x;                    /* X-Position auf Bildschirm */
  WORD    y;                    /* Y-Position auf Bildschirm */
  WORD    width;                /* Breite des Popups in Zeichen (ohne Schieber) */
  WORD    height;               /* H�he des Popups in Zeilen */
  WORD    char_width;           /* Breite eines Zeichens */
  WORD    char_height;          /* H�he eines Zeichens */
  WORD    x_offset;             /* X-Abstand der Zeichen vom linken Rand */
  WORD    y_offset;             /* Y-Abstand der Zeichen vom oberen Rand */
  WORD    y_alt;                /* Y-Alternative, falls nicht ganz in Desktop */
  WORD    font;                 /* Zeichensatz */
  WORD    point;                /* Gr��e des Fonts */
  WORD    color;                /* Farbe des Fonts */
  LONG    num_items;            /* Anzahl verf�gbarer Eintr�ge */
  WORD    vis_items;            /* Anzahl sichtbarer Eintr�ge  */
  LONG    first_item;           /* Erster Eintrag in Popup */
  LONG    selected;             /* Selektierter Eintrag in Popup */
  VOID    *memory;              /* Privater Speicher */
} POPUP_SPEC;

/****** VARIABLES ************************************************************/

/****** FUNCTIONS ************************************************************/

GLOBAL BOOLEAN init_popup _((VOID));
GLOBAL BOOLEAN term_popup _((VOID));

GLOBAL LONG    hndl_popup _((POP pop, UWORD message, BYTE *s));

#endif /* __POPUP__ */

