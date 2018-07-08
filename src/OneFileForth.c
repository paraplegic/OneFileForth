/*
  -- OneFileForth:  The evolution of a C based forth interpreter
  first written in 1982 (RForth), and re-written in 2008 to be 
  embedded on [sic] less powerful hardware.  This (third) version 
  was designed from the outset to be variable cell sized, compiled 
  from a single source file, and with the ability to run on hosted 
  operating systems or natively on the more capable SoC's on the 
  smaller end.

  Written by Robert S. Sciuk of Control-Q Research 
  Copyright 1982 - 2017.

  LICENSE:

  This code is offered into the public domain with no usage restrictions 
  under similar terms to SQLite, but with the one additional proviso that 
  OneFileForth and/or its derivatives must never be encumbered in any way 
  by a more restrictive license, and most specifically no GPL license of 
  any sort (including LGPL) may be applied.  Otherwise, and to quote 
  Dr. D.R. Hipp, author of SQLite:

	"May you do good and not evil
	 May you find forgiveness for yourself and forgive others
	 May you share freely, never taking more than you give."

  To use, simply download this file, and use your trusty C compiler:

	clang -o off -D NOCHECK -O2 OneFileForth.c

  Some hosted systems might require the -ldl option, and embedded systems
  might require a bit more effort.  No documentation is yet available, 
  and no warranty is offered, expressed or implied.  Enjoy.

*/

#define MAJOR		"00"
#define MINOR		"01"
#define REVISION	"66"

#include <stdarg.h>
#include <stdint.h>

// trying to avoid this to allow
// bare metal support ...
#ifdef NEVER
#include <stdio.h>
#include <string.h>
#endif

#ifndef register
#define register
#endif

#if defined( unix ) & !defined(linux)
#include <sys/time.h>
#include <sys/stdint.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>
#include <termios.h>
#include <time.h>
#include <locale.h>
#define HOSTED
#endif 

#if defined( linux ) || defined( __APPLE__ ) 
#include <stdint.h>
#include <unistd.h>
#include <termios.h>
#include <sys/time.h>
#include <sys/times.h>
#include <sys/types.h>
#include <sys/select.h>
#include <time.h>
#include <locale.h>
#define HOSTED
#endif

#if defined (__WIN32__)
#include <stdint.h>
#define HOSTED
#endif

// word size will be derived from the architecture
// compiler predefines, and inferred as follows:
#if defined (avr) || defined (AVR) || defined( __riscv )
#define NATIVE
#include <stdint.h>
#define _WORDSIZE 	2
#ifndef NULL
#define NULL 0
#endif
#endif

#if defined (i386) || defined (__arm__) || defined (AVR32) || defined(powerpc) || defined( __riscv )
#define _WORDSIZE 	4
#endif

#if defined __x86_64
#define _WORDSIZE 	8
#endif

// by pushing NATIVE on the compile line, we will
// override the HOSTED (default) mode, and not rely
// upon system calls including read()/write() to exist.
#ifdef NATIVE
#undef HOSTED
#define sz_FLASH	8192		// cells
#define FLASH_INIT_VAL	0xdead
#endif

#define sz_INBUF		127		// bytes
#define sz_STACK		32		// cells
#define sz_ColonDefs 		1024		// # entries
#define sz_TMPBUFFER		2048		// total buffer queue
#define nm_TMPBUFFER		8		// number of buffers

#ifndef sz_FLASH
#define sz_FLASH	16384		// cells
#define FLASH_INIT_VAL	0xdeadbeef
#endif

#ifdef HOSTED
#include <stdlib.h>
#include <fcntl.h>

#include <setjmp.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <sys/stat.h>
#include <dlfcn.h>

volatile sig_atomic_t sigval = 0 ;

#define OFF_PATH	"OFF_PATH"

#if !defined( __WIN32__ )
struct termios tty_normal_state ;
#endif
#define FLAVOUR 		"Hosted"
#define sz_FILES		4			/* nfiles */
#define INPUT			InputStack[ in_This ].file
#define OUTPUT			out_files[ out_This ]
jmp_buf env ;
#endif

#ifdef NATIVE
#define FLAVOUR 		"Native"
#define sz_FILES		1			/* nfiles */
#define INPUT			0
#define OUTPUT			1
#ifndef NULL
#define NULL			0
#endif
#endif

int  					in_This = -1, in_files[ sz_FILES ] = { 0 } ;
int  					out_This = 0, out_files[ sz_FILES ] = { 1 } ;

#if _WORDSIZE == 2
typedef int16_t		Wrd_t ;
typedef uint16_t	uWrd_t ;
typedef int8_t		Hlf_t ;
typedef uint8_t		uHlf_t ;
#define _HALFMASK	0xff
#endif

#if _WORDSIZE == 4
typedef int32_t		Wrd_t ;
typedef uint32_t	uWrd_t ;
typedef int16_t		Hlf_t ;
typedef uint16_t	uHlf_t ;
#define _HALFMASK	0xffff
#endif

#if _WORDSIZE == 8
typedef int64_t		Wrd_t ;
typedef uint64_t	uWrd_t ;
typedef int32_t		Hlf_t ;
typedef uint32_t	uHlf_t ;
#define _HALFMASK	0xffffffff
#endif

#if !defined(_WORDSIZE)
typedef int		Wrd_t ;
typedef unsigned int	uWrd_t ;
typedef short int	Hlf_t ;
typedef unsigned Hlf_t	uHlf_t ;
#endif

typedef void		(*Fptr_t)() ;
typedef Wrd_t		(*Cptr_t)() ;
typedef char *		Str_t ;
typedef int8_t		Byt_t ;
typedef uint8_t		uByt_t ;
typedef void		Nul_t ;
typedef void *		Opq_t ;

typedef Wrd_t		Cell_t ;
typedef uWrd_t		uCell_t ;

#define StartOf(x)	(&x[0])

//  -- a data stack ...
Cell_t stack[sz_STACK+1] = { 0 } ;
Cell_t *tos = (Cell_t *) StartOf( stack ) ;

//  -- and a return stack ...
Cell_t rstack[sz_STACK+1] = { 0 } ;
Cell_t *rtos = (Cell_t *) StartOf( rstack ) ;

//  -- and a user stack ...
Cell_t ustack[sz_STACK+1] = { 0 } ;
Cell_t *utos = (Cell_t *) StartOf( ustack ) ;

//  -- some input and scratch buffers ...
Byt_t  input_buffer[ sz_FILES * sz_INBUF ] = { 0 } ;
Byt_t *inbuf[] = {
	(Byt_t *) (input_buffer + (0 * sz_INBUF)),
#ifdef HOSTED
	(Byt_t *) (input_buffer + (1 * sz_INBUF)),
	(Byt_t *) (input_buffer + (2 * sz_INBUF)),
	(Byt_t *) (input_buffer + (3 * sz_INBUF)),
#endif // HOSTED
	NULL
} ;

// temp buffer circular queue ... 
// see implementation below for details.
#define CQ_MAX_BUFFER 65535
#define CQ_MIN_CHUNKS 1

typedef struct _cque_ {
  Str_t  cq_memory ;
  Wrd_t  cq_memsize ;
  Wrd_t  cq_n_elements ;
  Wrd_t  cq_chunksize ;
  Wrd_t  cq_next ;
  Str_t  cq_buffer ;
} Cir_Queue_t ;

//   temp buffer public interface ...
Cir_Queue_t     *tb_create( Cir_Queue_t *Q, Byt_t *chunk, Wrd_t size, Wrd_t n_elements ) ;
Cir_Queue_t     *tb_destroy( Cir_Queue_t *CQ ) ;
int              tb_bufsize( Cir_Queue_t *CQ ) ;
int            	 tb_nbufs( Cir_Queue_t *CQ ) ;
void            *tb_get( Cir_Queue_t *CQ ) ;

Cir_Queue_t T ;
Cir_Queue_t *TB = (Cir_Queue_t *) NULL ;
Byt_t  tmp_buffer[sz_TMPBUFFER] = { 0 } ;

uCell_t _ops = 0 ; 

#ifdef HOSTED
Str_t	off_path = (Str_t) NULL ;
#endif

//  -- useful macros ...
#define v_Off		0
#define v_On		1
#define push( x )	*(++tos) = (Cell_t) x
#define pop()		*(tos--)
#define nos		 tos[-1]
#define rpush( x )	*(++rtos) = x
#define rpop()		*(rtos--)
#define upush( x )	*(++utos) = x
#define upop()		*(utos--)
#define rnos		(rtos-1)
#define isNul( x )	(x == NULL)
#define WHITE_SPACE	" \t\r\n"
#define EOL			"\n\r"
#define inEOF		"<eof>"
#define MaxStr( x, y )	((str_length( x ) > str_length( y )) ? str_length( x ) : str_length( y ))
#define isMatch( x, y )	(str_match( (char *) x, (char *) y, MaxStr( (char *) x, (char *) y )))
#define __THIS__        ( (Str_t) __FUNCTION__ )
#define throw( x )	err_throw( __THIS__, __LINE__, x )
#define Abs( x )	((x < 0) ? (x*-1) : x) 
#define UNUSED( x )	x __attribute__((unused))

// nocheck determines argument checking at runtime, and also
// will determine if temporary buffers are cleared 
#ifdef NOCHECK
#define chk( x )	{}
#define dbg		'F'
#else
#define chk( x )	do { if( !checkstack( x, (Str_t) __func__ ) ) return ; } while(0)
#define dbg		'D'
#endif


/*
  -- forth primitives must be pre-declared ...
*/
Wrd_t checkstack(); 

void quit(); 
void banner(); 
void add(); 
void subt(); 
void mult(); 
void exponent(); 
void divide(); 
void modulo(); 
void absolute(); 
void dotS(); 
void dot(); 
void udot(); 
void bye(); 
void prompt(); 
void words(); 
void rdepth(); 
void depth(); 
void dupe(); 
void rot(); 
void nip(); 
void tuck(); 
void qdupe(); 
void drop(); 
void over(); 
void swap(); 
void pick(); 
void toR(); 
void Rto(); 
void Eof(); 
void cells(); 
void cellsize(); 
void wrd_fetch(); 
void wrd_store(); 
void reg_fetch(); 
void reg_store(); 
void crg_fetch(); 
void crg_store(); 
void hlf_fetch(); 
void hlf_store(); 
void byt_fetch(); 
void byt_store(); 
void lft_shift();
void rgt_shift();
void cmove();
void word();
void ascii();
void q_key();
void key();
void emit();
void type();
void cr();
void dp();
void stringptr();
void flashsize();
void flashptr();
void here();
void freespace();
void comma();
void doLiteral();
void colon();
void compile();
void semicolon();
void call() ;
void execute() ;
void doColon() ;
void tick() ;
void nfa() ;
void cfa() ;
void pfa() ;
void decimal() ;
void hex() ;
void sigvar() ;
void errvar() ;
void errval() ;
void errstr() ;
void errmax() ;
void base() ;
void trace() ;
void resetter() ;
void cold() ;
void see() ;
void pushPfa() ;
void does() ;
void allot() ;
void create() ;
void lambda() ;
void constant() ;
void variable() ;
void pvState() ;
void imState() ;
void normal() ;
void immediate() ;
void unresolved();
void fwd_mark();
void fwd_resolve();
void bkw_mark();
void bkw_resolve();
void q_branch();
void branch();
void begin();
void again();
void While();
void Repeat();
void Leave();
void Until();
void If();
void Else();
void Then();
void lt();
void gt();
void ge();
void le();
void eq();
void ne();
void And();
void and();
void or();
void xor();
void not();
void Buf();
void nBufs();
void pad();
void comment();
void flushtoeol();
void dotcomment();
void quote();
void dotquote();
void count();
void ssave();
void unssave();
void infile();
void filename();
void outfile();
void closeout();
#ifdef HOSTED
void isfile();
void sndtty();
void rcvtty();
void opentty();
void closetty();
void waitrdy();
void qdlopen();
void qdlclose();
void qdlsym();
void qdlerror();
void spinner();
void path();
#endif /* HOSTED */
void last_will();
void callout();
void clkspersec();
void plusplus();
void minusminus();
void utime();
void ops();
void noops();
void qdo();
void do_do();
void do_I();
void loop();
void do_loop();
void ploop();
void do_ploop();
void forget();
void fmt_start();
void fmt_digit();
void fmt_num();
void fmt_hold();
void fmt_sign();
void fmt_end();
void utf8_encode();
void accept();
void dump();
void find();
void version();
void code();
void data();
void align();
void fill();
void it_set();
void it_reset();
void it_doit( int sig );

/*
  -- dictionary is simply an array of struct ...
*/

typedef enum {
  Normal,
  Immediate,
  Undefined
} Flag_t ;

