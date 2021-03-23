/*************************************************************************
 * Copyright (c) 2011 AT&T Intellectual Property 
 * All rights reserved. This program and the accompanying materials
 * are made available under the terms of the Eclipse Public License v1.0
 * which accompanies this distribution, and is available at
 * http://www.eclipse.org/legal/epl-v10.html
 *
 * Contributors: Details at https://graphviz.org
 *************************************************************************/

#ifdef __cplusplus
extern "C" {
#endif



#ifndef EMBED_GRAPH_H_
#define EMBED_GRAPH_H_

#include <neatogen/defs.h>

    extern void embed_graph(vtx_data * graph, int n, int dim, DistType ***,
			    int);
    extern void center_coordinate(DistType **, int, int);

#endif

#ifdef __cplusplus
}
#endif
