
#include <cstdio>
#include <cassert>
#include <cstdlib>

extern "C" {
#   include <simple.h>
}

#include "include/cfg.h"
#include "include/basic_block.h"
#include "include/diag.h"

#ifdef TRANSFORM
#   define T(x) x
#else
#   define T(x)
#endif

#ifndef MAIN
#   define MAIN simple_instr *do_procedure(simple_instr *inlist, char *proc_name)
#endif

static void fprint_instr (FILE *fd, simple_instr *s);
static void fprint_type (FILE *fd, simple_type *t);
static void fprint_immed (FILE *fd, simple_immed *v);
static void fprint_reg (FILE *fd, simple_reg *r);

/*  Print out a simple_instr.  There is nothing magic about the
    output format.  I tried to make it look like some sort of combination
    of C and a generic assembly language so it would be familiar.  */

static void
fprint_instr (FILE *fd, simple_instr *s)
{
    int n = 0;
    switch (s->opcode) {

    case LOAD_OP: {
        fprintf(fd, "&nbsp; &nbsp; &nbsp; %s ", simple_op_name(s->opcode));
        fprint_type(fd, s->type);
        fprintf(fd, "&nbsp; &nbsp; &nbsp; ");
        fprint_reg(fd, s->u.base.dst);
        fprintf(fd, " = *");
        fprint_reg(fd, s->u.base.src1);
        fprintf(fd, "<BR ALIGN=\"LEFT\"/>");
        break;
    }

    case STR_OP: {
        fprintf(fd, "&nbsp; &nbsp; &nbsp; %s&nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; *", simple_op_name(s->opcode));
        fprint_reg(fd, s->u.base.src1);
        fprintf(fd, " = ");
        fprint_reg(fd, s->u.base.src2);
        fprintf(fd, "<BR ALIGN=\"LEFT\"/>");
        break;
    }

    case MCPY_OP: {
        fprintf(fd, "&nbsp; &nbsp; &nbsp; %s&nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; *", simple_op_name(s->opcode));
        fprint_reg(fd, s->u.base.src1);
        fprintf(fd, " = *");
        fprint_reg(fd, s->u.base.src2);
        fprintf(fd, "<BR ALIGN=\"LEFT\"/>");
        break;
    }

    case LDC_OP: {
        fprintf(fd, "&nbsp; &nbsp; &nbsp; %s ", simple_op_name(s->opcode));
        fprint_type(fd, s->type);
        fprintf(fd, "&nbsp; &nbsp; &nbsp; ");
        fprint_reg(fd, s->u.ldc.dst);
        fprintf(fd, " = ");
        fprint_immed(fd, &s->u.ldc.value);
        fprintf(fd, "<BR ALIGN=\"LEFT\"/>");
        break;
    }

    case JMP_OP: {
        fprintf(fd, "&nbsp; &nbsp; &nbsp; %s&nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; %s<BR ALIGN=\"LEFT\"/>",
            simple_op_name(s->opcode),
            s->u.bj.target->name);
        break;
    }

    case BTRUE_OP:
    case BFALSE_OP: {
        fprintf(fd, "&nbsp; &nbsp; &nbsp; %s&nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; ", simple_op_name(s->opcode));
        fprint_reg(fd, s->u.bj.src);
        fprintf(fd, ", %s<BR ALIGN=\"LEFT\"/>", s->u.bj.target->name);
        break;
    }

    case CALL_OP: {
        unsigned n, nargs;

        fprintf(fd, "&nbsp; &nbsp; &nbsp; %s ", simple_op_name(s->opcode));
        fprint_type(fd, s->type);
        fprintf(fd, "&nbsp; &nbsp; &nbsp; ");
        if (s->u.call.dst != NO_REGISTER) {
        fprint_reg(fd, s->u.call.dst);
        fprintf(fd, " = ");
        }
        fprintf(fd, "*");
        fprint_reg(fd, s->u.call.proc);
        fprintf(fd, " (");

        /* print the list of arguments */
        nargs = s->u.call.nargs;
        if (nargs != 0) {
        for (n = 0; n < nargs - 1; n++) {
            fprint_reg(fd, s->u.call.args[n]);
            fprintf(fd, ", ");
        }
        fprint_reg(fd, s->u.call.args[nargs - 1]);
        }
        fprintf(fd, ")<BR ALIGN=\"LEFT\"/>");
        break;
    }

    case MBR_OP: {
        unsigned n, ntargets;

        fprintf(fd, "&nbsp; &nbsp; &nbsp; %s&nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; ", simple_op_name(s->opcode));
        fprint_reg(fd, s->u.mbr.src);

        ntargets = s->u.mbr.ntargets;
        if (ntargets == 0) {
        fprintf(fd, " ( ), %s<BR ALIGN=\"LEFT\"/>", s->u.mbr.deflab->name);
        } else {
        fprintf(fd, " (%d to %d), %s, (",
            s->u.mbr.offset,
            s->u.mbr.offset + ntargets - 1,
            s->u.mbr.deflab->name);

        /* print the list of branch targets */
        for (n = 0; n < ntargets - 1; n++) {
            fprintf(fd, "%s, ", s->u.mbr.targets[n]->name);
        }
        fprintf(fd, "%s)<BR ALIGN=\"LEFT\"/>", s->u.mbr.targets[ntargets - 1]->name);
        }
        break;
    }

    case LABEL_OP: {
        fprintf(fd, "%s:<BR ALIGN=\"LEFT\"/>", s->u.label.lab->name);
        break;
    }

    case RET_OP: {
        fprintf(fd, "&nbsp; &nbsp; &nbsp; %s&nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; ", simple_op_name(s->opcode));
        if (s->u.base.src1 != NO_REGISTER) {
        fprint_reg(fd, s->u.base.src1);
        }
        fprintf(fd, "<BR ALIGN=\"LEFT\"/>");
        break;
    }

    case CVT_OP:
    case CPY_OP:
    case NEG_OP:
    case NOT_OP: {
        /* unary base instructions */
        fprintf(fd, "&nbsp; &nbsp; &nbsp; %s ", simple_op_name(s->opcode));
        fprint_type(fd, s->type);
        fprintf(fd, "&nbsp; &nbsp; &nbsp; ");
        fprint_reg(fd, s->u.base.dst);
        fprintf(fd, " = ");
        fprint_reg(fd, s->u.base.src1);
        fprintf(fd, "<BR ALIGN=\"LEFT\"/>");
        break;
    }

    default: {
        /* binary base instructions */
        assert(simple_op_format(s->opcode) == BASE_FORM);
        fprintf(fd, "&nbsp; &nbsp; &nbsp; %s ", simple_op_name(s->opcode));
        fprint_type(fd, s->type);
        fprintf(fd, "&nbsp; &nbsp; &nbsp; ");
        fprint_reg(fd, s->u.base.dst);
        fprintf(fd, " = ");
        fprint_reg(fd, s->u.base.src1);
        fprintf(fd, ", ");
        fprint_reg(fd, s->u.base.src2);
        fprintf(fd, "<BR ALIGN=\"LEFT\"/>");
    }
    }
}



