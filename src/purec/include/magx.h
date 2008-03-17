/************************************************************************/
/*      MAGX.H      #defines for MAGIC Extensions                       */
/*              started 01/11/91 Andreas Kromke                         */
/*                                                                      */
/*     for TURBO C                                                      */
/************************************************************************/

#if  !defined( __AES__ )
#define OBJECT void
#endif

/* ProgramHeader, Program header for executable files                   */
/************************************************************************/

typedef struct {
   int  ph_branch;        /* 0x00: Must be 0x601a!!        */
   long ph_tlen;          /* 0x02: Length of TEXT segments */
   long ph_dlen;          /* 0x06: Length of DATA segments */
   long ph_blen;          /* 0x0a: Length of BSS  segments */
   long ph_slen;          /* 0x0e: Length of symbol table  */
   long ph_res1;          /* 0x12: */
   long ph_res2;          /* 0x16: */
   int  ph_flag;          /* 0x1a: */
} PH;

/* New GEMDOS calls */

#define Sconfig(a,b)    gemdos(0x33, (int) (a), (long) (b)) /* KAOS 1.2 */
/* extern long Sconfig(int mode, long value); */
#define Fshrink(a)      Fwrite(a, 0L, (void *) -1L)         /* KAOS 1.2 */
#define Mgrow(a,b)      Mshrink(a,b)                        /* KAOS 1.2 */
#define Mblavail(a)     Mshrink(a,-1L)                      /* KAOS 1.2 */

/* Sconfig modes */

#define SC_GET   0                                          /* KAOS 1.2 */
#define SC_SET   1                                          /* KAOS 1.2 */
#define SC_VARS  2                                          /* KAOS 1.4 */

/* Sconfig bits */

#define SCB_PTHCK   0x001                                   /* KAOS 1.2 */
#define SCB_DSKCH   0x002                                   /* KAOS 1.2 */
#define SCB_BREAK   0x004                                   /* KAOS 1.2 */
#define SCB_NCTLC   0x008                                   /* KAOS 1.2 */
#define SCB_NFAST   0x010                                   /* KAOS 1.2 */
#define SCB_CMPTB   0x020                                   /* KAOS 1.4 */
#define SCB_NSMRT   0x040                                   /* KAOS 1.4 */
#define SCB_NGRSH   0x080                                   /* KAOS 1.4 */
#define SCB_NHALT   0x100                                   /* KAOS 1.4 */
#define SCB_RESVD   0x200                                   /* KAOS 1.4 */
#define SCB_PULLM   0x400                                   /* KAOS 1.4 */

/* Sconfig(2) -> */

typedef struct
   {
   char      *in_dos;                 /* Address of DOS semaphores   */
   int       *dos_time;               /* Address of DOS time         */
   int       *dos_date;               /* Address of DOS date         */
   long      res1;                    /*                             */
   long      res2;                    /*                             */
   long      res3;                    /* Is 0L                       */
   void      *act_pd;                 /* Running program             */
   long      res4;                    /*                             */
   int       res5;                    /*                             */
   void      *res6;                   /*                             */
   void      *res7;                   /* Internal DOS memory list    */
   void      (*resv_intmem)();        /* Extend DOS memory block     */
   long      (*etv_critic)();         /* etv_critic of GEMDOS        */
   char *    ((*err_to_str)(char e)); /* Conversion Code->Clear text */
   long      res8;                    /*                             */
   long      res9;                    /*                             */
   long      res10;                   /*                             */
   } DOSVARS;

/* Memory Control Block */

typedef struct
     {
     long mcb_magic;                    /* 'ANDR' or 'KROM' (last)    */
     long mcb_len;                      /* Net length                 */
     long mcb_owner;                    /* PD *                       */
     long mcb_prev;                     /* Previous block or NULL     */
     char mcb_data[0];
     } MCB;

/* GEMDOS device handles */

