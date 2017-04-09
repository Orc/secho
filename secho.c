/*
 * silly echo, inspired by a mocking discussion on the
 * plan 9 mailing list.
 *
 * Copyright (c) 2008 by David Loren Parsons.  All rights reserved.
 *
 * Distribution terms for this piece of work are found in the file
 * COPYRIGHT, which must be distributed with this code.
 */
#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#if HAVE_LIBGEN_H
# include <libgen.h>
#endif
#include <regex.h>
#include <ctype.h>

#include "cstring.h"

enum { ASCII, BINARY, HEX, OCTAL, DECIMAL, BASEN, SPQR } format = ASCII;
int base;		/* -B <base> */
enum { UPPERCASE, LOWERCASE, ASIS } whichcase = ASIS;
int zero = 0;
int count = 0;		/* return # of fields */
char *cmd = 0;		/* run command with each argument */
char delim = 0;		/* field delimiter */
int doescapes=0;	/* "allow escape sequences"  ? */
char *filename = 0;	/* read from file */
int cmdline = 1;	/* use cmdline arguments */
int width = 0;		/* linewidth */
int multicolumn = 0;	/* "multicolumn output" */
int numbered = 0;	/* oneperline + numbers */
int nonl = 0;		/* don't put a newline at the end */
int quiet = 0;		/* if ! -E && ! -o, output goes to /dev/null */
char outputdelim = ' ';	/* output delimiter */
int nononprint = 0;	/* strip nonprinting characters */
int visnonprint = 0;	/* make non-printing characters visible */
int wordwrap = 0;	/* wordwrap at width */
char *voice = 0;	/* "send to speaker, having the given voice say it" */
int plan9e = 0;		/* don't echo anything if no args */

FILE* output = 0;	/* output file, changed by -f */
int counter = 0;	/* # of lines, for -N & -C */
char *pgm = 0;

STRING(regex_t) regex = { 0 };

Cstring oline = { 0 };
Cstring arg = { 0 }, command = { 0 };


void printfmt(FILE *, char*, unsigned char);
void printc(unsigned char, FILE *);


#ifndef HAVE_BASENAME
char *
basename(char *p)
{
    char *q = strrchr(p, '/');

    return q ? (1+q) : p;
}
#endif


/* whine bitterly about something, then die
 */
void
die(char *fmt, ...)
{
    va_list ptr;

    fprintf(stderr, "%s: ", pgm);

    va_start(ptr,fmt);
    vfprintf(stderr, fmt, ptr);
    va_end(ptr);
    putc('\n', stderr);
    exit(1);
}


/* allocate memory or die trying
 */
void *
xmalloc(int size)
{
    void *ret = malloc(size);

    if ( !ret )
	die("cannot allocate %d byte%s\n", size, (size==1)?"":"s");
    return ret;
}
#define malloc xmalloc


/* reallocate memory or die trying
 */
void *
xrealloc(void *ptr, int size)
{
    void *ret = realloc(ptr, size);

    if ( !ret )
	die("cannot reallocate %d byte%s\n", size, (size==1)?"":"s");
    return ret;
}
#define realloc xrealloc


