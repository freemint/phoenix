/* PHOENIX System C-Interface */
/* Database: ORGANIZE */

#define TBL_ADRESSEN         20
#define TBL_TELEFON_BEZ      21
#define TBL_ADRESSE_BEZ      22
#define TBL_KATEGORIE_BEZ    23
#define TBL_NOTIZEN          24
#define TBL_AUFGABEN         25
#define TBL_WIEDERHOLEN      26
#define TBL_PRIORITŽT        27
#define TBL_INIT             28
#define TBL_SONSTIGES_BEZ    29

typedef struct
{
  LONG      DbAddress;             /*     0 */
  BYTE      Vorname [26];          /*     4 */
  BYTE      Nachname [26];         /*    30 */
  BYTE      Anrede [26];           /*    56 */
  BYTE      Spitzname [26];        /*    82 */
  BYTE      Hauptadresse [26];     /*   108 */
  BYTE      Strase [26];           /*   134 */
  BYTE      PLZ [26];              /*   160 */
  BYTE      Ort [26];              /*   186 */
  BYTE      Land [26];             /*   212 */
  BYTE      Telefon_1_Bez [26];    /*   238 */
  BYTE      Telefon_1 [26];        /*   264 */
  BYTE      Telefon_2_Bez [26];    /*   290 */
  BYTE      Telefon_2 [26];        /*   316 */
  BYTE      Telefon_3_Bez [26];    /*   342 */
  BYTE      Telefon_3 [26];        /*   368 */
  BYTE      Telefon_4_Bez [26];    /*   394 */
  BYTE      Telefon_4 [26];        /*   420 */
  BYTE      Firma [42];            /*   446 */
  BYTE      Titel [26];            /*   488 */
  BYTE      Abteilung [26];        /*   514 */
  BYTE      Zweitaddresse [26];    /*   540 */
  BYTE      Strase_2 [26];         /*   566 */
  BYTE      PLZ_2 [26];            /*   592 */
  BYTE      Ort_2 [26];            /*   618 */
  BYTE      Land_2 [26];           /*   644 */
  BYTE      Sonstiges_1_Bez [26];  /*   670 */
  BYTE      Sonstiges_1 [26];      /*   696 */
  BYTE      Sonstiges_2_Bez [26];  /*   722 */
  BYTE      Sonstiges_2 [26];      /*   748 */
  BYTE      Sonstiges_3_Bez [26];  /*   774 */
  BYTE      Sonstiges_3 [26];      /*   800 */
  BYTE      Sonstiges_4_Bez [26];  /*   826 */
  BYTE      Sonstiges_4 [26];      /*   852 */
  BYTE      Kategorie_1 [26];      /*   878 */
  BYTE      Kategorie_2 [26];      /*   904 */
  DATE      Geandert;              /*   930 */
  WORD      Nummer;                /*   934 */
  BYTE      Markiert [2];          /*   936 */
} ADRESSEN;

typedef struct
{
  LONG      DbAddress;             /*     0 */
  BYTE      Telefon_Bez [26];      /*     4 */
} TELEFON_BEZ;

typedef struct
{
  LONG      DbAddress;             /*     0 */
  BYTE      Adresse [26];          /*     4 */
} ADRESSE_BEZ;

typedef struct
{
  LONG      DbAddress;             /*     0 */
  BYTE      Kategorie_Bez [26];    /*     4 */
} KATEGORIE_BEZ;

typedef struct
{
  LONG      DbAddress;             /*     0 */
  BYTE      Notiztitel [52];       /*     4 */
  DATE      Datum;                 /*    56 */
  TIME      Uhrzeit;               /*    60 */
  BYTE      Notiz [1002];          /*    68 */
  BYTE      Kategorie_1 [26];      /*  1070 */
  BYTE      Kategorie_2 [26];      /*  1096 */
  DATE      Geandert;              /*  1122 */
} NOTIZEN;

typedef struct
{
  LONG      DbAddress;             /*     0 */
  BYTE      Aufgabe [52];          /*     4 */
  DATE      Fallig_Datum;          /*    56 */
  TIME      Fallig_Uhrzeit;        /*    60 */
  DATE      vorher_am;             /*    68 */
  BYTE      Prioritat [26];        /*    72 */
  BYTE      Montag [2];            /*    98 */
  BYTE      Dienstag [2];          /*   100 */
  BYTE      Mittwoch [2];          /*   102 */
  BYTE      Donnerstag [2];        /*   104 */
  BYTE      Freitag [2];           /*   106 */
  BYTE      Samstag [2];           /*   108 */
  BYTE      Sonntag [2];           /*   110 */
  BYTE      Wiederholen [26];      /*   112 */
  BYTE      Kategorie_1 [26];      /*   138 */
  BYTE      Kategorie_2 [26];      /*   164 */
} AUFGABEN;

typedef struct
{
  LONG      DbAddress;             /*     0 */
  BYTE      Wiederholen [26];      /*     4 */
} WIEDERHOLEN;

typedef struct
{
  LONG      DbAddress;             /*     0 */
  BYTE      Prioritat [26];        /*     4 */
} PRIORITAT;

typedef struct
{
  LONG      DbAddress;             /*     0 */
  BYTE      Init [26];             /*     4 */
} INIT;

typedef struct
{
  LONG      DbAddress;             /*     0 */
  BYTE      Sonstiges_Bez [26];    /*     4 */
} SONSTIGES_BEZ;