typedef struct _inbuf_ {
  Wrd_t file ;
  Wrd_t bytes_read ;
  Wrd_t bytes_this ;
  Wrd_t in_line ;
  Str_t name ;
  Str_t bytes ;
} Input_t ;

Input_t InputStack[sz_FILES] = {
	{ .file = -1, .bytes_read = -1, .bytes_this = -1, .in_line = 0, .name = (Str_t) NULL, .bytes = (Str_t) &inbuf[0] },
#ifdef HOSTED
	{ .file = -1, .bytes_read = -1, .bytes_this = -1, .in_line = 0, .name = (Str_t) NULL, .bytes = (Str_t) &inbuf[1] },
	{ .file = -1, .bytes_read = -1, .bytes_this = -1, .in_line = 0, .name = (Str_t) NULL, .bytes = (Str_t) &inbuf[2] },
	{ .file = -1, .bytes_read = -1, .bytes_this = -1, .in_line = 0, .name = (Str_t) NULL, .bytes = (Str_t) &inbuf[3] },
#endif // HOSTED
} ;

typedef struct _dict_ {
  Fptr_t  cfa ;
  Str_t   nfa ;
  Flag_t  flg ;
  Cell_t  *pfa ;
} Dict_t ;

Dict_t Primitives[] = {
  { quit, 	"quit", Normal, NULL },
  { banner,	"banner", Normal, NULL },
  { add,	"+", Normal, NULL },
  { subt,	"-", Normal, NULL },
  { mult,	"*", Normal, NULL },
  { exponent,	"^", Normal, NULL },
  { divide,	"/", Normal, NULL },
  { modulo,	"%", Normal, NULL },
  { absolute,	"abs", Normal, NULL },
  { dotS,	".s", Normal, NULL },
  { dot,	".", Normal, NULL },
  { udot,	"u.", Normal, NULL },
  { bye,	"bye", Normal, NULL },
  { words,	"words", Normal, NULL },
  { rdepth,	"rdepth", Normal, NULL },
  { depth,	"depth", Normal, NULL },
  { dupe,	"dup", Normal, NULL },
  { qdupe,	"?dup", Normal, NULL },
  { rot,	"rot", Normal, NULL },
  { nip,	"nip", Normal, NULL },
  { tuck,	"tuck", Normal, NULL },
  { drop,	"drop", Normal, NULL },
  { over,	"over", Normal, NULL },
  { swap,	"swap", Normal, NULL },
  { pick,	"pick", Normal, NULL },
  { toR,	">r", Normal, NULL },
  { Rto,	"r>", Normal, NULL },
  { Eof,	inEOF, Normal, NULL },
  { cells,	"cells", Normal, NULL },
  { cellsize,	"cellsize", Normal, NULL },
  { wrd_fetch,	"@", Normal, NULL },
  { wrd_store,	"!", Normal, NULL },
  { reg_fetch,	"r@", Normal, NULL },
  { reg_store,	"r!", Normal, NULL },
  { crg_fetch,	"cr@", Normal, NULL },
  { crg_store,	"cr!", Normal, NULL },
  { hlf_fetch,	"h@", Normal, NULL },
  { hlf_store,	"h!", Normal, NULL },
  { byt_fetch,	"c@", Normal, NULL },
  { byt_store,	"c!", Normal, NULL },
  { lft_shift,	"<<", Normal, NULL },
  { rgt_shift,	">>", Normal, NULL },
  { cmove,	"cmove", Normal, NULL },
  { word,	"word", Normal, NULL },
  { ascii,	"ascii", Immediate, NULL },
  { q_key,	"?key", Normal, NULL },
  { key,	"key", Normal, NULL },
  { emit,	"emit", Normal, NULL },
  { type,	"type", Normal, NULL },
  { cr,		"cr", Normal, NULL },
  { dp,		"dp", Normal, NULL },
  { stringptr,	"strings", Normal, NULL },
  { flashsize,	"flashsize", Normal, NULL },
  { flashptr,	"flash", Normal, NULL },
  { here,	"here", Normal, NULL },
  { freespace,	"freespace", Normal, NULL },
  { comma,	",", Normal, NULL },
  { doLiteral,	"(literal)", Normal, NULL },
  { colon,	":", Normal, NULL },
  { semicolon,	";", Normal, NULL },
  { execute,	"execute", Normal, NULL },
  { call,	"call", Normal, NULL },
  { doColon,	"(colon)", Normal, NULL },
  { tick,	"'", Immediate, NULL },
  { nfa,	">name", Normal, NULL },
  { cfa,	">code", Normal, NULL },
  { pfa,	">body", Normal, NULL },
  { decimal,	"decimal", Normal, NULL },
  { hex,	"hex", Normal, NULL },
  { base,	"base", Normal, NULL },
  { trace,	"trace", Normal, NULL },
  { sigvar,	"sigval", Normal, NULL },
  { errvar,	"err_var", Normal, NULL },
  { errval,	"err_val", Normal, NULL },
  { errstr,	"err_str", Normal, NULL },
  { errmax,	"err_max", Normal, NULL },
  { resetter,	"warm", Normal, NULL },
  { cold,	"cold", Normal, NULL },
  { see,	"see", Normal, NULL },
  { pushPfa,	"(variable)", Normal, NULL },
  { allot,	"allot", Normal, NULL },
  { create,	"create", Normal, NULL },
  { lambda,	"lambda", Normal, NULL },  // ( <str> -- ) 
  { does,	"does>", Normal, NULL },
  { constant,	"constant", Normal, NULL },
  { variable,	"variable", Normal, NULL },
  { normal,	"normal", Normal, NULL },
  { immediate,	"immediate", Normal, NULL },
  { imState,	"[", Immediate, NULL },
  { pvState,	"]", Immediate, NULL },
  { unresolved,	"unresolved", Normal, NULL },
  { fwd_mark,	">mark", Normal, NULL },
  { fwd_resolve,">resolve", Normal, NULL },
  { bkw_mark,	"<mark", Normal, NULL },
  { bkw_resolve,"<resolve", Normal, NULL },
  { q_branch,	"?branch", Normal, NULL },
  { branch,	"branch", Normal, NULL },
  { begin,	"begin", Immediate, NULL },
  { again,	"again", Immediate, NULL },
  { While,	"while", Immediate, NULL },
  { Repeat,	"repeat", Immediate, NULL },
  { Until,	"until", Immediate, NULL },
  { Leave,	"leave", Normal, NULL },
  { If,		"if", Immediate, NULL },
  { Else,	"else", Immediate, NULL },
  { Then,	"then", Immediate, NULL },
  { lt,		"<", Normal, NULL },
  { gt,		">", Normal, NULL },
  { ge,		">=", Normal, NULL },
  { le,		"<=", Normal, NULL },
  { eq,		"==", Normal, NULL },
  { ne,		"!=", Normal, NULL },
  { And,	"&", Normal, NULL },
  { and,	"and", Normal, NULL },
  { or,		"or", Normal, NULL },
  { xor,	"xor", Normal, NULL },
  { not,	"not", Normal, NULL },
  { Buf,	"buf", Normal, NULL },
  { nBufs,	"nbufs", Normal, NULL },
  { Buf,	"scratch", Normal, NULL },
  { pad,	"pad", Normal, NULL },
  { comment,	"(", Immediate, NULL },
  { flushtoeol,	"\\", Immediate, NULL },
  { dotcomment,	".(", Immediate, NULL },
  { quote,	"\"", Immediate, NULL },
  { dotquote,	".\"", Immediate, NULL },
  { count,	"count", Normal, NULL },
  { ssave,	"save", Normal, NULL },
  { unssave,	"unsave", Normal, NULL },
  { infile,	"infile", Normal, NULL },
  { filename,	"filename", Normal, NULL },
  { outfile,	"outfile", Normal, NULL },
  { closeout,	"closeout", Normal, NULL },
#ifdef HOSTED
  { isfile,	"isfile", Normal, NULL },
  { opentty,	"opentty", Normal, NULL },
  { closetty,	"closetty", Normal, NULL },
  { sndtty,	"sndtty", Normal, NULL },
  { waitrdy,	"waitrdy", Normal, NULL },
  { rcvtty,	"rcvtty", Normal, NULL },
  { qdlopen,	"dlopen", Normal, NULL },
  { qdlclose,	"dlclose", Normal, NULL },
  { qdlsym,	"dlsym", Normal, NULL },
  { qdlerror,	"dlerror", Normal, NULL },
  { last_will,	"atexit", Normal, NULL },
  { spinner,	"spin", Normal, NULL },
  { path,	"path", Normal, NULL }, // ( -- ptr )
  { it_set, "it_set", Normal, NULL },
  { it_reset, "it_reset", Normal, NULL },
  { it_doit, "it_doit", Normal, NULL },
#endif /* HOSTED */
  { callout,	"native", Normal, NULL },
  { clkspersec,	"clks", Normal, NULL },
  { plusplus,	"++", Normal, NULL },
  { minusminus,	"--", Normal, NULL },
  { utime,	"utime", Normal, NULL },
  { ops,	"ops", Normal, NULL },
  { noops,	"noops", Normal, NULL },
  { qdo,	"do", Immediate, NULL },
  { do_do,	"(do)", Normal, NULL },
  { do_I,	"i", Normal, NULL },
  { loop,	"loop", Immediate, NULL },
  { do_loop,	"(loop)", Normal, NULL },
  { ploop,	"+loop", Immediate, NULL },
  { do_ploop,	"(+loop)", Normal, NULL },
  { forget,	"forget", Normal, NULL },
  { fmt_start,	"<#", Normal, NULL },
  { fmt_digit,	"#", Normal, NULL },
  { fmt_num,	"#s", Normal, NULL },
  { fmt_hold,	"hold", Normal, NULL },
  { fmt_sign,	"sign", Normal, NULL },
  { fmt_end,	"#>", Normal, NULL },
  { utf8_encode, "utf8", Normal, NULL }, // ( ch buf len -- len )
  { accept,	"accept", Normal, NULL }, // ( buf len -- n )
  { find,	"find", Normal, NULL }, // ( ptr -- dp | 0  )
  { version,	"version", Normal, NULL }, // ( -- Mjr Mnr Rev )
  { code,	"code", Normal, NULL }, // ( -- adr )
  { data,	"data", Normal, NULL }, // ( -- adr )
  { align,	"align", Normal, NULL }, // ( adr -- adr' )
  { fill,	"fill", Normal, NULL }, // ( adr -- adr' )
  { NULL, 	NULL, 0, NULL }
} ;

Dict_t Colon_Defs[sz_ColonDefs] ;
Cell_t n_ColonDefs = 0 ;

Cell_t flash[sz_FLASH] = { FLASH_INIT_VAL } ;
Cell_t *flash_mem = StartOf( flash ) ;

// Some global state variables (see forget();)
Cell_t *Here ;
Cell_t *DictPtr ;
Byt_t  *String_Data = NULL ;
Byt_t  *String_LowWater = NULL ;
Cell_t  Base = 10 ;
Cell_t  Trace = 0 ;

typedef enum {
 state_Interactive,
 state_Compiling,
 state_Interpret,
 state_Immediate,
 state_Undefined
} State_t ;

State_t state = state_Interactive ;
State_t state_save = state_Interactive ;

/*
 -- error codes and strings
*/
typedef enum {
  err_OK = 0,
  err_StackOvr,
  err_StackUdr,
  err_DivZero,
  err_NoInput,
  err_BadBase,
  err_BadLiteral,
  err_BufOvr,
  err_NullPtr,
  err_NoSpace,
  err_BadState,
  err_UnResolved,
  err_CaughtSignal,
  err_Unsave,
  err_NoWord,
  err_TknSize,
  err_SysCall,
  err_BadString,
  err_NoFile,
  err_InStack,
  err_Range,
  err_Undefined
} Err_t ;

Str_t errors[] = {
  "-- Not an error.",
  "-- Stack overflow.",
  "-- Stack underflow.",
  "-- Division by zero.",
  "-- No more input.",
  "-- Radix is out of range.",
  "-- Bad literal conversion.",
  "-- Buffer overflow.",
  "-- NULL pointer.",
  "-- Dictionary space exhausted.",
  "-- Bad state.",
  "-- Unresolved branch.",
  "-- Caught a signal.",
  "-- Too late to un-save.",
  "-- No such word exists.",
  "-- Tkn too large.",
  "-- System call glitch.",
  "-- Bad String.",
  "-- No file access.",
  "-- Input stack overflow.",
  "-- Range error.",
  "-- Undefined error.",
  NULL,
} ;

typedef enum  {
  rst_unexpected = 0,
  rst_signalhdlr = 1,
  rst_catch = 2,
  rst_application = 3,
  rst_checkstack = 4,
  rst_coldstart = 5,
  rst_user = 6
} check_pt ;

Str_t resetfrom[] = {
  "unexpected",
  "sig_hdlr",
  "catch handler",
  "application",
  "checkstack",
  "cold start",
  "user",
  NULL
} ;

