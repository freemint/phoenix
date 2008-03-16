#include "import.h"
#include "phoenix.h"

#define TBL_KUNDEN 20 /* Nummer der Kunden-Tabelle */ 
#define TBL_RECHNUNGEN 21 /* Nummer der Rechnungen-Tabelle */
#define COLS_KUNDEN 4 /* Anzahl der Spalten in KUNDEN */ 
#define COLS_RECHNUNGEN 5 /* Anzahl der Spalten in RECHNUNGEN */
#define INXS_KUNDEN 3 /* Anzahl der Indizes in KUNDEN */ 
#define INXS_RECHNUNGEN 3 /* Anzahl der Indizes in RECHNUNGEN */
#define MAX_NAME 29 /* Maximale Anzahl von Buchstaben */

typedef CHAR NAMEN[MAX_NAME + 1]; /* Typ fur Vorname und Nachname */
typedef struct /* Struktur fur Kunden */ 
{ 
	LONG address; /* Adresse des Datensatzes */ 
	LONG KuNummer; /* Kundennummer */ 
	NAMEN Vorname; /* Vorname des Kunden */ 
	NAMEN Nachname; /* Nachname des Kunden */ 
} KUNDEN;

typedef struct /* Struktur fur Rechnungen */ 
{ 
	LONG address; /* Adresse des Datensatzes */ 
	LONG ReNummer; /* Rechnungsnummer */ 
	LONG KuNummer; /* Kundennummer */ 
	DATE Datum; /* Datum der Rechnung */ 
	DOUBLE Preis; /* Gesamtpreis der Rechnung */ 
} RECHNUNGEN;

LOCAL SYSTABLE systable [] = 
{ 
	{0L, TBL_KUNDEN, "KUNDEN", 0, COLS_KUNDEN, INXS_KUNDEN, 
	sizeof (KUNDEN), 1, 0, 0, 0, GRANT_ALL},
	{0L, TBL_RECHNUNGEN, "RECHNUNGEN", 0, COLS_RECHNUNGEN, INXS_RECHNUNGEN, 
	sizeof (RECHNUNGEN), 1, 0, 0, 0, GRANT_ALL} 
}; /* SYSTABLE */

LOCAL SYSCOLUMN syscolumn [] = 
{ 
	{0L, TBL_KUNDEN, 0, "Address", TYPE_DBADDRESS, 
	OFFSET (KUNDEN, address), sizeof (LONG), TYPE_DBADDRESS, 
	GRANT_ALL | COL_ISINDEX},
	{0L, TBL_KUNDEN, 1, "KuNummer", TYPE_LONG, 
	OFFSET (KUNDEN, KuNummer), sizeof (LONG), TYPE_LONG, 
	GRANT_ALL | COL_ISINDEX},
	{0L, TBL_KUNDEN, 2, "Vorname", TYPE_CHAR, 
	OFFSET (KUNDEN, Vorname), sizeof (NAMEN), TYPE_CHAR, 
	GRANT_ALL},
	{0L, TBL_KUNDEN, 3, "Nachname", TYPE_CHAR, 
	OFFSET (KUNDEN, Nachname), sizeof (NAMEN), TYPE_CHAR, 
	GRANT_ALL},
	{0L, TBL_RECHNUNGEN, 0, "Address", TYPE_DBADDRESS, 
	OFFSET (RECHNUNGEN, address), sizeof (LONG), TYPE_DBADDRESS, 
	GRANT_ALL | COL_ISINDEX},
	{0L, TBL_RECHNUNGEN, 1, "ReNummer", TYPE_LONG, 
	OFFSET (RECHNUNGEN, ReNummer), sizeof (LONG), TYPE_LONG, 
	GRANT_ALL | COL_ISINDEX},
	{0L, TBL_RECHNUNGEN, 2, "KuNummer", TYPE_LONG, 
	OFFSET (RECHNUNGEN, KuNummer), sizeof (LONG), TYPE_LONG, 
	GRANT_ALL | COL_ISINDEX},
	{0L, TBL_RECHNUNGEN, 3, "Datum", TYPE_DATE, 
	OFFSET (RECHNUNGEN, Datum), sizeof (DATE), TYPE_DATE, 
	GRANT_ALL},
	{0L, TBL_RECHNUNGEN, 4, "Preis", TYPE_FLOAT, 
	OFFSET (RECHNUNGEN, Preis), sizeof (DOUBLE), TYPE_FLOAT, 
	GRANT_ALL} 
}; /* SYSCOLUMN */

LOCAL SYSINDEX sysindex [] = 
{ 
	{0L, TBL_KUNDEN, 0, "", TYPE_DBADDRESS, 0L, 0L, 
	{1 * sizeof (INXCOL), {{0, 0}}}, 0},
	{0L, TBL_KUNDEN, 1, "", TYPE_LONG, 0L, 0L, 
	{1 * sizeof (INXCOL), {{1, 0}}}, INX_PRIMARY},
	{0L, TBL_KUNDEN, 2, "", TYPE_CHAR, 0L, 0L, 
	{2 * sizeof (INXCOL), {{2, MAX_NAME}, {3, MAX_NAME}}}, 0},
	{0L, TBL_RECHNUNGEN, 0, "", TYPE_DBADDRESS, 0L, 0L, 
	{1 * sizeof (INXCOL), {{0, 0}}}, 0},
	{0L, TBL_RECHNUNGEN, 1, "", TYPE_LONG, 0L, 0L, 
	{1 * sizeof (INXCOL), {{1, 0}}}, INX_PRIMARY},
	{0L, TBL_RECHNUNGEN, 2, "", TYPE_LONG, 0L, 0L, 
	{1 * sizeof (INXCOL), {{2, 0}}}, 0}
}; /* SYSINDEX */

LOCAL CHAR *short_month [12] = 
{ 
	"Jan", "Feb", "Mar", "Apr", "Mai", "Jun", 
	"Jul", "Aug", "Sep", "Okt", "Nov", "Dez" 
};

LOCAL CHAR *long_month [12] = 
{ 
	"Januar", "Februar", "Marz", "April", "Mai", "Juni", "Juli", 
	"August", "September", "Oktober", "November", "Dezember" 
};
 
INT main ()
{
	BASE *base; 
	SHORT err, i, status;
	init_conv (short_month, long_month, '.', ','); 
	db_init (0);
	base = db_create ("KUNDEN", "", 0, 16L, 16L);
	if (base == NULL) 
	{ 
		err = db_status (base); 
		printf ("Fehler beim Anlegen: %d\n", err); 
	} /* if */ 
	else 
	{ 
		for (i = 0; i < sizeof (systable) / sizeof (SYSTABLE); i++) 
			if (! db_insert (base, SYS_TABLE, &systable [i], &status)) 
			{ 
				err = db_status (base); 
				printf ("Fehler beim Einfugen: %d\n", err); 
			} /* if, for */
		for (i = 0; i < sizeof (syscolumn) / sizeof (SYSCOLUMN); i++) 
			if (! db_insert (base, SYS_COLUMN, &syscolumn [i], &status)) 
			{ 
				err = db_status (base); 
				printf ("Fehler beim Einfugen: %d\n", err); 
			} /* if, for */
		for (i = 0; i < sizeof (sysindex) / sizeof (SYSINDEX); i++) 
			if (! db_insert (base, SYS_INDEX, &sysindex [i], &status)) 
			{ 
				err = db_status (base); 
				printf ("Fehler beim Einfugen: %d\n", err); 
			} /* if, for */ 
	} /* else */
	if (base != NULL) 
		db_close (base);
	return (0); 
} /* main */