#define HDL_CON -1                                          /* TOS */
#define HDL_AUX -2                                          /* TOS */
#define HDL_PRN -3                                          /* TOS */
#define HDL_NUL -4                                          /* KAOS 1.2 */

/* GEMDOS standard handles */

#define STDIN   0                                           /* TOS */
#define STDOUT  1                                           /* TOS */
#define STDAUX  2                                           /* TOS */
#define STDPRN  3                                           /* TOS */
#define STDERR  4                                           /* TOS */
#define STDXTRA 5                                           /* TOS */

/* GEMDOS error codes */

#define EBREAK -68L /* User break (^C)                         KAOS 1.2 */
#define EXCPT  -69L /* 68000- exception ("bombs")              KAOS 1.2 */
#define EPTHOV -70L /* Path overflow                           MAGIC    */

/* GEMDOS Pexec modes */

#define EXE_LDEX    0                                       /* TOS */
#define EXE_LD      3                                       /* TOS */
#define EXE_EX      4                                       /* TOS */
#define EXE_BASE    5                                       /* TOS */
#define EXE_EXFR    6                                       /* TOS 1.4 */

/* GEMDOS Fseek modes */

#define SEEK_SET    0                                       /* TOS */
#define SEEK_CUR    1                                       /* TOS */
#define SEEK_END    2                                       /* TOS */

/* os_magic -> */

typedef struct
     {
     long magic;                   /* Must be $87654321               */
     void *membot;                 /* End of AES variables            */
     void *aes_start;              /* Start address                   */
     long magic2;                  /* Is 'MAGX'                       */
     long date;                    /* Creation date ddmmyyyy          */
     void (*chgres)(int res, int txt);  /* Change resolution          */
     long (**shel_vector)(void);   /* Resident desktop                */
     char *aes_bootdrv;            /* Booting was from here           */
     int  *vdi_device;             /* VDI driver used by AES          */
     void *reservd1;
     void *reservd2;
     void *reservd3;
     int  version;                 /* e.g. $0201 is V2.1              */
     int  release;                 /* 0=alpha..3=release              */
     } AESVARS;

/* Cookie MagX --> */

typedef struct
     {
     long    config_status;
     DOSVARS *dosvars;
     AESVARS *aesvars;
     } MAGX_COOKIE;

/* tail for default shell */

typedef struct
     {
     int  dummy;                   /* ein Nullwort               */
     long magic;                   /* 'SHEL', wenn ist Shell     */
     int  isfirst;                 /* erster Aufruf der Shell    */
     long lasterr;                 /* letzter Fehler             */
     int  wasgr;                   /* Programm war Grafikapp.    */
     } SHELTAIL;

/* shel_write modes for parameter "isover" */

#define SHW_IMMED        0                                  /* PC-GEM 2.x  */
#define SHW_CHAIN        1                                  /* TOS         */
#define SHW_DOS          2                                  /* PC-GEM 2.x  */
#define SHW_PARALLEL     100                                /* MAG!X       */
#define SHW_SINGLE       101                                /* MAG!X       */

/* menu_bar modes */

#define MENU_HIDE        0                                  /* TOS         */
#define MENU_SHOW        1                                  /* TOS         */
#define MENU_INSTL       100                                /* MAG!X       */

/* objc_edit definition */

#define ED_CRSR          100                                /* MAG!X       */
#define ED_DRAW          103                                /* MAG!X 2.00  */

/* Event definition */

#define WM_UNTOPPED      30                                 /* GEM  2.x    */
#define WM_ONTOP         31                                 /* AES 4.0     */
#define WM_BOTTOMED      33                                 /* AES 4.1     */
#define WM_ICONIFY       34                                 /* AES 4.1     */
#define WM_UNICONIFY     35                                 /* AES 4.1     */
#define WM_ALLICONIFY    36                                 /* AES 4.1     */
#define SH_WDRAW         72                                 /* MultiTOS    */
#define CH_EXIT          90                                 /* MultiTOS    */
#define WM_M_BDROPPED    100                                /* KAOS 1.4    */
#define SM_M_RES1        101                                /* MAG!X       */
#define SM_M_RES2        102                                /* MAG!X       */
#define SM_M_RES3        103                                /* MAG!X       */
#define SM_M_RES4        104                                /* MAG!X       */
#define SM_M_RES5        105                                /* MAG!X       */
#define SM_M_RES6        106                                /* MAG!X       */
#define SM_M_RES7        107                                /* MAG!X       */
#define SM_M_RES8        108                                /* MAG!X       */
#define SM_M_RES9        109                                /* MAG!X       */