Wrd_t promptVal ;
Str_t promptStr[] = {
  "ok ",
  "-- ",
  NULL
} ;

Str_t error_loc = (Str_t) NULL ;
Err_t error_code = 0 ;
Str_t digits = { "0123456789abcdefghijklmnopqrstuvwxyz" } ;

/*
 -- string and character handling stuff which converts
    numbers, reads tokens and performs the platform
    specific I/O.
*/

void catch() ;
void err_throw( Str_t f, Wrd_t l, Err_t e ) ;
Wrd_t put_str( Str_t s );
Wrd_t get_str( Wrd_t fd, Str_t buf, Wrd_t len );
Wrd_t inp( Wrd_t fd, Str_t buf, Wrd_t len );
Wrd_t outp( Wrd_t fd, Str_t buf, Wrd_t len );
Wrd_t str_match( Str_t a, Str_t b, Wrd_t len );
Wrd_t str_length( Str_t str );
Wrd_t str_literal( Str_t tkn, Wrd_t radix );
Wrd_t str_format( Str_t dst, Wrd_t dlen, Str_t fmt, ... );
Wrd_t str_format_ap( Str_t dst, Wrd_t dlen, Str_t fmt, va_list ap );
void str_set( Str_t dst, Byt_t dat, Wrd_t len );
Wrd_t str_copy( Str_t dst, Str_t src, Wrd_t len );
Wrd_t str_utoa( uByt_t *dst, Wrd_t dlen, Cell_t val, Wrd_t radix );
Wrd_t str_ntoa( Str_t dst, Wrd_t dlen, Cell_t val, Wrd_t radix, Wrd_t isSigned );
Str_t str_token( Input_t *inptr );
Str_t str_delimited( Str_t term ) ;
Str_t str_cache( Str_t tag );
Str_t str_seal( void );
Str_t str_uncache( Str_t tag );
Wrd_t ch_matches( Byt_t ch, Str_t anyOf );
Byt_t ch_tolower( Byt_t b );
Wrd_t utf8_encoder( Wrd_t ch, Str_t buf, Wrd_t len );
Wrd_t ch_index( Str_t str, Byt_t c );
void sig_hdlr( int sig );
Wrd_t io_cbreak( int fd );
Wrd_t fmt_out( Str_t fmt, ... );

#ifdef HOSTED

void sig_hdlr( int sig ){
  sigval = sig ;
  throw( err_CaughtSignal ) ;
//  if( sigval == SIGSEGV ){
//    catch();
//  }
  catch() ;
  return ;
}

Str_t  in_File = (Str_t) NULL ;
Str_t  in_Word = (Str_t) NULL ;
Cell_t quiet = 0 ;
Dict_t *lookup( Str_t tkn );
static int do_x_Once = 1 ;

void usage(int argc, char **argv )
{
  Wrd_t nx ;
  nx = fmt_out( "usage:\n\t%s [-i <infile>] [-q] [-x <word>]\n\n", argv[0] ) ;
}

#define STD_ARGS "i:x:qt"
void chk_args( int argc, char **argv )
{
  int ch, err=0 ; 
  while( (ch = getopt( argc, argv, STD_ARGS )) != -1 )
  {
    switch( ch )
    {
        case 'i':
          in_File = str_cache( optarg ) ;
          break ;
        case 'x':
          in_Word = str_cache( optarg ) ; 
          break ;
        case 'q':
          quiet++ ;
          break ;
		case 't':
		  push( 1 );
		  trace() ;
		  wrd_store();
          break ;
        default:
          err++ ;
    }
  }
  if( err )
  {
     usage( argc, argv );
     exit( 1 );
  }
  return ;
}

#endif // HOSTED

Str_t  Locale = (Str_t) NULL ;
Byt_t  found_eol = (Byt_t) 0 ;

// reset never forgets ...
// forget does that (see below).
void q_reset()
{

#ifdef HOSTED
  sigval = 0 ;
  signal( SIGINT, sig_hdlr ) ;
#ifndef __WIN32__
  signal( SIGQUIT, sig_hdlr ) ;
  signal( SIGHUP, sig_hdlr ) ;
  signal( SIGKILL, sig_hdlr ) ;
  signal( SIGBUS, sig_hdlr ) ;
  signal( SIGFPE, sig_hdlr ) ;
  signal( SIGSEGV, sig_hdlr ) ;
#endif
#endif

  decimal() ;
  promptVal = 0 ; 

  tos = (Cell_t *) StartOf( stack ) ; 
  *tos = FLASH_INIT_VAL ;

  rtos = (Cell_t *) StartOf( rstack ) ;
  *rtos = FLASH_INIT_VAL ;

  error_code = err_OK ;
  state = state_Interactive ;

}

/*
  -- innards of the `machine'.
*/
#ifdef NATIVE

void tracker( const char *F, int L );
void raise(){tracker((const char *) __FUNCTION__, __LINE__);}

#define x_UART_BASE 0x101F1000UL
#define x_UARTDR    (x_UART_BASE+0x000)
#define x_UARTFR    (x_UART_BASE+0x018)
volatile unsigned int * const UARTDR = (unsigned int *)0x101f1000;      // UART0 data register
volatile unsigned int * const UARTFR = (unsigned int *)0x101f1018;      // UART0 flag register
volatile unsigned int * const UARTCR = (unsigned int *)0x101f1030;      // UART0 control register
volatile unsigned int * const UARTLCR_H = (unsigned int *)0x101f102c;   // UART0 line control register


// defined in the assembler file ...
int GET8( unsigned adr );
int GET32( unsigned adr );
int PUT32( unsigned adr, unsigned char ch );

void uart_putc ( unsigned int c )
{
    while( ( *(UARTFR) & 0x20 ) != 0 );
    *(UARTDR) = c & 0xff ;
}

Cell_t uart_getc_ne( void )
{
  int ch ; 
  while ( *(UARTFR) == 0x90 ) ;

  ch = *(UARTDR) ;
  return ch ;
}

Cell_t uart_getc( void )
{
  int ch ; 
  while ( *(UARTFR) == 0x90 ) ;

  ch = *(UARTDR) ;
  uart_putc( ch ) ;
  return ch ;
}

int uart_can_recv( void )
{
   if( (*UARTFR) == 0x90 )
     return( 0 );

  return( 1 );
}

void uart_init(void)
{
   // GET32( x_UARTDR );
   return;
}

