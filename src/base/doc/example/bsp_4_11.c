#include "import.h"
#include "phoenix.h"

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
	SHORT err,status; 
	LONG address;
	init_conv (short_month, long_month, '.', ','); 
	db_init (0);
	base = db_open ("KUNDEN", "", BASE_FLUSH, 16L, 20,"Geiss", "XYZ");
	if (base == NULL) 
	{ 
		err = db_status (base);
		printf ("Fehler: %d\n", err); 
		return (0); 
	} /* if */
	printf ("Adresse des Datensatzes: "); 
	scanf ("%ld\n", &address); 
	if (! db_undelete (base, address, &status)) 
		printf ("Fehler %d beim Zuruckholen des Datensatzes!\n", status);
	if (base != NULL) 
		db_close (base);
	return (0); 
} /* main */
