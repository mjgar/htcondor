/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2004, Condor Team, Computer Sciences Department,
  * University of Wisconsin-Madison, WI.
  *
  * This source code is covered by the Condor Public License, which can
  * be found in the accompanying LICENSE.TXT file, or online at
  * www.condorproject.org.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  * AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
  * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  * WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
  * FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
  * HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
  * MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
  * ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
  * PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
  * RIGHT.
  *
  ****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/

#ifndef CONDOR_UNIVERSE_H
#define CONDOR_UNIVERSE_H

#include "condor_header_features.h"

BEGIN_C_DECLS

/*
Warning: These symbols must stay in sync
with the strings in condor_universe.c
*/

#define CONDOR_UNIVERSE_MIN       0  /* A placeholder, not a universe */
#define CONDOR_UNIVERSE_STANDARD  1  /* Single process relinked jobs */
#define CONDOR_UNIVERSE_PIPE      2  /* A placeholder, no longer used */
#define CONDOR_UNIVERSE_LINDA     3  /* A placeholder, no longer used */
#define CONDOR_UNIVERSE_PVM       4  /* Parallel Virtual Machine apps */
#define CONDOR_UNIVERSE_VANILLA   5  /* Single process non-relinked jobs */
#define CONDOR_UNIVERSE_PVMD      6  /* PVM daemon process */
#define CONDOR_UNIVERSE_SCHEDULER 7  /* A job run under the schedd */
#define CONDOR_UNIVERSE_MPI       8  /* Message Passing Interface jobs */
#define CONDOR_UNIVERSE_GLOBUS    9  /* Jobs managed by condor_gmanager */
#define CONDOR_UNIVERSE_JAVA      10 /* Jobs for the Java Virtual Machine */
#define CONDOR_UNIVERSE_PARALLEL  11 /* Generalized parallel jobs */
#define CONDOR_UNIVERSE_LOCAL     12 /* Job run locally by the schedd */
#define CONDOR_UNIVERSE_MAX       13 /* A placeholder, not a universe. */

/* To get the name of a universe, call this function */
const char *CondorUniverseName( int universe );
const char *CondorUniverseNameUcFirst( int universe );

/* To get the number of a universe from a string, call this.  Returns
   0 if the given string doesn't correspond to a known universe */
int CondorUniverseNumber( const char* univ );

BOOLEAN universeCanReconnect( int universe );

END_C_DECLS

#endif