int notmain( void )
{
  uart_init();

#else // NATIVE vs HOSTED ... 

int main( int argc, char **argv )
{

#endif

  forget() ; // puts the system in a known state ...
  q_reset() ;
  push( "stdin" ) ; 
  infile() ; 

#ifdef HOSTED
  Locale = str_cache( (Str_t) setlocale( LC_ALL, "" ) ) ;
  off_path = str_cache( getenv( OFF_PATH ) ) ;
  chk_args( argc, argv ) ;
  if( !isNul( in_File ) )
  {
     push( (Str_t) in_File );
     infile() ;
  } 
  if( !isNul( in_Word ) )
  {
      // quiet++ ; 
      do_x_Once = 0 ; 
      push( (Cell_t) lookup( in_Word ) ) ;
      execute() ;
  }
#else
  Locale = str_cache( "EMBEDDED" ) ;
#endif

  str_seal() ;
  banner() ;
  quit() ;
  return 0 ;
}

Wrd_t ch_matches( Byt_t ch, Str_t anyOf )
{
  Str_t  p ;

  p = (Str_t) StartOf( anyOf ) ;
  while( *p ){
    if( ch == *(p++) ){
      return 1 ;
    }
  }
  return 0 ; 
}

Byt_t ch_tolower( Byt_t b )
{
 if( b <= 'Z' && b >= 'A' ){
  return b ^ 0x20 ;
 }
 return b & 0xFF ;
}

Wrd_t utf8_encoder( Wrd_t ch, Str_t buf, Wrd_t len )
{

  str_set( buf, 0, len ) ;
  if (ch < 0x80) {
        buf[0] = (char)ch;
        return( 1 ) ;
  }

  if (ch < 0x800) {
        buf[0] = (ch>>6) | 0xC0;
        buf[1] = (ch & 0x3F) | 0x80;
	return( 2 ) ;
  }

  if (ch < 0x10000) {
        buf[0] = (ch>>12) | 0xE0;
        buf[1] = ((ch>>6) & 0x3F) | 0x80;
        buf[2] = (ch & 0x3F) | 0x80;
        return( 3 );
  }

  if (ch < 0x110000) {
        buf[0] = (ch>>18) | 0xF0;
        buf[1] = ((ch>>12) & 0x3F) | 0x80;
        buf[2] = ((ch>>6) & 0x3F) | 0x80;
        buf[3] = (ch & 0x3F) | 0x80;
        return( 4 ) ;
  }

  return( 0 ) ;
}

Wrd_t ch_index( Str_t str, Byt_t c )
{
  Byt_t *p, *start ; 

  p = start = (Byt_t *) StartOf( str ) ;
  while( *p ){
    if( *p == c ){
      return p - start ;
    }
    p++ ;
  }
  return -1;
}

Str_t str_token( Input_t *input )
{
  int tkn = 0 ;
  Byt_t this_char ;
  static Byt_t acc[sz_INBUF] ;

  found_eol = (Byt_t) 0 ; 
  do {
		if( input->bytes_read < 1 )
		{
			prompt() ;
			str_set( input->bytes, 0, sz_INBUF ) ;
			input->bytes_read = inp( INPUT, input->bytes, sz_INBUF ) ;
			if( input->bytes_read == 0 )
			{
				input->bytes[0] = (Byt_t) 0 ; 
				return inEOF ;
			}
			input->bytes_this = 0 ; 
			continue ;
		}

		if( input->bytes_this > (input->bytes_read - 1) )
		{
			input->bytes_this = input->bytes_read = -1 ;
			continue ;
		}


		// accumulate printing characters in the accumulater for the next token ...
		this_char = input->bytes[input->bytes_this++] ;

		if( !ch_matches( this_char, WHITE_SPACE ) )
		{
			acc[tkn++] = this_char ;
			acc[tkn] = (Byt_t) 0 ; 
			continue ; 
		}

	  	// errors and comments require eol, so flag it in a global.
		if( ch_matches( this_char, EOL ) )
		{
			input->in_line++ ;
			found_eol = this_char ;
		}

		// have a token, return the accumulator ...
		if( tkn > 0 )
		{
			return (Str_t) acc ;
		}

		// null tokens are simply ignored ... 
		if( found_eol )
		{
			return (Str_t) NULL ;
		}

  } while( 1 ) ;
}

Wrd_t str_match( Str_t a, Str_t b, Wrd_t len )
{
  int8_t i ;

  if( (str_length( a ) == len) && (str_length( b ) == len) ){
    for( i = 0 ; i < len ; i++ ){
      if( a[i] != b[i] ){
        return 0 ;
      }
    }
    return 1 ;
  }
  return 0 ;
}

Wrd_t str_length( Str_t str )
{
  Str_t  p ; 
  Wrd_t ret = 0 ;

  if( isNul( str ) ){
    return 0 ;
  }

  p = str ;
  while( *p++ ) ret++ ;
  return ret ; 
}

Wrd_t str_literal( Str_t tkn, Wrd_t radix )
{
  Wrd_t  ret, sign, digit, base ;
  Str_t p ;

  if( radix > str_length( digits ) ){
    put_str( tkn ) ;
    throw( err_BadBase ) ;
    return -1 ;
  }

  sign = 1 ;
  base = radix ;
  p = tkn ; 
  switch( *p++ ){
    case '-': /* negative */
     sign = -1 ;
     break ;
    case '+': /* positive */
     sign = 1 ;
     break ;
    case '$': /* hex constant */
     base = 16 ;
     break ;
    case '0': /* octal or hex constant */
     base = 8 ;
     if( *p == 'x' || *p == 'X' ){
      base = 16 ;
      p++ ;
     }
     break ;
    default: /* none of the above ... start over */
     p = tkn ;
     break ;
   }

   ret = 0 ; 
   while( *p ){
     digit = ch_index( digits, ch_tolower( *p++ ) ) ;
     if( digit < 0 || digit > (base - 1) ){
       fmt_out( "-- %s digit: '%x'\n", tkn, digit ) ;
       throw( err_BadLiteral ) ;
       return -1 ;
     }
     ret *= base ; 
     ret += digit ;
   }
   ret *= sign ;
   return ret ;
}

void str_set( Str_t dst, Byt_t dat, Wrd_t len )
{
  Str_t ptr ;

  for( ptr = dst ; ptr - dst < len ; ptr++ ){
    *ptr = (Byt_t) dat & 0xff ;
  } 
}

Wrd_t str_copy( Str_t dst, Str_t src, Wrd_t len )
{
  Wrd_t i ;
  Str_t from, to ; 

  to = dst ;
  from = src ;
  for( i = 0 ; i < len ; i++ ){
    *to++ = *from++ ;
  }
  return i ;
}

Wrd_t str_utoa( uByt_t *dst, Wrd_t dlen, Cell_t val, Wrd_t radix )
{

  uCell_t n, i, dig ;
  uByt_t *p, *q, buf[30] ;

  i = 0 ; 
  n = val ;
  do{
    dig = (n % radix) + '0' ;
    dig = (dig > '9') ? dig - '9' + 'a' - 1 : dig ; 
    buf[i++] = dig ;
    n /= radix ;
  } while( n != 0 ) ;
  buf[i] = (Byt_t) 0 ; 

  n = 0 ; 
  p = dst ;
  q = &buf[i] ;
  do {
    if( n > dlen ){
      throw( err_BufOvr ) ;
      return -1 ;
    }
    *p++ = *q-- ; n++ ;
  } while( q >= &buf[0] ); 
  *p++ = (Byt_t) 0 ; 

  return n ;
}
Wrd_t str_ntoa( Str_t dst, Wrd_t dlen, Cell_t val, Wrd_t radix, Wrd_t isSigned )
{
  Wrd_t i, sign, n ;
  Byt_t c, buf[30] ;
  Str_t p ;

  n = val ;
  sign = (Byt_t) 0 ;  
  if( isSigned && val < 0 ){
    sign = '-' ;
    n = -1 * n ;
  }

  i = 0 ; 
  do {
   c = '0' + (n % radix) ;
   c = (c > '9') ? c - '9' + 'a' - 1 : c ; 
   buf[i++] = c ;
   n /= radix ;
  } while( n != 0 ) ;

  buf[ i ] = ' ' ;
  if( sign ){
    buf[i] = sign ;
  } 

  if( i > dlen ){
    throw( err_BufOvr ) ;
    return -1 ;
  }

  if( !sign ){
    i-- ;
  } 
  n = i + 1 ;
  p = dst ;
  do {
    *p++ = buf[i--] ;
  } while( i > -1 ) ;
  *p++ = (Byt_t) 0 ; 
  return n ;
} 

Wrd_t str_format( Str_t dst, Wrd_t dlen, Str_t fmt, ... )
{
  Wrd_t rv ;
  va_list ap ;

  va_start( ap, fmt );
    rv = str_format_ap( dst, dlen, fmt, ap ) ;
  va_end( ap ) ;
  return rv ;
}
    
Wrd_t str_format_ap( Str_t dst, Wrd_t dlen, Str_t fmt, va_list ap )
{
  va_list ap2 ;
  Str_t p_fmt, p_dst, p_end, str ;
  Byt_t ch ;
  Wrd_t cell ;

  p_end = dst + dlen ;
  p_dst = dst ;
  p_fmt = fmt ;

  va_copy( ap2, ap );
   while( (ch = *(p_fmt++)) && (p_dst < p_end) ){
     if( ch == '%' ){
       ch = *(p_fmt++) ;
       switch( ch ){
        case '%': // %%
          *p_dst++ = ch & 0xff ;
          break ;
        case 'c': // %c
          ch = va_arg( ap2, int );
          *p_dst++ = ch & 0xff ;
          break ;
        case 's': // %s
          str = va_arg( ap2, Str_t );
          p_dst += str_copy( p_dst, str, str_length( str ) ) ;
          break ;
        case 'l': // %l
          ch = *(p_fmt++) ;
        case 'd': // %d
          cell = va_arg( ap2, Cell_t ) ;
          p_dst += str_ntoa( p_dst, dlen - (p_dst - dst) - 1, cell, Base, 1 ) ;
          break ;
        case 'x': // %x
          cell = va_arg( ap2, Cell_t ) ;
          p_dst += str_ntoa( p_dst, dlen - (p_dst - dst) - 1, cell, 16, 0 ) ;
          break ;
        case 'o': // %o
          cell = va_arg( ap2, Cell_t ) ;
          p_dst += str_ntoa( p_dst, dlen - (p_dst - dst) - 1, cell, 8, 0 ) ;
          break ;
        case 'u': // %u
          cell = va_arg( ap2, uCell_t ) ;
          p_dst += str_utoa( (uByt_t *) p_dst, dlen - (p_dst - dst) - 1, (uCell_t) cell, Base ) ;
          break ;
        default:
          break ;
       }
    } else {
      *p_dst++ = ch ;
    }
  }
  if( !isNul( p_dst ) && p_dst < p_end )
  {
    *p_dst++ = (Byt_t) 0 ;
  }
  return p_dst - dst - 1 ;
}

Str_t str_uncache( Str_t tag )
{
  Cell_t len ;

  len = str_length( tag ) + 1 ;
  String_Data += len ;
  return (Str_t) String_Data ;
}

Str_t str_cache( Str_t s )
{

  Cell_t len ;

  if( !isNul( s ) )
  {
    len = str_length( s ) + 1;
    String_Data -= len ;
    str_copy( (Str_t) String_Data, s, len ) ;
  }
  return (Str_t) String_Data ;
}

Str_t str_seal( void )
{
  String_LowWater = String_Data ;
  return (Str_t) String_Data ;
}

Dict_t *lookup( Str_t tkn )
{
  Dict_t *p ;
  Cell_t  i ;

  if( !isNul( tkn ) )
  {
    if( n_ColonDefs > 0 )
    {
      for( i = n_ColonDefs - 1 ; i > -1 ; i-- )
      {
        p = &Colon_Defs[ i ] ;
        if( isMatch( tkn, p ->nfa ) )
        {
          return p ;
        }
      }
    }

    p = StartOf( Primitives ) ;
    while( p ->nfa )
    {
       if( isMatch( tkn, p ->nfa ) )
       {
         return p ;
       }
       p++ ;
    }

  }
  return (Dict_t *) NULL ;

}

/* 
 -- Forth primitives ...
    for visibility within the interpreter, they
    must be pre-declared, and placed in the Primitive[]
    dictionary structure above ...
*/

void quit()
{
  Str_t tkn ;
  Dict_t *dp ;

#ifdef HOSTED
  Wrd_t beenhere = 0, n ;
  beenhere = setjmp( env ) ;
  if( beenhere > 0 ){
    catch();
    n = fmt_out( "-- Reset by %s.\n", resetfrom[beenhere] ) ;
    if( beenhere == rst_coldstart )
      banner() ;
  }
#endif
  for(;;){ // *outer loop*
    while( (tkn = str_token( &InputStack[in_This] )) ){
      dp = lookup( tkn );
      if( isNul( dp ) ){
        push( str_literal( tkn, Base ) ) ;
      } else {
        push( (Cell_t) dp ) ; 
        execute() ;
      }
      catch() ;
    } // *tkn*
  } // *ever*
} // *quit*

void banner()
{
  Wrd_t UNUSED( n );

#ifdef HOSTED
  if( quiet ) return ;
#endif

  n = fmt_out( "-- OneFileForth-%s alpha Version: %s.%s.%s%c (%s)\n", FLAVOUR, MAJOR, MINOR, REVISION, dbg, Locale ) ;
  n = fmt_out( "-- www.ControlQ.com\n\n" ) ;
}

void tracker( const char *fun, int line )
{
  Wrd_t UNUSED( n );

  n = fmt_out( "-- %s: %d\n", fun, line ) ;
}

void prompt()
{
  if( INPUT == 0 ){
    outp( OUTPUT, (Str_t) promptStr[promptVal], 3 ) ;
  }
}

void add()
{
  register Cell_t n ;

  chk( 2 ) ;
  n = pop() ;
  *tos += n ;
}

void subt()
{
  register Cell_t n ;

  chk( 2 ) ;
  n = pop() ;
  *tos -=  n ; 
}

void mult()
{
  register Cell_t n ;

  chk( 2 ) ;
  n = pop() ;
  *tos *= n ;
}

void exponent()
{
  register Cell_t n, exp ;

  chk( 2 ) ;
  n = pop() ;
  exp = *tos; 
  *tos = 1 ;
  for( ; n > 0 ; n-- ){
    *tos *= exp ;
  }
}

void divide()
{
  register Cell_t n ; 
 
  chk( 2 ) ;
  n = pop(); 
  if( n == 0 ){
    throw( err_DivZero ) ;
    return ;
  }
  *tos /= n ; 
}

void modulo()
{
  register Cell_t n ; 
 
  chk( 2 ) ;
  n = pop(); 
  if( n == 0 ){
    throw( err_DivZero ) ;
    return ;
  }
  *tos %= n ; 
}

void absolute() // -n -- n 
{
  *tos = Abs( *tos ) ;
}

void dotS()
{
  Cell_t i, num ;

  chk( 0 ) ; 
  depth() ; num = *tos ; dot() ;
  put_str( " : " ) ;
  for( i = 1; i <= num ; i++ )
  {
     push( stack[i] ) ; dot() ;
  }
}

void dot()
{
  Wrd_t n, val ;
  Str_t buf = tb_get( TB );

  chk( 1 ) ;
  val = pop() ;
  n = str_format( buf, tb_bufsize( TB ), "%d ", val ) ;
  outp( OUTPUT, buf, n ) ;
}

void udot()
{
  Wrd_t UNUSED( n );
  Str_t buf = tb_get( TB );

  chk( 1 ) ;
  n = str_format( buf, tb_bufsize( TB ), "%u ", (uCell_t) pop() ) ;
  outp( OUTPUT, buf, n ) ;
}

void bye()
{
#ifdef HOSTED
  exit( error_code ) ;
#endif
}

void words()
{
  Dict_t *p ;
  Cell_t i, llen, wlen, nwords ;

  llen = 0 ; 
  nwords = 0 ; 
  if( n_ColonDefs > 0 )
  {
    p = StartOf( Colon_Defs ) ;
    for( i = n_ColonDefs - 1 ; i > -1 ; i-- ){
      p = &Colon_Defs[i] ;
      wlen = str_length( p ->nfa ) ;
      if( (llen + wlen) > 72 )
      {
	fmt_out( "\n" );
	llen = 0 ;
      }
      llen += wlen ;
      fmt_out( "%s ", p ->nfa ) ;
      nwords++ ;
    }
  }

  p = StartOf( Primitives ) ;
  while( p ->nfa )
  {
    wlen = str_length( p ->nfa ) ;
    if( (llen + wlen) > 72 )
    {
       fmt_out( "\n" );
       llen = 0 ;
    }
    llen += wlen ;
    fmt_out( "%s ", p ->nfa ) ; 
    nwords++ ;
    p++ ;
  }
  fmt_out( "\n -- %d words.\n", nwords );
}

Wrd_t checkstack( Wrd_t n, Str_t fun )
{
  Wrd_t UNUSED( x ), d ;

  if( n > 0 ) {
    depth(); d = pop() ;
    if( d < n ){
      x = fmt_out( "-- Found %d of %d args expected in '%s'.\n", d, n, fun ) ; 
      throw( err_StackUdr ) ;
#ifdef HOSTED
      longjmp( env, rst_checkstack ) ;
#endif
    }
    return 1 ;
  }

  if( tos < (Cell_t *) StartOf( stack ) ){
    put_str( fun ) ; 
    throw( err_StackUdr ) ;
    return 0 ;
  }

  if( tos > &stack[sz_STACK] ){
    put_str( fun ) ; 
    throw( err_StackOvr ) ;
    return 0 ;
  }
  return 1 ;
}

void unresolved()
{
  throw( err_UnResolved ) ;
}

void fwd_mark()
{
  push( (Cell_t) Here ) ;
  push( (Cell_t) lookup( "unresolved" ) ) ;
  comma() ;
}

void fwd_resolve()
{
  Cell_t *p ;

  p = (Cell_t *) pop() ;
  *p = (Cell_t) Here ;
}

void bkw_mark()
{
  push( (Cell_t) Here ) ;
}

void bkw_resolve()
{
  comma() ;
}

void  begin()
{
  bkw_mark();
}         
  
void  again()
{
  push( (Cell_t) lookup( "branch" ) ) ;
  comma() ;
  bkw_resolve();
}

void  While()
{
  push( (Cell_t) lookup( "?branch" ) );
  comma() ;
  fwd_mark();
  swap();
}

void  Repeat()
{
  push( (Cell_t) lookup( "branch" ) );
  comma() ;
  bkw_resolve();
  fwd_resolve();
}

void  Leave()
{
  if( rtos > rstack )
    *rtos = 0 ; 
}

void  Until()
{
  push( (Cell_t) lookup( "?branch" ) );
  comma() ;
  bkw_resolve() ;
}

void  If()
{
  push( (Cell_t) lookup( "?branch" ) ) ;
  comma() ;
  fwd_mark() ;
}

void Else()
{
  push( (Cell_t) lookup( "branch" ) ) ;
  comma();
  fwd_mark() ;
  swap() ;
  fwd_resolve() ;
}

void Then()
{
  fwd_resolve() ;
}

void  lt()
{
  register Cell_t n ;

  chk( 2 ) ; 
  n = pop() ;
  *tos = (*tos < n) ? 1 : 0 ;
}

void  gt()
{
  register Cell_t n ;

  chk( 2 ) ; 
  n = pop() ;
  *tos = (*tos > n) ? 1 : 0 ;
}

void  ge()
{
  register Cell_t n ;
  
  chk( 2 ) ; 
  n = pop() ;
  *tos = (*tos >= n) ? 1 : 0 ;
}

void  ne()
{
  register Cell_t n ;
  
  chk( 2 ) ; 
  n = pop() ;
  *tos = (*tos != n) ? 1 : 0 ;
}

void  eq()
{
  register Cell_t n ;

  chk( 2 ) ; 
  n = pop() ;
  *tos = (*tos == n) ? 1 : 0 ;
}

void  le()
{
  register Cell_t n ;

  chk( 2 ) ; 
  n = pop() ;
  *tos = (*tos <= n) ? 1 : 0 ;
}

void And()
{
  register Cell_t n ;

  chk( 2 ) ; 
  n = pop() ;
  *tos &= n ;
}

void and()
{
  register Cell_t n ;

  chk( 2 ) ; 
  n = pop() ;
  *tos = *tos && n ;
}

void or()
{
  register Cell_t n ;

  chk( 2 ) ; 
  n = pop() ;
  *tos |= n ;
}

void xor()
{
  register Cell_t n ;

  chk( 2 ) ; 
  n = pop() ;
  *tos ^= n ;
}

void not()
{

  chk( 1 ) ; 
  *tos = ~(*tos) ;
}

void q_branch()
{
  Cell_t *ptr ;

  ptr = (Cell_t *) rpop() ;		// grab next word pointer
  if( pop() ){					// if .T. skip current pointer for next (?branch)
    rpush( (Cell_t) ++ptr ) ;
    return ;
  }
  rpush( *ptr ) ;				// else branch to ptr ... 
}

void branch()
{
  Cell_t *x ;

  x = (Cell_t *) rpop() ;		// always branch to next ... 
  rpush( *x ) ;
}

void rdepth()
{
  Cell_t d ;

  d = rtos - StartOf( rstack ) ;
  push( d ) ; 
}

void depth()
{
  Cell_t d ;

  d = tos - StartOf( stack ) ;
  push( d ) ; 
}

void dupe() // n1 -- n1 n1
{
  register Cell_t n ;

  chk( 1 ) ; 
  n = *tos;
  push( n ) ;
}

void qdupe() // n1 != 0 ? -- n1 n1 : n1
{ 
  register Cell_t n ;

  chk( 1 ) ; 
  if ( *tos )
  {
    n = *tos;
    push( n ) ;
  }
}

void rot() // n1 n2 n3 -- n2 n3 n1
{
  register Cell_t n ;

  chk( 3 ) ;

  n = *(tos-2) ;
  *(tos-2) = *(tos-1) ;
  *(tos-1) = *(tos) ;
  *tos = n ;
}

void nip() // n1 n2 -- n2
{

  chk( 2 ) ;

  swap() ;
  drop() ;
}

void tuck() // n1 n2 -- n2 n1 n2
{

  chk( 2 ) ;

  dupe() ;
  rot() ;
  swap() ;
}

void drop()
{
  chk( 1 ) ; 

  tos-- ;
}

void over() // n1 n2 -- n1 n2 n1 
{
  register Cell_t n ;

  chk( 2 ) ; 
  n = nos ;
  push( n ) ;
}

void Rto()
{
  push( rpop() ) ;
}

void toR()
{

  chk( 1 ) ;
  rpush( pop() ) ;
}

void swap()
{
  register Cell_t t ;

  chk( 2 ) ;
  t = *tos ;
  *tos = nos ;
  nos = t ;
}

void pick()
{
  Wrd_t ix ; 
  Cell_t tval ;
  chk( 1 ) ;
  ix = pop() ;
  depth(); tval = pop() ;
  if( ix < tval ){
    tval = *(tos-ix) ;
    push( tval ) ;
    return ;
  }
  throw( err_StackUdr ) ;
}

void Eof()
{
#ifdef HOSTED
  if( in_This > 0 )
  {
    close( INPUT ) ;
    INPUT = -1 ;
    in_This-- ;
    if( !isNul( in_Word ) && do_x_Once )
    {
      do_x_Once = 0 ; 
      push( (Cell_t) lookup( in_Word ) ) ;
      execute() ;
    }
    return ;
  }
  throw( err_NoInput ) ;
  catch() ;
  exit( 0 ) ;
#else
  return ;
#endif
}

void cells()
{
  chk( 1 ) ;
  *tos *= sizeof( Cell_t ) ;
}

void cellsize()
{
  push( sizeof( Cell_t ) ) ;
}

void err_throw( Str_t whence, Wrd_t line, Err_t err )
{
  Str_t buf = tb_get( TB ) ;

  str_format( buf, tb_bufsize( TB ), "%s():[%d]", whence, line ) ;
  error_loc = buf ;
  error_code = err ;
}

void catch()
{
  Wrd_t UNUSED( sz );
  Input_t *input = &InputStack[ in_This ];

  switch( error_code ){
    case err_OK:
      return ;

#ifdef HOSTED
    case err_CaughtSignal:
      chk( 0 ) ;
      sz = fmt_out( "%s (%d)\n", errors[error_code], error_code ) ;
      if( sigval == SIGSEGV ){
        sz = fmt_out( "-- SIGSEGV (%d) is generally non recoverable.\n", sigval ) ;
        goto reset;
      }

      Fptr_t ok = signal( sigval, sig_hdlr ) ;
      sz = fmt_out( "-- Signal %d handled. (%x)\n", sigval, ok ) ;
      if( sigval == SIGINT )
      {
        sz = fmt_out( "-- warm start suggested.\n" ) ;
        Leave();
      }
      return ;
      goto reset;

    case err_SysCall:
      sz = fmt_out( "%s (%d)\n", errors[error_code], error_code ) ;
      sz = fmt_out( "-- %d %s.\n", errno, (Str_t) strerror( errno ) ) ;
      sz = fmt_out( "-- Thrown by %s.\n", error_loc ) ;
      goto reset;

#endif

    case err_NoInput:
    default:
      chk( 0 ) ;
      sz = fmt_out( "%s (%d)\n", errors[error_code], error_code ) ;
      sz = fmt_out( "-- Error: code is %d.\n", error_code ) ;
      sz = fmt_out( "-- Thrown by %s.\n", error_loc ) ;
      if( error_code <= err_Undefined ){
        sz = fmt_out( "%s (%d).\n", errors[error_code], error_code ) ;
      }
      if( error_code == err_NoInput ){
        goto die ;
      }
      goto reset;
  }

  dump() ;
  sz = fmt_out( "-- Stack Dump: Depth = " ) ;
  dotS() ; cr() ;
  goto reset;

 die:
  dump() ;
  sz = fmt_out( "-- Stack Dump: Depth = " ) ;
  dotS() ; cr() ;
  if( error_code != err_OK && error_code != err_NoInput )
  {
    sz = fmt_out( "-- Abnormal Termination.\n" ) ;
  }
#ifdef HOSTED
  exit( error_code ) ;
#endif

 reset:
  dump() ;
  fmt_out( "-- Last input: %s\n", input->bytes) ; 
  q_reset() ;
  sz = fmt_out( "-- Remaining input flushed.\n" ) ;
  flushtoeol();
  sz = fmt_out( "-- Attempting Reset.\n" ) ;
#ifdef HOSTED
  longjmp( env, rst_catch );
#endif

}

void wrd_fetch()
{
  register Cell_t *p ;

  chk( 1 ) ; 
  p = (Cell_t *) pop() ;
  if( isNul( p ) ){
    throw( err_NullPtr ) ;
    return ;
  }
  push( *p ) ;
}

void wrd_store()
{
  register Cell_t *p, n ;

  chk( 2 ) ; 
  p = (Cell_t *) pop() ;
  n = pop() ;
  if( isNul( p ) ){
    throw( err_NullPtr ) ;
    return ;
  }
  *p = n ;
}

void reg_fetch()
{
  volatile register uWrd_t *p ;

  chk( 2 ) ; 
  p = (uWrd_t *) pop() ;
  if( isNul( p ) ){
    throw( err_NullPtr ) ;
    return ;
  }
  push( *p ) ;
}

void reg_store()
{
  volatile register uWrd_t *p ; 
  register Cell_t n ;

  chk( 2 ) ; 
  p = (uWrd_t *) pop() ;
  n = pop() ;
  if( isNul( p ) ){
    throw( err_NullPtr ) ;
    return ;
  }
  *p = n ;
}

void crg_fetch()
{
  volatile register Byt_t *p ;

  chk( 1 ) ; 
  p = (Byt_t *) pop() ;
  if( isNul( p ) ){
    throw( err_NullPtr ) ;
    return ;
  }
  push( *p & 0xff ) ;
}

void crg_store()
{
  volatile register Byt_t *p ; 
  register Cell_t n ;

  chk( 2 ) ; 
  p = (Byt_t *) pop() ;
  n = pop() ;
  if( isNul( p ) ){
    throw( err_NullPtr ) ;
    return ;
  }
  *p = n & 0xff ;
}

void hlf_fetch()
{
  register Hlf_t *p ;

  chk( 1 ) ; 
  p = (Hlf_t *) pop() ;
  if( isNul( p ) ){
    throw( err_NullPtr ) ;
    return ;
  }
  push( *p & _HALFMASK ) ;
}

void hlf_store()
{
  volatile register Hlf_t *p ; 
  register Cell_t n ;

  chk( 2 ) ; 
  p = (Hlf_t *) pop() ;
  n = pop() ;
  if( isNul( p ) ){
    throw( err_NullPtr ) ;
    return ;
  }
  *p = n & _HALFMASK ;
}

void byt_fetch()
{
  register Byt_t *p ;

  chk( 1 ) ; 
  p = (Byt_t *) pop();
  if( isNul( p ) ){
    throw( err_NullPtr ) ;
    return ;
  }
  push( *p & 0xff) ;
}

void byt_store()
{
  Byt_t *p ; 
  Cell_t n ;

  chk( 2 ) ; 
  p = (Byt_t *) pop() ;
  n = pop() ;
  if( isNul( p ) ){
    throw( err_NullPtr ) ;
    return ;
  }
  *p = (Byt_t) (n & 0xff) ;
}
 
void lft_shift()
{
  register Cell_t n ;

  chk( 2 ) ;
  n = pop() ;
  *tos <<= n ; 
}

void rgt_shift()
{
  register Cell_t n ;

  chk( 2 ) ;
  n = pop() ;
  *tos >>= n ; 
}

void quote()
{
  push( (Cell_t) str_delimited( "\"" ) ) ;
  if( state == state_Compiling ){
    ssave() ;
    push( (Cell_t) lookup( "(literal)" ) ) ;
    comma() ; 
    comma() ;
  }

}

void dotquote()
{
  quote() ;
  if( state == state_Compiling ){
    push( (Cell_t) lookup( "type" ) ) ;
    comma() ; 
    return ;
  }
  type() ;
}

void comment()
{
  push( (Cell_t) str_delimited( ")" ) ) ;
  drop();
}

void flushtoeol()
{
  Str_t UNUSED( tkn ) ;

  // toss tokens until the end of line is reached ... 
  do { tkn = str_token( &InputStack[ in_This ] ) ; } while ( ! found_eol ) ;

}

void dotcomment()
{
  push( (Cell_t) str_delimited( ")" ) ) ;
  type() ;
}

Str_t str_delimited( Str_t terminator )
{
  Str_t tkn, ptr, ret ; 
  Wrd_t len ;

  ++promptVal ;
  pad() ; ptr = ret = (Str_t) pop() ;
  do {
    word() ;
    tkn = (Str_t) pop() ;
    len = str_length( tkn ) ;
    if( tkn[len-1] == *terminator ){
      str_copy( ptr, tkn, len-1 ) ;
      ptr[len-1] = (Byt_t) 0 ; 
      break ;
    }
    str_copy( ptr, tkn, len );
    ptr += len ;
    *ptr++ = ' ' ;
  } while( 1 ) ;
  --promptVal ;
  return( ret ) ;
}

void count()
{
  Wrd_t len ;

  chk( 1 ) ; 
  len = str_length( (Str_t) *tos ) ;
  push( (Cell_t) len ) ;
}

void ssave()
{
  Str_t str ; 

  chk( 1 ) ;
  str = (Str_t) pop() ;
  push( (Cell_t) str_cache( str ) ) ;
}

void unssave()
{
  Byt_t *tag ;

  chk( 1 ) ;
  tag = (Byt_t *) pop();
  if( isMatch( tag, String_Data+2 ) ){
    str_uncache( (Str_t) tag ) ; 
    return ;
  }
  throw( err_Unsave ) ;
}

void nBufs()
{
  push( tb_nbufs( TB ) ) ;
}

void Buf()
{
  push( (Cell_t) tb_get( TB ) ) ;
  push( (Cell_t) tb_bufsize( TB ) ) ;
}

void pad()
{
  here() ;
  push( 20 ) ; 
  cells() ; 
  add() ; 
}

void cmove()  // ( n dst src -- )
{
  Cell_t len ; 
  Str_t src, dst ;

  len = pop() ;
  dst = (Str_t) pop() ;
  src = (Str_t) pop() ;
  str_copy( dst, src, len ) ;
}

void word()
{ 
  Str_t tkn ;
 
  do { tkn = str_token( &InputStack[ in_This ] ) ; } while ( isNul( tkn ) ) ;
  push( (Cell_t) tkn ) ;
}

void ascii()
{ 
  Str_t p ;

  word() ;
  p = (Str_t) pop() ;
  push( (Cell_t) *p ) ;
  if( state == state_Compiling )
  {
    push( (Cell_t) lookup( "(literal)" ) ) ;
    comma() ; 
    comma();
  }
}

void q_key()
{
#ifdef HOSTED
#if !defined( __WIN32__ )
  Wrd_t  rv ;

  push( (Cell_t) INPUT ) ;
  push( (Cell_t) 0 ) ;
  push( (Cell_t) 0 ) ;
  io_cbreak( INPUT ) ;
  waitrdy() ;
  io_cbreak( INPUT ) ;
#endif
#endif
#ifdef NATIVE
  push( uart_can_recv() );
#endif
}

void key()
{
#ifdef HOSTED
#if !defined( __WIN32__ )

  Byt_t ch ;
  Wrd_t nx, x ;

  while( ! io_cbreak( INPUT ) ) ;	// turn on cbreak ...
  nx = inp( INPUT, (Str_t) &ch, 1 ) ;
  while( io_cbreak( INPUT ) ) ; 	// turn off cbreak ...
  if( nx < 1 )
  {
    push( 0 ) ;
  } else {
    push( ch & 0xff ) ;
  }

#else

  push( (Cell_t) (getch() & 0xff) ) ;

#endif
#endif
#ifdef NATIVE

  push( (Cell_t) uart_getc_ne() & 0xff );

#endif
}

void emit()
{
  Wrd_t nbytes ;
  Byt_t buf[10] ;
  chk( 1 ) ; 
 
  nbytes = utf8_encoder( pop(), (Str_t) buf, (Wrd_t) 10 ) ;
  outp( OUTPUT, (Str_t) buf, nbytes ) ;
}

void type()
{
  Str_t str ;

  chk( 1 ) ; 
  str = (Str_t) pop() ;
  outp( OUTPUT, (Str_t) str, str_length( str ) ) ;
}

void cr()
{
  outp( OUTPUT, (Str_t) "\n", 1 ) ;
}

void dp()
{
  push( (Cell_t) DictPtr ) ;
}

void stringptr()
{
  push( (Cell_t) String_Data ) ;
}

void flashsize()
{
  push( (Cell_t) sz_FLASH * sizeof( Wrd_t ) ) ;
}

void flashptr()
{
  push( (Cell_t) flash_mem ) ;
}

void here()
{
  push( (Cell_t) Here ) ;
}

void freespace()
{
  push( (Cell_t) ((Str_t) String_Data - (Str_t) Here) ) ;
}

void comma()
{
  Cell_t space ;

  chk( 1 ) ; 
  freespace();
  space = pop() ;
  if( space > sizeof( Cell_t ) ){
    push( (Cell_t) Here++ ) ;
    wrd_store() ;
  } else {
    throw( err_NoSpace ) ;
  }
}

void doLiteral()
{
  Cell_t *p ; 
  p = (Cell_t *) rpop() ;
  push( *(p++) ) ;
  rpush( (Cell_t) p ) ;
}

void pushPfa()
{
  push( rpop() ) ;
}

void does()
{
  Dict_t *dp ; 
  Cell_t **p ;

  dp = &Colon_Defs[n_ColonDefs-1] ;
  push( (Cell_t) dp ->pfa ) ;
  dp ->pfa = Here ;
  push( (Cell_t) lookup( "(literal)" ) ) ;
  comma() ; /* push the original pfa  */
  comma() ;

  switch( state ){
    case state_Interactive:
      state = state_Compiling ;

    case state_Compiling:
      compile() ;
      return ;

    case state_Interpret: /* copy the does> behaviour into the new word */
      dp ->cfa = doColon ;
      while( (p = (Cell_t **) rpop()) ) {
        dp = (Dict_t *) *p ; ;
        if( isNul( dp ) ){
          rpush( 0 ) ; /* end of current word interpretation */
          push( 0 ) ; /* end of defined word (compile next) */
          comma() ;
          break ;
        }
        rpush( (Cell_t) ++p ) ;
        push( (Cell_t) dp ) ;
        comma() ;
      }
      break ;

    default:
      throw( err_BadState ) ;
  }
}

void allot()
{
  Cell_t n ;

  chk( 1 ) ;
  n = pop() ;
  Here += n ; 
}

void create()
{
  word();
  lambda() ;
}

void lambda()
{
  Str_t   tag ;
  Dict_t *dp ;

  tag = (Str_t) pop() ;
  dp = &Colon_Defs[n_ColonDefs++] ;

  dp ->nfa = str_cache( tag ) ; // cache tag ..
  dp ->cfa = pushPfa ;		// default behaviour (like variable)
  dp ->pfa = Here ;		// pfa points to current 

}

void doConstant()
{
  push( rpop() ) ;
  wrd_fetch() ;
}

void constant()
{
  create() ;
  comma() ;

  Dict_t *dp = &Colon_Defs[n_ColonDefs-1] ;
  dp ->cfa = doConstant ;
}

void variable()
{
  create();
  push( 0 ) ; 
  comma() ;
}

void colon()
{
  state = state_Compiling ;
  create();
  compile() ; 
}

void compile()
{
  Dict_t *dp ;
  Str_t   tkn ; 
  Cell_t *save, value ;

  save = Here ;
  dp = &Colon_Defs[n_ColonDefs-1] ;
  dp ->cfa = doColon ;

  ++promptVal ;
  while( (tkn = str_token( &InputStack[ in_This ] )) ){
    if( isMatch( tkn, ";" ) ){
      semicolon() ;
      break ;
    }
    dp = (Dict_t *) lookup( tkn ) ;
    if( !isNul( dp ) ){
      push( (Cell_t) dp ) ;
      if( state == state_Immediate || dp ->flg == Immediate ){
        execute() ; /* execute */
      } else {
        comma() ;   /* compile */
      }
    } else {
      value = (Cell_t) str_literal( tkn, Base ) ;
      if( error_code != err_OK ){
        str_uncache( (Str_t) String_Data ) ; 
        Here = save ;
        state = state_Interpret ;
        throw( err_BadString ) ;
        put_str( tkn ) ;
        return ; /* like it never happened */
      }
      push( value ) ;
      if( state != state_Immediate ){
        push( (Cell_t) lookup( "(literal)" ) ) ;
        comma() ;
        comma() ;
      }
    }
  }
}

void pvState()
{
  state = state_save ;
}

void imState()
{
  state_save = state ;
  state = state_Immediate ;
}

void normal()
{
  Dict_t *dp ;

  dp = &Colon_Defs[n_ColonDefs-1] ;
  dp ->flg = Normal ;
}

void immediate()
{
  Dict_t *dp ;

  dp = &Colon_Defs[n_ColonDefs-1] ;
  dp ->flg = Immediate ;
}

void call()
{
  Cptr_t fun ;
 
  chk( 1 ) ; 
  fun = (Cptr_t) pop() ;
  push( (*fun)() ) ;
}

void tracing( Dict_t *dp )
{

  dotS() ;
  put_str( "\t\t" ) ;

  if( isNul( dp ) )
    put_str( "next" ) ;
  else 
    put_str( dp ->nfa ) ;

  cr() ;
}

void execute()
{
  Dict_t *dp ; 

  chk( 1 ) ; 
  dp = (Dict_t *) pop() ;
  if( !isNul( dp ) )
  {

    if( dp ->pfa ){
      rpush( (Cell_t) dp->pfa ) ;
    }

    if( Trace )
       tracing( dp ) ;

    (*dp ->cfa)() ;
    catch() ;

  }
}

void doColon()
{
  Dict_t *dp ;
  Cell_t **p ;
  State_t save ;

  save = state ;
  state = state_Interpret ;

  while( (p = (Cell_t **) rpop()) ) {
    dp = (Dict_t *) *p ;
    if( isNul( dp ) ){
      break ;
    }
    rpush( (Cell_t) ++p ) ;
    push( (Cell_t) dp ) ;
    ++_ops ; execute();
  }

  state = save ;

}

void semicolon()
{

  if( state != state_Compiling ){
    throw( err_BadState );
    return ;
  }
  push( 0 ) ; /* next is NULL */
  comma() ;
  --promptVal ;
  state = state_Interactive ;
}

void tick()
{
  Str_t tkn ; 

  chk( 0 ) ; 
  word() ;
  tkn = (Str_t) pop() ;
  push( (Cell_t) lookup( tkn ) ) ;
  if( *tos == 0 ){
    put_str( tkn ) ;
    throw( err_NoWord ) ;
  }
  if( state == state_Compiling )
  {
    push( (Cell_t) lookup( "(literal)" ) ) ;
    comma() ; 
    comma();
  }
}

void nfa()
{
  Dict_t *dp ;

  chk( 1 ) ; 
  dp = (Dict_t *) pop();
  push( (Cell_t) dp ->nfa ) ;
}

void cfa()
{
  Dict_t *dp ;

  chk( 1 ) ; 
  dp = (Dict_t *) pop();
  push( (Cell_t) dp ->cfa ) ;
}

void pfa()
{
  Dict_t *dp ;

  chk( 1 ) ; 
  dp = (Dict_t *) pop() ;
  push( (Cell_t) dp ->pfa ) ;
}

void decimal()
{
  Base = 10 ; 
}

void hex()
{
  Base = 16 ; 
}

void sigvar()
{
#ifdef HOSTED
  push( (Cell_t) &sigval ); 
#endif
}

void errvar()
{
  push( (Cell_t) &error_code ); 
}

void errval()
{
  errvar();
  wrd_fetch();
}

void errstr()
{

  register Cell_t err = pop() ;

  if( err >= err_OK && err <= err_Undefined )
     push( errors[ err ] ) ;
  else
     throw( err_Range ) ;
  return ;
}

void errmax()
{
  push( err_Undefined ) ;
}

void trace()
{
  push( (Cell_t) &Trace ); 
}

void base()
{
  push( (Cell_t) &Base ); 
}

void resetter()
{
  put_str( "-- Warm start." ) ; cr();
  q_reset() ;
#ifdef HOSTED
  longjmp( env, rst_user ) ;
#endif
}

void cold()
{
  put_str( "-- Cold start." ) ; cr();
  q_reset() ;
  forget() ;
#ifdef HOSTED
  longjmp( env, rst_coldstart ) ;
#else
#endif
}

void see()
{
  register Dict_t *p, *r ; 
  Cell_t *ptr, n ; 

  chk( 1 ) ; 

  p = (Dict_t *) pop() ;
  if( isNul( p ->pfa ) ){
    n = fmt_out( "-- %s (%x) flg: %d is coded in C (%x).\n", p ->nfa, p, p->flg, p ->cfa ) ;
    return ;
  } else {
    if( p ->cfa == (Fptr_t) doConstant ){
      n = fmt_out( "-- %s constant value (0x%x).\n", p ->nfa, *p->pfa ) ;
      return ;
    }
    if( p ->cfa == (Fptr_t) pushPfa ){
      n = fmt_out( "-- %s variable value (0x%x).\n", p ->nfa, *p->pfa ) ;
      return ;
    }
    n = fmt_out( "-- %s (%x) word flg: %d.\n", p ->nfa, p, p->flg ) ;
  }
  ptr = p ->pfa ; 
  while( !isNul( ptr ) ){
    r = (Dict_t *) *ptr ;
    if( isNul( r ) ){			/* next == NULL */
      n = fmt_out( "%x  next\n", ptr ) ;
      break ;
    }
    Str_t buf = tb_get( TB ) ;
    if( r ->cfa  == (Fptr_t) branch ){
      n = str_format( buf, tb_bufsize( TB ), "%x  %s -> %x\n", ptr, r ->nfa, *(ptr+1) ) ;
      ptr++ ;
    } else  if( r ->cfa  == (Fptr_t) q_branch ){
      n = str_format( buf, tb_bufsize( TB ), "%x  %s -> %x\n", ptr, r ->nfa, *(ptr+1) ) ;
      ptr++ ;
    } else if( r ->cfa  == (Fptr_t) doLiteral ){
      n = str_format( buf, tb_bufsize( TB ), "%x  %s = %d\n", ptr, r ->nfa, *(ptr+1) ) ;
      ptr++ ;
    } else {
      n = str_format( buf, tb_bufsize( TB ), "%x  %s\n", ptr, r ->nfa ) ;
    }
    outp( OUTPUT, (Str_t) buf, n ) ;
    ptr++ ;
  }
}

/*
  -- I/O routines --

  must be written for the Atmel AVR's, but
  any HOSTED (Linux/Unix/Windows) system can simply use read/write.
  key and ?key are special cases (len == 1), 
  and cbreak has been added for processing for tty's.

  For Native ARM based systems, we will be implementing uart put/get functions
  and a get without echo (for key) ... 

*/

Wrd_t fmt_out( Str_t fmt, ... )
{
  va_list  ap ;
  Wrd_t    nx, siz ;
  Str_t    buf = tb_get( TB );

  siz = tb_bufsize( TB ) ;
  va_start( ap, fmt );
   nx = str_format_ap( buf, siz, fmt, ap ) ;
  va_end( ap ) ;

  outp( OUTPUT, buf, nx );
  return nx ;
}

Wrd_t put_str( Str_t s )
{
  register Cell_t n = 0;

  if( !isNul( s ) ){
    n = str_length( s ) ; 
    outp( OUTPUT, s, n ) ;
    outp( OUTPUT, " ", 1 ) ;
  }
  return n ;
}

Wrd_t get_str( Wrd_t fd, Str_t buf, Wrd_t len )
{
  Byt_t ch ;
  Wrd_t i, crlf = 0 ;

  str_set( buf, 0, len ) ;

  i = 0 ; 
  do {
    if( i > (len - 1) ){
      return i ;
    }

	key() ; ch = pop() & 0xff ;
    if( ch == 0 )
    {
      return i ;
    }

    if( ch_matches( ch, "\r\n" ) ){ 
       crlf++ ;
    }

    buf[i++] = (Byt_t) ch ;

  } while( crlf < 1 ) ;
  return i ;
}

Wrd_t io_cbreak( int fd )
{
#if defined( HOSTED ) 
#if !defined(__WIN32__)
  static int inCbreak = v_Off ;
  static struct termios tty_state, *tty_orig = (struct termios *) NULL ; 
  int rv ;

  if( isNul( tty_orig ) ){
    rv = tcgetattr( fd, &tty_normal_state );
    tty_orig = &tty_normal_state ;
  } 

  switch( inCbreak ){
    case v_Off:
      rv = tcgetattr( fd, &tty_state ) ;
      cfmakeraw( &tty_state ) ;
      rv = tcsetattr( fd, TCSANOW, &tty_state ) ; 
      inCbreak = v_On ;
      break ;
    case v_On:
      rv = tcsetattr( fd, TCSANOW, tty_orig ) ; 
      inCbreak = v_Off ;
  }
  return inCbreak ;
#endif
#else
  return v_On ;
#endif
}

void waitrdy() // fd secs usecs -- flag )
{
#ifdef HOSTED
#if !defined( __WIN32__ )
  Wrd_t rv, fd, secs, usecs ;
  fd_set fds ;
  struct timeval tmo ;

  usecs = pop() ;
  secs = pop() ;
  fd = pop() ;
 
  FD_ZERO( &fds ) ;
  FD_SET( fd, &fds ) ;

  tmo.tv_sec = secs ;
  tmo.tv_usec = usecs ;
  rv = select( fd+1, &fds, NULL, NULL, &tmo ) ;  
  if( rv < 0 ){
    throw( err_SysCall ) ;
  }
  push( FD_ISSET( fd, &fds ) ) ;
#endif
#endif
}

void sndtty() // ( fd ptr -- nx )
{
  Str_t str ;
  Wrd_t fd, len ;

  chk( 2 ) ; 
  
  len = str_length( (Str_t) *tos ) ;
  str = (Str_t) pop() ;
  fd = pop() ;
  push( (Cell_t) outp( fd, str, len ) ) ;
}

#ifdef HOSTED
void rcvtty() // ( fd n -- buf n )
{
  Str_t buf ;
  Wrd_t n, nr, fd ;

  chk( 2 ) ; 

  n = pop() ;
  fd = pop() ;
  here() ; buf = (Str_t) pop() + 8 * sizeof( Cell_t ) ;
  in_files[++in_This] = fd ; 
  nr = get_str( fd, buf, n ) ;
  --in_This ;
  push( (Cell_t) buf ) ;
  push( (Cell_t) nr ) ;
  return ;
}
#endif

void opentty()	// ( str -- fd )
{
#ifdef HOSTED
#if !defined(__WIN32__)
  Str_t fn ;
  Wrd_t rv, fd ;
  struct termios tty_state ;

  chk( 1 ) ; 

  fn = (Str_t) pop() ;
  if( !isNul( fn ) ){
    fd = open( fn, O_RDWR | O_NDELAY | O_NONBLOCK | O_NOCTTY ) ;
    if( fd < 0 ){
      throw( err_SysCall ) ;
      return ;
    }
    rv = tcgetattr( fd, &tty_state ) ;
    cfsetspeed( &tty_state, B115200 ) ; 
    tty_state.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG ) ;
    cfmakeraw( &tty_state ) ; 
    rv = tcsetattr( fd, TCSANOW, &tty_state ) ;
    if( rv < 0 ){
      throw( err_SysCall ) ;
      return ;
    }
  } 
  push( fd ) ; 
#endif
#endif
}

