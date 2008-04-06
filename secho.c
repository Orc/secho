/*
 * silly echo
 */
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <stdarg.h>
#ifdef linux
#include <unistd.h>
#else
#include <libgen.h>
#endif
#include <regex.h>

#include "cstring.h"

enum { ASCII, BINARY, HEX, OCTAL, DECIMAL,BASEN } format = ASCII;
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


#define OPTSTRING	"?019abCDEeilmnNOqtuVvwXxB:c:d:f:L:o:r:S:s:"

void
usage(int rc)
{
    fprintf(stderr, "usage: %s [-?019abCDEeilmNnOqtuVvwXx]\n"
		    "       %*s [-B base] [-c cmd] [-d char] [-f file] [-L len]\n"
		    "       %*s [-o file] [-r regex] [-S voice] [-s char] "
		    "[args...]\n", pgm, strlen(pgm), "", strlen(pgm), "");
    exit(rc);
}


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


void
basen(unsigned int c)
{
    static char base32[] = "0123456789abcdefghijklmnopqrstuvwxyz";

    if ( c/base )
	basen(c/base);
    c %= base;
    switch (whichcase) {
    case UPPERCASE: EXPAND(arg) = tolower(base32[c]); break;
    case LOWERCASE: EXPAND(arg) = toupper(base32[c]); break;
    default:        EXPAND(arg) = base32[c]; break;
    }
}


void
outc(unsigned char c)
{
    int i;
    
    switch (format) {
    case ASCII:
	    if ( (c == '') && !doescapes ) {
		EXPAND(arg) = '^';
		EXPAND(arg) = '[';
		break;
	    }
	    if ( !isprint(c) ) {
		if ( nononprint ) break;
		if ( visnonprint ) { Cprintf(&arg, "%#o", c); break; }
	    }
	    switch (whichcase) {
	    case UPPERCASE: EXPAND(arg) = toupper(c); break;
	    case LOWERCASE: EXPAND(arg) = tolower(c); break;
	    default: EXPAND(arg) = c; break;
	    }
	    break;
    case BINARY:
	    EXPAND(arg) = ' ';
	    for (i=7; i; --i)
		EXPAND(arg) = c & (1<<i) ? '1': '0';
	    break;
    case OCTAL:
	    Cprintf(&arg, " %03o", c);
	    break;
    case DECIMAL:
	    Cprintf(&arg, " %d", c);
	    break;
    case HEX:
	    Cprintf(&arg, (whichcase==UPPERCASE)?" %02X":" %02x", c);
	    break;
    default:
	    EXPAND(arg) = ' ';
	    basen(c);
	    break;
    }
}


int
match_re(char *text)
{
    int i;

    for ( i=0; i < S(regex); i++)
	if ( regexec(&T(regex)[i], text, 0, 0, 0) != 0 )
	    return 0;
    return 1;
}


flush(FILE *out)
{
    fwrite(T(oline), S(oline), 1, out);
    S(oline) = 0;
}


printc(char c, FILE *out)
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


void
printarg(FILE *out)
{
    int i;

    if ( counter > 1 )
	printc(outputdelim, out);
    if ( numbered )
	Cprintf(&oline, "%d\t", counter);
    for (i=0; i < S(arg); i++)
	printc(T(arg)[i], out);
}


void
filetokens(FILE *in, FILE *out)
{
    char c;
    char mydelim = zero ? 0 : (delim ? delim : '\n');

    do { 
	S(arg) = 0;

	while ( (c = getc(in)) != EOF && c != mydelim )
	    outc(c);
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


void
cmdtokens(char *in, FILE *out)
{
    do {
	S(arg) = 0;

	while ( *in && (*in != delim) )
	    outc(*in++);

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


main(argc, argv)
int argc;
char **argv;
{
    int opt;

    opterr = 1;

    pgm = basename(argv[0]);

    while ( (opt=getopt(argc,argv, OPTSTRING)) != EOF ) {
	switch (opt) {
	case '0':	delim = 0; zero = 1; break;
	case '1':	outputdelim='\n'; nonl = 0; break;
	case '9':	plan9e = 1; break;
	case 'a':	format = ASCII; break;
	case 'B':	format = BASEN;
			base = atoi(optarg);
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
	case 'c':	cmd = optarg; nonl = 1; break;
	case 'd':	delim = optarg[0]; break;
	case 'E':	output = stderr; break;
	case 'e':	doescapes = 1; break;
	case 'f':	filename = optarg; cmdline = 1; break;
	case 'i':	filename = "-"; cmdline = 0; break;
	case 'L':	width = atoi(optarg); break;
	case 'l':	whichcase = LOWERCASE; break;
	case 'm':	multicolumn = 1; break;
	case 'n':	nonl = 1; break;
	case 'N':	numbered = 1; outputdelim='\n'; nonl=0; break;
	case 'O':	format = OCTAL; break;
	case 'o':	if ( ! (output = fopen(optarg, "w")) )
			    die("can't write to %s", optarg);
			break;
	case 'q':	quiet = 1; break;
	case 'r':	add_re(optarg); break;
	case 'S':	voice = optarg; die("-S is not implemented here"); break;
	case 's':	outputdelim='\t'; break;
	case 'u':	whichcase = UPPERCASE; break;
	case 'V':	nononprint = 1; break;
	case 'v':	visnonprint = 1; break;
	case 'w':	wordwrap = 1; break;
	case 'X':	format = HEX; whichcase = UPPERCASE; break;
	case 'x':	format = HEX; whichcase = LOWERCASE; break;
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

	for (i=optind; i < argc; i++)
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
