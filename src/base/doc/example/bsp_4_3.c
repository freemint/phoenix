#include "import.h"
#include <phoenix.h>

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
{ /* wie oben */ 
	BYTE s [80];
	DATE date;
	TIME time;
	TIMESTAMP timestamp;
	SHORT w = 21;
	LONG l = 123456L;
	DOUBLE d = 1234.56;
	init_conv (short_month, long_month, '.', ',');
	gets (s);
	str2bin (TYPE_WORD, s, &w);
	printf ("WORD: %d\n\n", w);
	gets (s);
	str2bin (TYPE_LONG, s, &l);
	printf ("LONG: %ld\n\n", l);
	gets (s);
	str2bin (TYPE_FLOAT, s, &d);
	printf ("DOUBLE: %lf\n\n", d);
	gets (s);
	str2bin (TYPE_DATE, s, &date); 
	printf ("DATE: %02d.%02d.%02d\n\n", date.day, date.month, date.year);
	gets (s);
	str2bin (TYPE_TIME, s, &time); 
	printf ("TIME: %02d:%02d:%02d.%ld\n\n",time.hour,time.minute,time.second,time.micro);
	gets (s);
	str2bin (TYPE_TIMESTAMP, s, &timestamp); 
	printf ("TIMESTAMP: %02d.%02d.%02d %02d:%02d:%02d.%ld\n\n",
			timestamp.date.day,
			timestamp.date.month, 
			timestamp.date.year, 
			timestamp.time.hour, 
			timestamp.time.minute, 
			timestamp.time.second, 
			timestamp.time.micro);
	return (0);
}
