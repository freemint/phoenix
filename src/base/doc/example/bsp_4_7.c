#include "import.h"
#include "phoenix.h"

#define TBL_KUNDEN 20 /* Nummer der Kunden-Tabelle */ 
#define TBL_RECHNUNGEN 21 /* Nummer der Rechnungen-Tabelle */
#define MAX_NAME 29 /* Maximale Anzahl von Buchstaben */

typedef CHAR NAMEN [MAX_NAME + 1]; /* Typ fur Vorname und Nachname */

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
	SHORT err, status; 
	BOOLEAN ok; 
	KUNDEN kunde; 
	RECHNUNGEN rechnung;
	init_conv (short_month, long_month, '.', ','); 
	db_init (0);
	base = db_open ("KUNDEN", "", BASE_FLUSH, 16L, 20, "Geiss", "XYZ");
	if (base == NULL) 
	{ 
		err = db_status (base); 
		printf ("Fehler: %d\n", err); 
		return (0); 
	} /* if */
	kunde.KuNummer = 4711; 
	strcpy (kunde.Vorname, "Marcus"); 
	strcpy (kunde.Nachname, "Status");
	ok = db_insert (base, TBL_KUNDEN, &kunde, &status); 
	if (! ok) 
		printf ("Status: %d\n", status);
	rechnung.ReNummer = 1; 
	rechnung.KuNummer = 4711; 
	rechnung.Datum.day = 19; 
	rechnung.Datum.month = 06; 
	rechnung.Datum.year = 91; 
	rechnung.Preis = 1300.85;
	ok = db_insert (base, TBL_RECHNUNGEN, &rechnung, &status); 
	if (! ok) 
		printf ("Status: %d\n", status);
	if (base != NULL) 
		db_close (base);
	return (0); 
} /* main */
