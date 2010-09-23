/*****************************************************************************\
 *                        ANALYSIS PERFORMANCE TOOLS                         *
 *                                  MPItrace                                 *
 *              Instrumentation package for parallel applications            *
 *****************************************************************************
 *     ___     This library is free software; you can redistribute it and/or *
 *    /  __         modify it under the terms of the GNU LGPL as published   *
 *   /  /  _____    by the Free Software Foundation; either version 2.1      *
 *  /  /  /     \   of the License, or (at your option) any later version.   *
 * (  (  ( B S C )                                                           *
 *  \  \  \_____/   This library is distributed in hope that it will be      *
 *   \  \__         useful but WITHOUT ANY WARRANTY; without even the        *
 *    \___          implied warranty of MERCHANTABILITY or FITNESS FOR A     *
 *                  PARTICULAR PURPOSE. See the GNU LGPL for more details.   *
 *                                                                           *
 * You should have received a copy of the GNU Lesser General Public License  *
 * along with this library; if not, write to the Free Software Foundation,   *
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA          *
 * The GNU LEsser General Public License is contained in the file COPYING.   *
 *                                 ---------                                 *
 *   Barcelona Supercomputing Center - Centro Nacional de Supercomputacion   *
\*****************************************************************************/

/* -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=- *\
 | @file: $HeadURL: file:///mysql/svn/repos/ptools/extrae/trunk/src/merger/paraver/mpi_prv_semantics.c $
 | @last_commit: $Date: 2010-02-04 18:22:43 +0100 (Thu, 04 Feb 2010) $
 | @version:     $Revision: 160 $
\* -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=- */
#include "common.h"

static char UNUSED rcsid[] = "$Id: mpi_prv_semantics.c 160 2010-02-04 17:22:43Z harald $";

#ifdef HAVE_STDLIB_H
# include <stdlib.h>
#endif
#ifdef HAVE_STDIO_H
# include <stdio.h>
#endif

#include "timesync.h"
#include "paraver_state.h"
#include "paraver_generator.h"
#include "object_tree.h"
#include "trace_communication.h"

#if defined(PARALLEL_MERGE)
# include "parallel_merge_aux.h"
#endif

/******************************************************************************
 ***  trace_communication
 ******************************************************************************/

void trace_communicationAt (unsigned ptask, unsigned task_s, unsigned thread_s,
	unsigned task_r, unsigned thread_r, event_t *send_begin,
	event_t *send_end, event_t *recv_begin, event_t *recv_end, 
	int atposition, off_t position)
{
	struct thread_t *thread_r_info, *thread_s_info;
	unsigned long long log_s, log_r, phy_s, phy_r;
	unsigned cpu_r, cpu_s;

	/* Look for the receive partner ... in the sender events */
	thread_r_info = GET_THREAD_INFO(ptask, task_r, thread_r);
	cpu_r = thread_r_info->cpu;

	/* Look for the sender partner ... in the receiver events */
	thread_s_info = GET_THREAD_INFO(ptask, task_s, thread_s);
	cpu_s = thread_s_info->cpu;

	/* Synchronize event times */
	log_s = TIMESYNC(task_s-1, Get_EvTime (send_begin));
	phy_s = TIMESYNC(task_s-1, Get_EvTime (send_end));
	log_r = TIMESYNC(task_r-1, Get_EvTime (recv_begin));
	phy_r = TIMESYNC(task_r-1, Get_EvTime (recv_end));

	trace_paraver_communication (cpu_s, ptask, task_s, thread_s, log_s, phy_s,
	  cpu_r, ptask, task_r, thread_r, log_r, phy_r, Get_EvSize (recv_end),
		Get_EvTag (recv_end), atposition, position);
}

#if defined(PARALLEL_MERGE)
int trace_pending_communication (unsigned int ptask, unsigned int task,
	unsigned int thread, event_t * begin_s, event_t * end_s, unsigned int recvr)
{
	unsigned long long log_s, phy_s;

	/* Synchronize event times */
	log_s = TIMESYNC (task-1, Get_EvTime (begin_s));
	phy_s = TIMESYNC (task-1, Get_EvTime (end_s));

	trace_paraver_pending_communication (task, ptask, task, thread, log_s,
		phy_s, recvr + 1, ptask, recvr + 1, thread, 0ULL, 0ULL, Get_EvSize (begin_s), Get_EvTag (begin_s));
  return 0;
}
#endif
