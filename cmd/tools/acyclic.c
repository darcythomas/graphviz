/**
 * @file
 * @brief make directed graph acyclic
 */

/*************************************************************************
 * Copyright (c) 2011 AT&T Intellectual Property 
 * All rights reserved. This program and the accompanying materials
 * are made available under the terms of the Eclipse Public License v1.0
 * which accompanies this distribution, and is available at
 * http://www.eclipse.org/legal/epl-v10.html
 *
 * Contributors: Details at https://graphviz.org
 *************************************************************************/


/*
 * Written by Stephen North
 * Updated by Emden Gansner
 */

#include <stdbool.h>
#include <stdio.h>

#include <stdlib.h>
#include <cgraph/cgraph.h>
#include <cgraph/exit.h>
#include <cgraph/unreachable.h>
#include "openFile.h"

typedef struct {
    Agrec_t h;
    int mark;
    bool onstack: 1;
} Agnodeinfo_t;

#define ND_mark(n) (((Agnodeinfo_t*)((n)->base.data))->mark)
#define ND_onstack(n) (((Agnodeinfo_t*)((n)->base.data))->onstack)
#define graphName(g) (agnameof(g))

#include <getopt.h>

static FILE *inFile;
static FILE *outFile;
static int doWrite = 1;
static int Verbose;
static char *cmd;
static int num_rev;

/* addRevEdge:
 * Add a reversed version of e. The new edge has the same key.
 * We also copy the attributes, reversing the roles of head and 
 * tail ports.
 * This assumes we've already checked that such an edge does not exist.
 */
static void addRevEdge(Agraph_t * g, Agedge_t * e)
{
    Agsym_t* sym;
    Agedge_t* f = agedge (g, aghead(e), agtail(e), agnameof(e), 1);

    agcopyattr (e, f);

    num_rev++;
    sym = agattr (g, AGEDGE, TAILPORT_ID, 0);
    if (sym) agsafeset (f, HEADPORT_ID, agxget (e, sym), "");
    sym = agattr (g, AGEDGE, HEADPORT_ID, 0);
    if (sym) agsafeset (f, TAILPORT_ID, agxget (e, sym), "");
}

/* dfs:
 * Return the number of reversed edges for this component.
 */
static int dfs(Agraph_t * g, Agnode_t * t, int hasCycle)
{
    Agedge_t *e;
    Agedge_t *f;
    Agnode_t *h;

    ND_mark(t) = 1;
    ND_onstack(t) = true;
    for (e = agfstout(g, t); e; e = f) {
	f = agnxtout(g, e);
	if (agtail(e) == aghead(e))
	    continue;
	h = aghead(e);
	if (ND_onstack(h)) {
	    if (agisstrict(g)) {
		if (agedge(g, h, t, 0, 0) == 0)
		    addRevEdge(g, e);
	    } else {
		char* key = agnameof (e);
		if (!key || agedge(g, h, t, key, 0) == 0)
		    addRevEdge(g, e);
	    }
	    agdelete(g, e);
	    hasCycle = 1;
	} else if (ND_mark(h) == 0)
	    hasCycle |= dfs(g, h, hasCycle);
    }
    ND_onstack(t) = false;
    return hasCycle;
}

static char *useString = "Usage: %s [-nv?] [-o outfile] <file>\n\
  -o <file> - put output in <file>\n\
  -n        - do not output graph\n\
  -v        - verbose\n\
  -?        - print usage\n";

static void usage(int v)
{
    fprintf(stderr, useString, cmd);
    graphviz_exit(v);
}

static void init(int argc, char *argv[])
{
    int c;

    cmd = argv[0];
    opterr = 0;
    while ((c = getopt(argc, argv, ":vno:")) != -1)
	switch (c) {
	case 'o':
	    if (outFile != NULL)
		fclose(outFile);
	    outFile = openFile(argv[0], optarg, "w");
	    break;
	case 'n':
	    doWrite = 0;
	    break;
	case 'v':
	    Verbose = 1;
	    break;
	case '?':
	    if (optopt == '?')
		usage(0);
	    else {
		fprintf(stderr, "%s: option -%c unrecognized\n", cmd,
			optopt);
		usage(-1);
	    }
	    break;
	case ':':
	    fprintf(stderr, "%s: missing argument for option -%c\n",
		    cmd, optopt);
	    usage(-1);
	    break;
	default:
	    UNREACHABLE();
	}
    if (optind < argc) {
	inFile = openFile(argv[0], argv[optind], "r");
    } else
	inFile = stdin;
    if (!outFile)
	outFile = stdout;

}

int main(int argc, char *argv[])
{
    Agraph_t *g;
    Agnode_t *n;
    int rv = 0;

    init(argc, argv);

    if ((g = agread(inFile, NULL)) != NULL) {
	if (agisdirected (g)) {
	    aginit(g, AGNODE, "info", sizeof(Agnodeinfo_t), TRUE);
	    for (n = agfstnode(g); n; n = agnxtnode(g, n)) {
		if (ND_mark(n) == 0)
		    rv |= dfs(g, n, 0);
	    }
	    if (doWrite) {
		agwrite(g, outFile);
		fflush(outFile);
	    }
	    if (Verbose) {
		if (rv)
		    fprintf(stderr, "Graph \"%s\" has cycles; %d reversed edges\n", graphName(g), num_rev);
		else
		    fprintf(stderr, "Graph \"%s\" is acyclic\n", graphName(g));
	    }
	} else {
	    rv = -1;
	    if (Verbose)
		fprintf(stderr, "Graph \"%s\" is undirected\n", graphName(g));
	}
	graphviz_exit(rv);
    } else
	graphviz_exit(-1);
}