struct x_option options[] = {
    { '?', '?', "help",            0, "Print this help message" },
    { '@',  0 , "version",         0, "Print a version message" },
    { '%',  0 , "copyright",       0, "Print the copyright notice" },
    { '0', '0', 0,                 0, "Fields are separated with nulls" },
    { '1', '1', "single-column",   0, "One field per line" },
    { '9', '9', "plan9",           0, "Plan 9 compatability; echo nothing if\nno arguments were given" },
    { 'a', 'a', "ascii",           0, "Output in ASCII (default)" },
    { 'B', 'B', "base",        "BASE","Output in given base, 2..32." },
    { 'b', 'b', "binary",          0, "Output in binary" },
    { 'C', 'C', "count",           0, "Don't echo anything; just print the\nnumber of fields" },
    { 'c', 'c', "run",          "CMD","Run CMD on each argument, replacing $?\nwith the argument itself" },
    { 'D', 'D', "decimal",         0, "Output in decimal" },
    { 'd', 'd', "delimiter",   "CHAR","Input field delimiter" },
    { 'E', 'E', "to-stderr",       0, "Print to stderr instead of stdout" },
    { 'e', 'e', "allow-escapes",   0, "Allow escape sequences" },
    { 'f', 'f', "input",       "FILE","Read from FILE, then from command line\n(if any)" },
    { 'i', 'i', "from-stdin",      0, "Read arguments from stdin" },
    { 'L', 'L', "width",        "LEN","Line width set to LEN" },
    { 'l', 'l', "lowercase",       0, "Turn uppercase to lowercase" },
    { 'm', 'm', "multi-column",    0, "Multi-column output" },
    { 'N', 'n', "numbered",        0, "One field per line, numbering each field" },
    { 'n', 'n', 0,                 0, "Suppress newline" },
    { 'O', 'O', "octal",           0, "Output in octal" },
    { 'o', 'o', "output",      "FILE","Write to FILE instead of standard output" },
    { 'q', 'q', "quiet",           0, "Quiet mode; redirect output to /dev/null\nif not to a file" },
    { 'R', 'R', "SPQR",            0, "Print output in roman numerals" },
    { 'r', 'r', "pattern",       "RE","Print every thing that matches RE" },
    { 'S', 'S', "say",        "VOICE","Send to speaker, having the given\nvoice say it" },
    { 's', 's', "separator",   "CHAR","Separate output fields with CHAR" },
    { 't', 't', "tabs",            0, "Separate fields with tabs" },
    { 'u', 'u', "uppercase",       0, "Convert lowercase to uppercase" },
    { 'V', 'V', "no-nonprinting",  0, "Strip non-printing characters" },
    { 'v', 'v', "see-nonprinting", 0, "Make non-printing characters visible" },
    { 'X', 'X', "hexadecimal",     0, "Output in uppercase hexadecimal" },
    { 'x', 'x', 0,                 0, "Output in lowercase hexadecimal" },
};

#define NR(x)	(sizeof x / sizeof x[0])


/* spit out a usage message, then die
 */
void
usage(int rc)
{
    char eb[BUFSIZ];

    setbuf(stderr, eb);
    fprintf(stderr, "usage: %s [options] [args...]\n", pgm);
    showopts(stderr, NR(options), options);
    exit(rc);
}


/* printf onto the end of a Cstring
 */
void
Cprintf(Cstring *arg, char *fmt, ...)
{
    va_list ptr;
    int avail, needed;

    if ( (*arg).alloc <= S(*arg) )
	RESERVE(*arg,100);

    avail = (*arg).alloc - S(*arg);
    
    /* try writing the fmt into the existing
     * Cstring, and if that fails reserve enough
     * room to fit it and try again.
     */
    va_start(ptr, fmt);
    needed = vsnprintf(T(*arg)+S(*arg), avail, fmt, ptr);
    va_end(ptr);

    if ( needed >= avail ) {
	RESERVE(*arg, needed+2);
	
	va_start(ptr, fmt);
	vsnprintf(T(*arg)+S(*arg), needed+1, fmt, ptr);
	va_end(ptr);
    }
    S(*arg) += needed;
}


/* do a command, replacing every instance of $? with the
 * current argument.
 */
void
docommand(FILE *out)
{
    int i, j;
    
    S(command) = 0;

    for (i=0; cmd[i]; i++) {
	if ( (cmd[i] == '$') && (cmd[i+1] == '?') ) {
	    ++i;
	    STRINGCAT(command, T(arg), S(arg));
	}
	else
	    EXPAND(command) = cmd[i];
    }
    EXPAND(command) = 0;

    system(T(command));
}


/* display a number in base <n>
 */
void
basen(unsigned int c, FILE *out)
{
    static char base32[] = "0123456789abcdefghijklmnopqrstuvwxyz";

    if ( c/base )
	basen(c/base, out);
    c %= base;
    switch (whichcase) {
    case UPPERCASE: printc(toupper(base32[c]), out); break;
    case LOWERCASE: printc(tolower(base32[c]), out); break;
    default:        printc(base32[c], out);          break;
    }
}


/*
 * roman numeral output
 */
char *r1to10[] =  {  "2",   "0", "00",  "000",  "01",
		     "1", "10", "100", "1000", "02" };

struct {
    int val;
    char let[3];
} d_r[] = { { 0, "???" },
	    { 1, "IVX" },       { 10, "XLC" },    { 100, "CDM" },
	    { 1000, "Mvx" }, { 10000, "xlc" }, { 100000, "cdm" }, };
	    
#define NRDR	(sizeof d_r / sizeof d_r[0])

