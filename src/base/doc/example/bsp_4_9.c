#include "import.h"
#include "phoenix.h"

#define TBL_KUNDEN 20 /* Nummer der Kunden-Tabelle */ 
#define MAX_NAME 29 /* Maximale Anzahl von Buchstaben */

typedef CHAR NAMEN [MAX_NAME + 1]; /* Typ fur Vorname und Nachname */

typedef struct /* Struktur fur Kunden */ 
{ 
	LONG address; /* Adresse des Datensatzes */ 
	LONG KuNummer; /* Kundennummer */ 
	NAMEN Vorname; /* Vorname des Kunden */ 
	NAMEN Nachname; /* Nachname des Kunden */ 
} KUNDEN; 

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
	CURSOR *cursor; 
	SHORT err, status; 
	LONG address; 
	BOOLEAN ok; 
	KUNDEN kunde;
	init_conv (short_month, long_month, '.', ','); 
	db_init (0);
	base = db_open ("KUNDEN", "", BASE_FLUSH, 16L, 20,"Geiss", "XYZ");
	if (base == NULL) 
	{ 
		err = db_status (base); 
		printf ("Fehler: %d\n", err); 
		return (0); 
	} /* if */
	kunde.KuNummer = 4711; 
	cursor = db_newcursor (base);
	if (cursor != NULL) 
	{ 
		if (! db_search (base, TBL_KUNDEN, 1, ASCENDING, cursor, &kunde, 0L)) 
			printf ("Datensatz nicht gefunden\n"); 
		else 
		{ 
			address = db_readcursor (base, cursor, NULL); 
			if (address != 0) 
				if (db_reclock (base, address)) 
					ok = db_delete (base, TBL_KUNDEN, address, &status);
			if (! ok) 
				printf ("Fehler beim Loschen: %d\n", status); 
		} /* else */ 
	} /* if */ 
	else printf ("Kein Cursor mehr frei!\n");
	if (base != NULL) 
		db_close (base);
	return (0); 
} /* main */
