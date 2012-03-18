/*  Simple Compiler Pass Main Program */

#include <stdlib.h>
#include <stdio.h>

extern void init_suif(int& argc, char * argv[]);
extern void compile(char *infile, char *outfile);
static void usage(char *progname);

int
main (int argc, char *argv[])
{
    char *infile, *outfile;

    init_suif(argc, argv);

    /* read the command line arguments */
    if ((argc < 2) || (argc > 3)) usage(argv[0]);
        infile = argv[1];
    if (argc > 2) {
        outfile = argv[2];
    } else {
        /*If no outfile, write to bit bucket instead*/
        /*Using NULL triggers an assert failure in Simple-SUIF*/
        outfile = "/dev/null";
    }

    compile(infile, outfile);
    return 0;
}



void
usage (char *progname)
{
    fprintf(stderr, "Usage: %s infile [outfile]\n", progname);
    exit(-1);
}