void
baser(FILE *out, unsigned char number)
{
    int i, j, rep;

    if ( number == 0 )
	printfmt(out, "nil", 0);
    else
	for ( i=6; i > 0; --i ) {
	    if ( rep = (number / d_r[i].val) )
		for (j=0; r1to10[rep][j]; j++)
		    printc(d_r[i].let[r1to10[rep][j]-'0'], out);
	    number %= d_r[i].val;
	}
}


/* print a character, processing it according to whichever of the
 * bewildering collection of options were used.
 */
void
outc(unsigned char c, FILE *out)
{
    int i;
    
    switch (format) {
    case ASCII:
	    if ( (c == '') && !doescapes ) {
		printc('^', out);
		printc('[', out);
		break;
	    }
	    if ( !isprint(c) ) {
		if ( nononprint ) break;
		if ( visnonprint ) { printfmt(out, "%#o", c); break; }
	    }
	    switch (whichcase) {
	    case UPPERCASE: printc(toupper(c), out); break;
	    case LOWERCASE: printc(tolower(c), out); break;
	    default:        printc(c, out);          break;
	    }
	    break;
    case BINARY:
	    printc(' ', out);
	    for (i=7; i; --i)
		printc( c & (1<<i) ? '1': '0', out);
	    break;
    case OCTAL:
	    printfmt(out, " %03o", c);
	    break;
    case DECIMAL:
	    printfmt(out, " %d", c);
	    break;
    case HEX:
	    printfmt(out, (whichcase==UPPERCASE)?" %02X":" %02x", c);
	    break;
    case SPQR:
	    printc(' ', out);
	    baser(out, c);
	    break;
    default:
	    printc(' ', out);
	    basen(c, out);
	    break;
    }
}


/* match a string against every regular expression give,
 * only returning 1 of nothing matches.
 */
int
match_re(char *text)
{
    int i;

    for ( i=0; i < S(regex); i++)
	if ( regexec(&T(regex)[i], text, 0, 0, 0) != 0 )
	    return 0;
    return 1;
}


/* flush any leftover parts of the output buffer
 */
void
flush(FILE *out)
{
    fwrite(T(oline), S(oline), 1, out);
    S(oline) = 0;
}


/* write a character (in some arbitrary printf format)
 * to the output buffer
 */
void
printfmt(FILE *out, char *fmt, unsigned char arg)
{
    int i;
    static Cstring Ppbuf = { 0 };

    S(Ppbuf) = 0;
    Cprintf(&Ppbuf, fmt, arg);

    for (i=0; i < S(Ppbuf); i++)
	printc(T(Ppbuf)[i], out);
}

 
/* write a character to the output buffer, printing
 * as necessary.
 */
void
printc(unsigned char c, FILE *out)
{
    if ( width ) {
	int tb;
	if ( c == '\t' ) {
	    if ( !(tb = S(oline)%8) )
		tb = 8;

	    while (tb-- > 0)
		printc(' ', out);
	}
	if ( c == '\n' || c == '' ) {
	    flush(out);
	    putc(c, out);
	}
	else {
	    if ( S(oline) >= width ) {
		int eol;
		if ( wordwrap ) {

		    for ( eol=S(oline)-1; (eol > 0) && !isspace(T(oline)[eol-1]); --eol)
			;
		    if ( eol <= 0 ) eol = width;
		}
		else
		    eol = width;
		    
		fwrite(T(oline), eol, 1, out);
		putc('\n', out);
		CLIP(oline,0,eol);
	    }
	    EXPAND(oline) = c;
	}
    }
    else
	putc(c, out);
}


/* print an argument.
 */
void
printarg(FILE *out)
{
    int i;

    if ( counter > 1 )
	printc(outputdelim, out);
    if ( numbered )
	printfmt(out, "%d\t", counter);
    for (i=0; i < S(arg); i++)
	outc(T(arg)[i], out);
}


/* read arguments from a file.
 */
void
filetokens(FILE *in, FILE *out)
{
    char c;
    char mydelim = zero ? 0 : (delim ? delim : '\n');

    do { 
	S(arg) = 0;

	while ( (c = getc(in)) != EOF && c != mydelim )
	    EXPAND(arg) = c;
	if ( (c == EOF) && (S(arg) == 0) )
	    break;

	EXPAND(arg) = 0;
	S(arg)--;

	if ( !match_re(T(arg)) )
	    continue;
	
	counter++;
	if ( cmd )
	    docommand(out);
	else if ( out )
	    printarg(out);
    } while ( c != EOF );
}


/* read arguments off the command line
 */
