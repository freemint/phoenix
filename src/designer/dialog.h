/*****************************************************************************
 *
 * Module : DIALOG.H
 * Author : Dieter Gei�
 *
 * Creation date    : 25.05.90
 * Last modification: $Id$
 *
 *
 * Description: This module defines the dialog definitions.
 *
 * History:
 * 11.10.93: Function call_helpfunc added
 * 25.05.90: Creation of body
 *****************************************************************************/

#ifndef __DIALOG__
#define __DIALOG__

/****** DEFINES **************************************************************/

#define CLASS_DIALOG  3                 /* Klasse Dialogfenster */

#define NUM_SEP       5                 /* Anzahl Separatoren */
#define SEP_OPEN      '['               /* Zeichen f�r Separator offen */
#define SEP_CLOSE     ']'               /* Zeichen f�r Separator geschlossen */
#define SEP_LINE      '|'               /* Zeichen f�r Zeilentrenner */

/****** TYPES ****************************************************************/

typedef BOOLEAN (*HELPFUNC) _((BYTE *helpmsg));

/****** VARIABLES ************************************************************/

/****** FUNCTIONS ************************************************************/

GLOBAL BOOLEAN init_dialog   _((BYTE **alerts, OBJECT *tree, WORD index, BYTE *title));
GLOBAL BOOLEAN term_dialog   _((VOID));

GLOBAL VOID    set_helpfunc  _((HELPFUNC help));
GLOBAL VOID    call_helpfunc _((BYTE *helptext));

GLOBAL WINDOWP crt_dialog    _((OBJECT *obj, OBJECT *menu, WORD icon, BYTE *title, UWORD flags));
GLOBAL BOOLEAN open_dialog   _((WORD icon));

GLOBAL WORD    hndl_alert    _((WORD alert_id));
GLOBAL WORD    open_alert    _((BYTE *alertmsg));
GLOBAL BOOLEAN set_alert     _((BOOLEAN as_dialog));

GLOBAL VOID    hndl_modal    _((BOOLEAN use_timer));

#endif /* __DIALOG__ */