void closetty() // ( fd -- )
{
#ifdef HOSTED
  chk( 1 ) ; 
  close( (Wrd_t) pop() ) ;
#endif
}

void infile()
{
  chk( 1 ) ;

  Str_t fn = (Str_t) pop() ;

  if( isMatch( fn, "stdin" ) ) // intialize ...
  {
	in_This = 0 ;
	InputStack[ in_This ].file = 0 ; 
	InputStack[ in_This ].bytes_read = -1 ; 
	InputStack[ in_This ].bytes_this = -1 ; 
	InputStack[ in_This ].in_line = 0 ; 
	InputStack[ in_This ].name = str_cache( fn ) ;
	InputStack[ in_This ].bytes = (Str_t) inbuf[ in_This ] ; 
	return ;
  }

#ifdef HOSTED

  if( in_This < sz_FILES ) // push a new input file ...
  {
	if( !isNul( fn ) )
	{
		in_This += 1 ;
		InputStack[ in_This ].bytes_read = -1 ; 
		InputStack[ in_This ].bytes_this = -1 ; 
		InputStack[ in_This ].in_line = 0 ; 
		InputStack[ in_This ].name = str_cache( fn ) ;
		InputStack[ in_This ].bytes = (Str_t) inbuf[ in_This ] ; 
		InputStack[ in_This ].file = open( InputStack[ in_This ].name , O_RDONLY ) ;

		if( InputStack[ in_This ].file < 0 )
		{
			str_uncache( fn ) ;
			if( !isNul( off_path ) ) // add the file path and try again ...
			{
				Str_t buf = tb_get( TB ) ;
				str_format( buf, tb_bufsize( TB ), "%s/%s", (Str_t) off_path, (Str_t) fn ) ;
    				InputStack[ in_This ].name = str_cache( (Str_t) buf ) ;
				InputStack[ in_This ].file = open( InputStack[ in_This ].name , O_RDONLY ) ;
			} 
		}

		if( InputStack[ in_This ].file < 0 )
		{
			in_This-- ;
			throw( err_NoFile ) ;
		}
	}

	return ;

  } 

  throw( err_InStack ) ;
  return ;

#endif
}

