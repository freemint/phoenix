/*****************************************************************************/
/*                                                                           */
/* Modul: PRINTER.H                                                          */
/* Datum: 23/11/89                                                           */
/*                                                                           */
/*****************************************************************************/

#ifndef __PRINTER__
#define __PRINTER__

/****** DEFINES **************************************************************/

/****** TYPES ****************************************************************/

/****** VARIABLES ************************************************************/

/****** FUNCTIONS ************************************************************/

GLOBAL BOOLEAN prn_ready    _((WORD port));
GLOBAL BOOLEAN prn_check    _((WORD port));

GLOBAL BOOLEAN init_printer _((VOID));
GLOBAL BOOLEAN term_printer _((VOID));

#endif /* __PRINTER__ */