void
cmdtokens(char *in, FILE *out)
{
    do {
	S(arg) = 0;

	while ( *in && (*in != delim) )
	    EXPAND(arg) = *in++;

	if ( *in ) ++in;

	EXPAND(arg) = 0;
	S(arg)--;

	if ( !match_re(T(arg)) )
	    continue;
	
	counter++;
	if ( cmd )
	    docommand(out);
	else if ( out )
	    printarg(out);
    } while ( *in );
}


/* add a new regular expression to the gantlet
 */
void
add_re(char *pat)
{
    int rc;
    char oops[80];

#ifndef REG_BASIC
#define REG_BASIC 0
#endif
    rc = regcomp(&EXPAND(regex), pat, REG_BASIC);

    if ( rc ) {
	regerror(rc, &T(regex)[S(regex)-1], oops, sizeof oops);
	die("%s: %s", pat, oops);
    }
}


/* frankenecho, in the flesh
 */
int
main(argc, argv)
int argc;
char **argv;
{
    int opt;
    extern char version[];
    extern char copyright[];

    opterr = 1;

    pgm = basename(argv[0]);

    while ( (opt=x_getopt(argc,argv, NR(options), options)) != EOF ) {
	switch (opt) {
	case '0':	delim = 0; zero = 1; break;
	case '1':	outputdelim='\n'; nonl = 0; break;
	case '9':	plan9e = 1; break;
	case 'a':	format = ASCII; break;
	case 'B':	format = BASEN;
			base = atoi(x_optarg);
			if ( base <= 0 || base > 32 )
			    die("bad base for -B argument");
			switch (base) {
			case 2: format = BINARY; break;
			case 8: format = OCTAL; break;
			case 10: format = DECIMAL; break;
			case 16: format = HEX; break;
			}
			break;
	case 'b':	format = BINARY; break;
	case 'C':	quiet = 2; count = 1; break;
	case 'c':	cmd = x_optarg; nonl = 1; break;
	case 'd':	delim = x_optarg[0]; break;
	case 'E':	output = stderr; break;
	case 'e':	doescapes = 1; break;
	case 'f':	filename = x_optarg; cmdline = 1; break;
	case 'i':	filename = "-"; cmdline = 0; break;
	case 'L':	width = atoi(x_optarg); break;
	case 'l':	whichcase = LOWERCASE; break;
	case 'm':	multicolumn = 1; break;
	case 'n':	nonl = 1; break;
	case 'N':	numbered = 1; outputdelim='\n'; nonl=0; break;
	case 'O':	format = OCTAL; break;
	case 'o':	if ( ! (output = fopen(x_optarg, "w")) )
			    die("can't write to %s", x_optarg);
			break;
	case 'q':	quiet = 1; break;
	case 'R':	format = SPQR; whichcase = ASIS; break;
	case 'r':	add_re(x_optarg); break;
	case 'S':	voice = x_optarg; die("-S is not implemented here"); break;
	case 't':	outputdelim='\t'; break;
	case 's':	outputdelim=x_optarg[0]; break;
	case 'u':	whichcase = UPPERCASE; break;
	case 'V':	nononprint = 1; break;
	case 'v':	visnonprint = 1; break;
	case 'w':	wordwrap = 1; break;
	case 'X':	format = HEX; whichcase = UPPERCASE; break;
	case 'x':	format = HEX; whichcase = LOWERCASE; break;
	case '@':	printf("version %s\n", version); exit(0);
	case '%':	printf("%s\n", copyright); exit(0);
	case '?':	usage(0);
	default:	usage(1);
	}
    }

    switch ( quiet ) {
    case 0: output = stdout; break;
    case 1: if ( output ) quiet = 0; break;
    case 2: output = 0;
    }

    if ( filename ) {
	FILE* input;

	if ( strcmp(filename, "-") == 0 )
	    filetokens(stdin, output);
	else if ( input = fopen(filename, "r") ) {
	    filetokens(input, output);
	    fclose(input);
	}
	else
	    die("can't open %s for input", filename);
    }

    if ( cmdline ) {
	int i;

	for (i=x_optind; i < argc; i++)
	    cmdtokens(argv[i], output);
    }

    if ( output )  {
	flush(output);
	if ( !(nonl || (plan9e && (counter == 0))) )
	    putc('\n', output);
    }
    
    if ( count )
	fprintf( output ? output : stdout, "%d\n", counter);
    exit(0);
}