void filename()
{
	push( (Str_t) InputStack[ in_This ].name ) ;
}

void outfile()
{
#ifdef HOSTED
  Str_t fn ;
  Cell_t fd, fexists ;
  uCell_t fflg = O_CREAT | O_RDWR | O_APPEND ;

  fn = (Str_t) pop() ;
  if( !isNul( fn ) ){
#if !defined( __WIN32__ )
    uCell_t fprm = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH ;
    fd = open( fn, fflg, fprm ) ;
#else
    fd = open( fn, fflg ) ;
#endif
    if( fd < 0 ){
      throw( err_NoFile ) ;
      return ;
    }
    out_files[++out_This] = fd ;
    return ;
  } 
#endif
}

void closeout()
{
#ifdef HOSTED
  if( out_This > 0 ){
    close( OUTPUT ) ;
    out_This-- ;
  }
#endif
}

void isfile()
{
#ifdef HOSTED
  struct stat sbuf ;
  Cell_t rv ;
  Str_t fn = (Str_t) pop() ;

  if( !isNul( fn ) )
  {
    rv = stat( (const Str_t ) fn, &sbuf ) ;
    if( rv < 0 )
    {
      throw( err_SysCall ) ;
    }
  }
  push( (rv == 0) ? 1 : 0 ) ; 
#endif
}

