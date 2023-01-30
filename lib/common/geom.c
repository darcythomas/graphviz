/*************************************************************************
 * Copyright (c) 2011 AT&T Intellectual Property 
 * All rights reserved. This program and the accompanying materials
 * are made available under the terms of the Eclipse Public License v1.0
 * which accompanies this distribution, and is available at
 * http://www.eclipse.org/legal/epl-v10.html
 *
 * Contributors: Details at https://graphviz.org
 *************************************************************************/

/* geometric functions (e.g. on points and boxes) with application to, but
 * no specific dependence on graphs */

#include "config.h"

#include <common/geom.h>
#include <common/geomprocs.h>
#include <stdbool.h>

/*
 *--------------------------------------------------------------
 *
 * lineToBox --
 *
 *      Determine whether a line lies entirely inside, entirely
 *      outside, or overlapping a given rectangular area.
 *
 * Results:
 *      -1 is returned if the line given by p and q
 *      is entirely outside the rectangle given by b.
 * 	0 is returned if the polygon overlaps the rectangle, and
 *	1 is returned if the polygon is entirely inside the rectangle.
 *
 * Side effects:
 *      None.
 *
 *--------------------------------------------------------------
 */

/* This code steals liberally from algorithms in tk/generic/tkTrig.c -- jce */

int lineToBox(pointf p, pointf q, boxf b)
{
    /*
     * First check the two points individually to see whether they
     * are inside the rectangle or not.
     */

    bool inside1 = INSIDE(p, b);
    bool inside2 = INSIDE(q, b);
    if (inside1 != inside2) {
        return 0;
    }
    if (inside1 && inside2) {
        return 1;
    }

    /*
     * Both points are outside the rectangle, but still need to check
     * for intersections between the line and the rectangle.  Horizontal
     * and vertical lines are particularly easy, so handle them
     * separately.
     */

    if (p.x == q.x) {
        /*
         * Vertical line.
         */

        if (((p.y >= b.LL.y) ^ (q.y >= b.LL.y)) && BETWEEN(b.LL.x, p.x, b.UR.x)) {
            return 0;
        }
    } else if (p.y == q.y) {
        /*
         * Horizontal line.
         */
        if (((p.x >= b.LL.x) ^ (q.x >= b.LL.x)) && BETWEEN(b.LL.y, p.y, b.UR.y)) {
            return 0;
        }
    } else {
        double m, x, y, low, high;

        /*
         * Diagonal line.  Compute slope of line and use
         * for intersection checks against each of the
         * sides of the rectangle: left, right, bottom, top.
         */

        m = (q.y - p.y)/(q.x - p.x);
        if (p.x < q.x) {
            low = p.x;  high = q.x;
        } else {
            low = q.x; high = p.x;
        }

        /*
         * Left edge.
         */

        y = p.y + (b.LL.x - p.x)*m;
        if (BETWEEN(low, b.LL.x, high) && BETWEEN(b.LL.y, y, b.UR.y)) {
            return 0;
        }

        /*
         * Right edge.
         */

        y += (b.UR.x - b.LL.x)*m;
        if (BETWEEN(b.LL.y, y, b.UR.y) && BETWEEN(low, b.UR.x, high)) {
            return 0;
        }

        /*
         * Bottom edge.
         */

        if (p.y < q.y) {
            low = p.y;  high = q.y;
        } else {
            low = q.y; high = p.y;
        }
        x = p.x + (b.LL.y - p.y)/m;
        if (BETWEEN(b.LL.x, x, b.UR.x) && BETWEEN(low, b.LL.y, high)) {
            return 0;
        }

        /*
         * Top edge.
         */

        x += (b.UR.y - b.LL.y)/m;
        if (BETWEEN(b.LL.x, x, b.UR.x) && BETWEEN(low, b.UR.y, high)) {
            return 0;
        }
    }
    return -1;
}

void rect2poly(pointf *p)
{
    p[3].x = p[2].x = p[1].x;
    p[2].y = p[1].y;
    p[3].y = p[0].y;
    p[1].x = p[0].x;
}

static pointf rotatepf(pointf p, int cwrot)
{
    static double sina, cosa;
    static int last_cwrot;
    pointf P;

    /* cosa is initially wrong for a cwrot of 0
     * this caching only works because we are never called for 0 rotations */
    if (cwrot != last_cwrot) {
	sincos(cwrot / (2 * M_PI), &sina, &cosa);
	last_cwrot = cwrot;
    }
    P.x = p.x * cosa - p.y * sina;
    P.y = p.y * cosa + p.x * sina;
    return P;
}

pointf cwrotatepf(pointf p, int cwrot)
{
    double x = p.x, y = p.y;
    switch (cwrot) {
    case 0:
	break;
    case 90:
	p.x = y;
	p.y = -x;
	break;
    case 180:
	p.x = x;
	p.y = -y;
	break;
    case 270:
	p.x = y;
	p.y = x;
	break;
    default:
	if (cwrot < 0)
	    return ccwrotatepf(p, -cwrot);
        if (cwrot > 360)
	    return cwrotatepf(p, cwrot%360);
	return rotatepf(p, cwrot);
    }
    return p;
}

pointf ccwrotatepf(pointf p, int ccwrot)
{
    double x = p.x, y = p.y;
    switch (ccwrot) {
    case 0:
	break;
    case 90:
	p.x = -y;
	p.y = x;
	break;
    case 180:
	p.x = x;
	p.y = -y;
	break;
    case 270:
	p.x = y;
	p.y = x;
	break;
    default:
	if (ccwrot < 0)
	    return cwrotatepf(p, -ccwrot);
        if (ccwrot > 360)
	    return ccwrotatepf(p, ccwrot%360);
	return rotatepf(p, 360-ccwrot);
    }
    return p;
}

boxf flip_rec_boxf(boxf b, pointf p)
{
    boxf r;
    /* flip box */
    r.UR.x = b.UR.y;
    r.UR.y = b.UR.x;
    r.LL.x = b.LL.y;
    r.LL.y = b.LL.x;
    /* move box */
    r.LL.x += p.x;
    r.LL.y += p.y;
    r.UR.x += p.x;
    r.UR.y += p.y;
    return r;
}

#define SMALL 0.0000000001

/* ptToLine2:
 * Return distance from point p to line a-b squared.
 */
double ptToLine2 (pointf a, pointf b, pointf p)
{
  double dx = b.x-a.x;
  double dy = b.y-a.y;
  double a2 = (p.y-a.y)*dx - (p.x-a.x)*dy;
  a2 *= a2;   /* square - ensures that it is positive */
  if (a2 < SMALL) return 0.;  /* avoid 0/0 problems */
  return a2 / (dx*dx + dy*dy);
}

#define dot(v,w) (v.x*w.x+v.y*w.y)

/* line_intersect:
 * Computes intersection of lines a-b and c-d, returning intersection
 * point in *p.
 * Returns 0 if no intersection (lines parallel), 1 otherwise.
 */
int line_intersect (pointf a, pointf b, pointf c, pointf d, pointf* p)
{

    pointf mv = sub_pointf(b,a);
    pointf lv = sub_pointf(d,c);
    pointf ln = perp (lv);
    double lc = -dot(ln,c);
    double dt = dot(ln,mv);

    if (fabs(dt) < SMALL) return 0;

    *p = sub_pointf(a,scale((dot(ln,a)+lc)/dt,mv));
    return 1;
}