/* AES wind_s/get()- Modes */

#define WF_ICONIFY       26                                 /* AES 4.1     */
#define WF_UNICONIFY     27                                 /* AES 4.1     */
#define WF_UNICONIFYXYWH 28                                 /* AES 4.1     */
#define WF_M_BACKDROP    100                                /* KAOS 1.4    */
#define WF_M_OWNER       101                                /* KAOS 1.4    */
#define WF_M_WINDLIST    102                                /* KAOS 1.4    */

/* Window definition */

#define HOTCLOSEBOX      0x1000                             /* GEM 2.x     */
#define BACKDROP         0x2000                             /* KAOS 1.4    */
#define ICONIFIER        0x4000                             /* AES 4.1     */
#define SMALLER ICONIFIER

/* MAG!X Object types  */

#define G_SWBUTTON       34                                 /* MAG!X       */
#define G_POPUP          35                                 /* MAG!X       */

typedef struct {
     char *string;                 	/* Perhaps "TOS|KAOS|MAGIC"            */
     int  num;                      /* No. of current character string     */
     int  maxnum;                   /* Maximum permitted <num>             */
     } SWINFO;

typedef struct {
     OBJECT *tree;                  /* Popup menu                          */
     int  obnum;                    /* Current object of <tree>            */
     } POPINFO;

/* Object states */

#define WHITEBAK         0x40                               /* TOS         */
#define DRAW3D           0x80                               /* GEM 2.x     */

/* form_xdo definitions */

typedef struct {
     char scancode;
     char nclicks;
     int  objnr;
     } SCANX;

typedef struct {
     SCANX *unsh;
     SCANX *shift;
     SCANX *ctrl;
     SCANX *alt;
     void  *resvd;
     } XDO_INF;

/* AES function prototypes */

void _appl_yield    ( void );                               /* TOS         */
int  vq_aes         ( void );                               /* TOS         */
void appl_yield     ( void );                               /* GEM 2.x     */
void appl_bvset     ( int  disks,  int harddisks );         /* GEM 2.x     */
void shel_rdef      ( char *cmd, char *dir );               /* GEM 2.x     */
void shel_wdef      ( char *cmd, char *dir );               /* GEM 2.x     */
int  menu_unregister( int menu_id );                        /* GEM 2.x     */
int  scrp_clear     ( void );                               /* GEM 2.x     */
int  xgrf_stepcalc  (                                       /* GEM 2.x     */
                      int orgw, int orgh,
                      int xc, int yc, int w, int h,
                      int *cx, int *cy,
                      int *stepcnt, int *xstep, int *ystep
                    );
int  xgrf_2box      (                                       /* GEM 2.x     */
                      int xc, int yc, int w, int h,
                      int corners, int stepcnt,
                      int xstep, int ystep, int doubled
                    );
int  menu_click     ( int val, int setit );                 /* GEM 3.x     */
int  form_popup     ( OBJECT *tree, int x, int y );         /* MAG!X       */
int  form_xerr      ( long errcode, char *errfile );        /* MAG!X       */
int  form_xdo       (                                       /* MAG!X       */
                      OBJECT *tree, int startob,
                      int *lastcrsr, XDO_INF *tabs,
                      void *flydial
                    );
int  form_xdial     (                                       /* MAG!X       */
                      int flag,
                      int ltx, int lty, int ltw, int lth,
                      int bgx, int bgy, int bgw, int bgh,
                      void **flydial
                    );