Wrd_t outp( Wrd_t fd, Str_t buf, Wrd_t len )
{
#ifdef HOSTED
  Wrd_t nx ;

 again:
  nx = write( fd, buf, len ) ;
  if( nx < 0 )
  {
    if( errno == EINTR )
      goto again ;
  }
  return nx ; 

#endif
#ifdef NATIVE
  Wrd_t i ;

  for( i = 0 ; i < len; i++ )
  {
    uart_putc( (unsigned) buf[i]&0xff );
  }
  return i ;

#endif
}

Wrd_t inp( Wrd_t fd, Str_t buf, Wrd_t len )
{

#ifdef HOSTED
  Wrd_t nx ;

 again:
  nx = read( fd, buf, len ) ;
  if( nx < 0 )
  {
    if( errno == EINTR )
      goto again ;
  }
  return nx ; 
#endif

#ifdef NATIVE
  Cell_t c, x, i = 0 ;
  
  str_set( buf, 0, len ) ;
  while( i < len )
  {
    buf[i++] = c = uart_getc();
    if( c == 0x15 ) // NAK
    {
       str_set( buf, 0, len ) ;
       int slen = str_length( buf ) ;
       uart_putc( 0x0a ) ;
       for( x = 0 ; x < slen; x++ )
           uart_putc( 0x20 ) ;
       uart_putc( 0x0a ) ;
       i = 0 ;
       continue ;
    }
    if( c == 0x08 || c == 0x7f ) // backspace || delete
    {
       buf[--i] = 0x00 ; // backspace
       buf[--i] = 0x00 ; // bad char
       i = i < 0 ? 0 : i ;
       uart_putc( 0x20 ) ; // space
       uart_putc( 0x08 ) ; // backspace
       continue ;
    }
    if( c == 0x0d ) // return 
    {
       uart_putc( 0x0a ) ; // newline
       break ;
    }
  }
  return i ;
#endif
}

#if defined avr || defined AVR
Wrd_t read( Wrd_t fd, Str_t buf, Wrd_t len ) {}
Wrd_t write( Wrd_t fd, Str_t buf, Wrd_t len ) {}
#endif

#ifdef HOSTED 
void qdlopen()
{
  Str_t lib ;
  Opq_t opaque ;

  chk( 1 ) ; 
  lib = (Str_t) pop() ;
  opaque = dlopen( lib, RTLD_NOW | RTLD_GLOBAL ) ;
  push( (Cell_t) opaque ) ;
}

void qdlclose()
{
  Opq_t opaque ;

  chk( 1 ) ; 
  opaque = (Opq_t) pop() ;
  push( (Cell_t) dlclose( opaque ) ) ;
}

void qdlsym()
{
  Str_t symbol ;
  Opq_t handle ;

  chk( 2 ) ; 
  symbol = (Str_t) pop() ;
  handle = (Opq_t) pop() ;
  push( (Cell_t) dlsym( handle, symbol ) ) ;
}

void qdlerror()
{
  push( (Cell_t) dlerror() ) ;
}

void last_will()
{
  Opq_t cmd ;

  chk( 1 ) ; 
  cmd = (Opq_t) pop() ;
  atexit( cmd ) ;
}

void spinner()
{
  static Wrd_t ix = 0 ;
  Byt_t f[4] = { '-', '\\', '|', '/' } ;

  push( f[ ix++ % 4 ] ) ;
  emit();
  push( '\r' ) ;
  emit();

}
#endif /* HOSTED */