/*  Print a simple type.  The format is (x.n) where x represents the base
    type and n is the number of bits (e.g. (s.32) for signed 32-bit type).  */

static void
fprint_type (FILE *fd, simple_type *t)
{
    char tag;
    switch (t->base) {
    case VOID_TYPE: {
        tag = 'v';
        break;
    }
    case ADDRESS_TYPE: {
        tag = 'a';
        break;
    }
    case SIGNED_TYPE: {
        tag = 's';
        break;
    }
    case UNSIGNED_TYPE: {
        tag = 'u';
        break;
    }
    case FLOAT_TYPE: {
        tag = 'f';
        break;
    }
    case RECORD_TYPE: {
        tag = 'r';
        break;
    }
    default: {
        //simple_error("unknown simple_type_base");
    }
    }
    fprintf(fd, "(%c.%d)", tag, t->len);
}



/*  Print out an immediate value.  Strings are printed with quotation marks
    around them.  Nothing else very exciting....  */

static void
fprint_immed (FILE *fd, simple_immed *v)
{
    switch (v->format) {
    case IMMED_INT: {
        fprintf(fd, "%d", v->u.ival);
        break;
    }
    case IMMED_FLOAT: {
        fprintf(fd, "%f", v->u.fval);
        break;
    }
    case IMMED_SYMBOL: {
        fprintf(fd, "&amp;%s ", v->u.s.symbol->name);
        if (v->u.s.offset < 0) {
        fprintf(fd, "- %d", -v->u.s.offset);
        } else {
        fprintf(fd, "+ %d", v->u.s.offset);
        }
        break;
    }
    default: {
        //simple_error("unknown immed_type");
    }
    }
}

static void
fprint_reg (FILE *fd, simple_reg *r)
{
#ifdef USE_VAR_NAMES
    fprintf(fd, "%s", r->var->name);
#else
    switch (r->kind) {
    case MACHINE_REG: {
        fprintf(fd, "m%d", r->num);
        break;
    }
    case TEMP_REG: {
        fprintf(fd, "t%d", r->num);
        break;
    }
    case PSEUDO_REG: {
        fprintf(fd, "r%d", r->num);
        break;
    }
    default: {
        simple_error("unknown register kind");
    }
    }
#endif
}

static bool printed_open(false);
static unsigned cluster_id(0U);

/// print out basic block information
static bool print_basic_block(basic_block *bb) throw() {

    printf("n%ud%u [label=<<FONT SIZE=\"0.1\" COLOR=\"white\">%u</FONT>%u | ", cluster_id, bb->id, cluster_id, bb->id);

    if(0 != bb->first) {
        for(simple_instr *in(bb->first); in != bb->last->next; in = in->next) {
            if(in != bb->first) {
                //printf("\n");
            }
            fprint_instr(stdout, in);
        }
    }

    printf(" >];\n");

    const std::set<basic_block *> &successors(bb->successors());
    std::set<basic_block *>::const_iterator it(successors.begin());
    const std::set<basic_block *>::const_iterator end(successors.end());

    for(; it != end; ++it) {
        printf("n%ud%u -> n%ud%u;\n", cluster_id, bb->id, cluster_id, (*it)->id);
    }

    return true;
}

static void print_close_digraph(void) throw() {
    printf("}\n");
}

/// print out the basic blocks as a DOT digraph
MAIN {

    cfg flow_graph(inlist);

    if(!printed_open) {
        printed_open = true;
        printf("digraph {\n");
        atexit(&print_close_digraph);
    }

    printf("subgraph cluster%u {\n", cluster_id);
    printf("label=\"%s\";", proc_name);
    printf("node [fontname=Courier shape=record nojustify=false labeljust=l];\n");
    flow_graph.for_each_basic_block(&print_basic_block);
    printf("}\n");

    ++cluster_id;

    return inlist;
}