void callout()
{
  Cptr_t fun ;
  Cell_t i, n ;
  Cell_t args[10] ;

  fun = (Cptr_t) pop() ;
  n = pop() ;

  chk( n ) ; /* really need n+2 items ... */

  for( i = n-1 ; i >= 0 ; i-- ){
    args[i] = pop() ;
  }

  switch( n ){
    case 0:
      push( (*fun)() ) ;
      break ;
    case 1:
      push( (*fun)( args[0] ) ) ;
      break ;
    case 2:
      push( (*fun)( args[0], args[1] ) ) ;
      break ;
    case 3:
      push( (*fun)( args[0], args[1], args[2] ) ) ;
      break ;
    case 4:
      push( (*fun)( args[0], args[1], args[2], args[3] ) ) ;
      break ;
    case 5:
      push( (*fun)( args[0], args[1], args[2], args[3], args[4] ) ) ;
      break ;
    case 6:
      push( (*fun)( args[0], args[1], args[2], args[3], args[4], args[5] ) ) ;
      break ;
    case 7:
      push( (*fun)( args[0], args[1], args[2], args[3], args[4], args[5], args[6] ) ) ;
      break ;
    case 8:
      push( (*fun)( args[0], args[1], args[2], args[3], args[4], args[5], args[6], args[7] ) ) ;
      break ;
    case 9:
      push( (*fun)( args[0], args[1], args[2], args[3], args[4], args[5], args[6], args[7], args[8] ) ) ;
      break ;
  }
  return ;
}

void clkspersec()
{
#ifdef HOSTED
  push( CLOCKS_PER_SEC ) ;
#else
  push( -1 ) ;
#endif
}

void plusplus()
{
  *(tos) += 1; 
}

void minusminus()
{
  *(tos) -= 1; 
}

void utime()
{
#ifdef HOSTED
  struct timeval tv ;
  uint64_t rv = 0ULL ;

  gettimeofday( &tv, NULL ) ;
  push( ( tv.tv_sec * 1000000 ) + tv.tv_usec ) ;
#else
  push( -1 ) ;
#endif
}

void ops()
{
   push( _ops ) ; 
}

void noops()
{
   _ops = 0 ; 
}

void qdo()
{
  push( (Cell_t) lookup( "(do)" ) ) ; 
  comma() ;
  bkw_mark();
}

void do_do()
{
  Cell_t nxt ;

  chk( 2 ) ;
  nxt = rpop() ;
  swap() ;
  rpush( pop() ) ; /* end point */
  rpush( pop() ) ; /* index */
  rpush( nxt ) ; 
}

void do_loop()
{
  Cell_t nxt ;

  nxt = (Cell_t ) rpop() ;	// save nxt
  if ( *(rtos)+1 < *(rnos) ){	// if i < n
    *rtos += 1; 		// ++i
    rpush( nxt ) ; 		// restore nxt
    push( 0 ) ;			// flag for ?branch
    return ;
  }

  rtos-- ; rtos-- ;		// drop i, n
  rpush( nxt ) ;		// restore nxt
  push( 1 ) ;			// flag for ?branch

}

void loop()
{

  push( (Cell_t) lookup( "(loop)" ) );
  comma(); 

  push( (Cell_t) lookup( "?branch" ) );
  comma() ;
  bkw_resolve() ;

}

void do_I()
{
  push( *rnos );
}

void ploop()
{

  push( (Cell_t) lookup( "(+loop)" ) );
  comma(); 

  push( (Cell_t) lookup( "?branch" ) );
  comma() ;
  bkw_resolve() ;

}

void do_ploop()
{
  Cell_t nxt, inc ;

  inc = pop() ;
  nxt = (Cell_t ) rpop() ;
  if ( inc > 0 ) /* positive increment */
  {
    if ( ( *(rtos) + inc )< *(rnos) ){
      *rtos += inc ;
      rpush( nxt ) ; 
      push( 0 ) ;
      return ;
    }

  } else { /* negative increment */
    if ( ( *(rtos) + inc )> *(rnos) ){
      *rtos += inc ;
      rpush( nxt ) ; 
      push( 0 ) ;
      return ;
    }

  }

  rtos-- ; rtos-- ;		// drop i, n ...
  rpush( nxt ) ;		// restore nxt ...
  push( 1 ) ;			// flag for ?branch ...

}

void forget()
{
  if( !isNul( TB ) )
  {
    TB = tb_destroy( TB ) ;
  }
  TB = tb_create( &T, (Byt_t *) tmp_buffer, (Wrd_t) sz_TMPBUFFER, (Wrd_t) nm_TMPBUFFER ) ;

  Here = (Cell_t *) StartOf( flash ) ;		// erase colon defs vars and constants ...
  DictPtr = (Cell_t *) StartOf( flash ) ;	// set the dictptr to here ...
  n_ColonDefs = 0 ; 				// uncount the colon defs ... 
  Base = 10 ;
  Trace = 0 ;
  state = state_Interactive ;

  if( isNul( String_LowWater ) )
	String_Data = (Byt_t *) (&flash[sz_FLASH - 1]) ;	// erase the string data referenced in the dictionary
  else
  	String_Data = (Byt_t *) String_LowWater ;
}

int sign_is_negative = 0 ; 

void fmt_start() 	// ( n -- <ptr> n )
{
  Str_t ptr ;

  chk( 1 ) ;

  sign_is_negative = 0 ;
  ptr = tb_get( TB ) + tb_bufsize( TB ) - 1 ;
  *(ptr--) = (Byt_t) 0 ;		// drop a null byte at the end of the buffer ...
  push( ptr ) ;					// back the buffer up for the next char ... 
  swap() ;						// ( n ptr -- ptr n )
}

void fmt_digit()	// ( <ptr> n -- <ptr-1> n2 ) : # dup base @ % . base @ / ;
{
  register Cell_t n, digit ;
  Str_t ptr ;

  if( *tos )
  {
    // fmt_sign() ;
    n = pop() ;						//	( ptr n -- ptr )
    ptr = (Str_t) pop() ;			//  ( ptr -- )
    digit = ( (Abs( n ) % Base) ) ;
    n /= Base ;
    *(ptr--) = digits[digit] ;
    push( ptr ) ;					// ( ptr -- ptr-1 )
    push( n ) ;						// ( ptr -- ptr n2 )
  } else {
    push( (Cell_t) '0' ) ;			// ( ptr n -- ptr n 0 )
    fmt_hold() ;					// ( ptr n 0 -- ptr-1 n )
  }
}

void fmt_hold() // ( <ptr> x n -- <ptr-1> x )
{
  Cell_t n ;
  Str_t ptr ;

  n = pop() ;						// ( ptr x n -- ptr x )
  ptr = (Str_t) nos ;				// ( ptr x -- ptr x )
  *ptr = (Byt_t) n ;				// ( ptr x -- ptr-1 x )
  nos = (Cell_t) --ptr ;
}

void fmt_sign() // ( <ptr> n -- <ptr> n )
{
  if( *tos != 0 && *tos < 0 )
  {
    sign_is_negative = 1 ;
  }
}

void fmt_num() // ( <ptr> n -- <ptr*> 0 )
{
  while( *tos ) fmt_digit() ;
}

void fmt_end() // ( <ptr> n -- <ptr> ) 
{
  if( sign_is_negative )
  {
    push( (Cell_t) '-' ) ;
    fmt_hold() ;
  }
  drop() ;
  plusplus() ;
}

void utf8_encode() // ( char buf len -- len )
{
  Cell_t ch, len ;
  Str_t  buf ;

  chk( 3 ) ;
  len = (Wrd_t) pop() ;
  buf = (Str_t) pop() ;
  ch  = (Wrd_t) pop() ;

  push( (Wrd_t) utf8_encoder( ch, buf, len ) ) ;
}

void accept() // ( buf len -- len )
{
  Cell_t len ;
  Str_t  buf ;

  len = (Wrd_t) pop() ;
  buf = (Str_t) pop() ;

  push( (Wrd_t) get_str( INPUT, buf, len ) ) ;
  
}

void dump()
{
  Cell_t  **p ;
  Dict_t *dp ;

  fmt_out( "-- Input File: %s Line: %d:\n", InputStack[ in_This ].name, InputStack[ in_This ].in_line ) ;
  fmt_out( "-- Forth Backtrace:\n" ) ;
  while( rtos != StartOf( rstack ) )
  {
    p = (Cell_t **) rpop() ;
    if( !isNul( p ) )
    {
        dp = (Dict_t *) *p ;
        if( !isNul( dp ) )
		  fmt_out( "  -- %x %x (%s)\n", (p), dp, dp->nfa ) ;

        dp = (Dict_t *) *(p-1) ;
        if( !isNul( dp ) )
        {
		  fmt_out( "  -- %x %x (%s)\n", (p-1), dp, dp->nfa ) ;
        }
    }
  }
}

void find() // ( strptr -- dp|0 )
{
  Str_t tkn = (Str_t) pop() ;
  push( ((Dict_t *) lookup( (Str_t) tkn )) ) ;
}

#ifdef HOSTED
void path()
{
	push( off_path ) ; 
}
#endif

void version()
{
  push( str_literal( MAJOR, Base ) ) ;
  push( str_literal( MINOR, Base ) ) ;
  push( str_literal( REVISION, Base ) ) ;

}

void data()
{
  push( (Cell_t *) Colon_Defs );
}

void code()
{
  push( (Cell_t *) Primitives );
}

void align() // ( adr -- adr' )
{
	Cell_t adr = *tos ;

	while( adr % sizeof( Cell_t ) )
	{
      adr++;
	}
	*tos = adr ;
}

void fill() // ( dst n char -- )
{
  Byt_t ch ;
  Wrd_t n ;
  Str_t dst ;

  chk( 3 ) ; 
  ch = (Byt_t) pop() ;
  n = (Wrd_t) pop() ;
  dst = (Str_t) pop() ;
  str_set( dst, ch, n ) ;
}

// a late addition to OneFileForth is a circular buffer queue designed to
// return a reasonably sized buffer chunk from a fixed memory location in
// a round robin fashion, such that internal memory requirements will not
// conflict ... this is an internal only implementation ... see Buf().
Cir_Queue_t     *tb_create( Cir_Queue_t *rv, Byt_t *memory, Wrd_t size, Wrd_t n_elements )
{
  Wrd_t chunksz = 0 ;

  if( size > CQ_MAX_BUFFER )
  {
    return (Cir_Queue_t *) rv ;
  }

  if( n_elements < CQ_MIN_CHUNKS )
  {
    return (Cir_Queue_t *) rv ;
  }

  chunksz = (size / n_elements ) ;

  str_set( (Str_t) memory, 0, size ) ;
  rv -> cq_memory = (Str_t) memory ;
  rv -> cq_memsize = size ;
  rv -> cq_chunksize = (uint16_t) chunksz ;
  rv -> cq_n_elements = n_elements ;
  rv -> cq_next = 0 ;
  rv -> cq_buffer = (Str_t) memory ;
  return rv ;
}

Cir_Queue_t *tb_destroy( Cir_Queue_t *CQ )
{
  if( !isNul( CQ ) )
  {
	CQ ->cq_next = 0 ; 
    str_set( (Str_t) CQ ->cq_memory, 0, CQ ->cq_memsize ) ;
  }
  return (Cir_Queue_t *) NULL ;
}
 
int     tb_nbufs( Cir_Queue_t *CQ )
{
  return CQ->cq_n_elements ;
}

int     tb_bufsize( Cir_Queue_t *CQ )
{
  if( !isNul( CQ ) )
  {
    return (int) CQ-> cq_chunksize ;
  }
  return (int) -1;
}
   
void    *tb_get( Cir_Queue_t *CQ )
{
  int ix ;
  void *rv = (void *) NULL ;

  if( !isNul( CQ ) )
  {
    ix = CQ->cq_next++ ;
    CQ->cq_next = ( CQ->cq_next < CQ->cq_n_elements ) ? CQ->cq_next : 0 ;
    rv = (void *) ( CQ->cq_buffer + ( ix * CQ->cq_chunksize ) ) ;
    str_set( rv, 0, CQ->cq_chunksize  ) ;
  }
  return rv ;
}

#ifdef HOSTED
Fptr_t it_handler = NULL ;
void	it_doit( int signal ) // ( -- ) 
{
  if( !isNul( it_handler ) )
  {
    push( it_handler ) ;
    execute() ;
  }
}

void	it_reset()	// ( -- ) 
{
  push( 0 ) ; 
  push( 0 ) ; 
  push( 0 ) ; 
  it_set() ;
}

void	it_set()	// ( secs usecs pfa -- )
{
  struct sigaction action ;
  struct itimerval timer ;

  chk( 3 ) ;

  memset( &action, 0, sizeof( action ) ) ;
  memset( &timer, 0, sizeof( timer ) ) ;

  Wrd_t usec, sec ;
  Fptr_t wordptr ;

  it_handler = (Fptr_t) pop() ;
  usec = pop() ;
  sec = pop() ;

  action.sa_handler = it_doit ;
  sigaction( SIGALRM, &action, NULL ) ;
 
  timer.it_value.tv_sec = sec ;
  timer.it_value.tv_usec = usec ;
  timer.it_interval.tv_sec = sec ;
  timer.it_interval.tv_usec = usec ;
  if( setitimer( ITIMER_REAL, &timer, NULL ) ) 
    throw( err_SysCall ) ;

}
#endif
