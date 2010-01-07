/*****************************************************************************\
 *                        ANALYSIS PERFORMANCE TOOLS                         *
 *                                  MPItrace                                 *
 *              Instrumentation package for parallel applications            *
 *****************************************************************************
 *                                                             ___           *
 *   +---------+     http:// www.cepba.upc.edu/tools_i.htm    /  __          *
 *   |    o//o |     http:// www.bsc.es                      /  /  _____     *
 *   |   o//o  |                                            /  /  /     \    *
 *   |  o//o   |     E-mail: cepbatools@cepba.upc.edu      (  (  ( B S C )   *
 *   | o//o    |     Phone:          +34-93-401 71 78       \  \  \_____/    *
 *   +---------+     Fax:            +34-93-401 25 77        \  \__          *
 *    C E P B A                                               \___           *
 *                                                                           *
 * This software is subject to the terms of the CEPBA/BSC license agreement. *
 *      You must accept the terms of this license to use this software.      *
 *                                 ---------                                 *
 *                European Center for Parallelism of Barcelona               *
 *                      Barcelona Supercomputing Center                      *
\*****************************************************************************/

/* -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=- *\
 | @file: $HeadURL: https://svn.bsc.es/repos/ptools/mpitrace/fusion/trunk/src/tracer/wrappers/PACX_/pacx_wrapper.c $
 | 
 | @last_commit: $Date: 2009-12-16 12:59:18 +0100 (dc, 16 des 2009) $
 | @version:     $Revision: 89 $
 | 
 | History:
\* -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=- */
#include "common.h"

static char UNUSED rcsid[] = "$Id: pacx_wrapper.c 89 2009-12-16 11:59:18Z gllort $";

#ifdef HAVE_STDIO_H
# include <stdio.h>
#endif
#ifdef HAVE_STDLIB_H
# include <stdlib.h>
#endif
#ifdef HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif
#ifdef HAVE_SYS_STAT_H
# include <sys/stat.h>
#endif
#ifdef HAVE_FCNTL_H
# include <fcntl.h>
#endif
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif
#ifdef HAVE_STRING_H
# include <string.h>
#endif

#include "hash_table.h"
#include "pacx_wrapper.h"
#include "wrapper.h"
#include "clock.h"
#include "hash_table.h"
#include "signals.h"
#include "myrinet_hwc.h"
#include "misc_wrapper.h"
#include "pacx_interface.h"
#include "mode.h"

#include <mpi.h>
#include "mpif.h"

#if defined(C_SYMBOLS) && defined(FORTRAN_SYMBOLS)
# define COMBINED_SYMBOLS
#endif

#if defined(HAVE_MRNET)
# include "mrnet_be.h"
#endif

#define PACX_COMM_WORLD_ALIAS 1
#define PACX_COMM_SELF_ALIAS  2

#if !defined(PACX_HAS_MPI_F_STATUS_IGNORE)
# define MPI_F_STATUS_IGNORE   ((PACX_Fint *) 0)
# define MPI_F_STATUSES_IGNORE ((PACX_Fint *) 0)
#endif

/*
	He d'incloure la capc,alera del misc_wrapper per poder comenc,ar 
	a tracejar quan es cridi al PACX_init i acabar al PACX_Finalize.
*/
#include "misc_wrapper.h"

/* Cal tenir requests persistents per algunes operacions */
#include "persistent_requests.h"

#if defined(IS_BGL_MACHINE)
# include "rts.h"
#endif

#if defined(IS_BGP_MACHINE)
# include "spi/kernel_interface.h"
# include "common/bgp_personality.h"
# include "common/bgp_personality_inlines.h"
#endif

#ifdef HAVE_NETINET_IN_H
# include <netinet/in.h>
#endif

#define PACX_CHECK(pacx_error, routine) \
	if (pacx_error != MPI_SUCCESS) \
	{ \
		fprintf (stderr, "Error in PACX call %s (file %s, line %d, routine %s) returned %d\n", \
			#routine, __FILE__, __LINE__, __func__, pacx_error); \
		fflush (stderr); \
		exit (1); \
	}

static char * PACX_Distribute_XML_File (int rank, int world_size, char *origen);
static void Gather_MPITS(void);
static void Trace_PACX_Communicator (int tipus_event, PACX_Comm newcomm);

/* int mpit_gathering_enabled = FALSE; */

/******************************************************************************
 ********************      L O C A L    V A R I A B L E S        **************
 ******************************************************************************/

static void OMPItrace_MPI_stats_Wrapper (iotimer_t timestamp);

#define MAX_WAIT_REQUESTS 16384

static hash_t requests;         /* Receive requests stored in a hash in order to search them fast */
static PR_Queue_t PR_queue;     /* Persistent requests queue */
static int *ranks_aux;          /* Auxiliary ranks vector */
static int *ranks_global;       /* Global ranks vector (from 1 to NProcs) */
static PACX_Group grup_global;   /* Group attached to the PACX_COMM_WORLD */
static PACX_Fint grup_global_F;  /* Group attached to the PACX_COMM_WORLD (Fortran) */

#if defined(IS_BGL_MACHINE)     /* BGL, s'intercepten algunes crides barrier dins d'altres cols */
static int BGL_disable_barrier_inside = 0;
#endif

/* PACX Stats */
static int P2P_Bytes_Sent        = 0;      /* Sent bytes by point to point PACX operations */
static int P2P_Bytes_Recv        = 0;      /* Recv bytes by point to point PACX operations */
static int GLOBAL_Bytes_Sent     = 0;      /* Sent "bytes" by PACX global operations */
static int GLOBAL_Bytes_Recv     = 0;      /* Recv "bytes" by PACX global operations */
static int P2P_Communications    = 0;  /* Number of point to point communications */
static int GLOBAL_Communications = 0;  /* Number of global operations */
static int Elapsed_Time_In_MPI   = 0;  /* Time inside MPI/PACX calls */

/****************************************************************************
 *** Variables de tamany necessari per accedir a camps de resulstats Fortran
 ****************************************************************************/

#if defined(IS_BG_MACHINE)
static void BG_gettopology (void)
{
#if defined(IS_BGL_MACHINE)
	BGLPersonality personality;
	unsigned personality_size = sizeof (personality);
	iotimer_t t1, t2;

	rts_get_personality (&personality, personality_size);
	t1 = TIME;
	TRACE_MISCEVENT (t1, USER_EV, BG_PERSONALITY_TORUS_X, personality.xCoord);
	TRACE_MISCEVENT (t1, USER_EV, BG_PERSONALITY_TORUS_Y, personality.yCoord);
	TRACE_MISCEVENT (t1, USER_EV, BG_PERSONALITY_TORUS_Z, personality.zCoord);
	TRACE_MISCEVENT (t1, USER_EV, BG_PERSONALITY_PROCESSOR_ID, rts_get_processor_id ());
#endif

#if defined(IS_BGP_MACHINE)
	_BGP_Personality_t personality;
	unsigned personality_size = sizeof (personality);
	iotimer_t t1, t2;
	
	Kernel_GetPersonality (&personality, personality_size);
	t1 = TIME;
	TRACE_MISCEVENT (t1, USER_EV, BG_PERSONALITY_TORUS_X, BGP_Personality_xCoord(&personality));
	TRACE_MISCEVENT (t1, USER_EV, BG_PERSONALITY_TORUS_Y, BGP_Personality_yCoord(&personality));
	TRACE_MISCEVENT (t1, USER_EV, BG_PERSONALITY_TORUS_Z, BGP_Personality_zCoord(&personality));
	TRACE_MISCEVENT (t1, USER_EV, BG_PERSONALITY_PROCESSOR_ID, BGP_Personality_rankInPset (&personality));
#endif

	t2 = TIME;
	TRACE_MISCEVENT (t2, USER_EV, BG_PERSONALITY_TORUS_X, 0);
	TRACE_MISCEVENT (t2, USER_EV, BG_PERSONALITY_TORUS_Y, 0);
	TRACE_MISCEVENT (t2, USER_EV, BG_PERSONALITY_TORUS_Z, 0);
	TRACE_MISCEVENT (t2, USER_EV, BG_PERSONALITY_PROCESSOR_ID, 0);
}
#endif

#if defined(IS_MN_MACHINE)
#define MAX_BUFFER 1024
static void MN_gettopology (void) 
{
	char hostname[MAX_BUFFER];
	int rc;
	int server,center,blade;
	int linear_host;
	int linecard, host;

	if (gethostname(hostname, MAX_BUFFER - 1) == 0)
	{
		iotimer_t temps = TIME;

		rc = sscanf(hostname, "s%dc%db%d", &server, &center, &blade);
		if (rc != 3) return;
		linear_host = server*(4*14) + (center - 1) * 14 + (blade - 1);
		linecard = linear_host / 16;
		host = linear_host % 16;

		TRACE_MISCEVENT(temps, USER_EV, MN_LINEAR_HOST_EVENT, linear_host);
		TRACE_MISCEVENT(temps, USER_EV, MN_LINECARD_EVENT, linecard);
		TRACE_MISCEVENT(temps, USER_EV, MN_HOST_EVENT, host);
	}
	else
		fprintf(stderr, "mpitrace: could not get hostname, is it longer than %d bytes?\n", (MAX_BUFFER-1));
}
#endif

static void GetTopology (void)
{
#if defined(IS_MN_MACHINE)
	MN_gettopology();
#elif defined(IS_BG_MACHINE)
	BG_gettopology();
#endif
}

/******************************************************************************
 *** CheckGlobalOpsTracingIntervals()
 ******************************************************************************/
void CheckGlobalOpsTracingIntervals (void)
{
	int result;

	result = GlobalOp_Changes_Trace_Status (PACX_NumOpsGlobals);
	if (result == SHUTDOWN)
		MPItrace_shutdown_Wrapper();
	else if (result == RESTART)
		MPItrace_restart_Wrapper();
}

/******************************************************************************
 ***  get_rank_obj_C
 ******************************************************************************/

static int get_rank_obj_C (PACX_Comm comm, int dest, int *receiver)
{
	int ret, inter;
	PACX_Group group;

	/* If rank in PACX_COMM_WORLD or if dest is PROC_NULL or any source,
	   return value directly */
	if (comm == PACX_COMM_WORLD || dest == MPI_PROC_NULL || dest == MPI_ANY_SOURCE)
	{
		*receiver = dest;
	}
	else
	{
		ret = PPACX_Comm_test_inter (comm, &inter);	
		PACX_CHECK (ret, PPACX_Comm_test_inter);

		if (inter)
		{
			ret = PPACX_Comm_remote_group (comm, &group);
			PACX_CHECK (ret, PPACX_Comm_remote_group);
		}
		else
		{
			ret = PPACX_Comm_group (comm, &group);
			PACX_CHECK (ret, PPACX_Comm_group);
		}

		/* Translate the rank */
		ret = PPACX_Group_translate_ranks (group, 1, &dest, grup_global, receiver);
		PACX_CHECK (ret, PPACX_Group_translate_ranks);
		
		ret = PPACX_Group_free (&group);
		PACX_CHECK (ret, PPACX_Group_free);
	}
	return MPI_SUCCESS;
}

/******************************************************************************
 ***  Traceja_Persistent_Request
 ******************************************************************************/

static void Traceja_Persistent_Request (PACX_Request* reqid, iotimer_t temps)
{
  persistent_req_t *p_request;
  hash_data_t hash_req;
  int inter;
  int size, src_world, ret;

  /*
   * S'intenta recuperar la informacio d'aquesta request per tracejar-la 
   */
  p_request = PR_Busca_request (&PR_queue, reqid);
  if (p_request == NULL)
    return;

	/* 
	  HSG, aixo es pot emmagatzemar a la taula de hash! A mes,
	  pot ser que hi hagi un problema a l'hora de calcular els  bytes p2p
	  pq ignora la quantitat de dades enviada
	*/
#warning "Aixo es pot millorar"
  ret = PPACX_Type_size (p_request->datatype, &size);
	PACX_CHECK(ret, PPACX_Type_size);

	if (get_rank_obj_C (p_request->comm, p_request->task, &src_world) != MPI_SUCCESS)
		return;

	if (p_request->tipus == PACX_IRECV_EV)
	{
		/*
		 * Als recv guardem informacio pels WAITs 
		*/
		hash_req.key = *reqid;
		hash_req.commid = p_request->comm;
		hash_req.partner = p_request->task;
		hash_req.tag = p_request->tag;
		hash_req.size = p_request->count * size;

		if (p_request->comm == PACX_COMM_WORLD)
		{
			hash_req.group = PACX_GROUP_NULL;
		}
		else
		{
			ret = PPACX_Comm_test_inter (p_request->comm, &inter);
			PACX_CHECK (ret, PPACX_Comm_test_inter);
			
			if (inter)
			{
				ret = PPACX_Comm_remote_group (p_request->comm, &hash_req.group);
				PACX_CHECK (ret, PPACX_Comm_remote_group);
			}
			else
			{
				ret = PPACX_Comm_group (p_request->comm, &hash_req.group);	
				PACX_CHECK (ret, PPACX_Comm_group);
			}
     }

     hash_add (&requests, &hash_req);
  }

  /* PACX Stats */
  P2P_Communications ++;
  if (p_request->tipus == PACX_IRECV_EV)
  {
     /* Bytes received are computed at PACX_Wait or PACX_Test */
  }
  else if (p_request->tipus == PACX_ISEND_EV)
  {
     P2P_Bytes_Sent += size;
  }

  /*
   *   event : PERSIST_REQ_EV                        value : Request type
   *   target : MPI_ANY_SOURCE or sender/receiver    size  : buffer size
   *   tag : message tag or PACX_ANY_TAG              commid: Communicator id
   *   aux: request id
   */
  TRACE_MPIEVENT_NOHWC (temps, PACX_PERSIST_REQ_EV, p_request->tipus, src_world,
                        size, p_request->tag, p_request->comm, p_request->req);
}


/******************************************************************************
 *** CheckControlFile()
 ******************************************************************************/

/* This counter indicates when will be the next check for the control file */
unsigned int NumOpsGlobalsCheckControlFile        = 10;
unsigned int NumOpsGlobalsCheckControlFile_backup = 10;

unsigned long long CalculateNumOpsForPeriod (unsigned long long wannaPeriod,
	unsigned long long NumOfGlobals, unsigned long long runnedPeriod)
{
	if (runnedPeriod <= wannaPeriod * NumOfGlobals)
		return (wannaPeriod * NumOfGlobals) / runnedPeriod;

	return 1;
}

void CheckControlFile(void)
{
	unsigned int prevtracejant = tracejant;
	unsigned int wannatrace = 0;

	NumOpsGlobalsCheckControlFile--;
	
	if (!NumOpsGlobalsCheckControlFile)
	{
		if (TASKID == 0)
		{
			wannatrace = file_exists (ControlFileName);
			if (wannatrace != prevtracejant)
			{
				fprintf (stdout, "mpitrace: Tracing is %s via control file\n", (wannatrace)?"activated":"deactivated");
				if (wannatrace)
					mpitrace_on = TRUE;
			}

			if (WantedCheckControlPeriod != 0)
			{
				NumOpsGlobalsCheckControlFile_backup = CalculateNumOpsForPeriod (WantedCheckControlPeriod, NumOpsGlobalsCheckControlFile_backup, TIME - initTracingTime);
				fprintf (stderr, "mpitrace: Control file check change, now every %u global ops (%llu s)\n", NumOpsGlobalsCheckControlFile_backup, WantedCheckControlPeriod / 1000000000);
			}
		}

		/* Broadcast the following num of global-num-ops before being checked*/
		PPACX_Bcast (&NumOpsGlobalsCheckControlFile_backup, 1, PACX_LONG_LONG_INT, 0, 
			PACX_COMM_WORLD);

		/* Broadcast both mpitrace_on & tracing */
		{
			int valors[2] = { wannatrace, mpitrace_on };
			PPACX_Bcast (valors, 2, PACX_INT, 0, PACX_COMM_WORLD);
			wannatrace = valors[0];
			mpitrace_on = valors[1];

			if (mpitrace_on)
			{
				/* Turn on if it was off, and turn off it it was on */
				if (wannatrace && !prevtracejant)
					MPItrace_restart_Wrapper();
				else if (!wannatrace && prevtracejant)
					MPItrace_shutdown_Wrapper();
			}
		}

		/* If the tracing has been enabled, just change the init tracing time. */
		/* If not, just reset init tracing time so as the next period will be 
		   calculated from this point */
		if (mpitrace_on && initTracingTime == 0)
			initTracingTime = TIME;

		NumOpsGlobalsCheckControlFile = NumOpsGlobalsCheckControlFile_backup;
	}
}

/******************************************************************************
 ***  InitPACXCommunicators
 ******************************************************************************/

static void InitPACXCommunicators (void)
{
	int i;

	/** Inicialitzacio de les variables per la creacio de comunicadors **/
	ranks_global = malloc (sizeof(int)*NumOfTasks);
	if (ranks_global == NULL)
	{
		fprintf (stderr, "mpitrace: Error! Unable to get memory for 'ranks_global'");
		exit (0);
	}
	ranks_aux = malloc (sizeof(int)*NumOfTasks);
	if (ranks_aux == NULL)
	{
		fprintf (stderr, "mpitrace: Error! Unable to get memory for 'ranks_aux'");
		exit (0);
	}

	for (i = 0; i < NumOfTasks; i++)
		ranks_global[i] = i;

	PPACX_Comm_group (PACX_COMM_WORLD, &grup_global);
	grup_global_F = PACX_Group_c2f(grup_global);
}


/******************************************************************************
 ***  remove_file_list
 ******************************************************************************/
void remove_file_list (void)
{
	char tmpname[1024];

	if (TASKID == 0)
	{
		sprintf (tmpname, "%s/%s.mpits", final_dir, appl_name);
		unlink (tmpname);
	}
}

/******************************************************************************
 ***  Get_Nodes_Info
 ******************************************************************************/

char **TasksNodes = NULL;

void Share_Nodes_Info (void)
{
	int i, rc;
	char hostname[MPI_MAX_PROCESSOR_NAME];
	int hostname_length;
	char *buffer_names = NULL;

	/* Get processor name */
	rc = PMPI_Get_processor_name (hostname, &hostname_length);
	PACX_CHECK(rc, PMPI_Get_processor_name);

	/* Change spaces " " into underscores "_" (BLG nodes use to have spaces in their names) */
	for (i = 0; i < hostname_length; i++)
		if (' ' == hostname[i])
			hostname[i] = '_';

	/* Share information among all tasks */
	buffer_names = (char*) malloc (sizeof(char) * NumOfTasks * MPI_MAX_PROCESSOR_NAME);
	rc = PPACX_Allgather (hostname, MPI_MAX_PROCESSOR_NAME, PACX_CHAR, buffer_names, MPI_MAX_PROCESSOR_NAME, PACX_CHAR, PACX_COMM_WORLD);
	PACX_CHECK(rc, PPACX_Gather);

	/* Store the information in a global array */
	TasksNodes = (char **)malloc (NumOfTasks * sizeof(char *));
	for (i=0; i<NumOfTasks; i++)
	{
		char *tmp = &buffer_names[i*MPI_MAX_PROCESSOR_NAME];
		TasksNodes[i] = (char *)malloc((strlen(tmp)+1) * sizeof(char));
		strcpy (TasksNodes[i], tmp);
	}

	/* Free the local array, not the global one */
	free (buffer_names);
}

/******************************************************************************
 ***  Generate_Task_File_List
 ******************************************************************************/
static int Generate_Task_File_List (int n_tasks, char **node_list)
{
    int ierror, val = getpid ();
    int i, filedes, ret, thid;
    pid_t *buffer_pids = NULL;
    char tmpname[1024];
    int *buffer_threads = NULL;
    int nthreads = Backend_getNumberOfThreads();

    if (TASKID == 0)
    {
        buffer_pids = (pid_t *) malloc (sizeof(pid_t) * NumOfTasks);
        buffer_threads = (int*) malloc (sizeof(int) * NumOfTasks);
    }

    /* Share PID and number of threads of each PACX task */
    ierror = PPACX_Gather (&val, 1, PACX_INT, buffer_pids, 1, PACX_INT, 0, PACX_COMM_WORLD);
    PACX_CHECK(ierror, PPACX_Gather);

    ierror = PPACX_Gather (&nthreads, 1, PACX_INT, buffer_threads, 1, PACX_INT, 0, PACX_COMM_WORLD);
    PACX_CHECK(ierror, PPACX_Gather);

    if (TASKID == 0)
    {
        sprintf (tmpname, "%s/%s.mpits", final_dir, appl_name);

        filedes = open (tmpname, O_RDWR | O_CREAT | O_TRUNC, 0644);
        if (filedes < 0)
            return -1;

        for (i = 0; i < NumOfTasks; i++)
        {
            char tmp_line[2048];

            for (thid = 0; thid < buffer_threads[i]; thid++)
            {
                //Tracefile_Name (tmpname, final_dir, appl_name, buffer_pids[i], i, thid);
				FileName_PTT(tmpname, final_dir, appl_name, buffer_pids[i], i, thid, EXT_MPIT);

                sprintf (tmp_line, "%s on %s\n", tmpname, node_list[i]);
                ret = write (filedes, tmp_line, strlen (tmp_line));
                if (ret != strlen (tmp_line))
                {
                    close (filedes);
                    return -1;
                }
            }
        }
        close (filedes);
    }

    if (TASKID == 0)
    {
      free (buffer_threads);
      free (buffer_pids);
    }

    return 0;
}

#if defined(IS_CELL_MACHINE)
/******************************************************************************
 ***  generate_spu_file_list
 ******************************************************************************/

int generate_spu_file_list (int number_of_spus)
{
	int ierror, val = getpid ();
	int i, filedes, ret, thid, hostname_length;
	int *buffer_numspus, *buffer_threads, *buffer_pids, *buffer_names;
	char tmpname[1024];
	char hostname[MPI_MAX_PROCESSOR_NAME];
	int nthreads = Backend_getNumberOfThreads();

	if (TASKID == 0)
	{
		buffer_threads = (int*) malloc (sizeof(int) * NumOfTasks);
		buffer_numspus = (int*) malloc (sizeof(int) * NumOfTasks);
		buffer_pids    = (int*) malloc (sizeof(int) * NumOfTasks);
		buffer_names   = (char*) malloc (sizeof(char) * NumOfTasks * MPI_MAX_PROCESSOR_NAME);
	}

	/* Share CELL count threads of each PACX task */
	ierror = PMPI_Get_processor_name (hostname, &hostname_length);
	PACX_CHECK(rc, PMPI_Get_processor_name);

	/* Some machines include " " spaces on their name (mainly BGL nodes)
	   change to underscore */
	for (i = 0; i < hostname_length; i++)
		if (' ' == hostname[i])
			hostname[i] = '_';

	ierror = PPACX_Gather (hostname, MPI_MAX_PROCESSOR_NAME, PACX_CHAR, buffer_names, MPI_MAX_PROCESSOR_NAME, PACX_CHAR, 0, PACX_COMM_WORLD);
	PACX_CHECK(ierror, PPACX_Gather);

	ierror = PPACX_Gather (&nthreads, 1, PACX_INT, buffer_threads, 1, PACX_INT, 0, PACX_COMM_WORLD);
	PACX_CHECK(ierror, PPACX_Gather);

	ierror = PPACX_Gather (&number_of_spus, 1, PACX_INT, buffer_numspus, 1, PACX_INT, 0, PACX_COMM_WORLD);
	PACX_CHECK(ierror, PPACX_Gather);

	ierror = PPACX_Gather (&val, 1, PACX_INT, buffer_pids, 1, PACX_INT, 0, PACX_COMM_WORLD);
	PACX_CHECK(ierror, PPACX_Gather);

	if (TASKID == 0)
	{
		sprintf (tmpname, "%s/%s.mpits", final_dir, appl_name);

		filedes = open (tmpname, O_WRONLY | O_APPEND , 0644);
		if (filedes < 0)
			return -1;

		/* For each task, provide the line for each SPU thread. If the application
		has other threads, skip those identifiers. */
		for (i = 0; i < NumOfTasks; i++)
		{
			char tmp_line[2048];

			for (thid = 0; thid < buffer_numspus[i]; thid++)
			{
				/* Tracefile_Name (tmpname, final_dir, appl_name, buffer_pids[i], i, thid+buffer_threads[i]); */
				FileName_PTT(tmpname, final_dir, appl_name, buffer_pids[i], i, thid+buffer_threads[i], EXT_MPIT);

				sprintf (tmp_line, "%s on %s-SPU%d\n", tmpname, &buffer_names[i*MPI_MAX_PROCESSOR_NAME], thid);

				ret = write (filedes, tmp_line, strlen (tmp_line));
				if (ret != strlen (tmp_line))
				{
					close (filedes);
					return -1;
				}
			}
		}
		close (filedes);
	}

	if (TASKID == 0)
	{
		free (buffer_threads);
		free (buffer_numspus);
		free (buffer_pids);
		free (buffer_names);
	}

  return 0;
}
#endif /* IS_CELL_MACHINE */

#if defined(FORTRAN_SYMBOLS)

/* Some C libraries do not contain the pacx_init symbol (fortran)
	 When compiling the combined (C+Fortran) dyninst module, the resulting
	 module CANNOT be loaded if pacx_init is not found. The top #if def..
	 is a workaround for this situation
*/
/*#if (defined(COMBINED_SYMBOLS) && defined(PACX_C_CONTAINS_FORTRAN_PACX_INIT) || \
     !defined(COMBINED_SYMBOLS) && defined(FORTRAN_SYMBOLS))
*/
/******************************************************************************
 ***  PPACX_Init_Wrapper
 ******************************************************************************/
void PPACX_Init_Wrapper (PACX_Fint *ierror)
/* Aquest codi nomes el volem per traceig sequencial i per pacx_init de fortran */
{
	int res;
	PACX_Fint me, ret, comm, tipus_enter;
	iotimer_t temps_inici_PACX_Init, temps_final_PACX_Init;
	char *config_file;

	mptrace_IsMPI = TRUE;

	hash_init (&requests);
	PR_queue_init (&PR_queue);

	CtoF77 (ppacx_init) (ierror);

	comm = PACX_Comm_c2f (PACX_COMM_WORLD);
	tipus_enter = PACX_Type_c2f (PACX_INT);

	CtoF77 (ppacx_comm_rank) (&comm, &me, &ret);
	PACX_CHECK(ret, ppacx_comm_rank);

	CtoF77 (ppacx_comm_size) (&comm, &NumOfTasks, &ret);
	PACX_CHECK(ret, ppacx_comm_size);

	InitPACXommunicators();

	/* We have to gather the task id */ 
	TaskID_Setup (me);

#if defined(SAMPLING_SUPPORT)
	/* If sampling is enabled, just stop all the processes at the same point
	   and continue */
	CtoF77 (ppacx_barrier) (&comm, &res);
#endif

	if (getenv ("MPTRACE_CONFIG_FILE") != NULL)
		/* Obtain a localized copy *except for the master process* */
		config_file = PACX_Distribute_XML_File (TASKID, NumOfTasks, getenv ("MPTRACE_CONFIG_FILE"));
	else
		config_file = NULL;

	/* Initialize the backend */
	res = Backend_preInitialize (TASKID, NumOfTasks, config_file);

	/* Remove the local copy only if we're not the master */
	if (me != 0)
		unlink (config_file);
	free (config_file);

	if (!res)
		return;

	Share_Nodes_Info ();
	Generate_Task_File_List (NumOfTasks, TasksNodes);

	/* Take the time now, we can't put MPIINIT_EV before APPL_EV */
	temps_inici_PACX_Init = TIME;
	
	/* Call a barrier in order to synchronize all tasks using MPIINIT_EV / END*/
	CtoF77 (ppacx_barrier) (&comm, &res);

	initTracingTime = temps_final_PACX_Init = TIME;

	/* End initialization of the backend  (put MPIINIT_EV { BEGIN/END } ) */
	if (!Backend_postInitialize (me, NumOfTasks, temps_inici_PACX_Init, temps_final_PACX_Init, TasksNodes))
		return;

	/* Annotate topologies (if available) */
	GetTopology();

	/* Annotate already built communicators */
	Trace_PACX_Communicator (PACX_COMM_CREATE_EV, PACX_COMM_WORLD);
	Trace_PACX_Communicator (PACX_COMM_CREATE_EV, PACX_COMM_SELF);
}
/*
#endif
*/
/*
#if (defined(COMBINED_SYMBOLS) && defined(PACX_C_CONTAINS_FORTRAN_PACX_INIT) || \
     !defined(COMBINED_SYMBOLS) && defined(FORTRAN_SYMBOLS))
*/

#if defined(PACX_HAS_INIT_THREAD)
/******************************************************************************
 ***  PPACX_Init_thread_Wrapper
 ******************************************************************************/
void PPACX_Init_thread_Wrapper (PACX_Fint *required, PACX_Fint *provided, PACX_Fint *ierror)
/* Aquest codi nomes el volem per traceig sequencial i per pacx_init de fortran */
{
	unsigned int me, ret;
	int res;
	PACX_Fint comm;
	PACX_Fint tipus_enter;
	iotimer_t temps_inici_PACX_Init, temps_final_PACX_Init;
	char *config_file;

	mptrace_IsMPI = TRUE;

	hash_init (&requests);
	PR_queue_init (&PR_queue);

	if (*required == PACX_THREAD_MULTIPLE || *required == PACX_THREAD_SERIALIZED)
		fprintf (stderr, "mpitrace: WARNING! Instrumentation library does not support PACX_THREAD_MULTIPLE and PACX_THREAD_SERIALIZED modes\n");

	CtoF77 (ppacx_init_thread) (required, provided, ierror);

	/* OpenMPI does not allow us to do this before the PACX_Init! */
	comm = PACX_Comm_c2f (PACX_COMM_WORLD);
	tipus_enter = PACX_Type_c2f (PACX_INT);

	CtoF77 (ppacx_comm_rank) (&comm, &me, &ret);
	PACX_CHECK(ret, ppacx_comm_rank);

	CtoF77 (ppacx_comm_size) (&comm, &NumOfTasks, &ret);
	PACX_CHECK(ret, ppacx_comm_size);

	InitPACXCommunicators();

	/* We have to gather the task id */ 
	TaskID_Setup (me);

#if defined(SAMPLING_SUPPORT)
	/* If sampling is enabled, just stop all the processes at the same point
	   and continue */
	CtoF77 (ppacx_barrier) (&comm, &res);
#endif

	if (getenv ("MPTRACE_CONFIG_FILE") != NULL)
		/* Obtain a localized copy *except for the master process* */
		config_file = PACX_Distribute_XML_File (TASKID, NumOfTasks, getenv ("MPTRACE_CONFIG_FILE"));
	else
		config_file = NULL;

	/* Initialize the backend */
	res = Backend_preInitialize (TASKID, NumOfTasks, config_file);

	/* Remove the local copy only if we're not the master */
	if (me != 0)
		unlink (config_file);
	free (config_file);

	if (!res)
		return;

	Share_Nodes_Info ();
	Generate_Task_File_List (NumOfTasks, TasksNodes);

	/* Take the time now, we can't put MPIINIT_EV before APPL_EV */
	temps_inici_PACX_Init = TIME;
	
	/* Call a barrier in order to synchronize all tasks using MPIINIT_EV / END*/
	CtoF77 (ppacx_barrier) (&comm, &res);

	initTracingTime = temps_final_PACX_Init = TIME;

	/* End initialization of the backend  (put MPIINIT_EV { BEGIN/END } ) */
	if (!Backend_postInitialize (me, NumOfTasks, temps_inici_PACX_Init, temps_final_PACX_Init, TasksNodes))
		return;

	/* Annotate topologies (if available) */
	GetTopology();

	/* Annotate already built communicators */
	Trace_PACX_Communicator (PACX_COMM_CREATE_EV, PACX_COMM_WORLD);
	Trace_PACX_Communicator (PACX_COMM_CREATE_EV, PACX_COMM_SELF);
}
#endif /* PACX_HAS_INIT_THREAD */


/******************************************************************************
 ***  PPACX_Finalize_Wrapper
 ******************************************************************************/
void PPACX_Finalize_Wrapper (PACX_Fint *ierror)
{
	if (!mpitrace_on)
		return;

#if defined(IS_BGL_MACHINE)
	BGL_disable_barrier_inside = 1;
#endif

	TRACE_MPIEVENT (TIME, PACX_FINALIZE_EV, EVT_BEGIN, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY);

	TRACE_MYRINET_HWC();

	TRACE_MPIEVENT (TIME, PACX_FINALIZE_EV, EVT_END, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY);

#if defined(IS_BGL_MACHINE)
	BGL_disable_barrier_inside = 0;
#endif

#if HAVE_MRNET
	if (MRNet_isEnabled())
	{
	    Quit_MRNet(TASKID);
	}
#endif

	/* fprintf(stderr, "[T: %d] Invoking Thread_Finalization\n", TASKID); */

	/* Es tanca la llibreria de traceig */
	Thread_Finalization ();

	if (mpit_gathering_enabled)
		Gather_MPITS();

	CtoF77(ppacx_finalize) (ierror);
}


/******************************************************************************
 ***  get_rank_obj
 ******************************************************************************/

static int get_rank_obj (int *comm, int *dest, int *receiver)
{
	int ret, inter, one = 1;
	int group;
	PACX_Fint comm_world = PACX_Comm_c2f(PACX_COMM_WORLD);

	/*
	* Getting rank in PACX_COMM_WORLD from rank in comm 
	*/
	if (*comm == comm_world || *dest == MPI_PROC_NULL || *dest == MPI_ANY_SOURCE)
	{
		*receiver = *dest;
	}
	else
	{
		CtoF77 (ppacx_comm_test_inter) (comm, &inter, &ret);
		PACX_CHECK(ret, ppacx_comm_test_inter);

		if (inter)
		{
			CtoF77 (ppacx_comm_remote_group) (comm, &group, &ret);
			PACX_CHECK(ret, ppacx_comm_remote_group);
		}
		else
		{
			CtoF77 (ppacx_comm_group) (comm, &group, &ret);
			PACX_CHECK(ret, ppacx_comm_group);
		}

		/* Make translation */
		CtoF77 (ppacx_group_translate_ranks) (&group, &one, dest, &grup_global_F, receiver, &ret);
		PACX_CHECK(ret, ppacx_group_translate_ranks);

		CtoF77 (ppacx_group_free) (&group, &ret);
		PACX_CHECK(ret, ppacx_group_free);
	}
	return MPI_SUCCESS;
}


/******************************************************************************
 ***  PPACX_BSend_Wrapper
 ******************************************************************************/

void PPACX_BSend_Wrapper (void *buf, PACX_Fint *count, PACX_Fint *datatype,
	PACX_Fint *dest, PACX_Fint *tag, PACX_Fint *comm, PACX_Fint *ierror)
{
  int size, receiver, ret;
	PACX_Comm c = PACX_Comm_f2c (*comm);

  CtoF77 (ppacx_type_size) (datatype, &size, &ret);
	PACX_CHECK(ret, ppacx_type_size);

  if ((ret = get_rank_obj (comm, dest, &receiver)) != MPI_SUCCESS)
  {
    *ierror = ret;
    return;
  }
  size *= (*count);

  /* PACX_ Stats */
  P2P_Communications ++;
  P2P_Bytes_Sent += size;

  /*
   *   event : BSEND_EV                      value : EVT_BEGIN
   *   target : receiver                     size  : send message size
   *   tag : message tag
   */
  TRACE_MPIEVENT (TIME, PACX_BSEND_EV, EVT_BEGIN, receiver, size, *tag, c, EMPTY);

  CtoF77 (ppacx_bsend) (buf, count, datatype, dest, tag, comm, ierror);

  /*
   *   event : BSEND_EV                      value : EVT_END
   *   target : receiver                     size  : send message size
   *   tag : message tag
   */
  TRACE_MPIEVENT (TIME, PACX_BSEND_EV, EVT_END, receiver, size, *tag, c, EMPTY);
}

/******************************************************************************
 ***  PPACX_SSend_Wrapper
 ******************************************************************************/

void PPACX_SSend_Wrapper (void *buf, PACX_Fint *count, PACX_Fint *datatype,
	PACX_Fint *dest, PACX_Fint *tag, PACX_Fint *comm, PACX_Fint *ierror)
{
  int size, receiver, ret;
	PACX_Comm c = PACX_Comm_f2c (*comm);

  CtoF77 (ppacx_type_size) (datatype, &size, &ret);
	PACX_CHECK(ret, ppacx_type_size);

  if ((ret = get_rank_obj (comm, dest, &receiver)) != MPI_SUCCESS)
  {
    *ierror = ret;
    return;
  }
  size *= (*count);

  /* PACX_ Stats */
  P2P_Communications ++;
  P2P_Bytes_Sent += size;

  /*
   *   event : SSEND_EV                      value : EVT_BEGIN
   *   target : receiver                     size  : send message size
   *   tag : message tag
   */
  TRACE_MPIEVENT (TIME, PACX_SSEND_EV, EVT_BEGIN, receiver, size, *tag, c, EMPTY);

  CtoF77 (ppacx_ssend) (buf, count, datatype, dest, tag, comm, ierror);

  /*
   *   event : SSEND_EV                      value : EVT_END
   *   target : receiver                     size  : send message size
   *   tag : message tag
   */
  TRACE_MPIEVENT (TIME, PACX_SSEND_EV, EVT_END, receiver, size, *tag, c, EMPTY);
}

/******************************************************************************
 ***  PPACX_RSend_Wrapper
 ******************************************************************************/

void PPACX_RSend_Wrapper (void *buf, PACX_Fint *count, PACX_Fint *datatype,
	PACX_Fint *dest, PACX_Fint *tag, PACX_Fint *comm, PACX_Fint *ierror)
{
  int size, receiver, ret;
	PACX_Comm c = PACX_Comm_f2c (*comm);

  CtoF77 (ppacx_type_size) (datatype, &size, &ret);
	PACX_CHECK(ret, ppacx_type_size);

  if ((ret = get_rank_obj (comm, dest, &receiver)) != MPI_SUCCESS)
  {
    *ierror = ret;
    return;
  }
  size *= (*count);

  /* PACX_ Stats */
  P2P_Communications ++;
  P2P_Bytes_Sent += size;

  /*
   *   event : RSEND_EV                      value : EVT_BEGIN
   *   target : receiver                     size  : send message size
   *   tag : message tag
   */
  TRACE_MPIEVENT (TIME, PACX_RSEND_EV, EVT_BEGIN, receiver, size, *tag, c, EMPTY);

  CtoF77 (ppacx_rsend) (buf, count, datatype, dest, tag, comm, ierror);

  /*
   *   event : RSEND_EV                      value : EVT_END
   *   target : receiver                     size  : send message size
   *   tag : message tag
   */
  TRACE_MPIEVENT (TIME, PACX_RSEND_EV, EVT_END, receiver, size, *tag, c, EMPTY);
}

/******************************************************************************
 ***  PPACX_Send_Wrapper
 ******************************************************************************/

void PPACX_Send_Wrapper (void *buf, PACX_Fint *count, PACX_Fint *datatype,
	PACX_Fint *dest, PACX_Fint *tag, PACX_Fint *comm, PACX_Fint *ierror)
{
  int size, receiver, ret;
	PACX_Comm c = PACX_Comm_f2c (*comm);

  CtoF77 (ppacx_type_size) (datatype, &size, &ret);
	PACX_CHECK(ret, ppacx_type_size);

  if ((ret = get_rank_obj (comm, dest, &receiver)) != MPI_SUCCESS)
  {
    *ierror = ret;
    return;
  }
  size *= (*count);

  /* PACX_ Stats */
  P2P_Communications ++;
  P2P_Bytes_Sent += size;

  /*
   *   event : SEND_EV                       value : EVT_BEGIN
   *   target : receiver                     size  : send message size
   *   tag : message tag
   */
  TRACE_MPIEVENT (TIME, PACX_SEND_EV, EVT_BEGIN, receiver, size, *tag, c, EMPTY);

  CtoF77 (ppacx_send) (buf, count, datatype, dest, tag, comm, ierror);

  /*
   *   event : SEND_EV                       value : EVT_END
   *   target : receiver                     size  : send message size
   *   tag : message tag
   */
  TRACE_MPIEVENT (TIME, PACX_SEND_EV, EVT_END, receiver, size, *tag, c, EMPTY);
}


/******************************************************************************
 ***  PPACX_IBSend_Wrapper
 ******************************************************************************/

void PPACX_IBSend_Wrapper (void *buf, PACX_Fint *count, PACX_Fint *datatype,
	PACX_Fint *dest, PACX_Fint *tag, PACX_Fint *comm, PACX_Fint *request,
	PACX_Fint *ierror)
{
  int size, receiver, ret;
	PACX_Comm c = PACX_Comm_f2c (*comm);

  CtoF77 (ppacx_type_size) (datatype, &size, &ret);
	PACX_CHECK(ret, ppacx_type_size);

  if ((ret = get_rank_obj (comm, dest, &receiver)) != MPI_SUCCESS)
  {
    *ierror = ret;
    return;
  }
  size *= (*count);

  /* PACX_ Stats */
  P2P_Communications ++;
  P2P_Bytes_Sent += size;

  /*
   *   event : IBSEND_EV                     value : EVT_BEGIN
   *   target : ---                          size  : ---
   *   tag : ---                             commid: ---
   *   aux : ---
   */
  TRACE_MPIEVENT (TIME, PACX_IBSEND_EV, EVT_BEGIN, receiver, size, *tag, c, EMPTY);

  CtoF77 (ppacx_ibsend) (buf, count, datatype, dest, tag, comm, request,
                        ierror);

  /*
   *   event : IBSEND_EV                     value : EVT_END
   *   target : ---                          size  : ---
   *   tag : ---                             commid: ---
   *   aux : ---
   */
  TRACE_MPIEVENT (TIME, PACX_IBSEND_EV, EVT_END, receiver, size, *tag, c, EMPTY);
}

/******************************************************************************
 ***  PPACX_ISend_Wrapper
 ******************************************************************************/

void PPACX_ISend_Wrapper (void *buf, PACX_Fint *count, PACX_Fint *datatype,
	PACX_Fint *dest, PACX_Fint *tag, PACX_Fint *comm, PACX_Fint *request,
	PACX_Fint *ierror)
{
  int size, receiver, ret;
	PACX_Comm c = PACX_Comm_f2c (*comm);

  CtoF77 (ppacx_type_size) (datatype, &size, &ret);
	PACX_CHECK(ret, ppacx_type_size);

  if ((ret = get_rank_obj (comm, dest, &receiver)) != MPI_SUCCESS)
  {
    *ierror = ret;
    return;
  } 
  size *= (*count);

  /* PACX_ Stats */
  P2P_Communications ++;
  P2P_Bytes_Sent += size;

  /*
   *   event : ISEND_EV                      value : EVT_BEGIN
   *   target : ---                          size  : ---
   *   tag : ---                             commid: ---
   *   aux : ---
   */
  TRACE_MPIEVENT (TIME, PACX_ISEND_EV, EVT_BEGIN, receiver, size, *tag, c, EMPTY);

  CtoF77 (ppacx_isend) (buf, count, datatype, dest, tag, comm, request,
                       ierror);
  /*
   *   event : ISEND_EV                      value : EVT_END
   *   target : ---                          size  : ---
   *   tag : ---                             commid: ---
   *   aux : ---
   */
  TRACE_MPIEVENT (TIME, PACX_ISEND_EV, EVT_END, receiver, size, *tag, c, EMPTY);
}

/******************************************************************************
 ***  PPACX_ISSend_Wrapper
 ******************************************************************************/

void PPACX_ISSend_Wrapper (void *buf, PACX_Fint *count, PACX_Fint *datatype,
	PACX_Fint *dest, PACX_Fint *tag, PACX_Fint *comm, PACX_Fint *request,
	PACX_Fint *ierror)
{
  int size, receiver, ret;
	PACX_Comm c = PACX_Comm_f2c (*comm);

  CtoF77 (ppacx_type_size) (datatype, &size, &ret);
	PACX_CHECK(ret, ppacx_type_size);

  if ((ret = get_rank_obj (comm, dest, &receiver)) != MPI_SUCCESS)
  {
    *ierror = ret;
    return;
  }
  size *= (*count);

  /* PACX_ Stats */
  P2P_Communications ++;
  P2P_Bytes_Sent += size;

  /*
   *   event : ISSEND_EV                     value : EVT_BEGIN
   *   target : ---                          size  : ---
   *   tag : ---                             commid: ---
   *   aux : ---
   */
  TRACE_MPIEVENT (TIME, PACX_ISSEND_EV, EVT_BEGIN, receiver, size, *tag, c, EMPTY);

  CtoF77 (ppacx_issend) (buf, count, datatype, dest, tag, comm, request,
                        ierror);

  /*
   *   event : ISSEND_EV                     value : EVT_END
   *   target : ---                          size  : ---
   *   tag : ---                             commid: ---
   *   aux : ---
   */
  TRACE_MPIEVENT (TIME, PACX_ISSEND_EV, EVT_END, receiver, size, *tag, c, EMPTY);
}

/******************************************************************************
 ***  PPACX_IRSend_Wrapper
 ******************************************************************************/

void PPACX_IRSend_Wrapper (void *buf, PACX_Fint *count, PACX_Fint *datatype,
	PACX_Fint *dest, PACX_Fint *tag, PACX_Fint *comm, PACX_Fint *request,
	PACX_Fint *ierror)
{
  int size, receiver, ret;
	PACX_Comm c = PACX_Comm_f2c (*comm);

  CtoF77 (ppacx_type_size) (datatype, &size, &ret);
	PACX_CHECK(ret, ppacx_type_size);

  if ((ret = get_rank_obj (comm, dest, &receiver)) != MPI_SUCCESS)
  {
    *ierror = ret;
    return;
  }
  size *= (*count);

  /* PACX_ Stats */
  P2P_Communications ++;
  P2P_Bytes_Sent += size;

  /*
   *   event : IRSEND_EV                     value : EVT_BEGIN
   *   target : ---                          size  : ---
   *   tag : ---                             commid: ---
   *   aux : ---
   */
  TRACE_MPIEVENT (TIME, PACX_IRSEND_EV, EVT_BEGIN, receiver, size, *tag, c, EMPTY);

  CtoF77 (ppacx_irsend) (buf, count, datatype, dest, tag, comm, request,
                        ierror);

  /*
   *   event : IRSEND_EV                     value : EVT_END
   *   target : ---                          size  : ---
   *   tag : ---                             commid: ---
   *   aux : ---
   */
  TRACE_MPIEVENT (TIME, PACX_IRSEND_EV, EVT_END, receiver, size, *tag, c, EMPTY);
}

/******************************************************************************
 ***  PPACX_Recv_Wrapper
 ******************************************************************************/

void PPACX_Recv_Wrapper (void *buf, PACX_Fint *count, PACX_Fint *datatype,
	PACX_Fint *source, PACX_Fint *tag, PACX_Fint *comm, PACX_Fint *status, 
	PACX_Fint *ierror)
{
	PACX_Fint my_status[SIZEOF_PACX_STATUS], *ptr_status;
	PACX_Comm c = PACX_Comm_f2c(*comm);
  int size, src_world, sender_src, ret, recved_count, sended_tag;

  CtoF77 (ppacx_type_size) (datatype, &size, &ret);
	PACX_CHECK(ret, ppacx_type_size);

	if ((ret = get_rank_obj (comm, source, &src_world)) != MPI_SUCCESS)
	{
		*ierror = ret;
		return;
	}

  /*
   *   event : RECV_EV                      value : EVT_BEGIN    
   *   target : MPI_ANY_SOURCE or sender    size  : receive buffer size    
   *   tag : message tag or PACX_ANY_TAG     commid: Communicator identifier
   *   aux: ---
   */
  TRACE_MPIEVENT (TIME, PACX_RECV_EV, EVT_BEGIN, src_world, (*count) * size, *tag, c, EMPTY);

	ptr_status = (status == MPI_F_STATUS_IGNORE)?my_status:status;

  CtoF77 (ppacx_recv) (buf, count, datatype, source, tag, comm, ptr_status,
                      ierror);

	CtoF77 (ppacx_get_count) (ptr_status, datatype, &recved_count, &ret);
	PACX_CHECK(ret, ppacx_get_count);

	if (recved_count != MPI_UNDEFINED)
		size *= recved_count;
	else
		size = 0;

	sender_src = ptr_status[PACX_SOURCE_OFFSET];
	sended_tag = ptr_status[PACX_TAG_OFFSET];

	/* PACX_ Stats */
	P2P_Communications ++;
	P2P_Bytes_Recv += size;

  if ((ret = get_rank_obj (comm, &sender_src, &src_world)) != MPI_SUCCESS)
  {
    *ierror = ret;
    return;
  }

  /*
   *   event : RECV_EV                      value : EVT_END
   *   target : sender                      size  : received message size    
   *   tag : message tag
   */
  TRACE_MPIEVENT (TIME, PACX_RECV_EV, EVT_END, src_world, size, sended_tag, c, EMPTY);
}

/******************************************************************************
 ***  PPACX_IRecv_Wrapper
 ******************************************************************************/

#if defined(MPICH)
	/* HSG this function has no prototype in the MPICH header! but it's needed to
	   convert requests from Fortran to C!
	*/
# warning "MPIR_ToPointer has no prototype"
	extern PACX_Request MPIR_ToPointer(PACX_Fint);
#endif

void PPACX_IRecv_Wrapper (void *buf, PACX_Fint *count, PACX_Fint *datatype,
	PACX_Fint *source, PACX_Fint *tag, PACX_Fint *comm, PACX_Fint *request,
	PACX_Fint *ierror)
{
  hash_data_t hash_req;
  unsigned int inter, ret, size, src_world;
	PACX_Comm c = PACX_Comm_f2c(*comm);

  CtoF77 (ppacx_type_size) (datatype, &size, &ret);
	PACX_CHECK(ret, ppacx_type_size);

	if ((ret = get_rank_obj (comm, source, &src_world)) != MPI_SUCCESS)
	{
		*ierror = ret;
		return;
	}

  /*
   *   event : IRECV_EV                     value : EVT_BEGIN
   *   target : ---                         size  : ---
   *   tag : ---
   */
  TRACE_MPIEVENT (TIME, PACX_IRECV_EV, EVT_BEGIN, src_world, (*count) * size, *tag, c, EMPTY);

  CtoF77 (ppacx_irecv) (buf, count, datatype, source, tag, comm, request,
                       ierror);

  hash_req.key = PACX_Request_f2c(*request);
  hash_req.commid = c;
	hash_req.partner = *source;
	hash_req.tag = *tag;
	hash_req.size = *count * size;
	
  if (c == PACX_COMM_WORLD)
	{
    hash_req.group = PACX_GROUP_NULL;
	}
  else
  {
		PACX_Fint group;
    CtoF77 (ppacx_comm_test_inter) (comm, &inter, &ret);
		PACX_CHECK(ret, ppacx_comm_test_inter);

    if (inter)
		{
      CtoF77 (ppacx_comm_remote_group) (comm, &group, &ret);
			PACX_CHECK(ret, ppacx_comm_remote_group);
		}
    else
		{
      CtoF77 (ppacx_comm_group) (comm, &group, &ret);
			PACX_CHECK(ret, ppacx_comm_group);
		}
		hash_req.group = PACX_Group_f2c(group);
  }

  hash_add (&requests, &hash_req);

  /*
   *   event : IRECV_EV                     value : EVT_END
   *   target : request                     size  : ---
   *   tag : ---
   */
  TRACE_MPIEVENT (TIME, PACX_IRECV_EV, EVT_END, src_world, (*count) * size, *tag, c, hash_req.key);
}


/******************************************************************************
 ***  PPACX_Reduce_Wrapper
 ******************************************************************************/

void PPACX_Reduce_Wrapper (void *sendbuf, void *recvbuf, PACX_Fint *count,
	PACX_Fint *datatype, PACX_Fint *op, PACX_Fint *root, PACX_Fint *comm,
	PACX_Fint *ierror)
{
  int me, ret, size;
	PACX_Comm c = PACX_Comm_f2c(*comm);

  CtoF77 (ppacx_comm_rank) (comm, &me, &ret);
	PACX_CHECK(ret, ppacx_comm_rank);

  CtoF77 (ppacx_type_size) (datatype, &size, &ret);
	PACX_CHECK(ret, ppacx_type_size);

  size *= *count;

  /* PACX_ Stats */
  GLOBAL_Communications ++;
  if (me == *root)
  {
     GLOBAL_Bytes_Recv += size;
  }
  else
  {
     GLOBAL_Bytes_Sent += size;
  }

  /*
   *   event : REDUCE_EV                    value : EVT_BEGIN
   *   target: reduce operation ident.      size : bytes send (non root) /received (root)
   *   tag : rank                           commid: communicator Id
   *   aux : root rank
   */
  TRACE_MPIEVENT (TIME, PACX_REDUCE_EV, EVT_BEGIN, *op, size, me, c, *root);

  CtoF77 (ppacx_reduce) (sendbuf, recvbuf, count, datatype, op, root, comm,
                        ierror);

  /*
   *   event : REDUCE_EV                    value : EVT_END
   *   target : ---                         size  : ---
   *   tag : ---
   */
  TRACE_MPIEVENT (TIME, PACX_REDUCE_EV, EVT_END, EMPTY, EMPTY, EMPTY, c, EMPTY);
}


/******************************************************************************
 ***  PPACX_AllReduce_Wrapper
 ******************************************************************************/

void PPACX_AllReduce_Wrapper (void *sendbuf, void *recvbuf, PACX_Fint *count,
	PACX_Fint *datatype, PACX_Fint *op, PACX_Fint *comm, PACX_Fint *ierror)
{
  int me, ret, size;
	PACX_Comm c = PACX_Comm_f2c(*comm);

  CtoF77 (ppacx_comm_rank) (comm, &me, &ret);
	PACX_CHECK(ret, ppacx_comm_rank);

  CtoF77 (ppacx_type_size) (datatype, &size, &ret);
	PACX_CHECK(ret, ppacx_type_size);

  size *= *count;

  /* PACX_ Stats */
  GLOBAL_Communications ++;
  GLOBAL_Bytes_Sent += size;
  GLOBAL_Bytes_Recv += size;

  /*
   *   event : ALLREDUCE_EV                 value : EVT_BEGIN
   *   target: reduce operation ident.      size : bytes send and received
   *   tag : rank                           commid: communicator Id
   *   aux : ---
   */
  TRACE_MPIEVENT (TIME, PACX_ALLREDUCE_EV, EVT_BEGIN, *op, size, me, c, PACX_CurrentOpGlobal);

  CtoF77 (ppacx_allreduce) (sendbuf, recvbuf, count, datatype, op, comm,
                           ierror);

  /*
   *   event : ALLREDUCE_EV                 value : EVT_END
   *   target : ---                         size  : ---
   *   tag : ---
   */
  TRACE_MPIEVENT (TIME, PACX_ALLREDUCE_EV, EVT_END, EMPTY, EMPTY, EMPTY, c,
		PACX_CurrentOpGlobal);
}



/******************************************************************************
 ***  PPACX_Probe_Wrapper
 ******************************************************************************/

void PPACX_Probe_Wrapper (PACX_Fint *source, PACX_Fint *tag, PACX_Fint *comm,
	PACX_Fint *status, PACX_Fint *ierror)
{
	PACX_Comm c = PACX_Comm_f2c(*comm);

  /*
   *   event : PROBE_EV                     value : EVT_BEGIN
   *   target : ---                         size  : ---
   *   tag : ---
   */
  TRACE_MPIEVENT (TIME, PACX_PROBE_EV, EVT_BEGIN, EMPTY, EMPTY, EMPTY, c, EMPTY);

  CtoF77 (ppacx_probe) (source, tag, comm, status, ierror);

  /*
   *   event : PROBE_EV                     value : EVT_END
   *   target : ---                         size  : ---
   *   tag : ---
   */
  TRACE_MPIEVENT (TIME, PACX_PROBE_EV, EVT_END, EMPTY, EMPTY, EMPTY, c, EMPTY);
}

/******************************************************************************
 ***  PPACX_IProbe_Wrapper
 ******************************************************************************/

void Bursts_PPACX_IProbe_Wrapper (PACX_Fint *source, PACX_Fint *tag, PACX_Fint *comm,
	PACX_Fint *flag, PACX_Fint *status, PACX_Fint *ierror)
{
	PACX_Comm c = PACX_Comm_f2c(*comm);

     /*
      *   event : IPROBE_EV                     value : EVT_BEGIN
      *   target : ---                          size  : ---
      *   tag : ---
      */
	TRACE_MPIEVENT (TIME, PACX_IPROBE_EV, EVT_BEGIN, EMPTY, EMPTY, EMPTY, c, EMPTY);

	CtoF77 (ppacx_iprobe) (source, tag, comm, flag, status, ierror);

     /*
      *   event : IPROBE_EV                    value : EVT_END
      *   target : ---                         size  : ---
      *   tag : ---
      */
	TRACE_MPIEVENT (TIME, PACX_IPROBE_EV, EVT_END, EMPTY, EMPTY, EMPTY, c, EMPTY);
}

void Normal_PPACX_IProbe_Wrapper (PACX_Fint *source, PACX_Fint *tag, PACX_Fint *comm,
	PACX_Fint *flag, PACX_Fint *status, PACX_Fint *ierror)
{
  static int IProbe_Software_Counter = 0;
  iotimer_t begin_time, end_time;
  static iotimer_t elapsed_time_outside_iprobes = 0, last_iprobe_exit_time = 0;
	PACX_Comm c = PACX_Comm_f2c(*comm);

  begin_time = TIME;

  if (IProbe_Software_Counter == 0) {
    /* Primer Iprobe */
    elapsed_time_outside_iprobes = 0;
  }
  else {
    elapsed_time_outside_iprobes += (begin_time - last_iprobe_exit_time);
  }

  CtoF77 (ppacx_iprobe) (source, tag, comm, flag, status, ierror);

  end_time = TIME; 
  last_iprobe_exit_time = end_time;

	if (tracejant_mpi)
  {
    if (*flag)
    {
      /*
       *   event : IPROBE_EV                     value : EVT_BEGIN
       *   target : ---                          size  : ---
       *   tag : ---
       */
      if (IProbe_Software_Counter != 0) {
        TRACE_EVENT (begin_time, PACX_TIME_OUTSIDE_IPROBES_EV, elapsed_time_outside_iprobes);
        TRACE_EVENT (begin_time, PACX_IPROBE_COUNTER_EV, IProbe_Software_Counter);
      }
      TRACE_MPIEVENT (begin_time, PACX_IPROBE_EV, EVT_BEGIN, EMPTY, EMPTY, EMPTY, c, EMPTY);

     /*
      *   event : IPROBE_EV                    value : EVT_END
      *   target : ---                         size  : ---
      *   tag : ---
      */
      TRACE_MPIEVENT (end_time, PACX_IPROBE_EV, EVT_END, EMPTY, EMPTY, EMPTY, c, EMPTY);
      IProbe_Software_Counter = 0;
    }
    else
    {
      if (IProbe_Software_Counter == 0)
      {
        /* El primer iprobe que falla */
        TRACE_EVENTANDCOUNTERS (begin_time, PACX_IPROBE_COUNTER_EV, 0, TRUE);
      }
      IProbe_Software_Counter ++;
    }
  }
}

void PPACX_IProbe_Wrapper (PACX_Fint *source, PACX_Fint *tag, PACX_Fint *comm,
    PACX_Fint *flag, PACX_Fint *status, PACX_Fint *ierror)
{
   if (CURRENT_TRACE_MODE(THREADID) == TRACE_MODE_BURSTS)
   {
      Bursts_PPACX_IProbe_Wrapper (source, tag, comm, flag, status, ierror);
   }
   else
   {
      Normal_PPACX_IProbe_Wrapper (source, tag, comm, flag, status, ierror);
   }
}

/******************************************************************************
 ***  PPACX_Barrier_Wrapper
 ******************************************************************************/

void PPACX_Barrier_Wrapper (PACX_Fint *comm, PACX_Fint *ierror)
{
  PACX_Comm c = PACX_Comm_f2c (*comm);
  int me, ret;

  CtoF77 (ppacx_comm_rank) (comm, &me, &ret);
	PACX_CHECK(ret, ppacx_comm_rank);

  /* PACX_ Stats */
  GLOBAL_Communications ++;

  /*
   *   event : BARRIER_EV                    value : EVT_BEGIN
   *   target : ---                          size  : ---
   *   tag : rank                            commid: comunicator id
   *   aux : ---
   */

#if defined(IS_BGL_MACHINE)
  if (!BGL_disable_barrier_inside)
  {
    TRACE_MPIEVENT (TIME, PACX_BARRIER_EV, EVT_BEGIN, EMPTY, EMPTY, me, c,
                    PACX_CurrentOpGlobal);
  }
#else
  TRACE_MPIEVENT (TIME, PACX_BARRIER_EV, EVT_BEGIN, EMPTY, EMPTY, me, c,
                  PACX_CurrentOpGlobal);
#endif

  CtoF77 (ppacx_barrier) (comm, ierror);

  /*
   *   event : BARRIER_EV                   value : EVT_END
   *   target : ---                         size  : ---
   *   tag : ---
   */

#if defined(IS_BGL_MACHINE)
  if (!BGL_disable_barrier_inside)
  {
    TRACE_MPIEVENT (TIME, PACX_BARRIER_EV, EVT_END, EMPTY, EMPTY, EMPTY, c,
                    PACX_CurrentOpGlobal);
  }
#else
  TRACE_MPIEVENT (TIME, PACX_BARRIER_EV, EVT_END, EMPTY, EMPTY, EMPTY, c,
                  PACX_CurrentOpGlobal);
#endif

}

/******************************************************************************
 ***  PPACX_Cancel_Wrapper
 ******************************************************************************/

void PPACX_Cancel_Wrapper (PACX_Fint *request, PACX_Fint *ierror)
{
	PACX_Request req = PACX_Request_f2c(*request);

  /*
   *   event : CANCEL_EV                    value : EVT_BEGIN
   *   target : request to cancel           size  : ---
   *   tag : ---
   */
  TRACE_MPIEVENT (TIME, PACX_CANCEL_EV, EVT_BEGIN, req, EMPTY, EMPTY, EMPTY,
                  EMPTY);

	if (hash_search (&requests, req) != NULL)
		hash_remove (&requests, req);

  CtoF77 (ppacx_cancel) (request, ierror);

  /*
   *   event : CANCEL_EV                    value : EVT_END
   *   target : request to cancel           size  : ---
   *   tag : ---
   */
  TRACE_MPIEVENT (TIME, PACX_CANCEL_EV, EVT_END, req, EMPTY, EMPTY, EMPTY, EMPTY);
}

/******************************************************************************
 ***  get_Irank_obj
 ******************************************************************************/

static int get_Irank_obj (hash_data_t * hash_req, int *src_world, int *size,
	int *tag, int *status)
{
  int ret, one = 1;
  PACX_Fint tbyte = PACX_Type_c2f(PACX_BYTE);
	int recved_count, dest;

#if defined(DEAD_CODE)
	if (MPI_F_STATUS_IGNORE != status)
	{
		CtoF77 (ppacx_get_count) (status, &tbyte, &recved_count, &ret);
		PACX_CHECK(ret, ppacx_get_count);

		if (recved_count != MPI_UNDEFINED)
			*size = recved_count;
		else
			*size = 0;

		*tag = status[PACX_TAG_OFFSET];
		dest = status[PACX_SOURCE_OFFSET];
  }
	else
	{
		*tag = hash_req->tag;
		*size = hash_req->size;
		dest = hash_req->partner;
	}
#endif

	CtoF77 (ppacx_get_count) (status, &tbyte, &recved_count, &ret);
	PACX_CHECK(ret, ppacx_get_count);

	if (recved_count != MPI_UNDEFINED)
		*size = recved_count;
	else
		*size = 0;

	*tag = status[PACX_TAG_OFFSET];
	dest = status[PACX_SOURCE_OFFSET];

	if (PACX_GROUP_NULL != hash_req->group)
	{
		PACX_Fint group = PACX_Group_c2f(hash_req->group);
		CtoF77 (ppacx_group_translate_ranks) (&group, &one, &dest, &grup_global_F, src_world, &ret);
		PACX_CHECK(ret, ppacx_group_translate_ranks);
	}
	else
		*src_world = dest;

  return MPI_SUCCESS;
}


/******************************************************************************
 ***  PPACX_Test_Wrapper
 ******************************************************************************/

void Bursts_PPACX_Test_Wrapper (PACX_Fint *request, PACX_Fint *flag, PACX_Fint *status,
	PACX_Fint *ierror)
{
	PACX_Request req;
  hash_data_t *hash_req;
  int src_world, size, tag, ret;
  iotimer_t temps_final;

  /*
   *   event : TEST_EV                      value : EVT_BEGIN
   *   target : request to test             size  : ---
   *   tag : ---
   */
  TRACE_MPIEVENT (TIME, PACX_TEST_EV, EVT_BEGIN, *request, EMPTY, EMPTY, EMPTY,
                  EMPTY);

  req = PACX_Request_f2c (*request);

  CtoF77 (ppacx_test) (request, flag, status, ierror);

  temps_final = TIME;

  if (*flag && ((hash_req = hash_search (&requests, req)) != NULL))
  {
		if ((ret = get_Irank_obj (hash_req, &src_world, &size, &tag, status)) != MPI_SUCCESS)
		{
			*ierror = ret;
			return;
		}
    if (hash_req->group != PACX_GROUP_NULL)
    {
			PACX_Fint group = PACX_Group_c2f(hash_req->group);
      CtoF77 (ppacx_group_free) (&group, &ret);
			PACX_CHECK (ret, ppacx_group_free);
    }

    P2P_Communications ++;
    P2P_Bytes_Recv += size; /* get_Irank_obj above return size (number of bytes received) */

    /*
     *   event : IRECVED_EV                 value : ---
     *   target : sender                    size  : received message size
     *   tag : message tag                  commid: communicator identifier
     *   aux : request
     */
    TRACE_MPIEVENT_NOHWC (temps_final, PACX_IRECVED_EV, EMPTY, src_world, size, tag, hash_req->commid, req);
    hash_remove (&requests, req);
  }
  /*
   *   event : TEST_EV                    value : EVT_END
   *   target : ---                       size  : ---
   *   tag : ---
   */
  TRACE_MPIEVENT (temps_final, PACX_TEST_EV, EVT_END, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY);
}

void Normal_PPACX_Test_Wrapper (PACX_Fint *request, PACX_Fint *flag, PACX_Fint *status,
	PACX_Fint *ierror)
{
	PACX_Request req;
  hash_data_t *hash_req;
  int src_world, size, tag, ret;
  iotimer_t temps_inicial, temps_final;
  static int Test_Software_Counter = 0;

  temps_inicial = TIME;

  req = PACX_Request_f2c(*request);

  CtoF77 (ppacx_test) (request, flag, status, ierror);

  temps_final = TIME;

  if (*flag && ((hash_req = hash_search (&requests, req)) != NULL))
  {
    /*
     *   event : TEST_EV                      value : EVT_BEGIN
     *   target : request to test             size  : ---
     *   tag : ---
     */
    if (Test_Software_Counter != 0) {
			TRACE_EVENT (temps_inicial, PACX_TEST_COUNTER_EV, Test_Software_Counter);
    }
    TRACE_MPIEVENT (temps_inicial, PACX_TEST_EV, EVT_BEGIN, *request, EMPTY, EMPTY, EMPTY, EMPTY);
    Test_Software_Counter = 0;

		if ((ret = get_Irank_obj (hash_req, &src_world, &size, &tag, status)) != MPI_SUCCESS)
		{
			*ierror = ret;
			return;
		}
    if (hash_req->group != PACX_GROUP_NULL)
    {
			PACX_Fint group = PACX_Group_c2f (hash_req->group);
      CtoF77 (ppacx_group_free) (&group, &ret);
			PACX_CHECK (ret, ppacx_group_free);
    }

    /*
     *   event : IRECVED_EV                 value : ---
     *   target : sender                    size  : received message size
     *   tag : message tag                  commid: communicator identifier
     *   aux : request
     */
    TRACE_MPIEVENT_NOHWC (temps_final, PACX_IRECVED_EV, EMPTY, src_world, size, tag, hash_req->commid, req);
    hash_remove (&requests, req);

    /*
     *   event : TEST_EV                    value : EVT_END
     *   target : ---                       size  : ---
     *   tag : ---
     */
    TRACE_MPIEVENT (temps_final, PACX_TEST_EV, EVT_END, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY);
  }
  else {
    if (Test_Software_Counter == 0) {
      /* El primer test que falla */
      TRACE_EVENTANDCOUNTERS    (temps_inicial, PACX_TEST_COUNTER_EV, 0, TRUE);
    }
    Test_Software_Counter ++;
  }
}

void PPACX_Test_Wrapper (PACX_Fint *request, PACX_Fint *flag, PACX_Fint *status,
    PACX_Fint *ierror)
{
	PACX_Fint my_status[SIZEOF_PACX_STATUS], *ptr_status;

	ptr_status = (MPI_F_STATUS_IGNORE == status)?my_status:status;

   if (CURRENT_TRACE_MODE(THREADID) == TRACE_MODE_BURSTS)
   {
      Bursts_PPACX_Test_Wrapper(request, flag, ptr_status, ierror);
   }
   else
   {
      Normal_PPACX_Test_Wrapper(request, flag, ptr_status, ierror);
   }
}

/******************************************************************************
 ***  PPACX_Wait_Wrapper
 ******************************************************************************/

void PPACX_Wait_Wrapper (PACX_Fint *request, PACX_Fint *status, PACX_Fint *ierror)
{
	PACX_Fint my_status[SIZEOF_PACX_STATUS], *ptr_status;
  hash_data_t *hash_req;
  iotimer_t temps_final;
  int src_world, size, tag, ret;
	PACX_Request req = PACX_Request_f2c(*request);

  /*
   *   event : WAIT_EV                      value : EVT_BEGIN
   *   target : request to test             size  : ---
   *   tag : ---
   */
  TRACE_MPIEVENT (TIME, PACX_WAIT_EV, EVT_BEGIN, req, EMPTY, EMPTY, EMPTY, EMPTY);

	ptr_status = (MPI_F_STATUS_IGNORE == status)?my_status:status;

  CtoF77 (ppacx_wait) (request, ptr_status, ierror);

  temps_final = TIME;

  if (*ierror == MPI_SUCCESS && ((hash_req = hash_search (&requests, req)) != NULL))
  {
		if ((ret = get_Irank_obj (hash_req, &src_world, &size, &tag, ptr_status)) != MPI_SUCCESS)
		{
			*ierror = ret;
			return;
		}
    if (hash_req->group != PACX_GROUP_NULL)
    {
			PACX_Fint group = PACX_Group_c2f (hash_req->group);
      CtoF77 (ppacx_group_free) (&group, &ret);
			PACX_CHECK (ret, ppacx_group_free);
    }

    /* PACX_ Stats */
    P2P_Communications ++; 
    P2P_Bytes_Recv += size; /* get_Irank_obj above returns size (the number of bytes received) */

    /*
     *   event : IRECVED_EV                 value : ---
     *   target : sender                    size  : received message size
     *   tag : message tag                  commid: communicator identifier
     *   aux : request
     */
    TRACE_MPIEVENT_NOHWC (temps_final, PACX_IRECVED_EV, EMPTY, src_world, size, tag, hash_req->commid, req); /* NOHWC */
    hash_remove (&requests, req);
  }
  /*
   *   event : WAIT_EV                    value : EVT_END
   *   target : ---                       size  : ---
   *   tag : ---
   */
  TRACE_MPIEVENT (temps_final, PACX_WAIT_EV, EVT_END, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY);
}

/******************************************************************************
 ***  PPACX_WaitAll_Wrapper
 ******************************************************************************/

void PPACX_WaitAll_Wrapper (PACX_Fint * count, PACX_Fint array_of_requests[],
	PACX_Fint array_of_statuses[][SIZEOF_PACX_STATUS], PACX_Fint * ierror)
{
	PACX_Fint my_statuses[MAX_WAIT_REQUESTS][SIZEOF_PACX_STATUS], *ptr_statuses;
	PACX_Request save_reqs[MAX_WAIT_REQUESTS];
  hash_data_t *hash_req;
  int src_world, size, tag, ret, ireq;
  iotimer_t temps_final;
	int i;

  /*
   *   event : WAITALL_EV                      value : EVT_BEGIN
   *   target : ---                            size  : ---
   *   tag : ---
   */
  TRACE_MPIEVENT (TIME, PACX_WAITALL_EV, EVT_BEGIN, EMPTY, EMPTY, EMPTY, EMPTY,
                  EMPTY);

  /*
   * Arreglar-ho millor de cara a OMP
   */
  if (*count > MAX_WAIT_REQUESTS)
    fprintf (stderr, "PANIC: too many requests in pacx_waitall\n");
	else
		for (i = 0; i < *count; i++)
			save_reqs[i] = PACX_Request_f2c(array_of_requests[i]);

	ptr_statuses = (MPI_F_STATUSES_IGNORE == array_of_statuses)?my_statuses:array_of_statuses;

  CtoF77 (ppacx_waitall) (count, array_of_requests, ptr_statuses, ierror);

  temps_final = TIME;
  if (*ierror == MPI_SUCCESS)
  {
    for (ireq = 0; ireq < *count; ireq++)
    {
      if ((hash_req = hash_search (&requests, save_reqs[ireq])) != NULL)
      {
				if ((ret = get_Irank_obj (hash_req, &src_world, &size, &tag, &ptr_statuses[ireq*SIZEOF_PACX_STATUS])) != MPI_SUCCESS)
				{
					*ierror = ret;
					return;
				}
        if (hash_req->group != PACX_GROUP_NULL)
        {
					PACX_Fint group = PACX_Group_c2f(hash_req->group);
          CtoF77 (ppacx_group_free) (&group, &ret);
					PACX_CHECK(ret, ppacx_group_free);
        }

        /* PACX_ Stats */
        P2P_Communications ++; 
        P2P_Bytes_Recv += size; /* get_Irank_obj above returns size (the number of bytes received) */

        /*
         *   event : IRECVED_EV                 value : ---
         *   target : sender                    size  : received message size
         *   tag : message tag                  commid: communicator identifier
         *   aux : request
         */
        TRACE_MPIEVENT_NOHWC (temps_final, PACX_IRECVED_EV, EMPTY, src_world, size, tag, hash_req->commid, save_reqs[ireq]);
        hash_remove (&requests, save_reqs[ireq]);
      }
    }
  }
  /*
   *   event : WAIT_EV                    value : EVT_END
   *   target : ---                       size  : ---
   *   tag : ---
   */
  TRACE_MPIEVENT (temps_final, PACX_WAITALL_EV, EVT_END, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY);
}


/******************************************************************************
 ***  PPACX_WaitAny_Wrapper
 ******************************************************************************/

void PPACX_WaitAny_Wrapper (PACX_Fint *count, PACX_Fint array_of_requests[],
	PACX_Fint *index, PACX_Fint *status, PACX_Fint *ierror)
{
	PACX_Fint my_status[SIZEOF_PACX_STATUS], *ptr_status;
	PACX_Request save_reqs[MAX_WAIT_REQUESTS];
  hash_data_t *hash_req;
	int src_world, size, tag, ret, i;
  iotimer_t temps_final;

  TRACE_MPIEVENT (TIME, PACX_WAITANY_EV, EVT_BEGIN, EMPTY, EMPTY, EMPTY, EMPTY,
                  EMPTY);

  if (*count > MAX_WAIT_REQUESTS)
    fprintf (stderr, "PANIC: too many requests in pacx_waitall\n");
	else
		for (i = 0; i < *count; i++)
			save_reqs[i] = PACX_Request_f2c(array_of_requests[i]);

	ptr_status = (MPI_F_STATUS_IGNORE == status)?my_status:status;

  CtoF77 (ppacx_waitany) (count, array_of_requests, index, ptr_status, ierror);

  temps_final = TIME;

  if (*index != MPI_UNDEFINED && *ierror == MPI_SUCCESS)
  {
		PACX_Request req = save_reqs[*index-1];

    if ((hash_req = hash_search (&requests, req)) != NULL)
    {
			if ((ret = get_Irank_obj (hash_req, &src_world, &size, &tag, ptr_status)) != MPI_SUCCESS)
			{
				*ierror = ret;
				return;
			}

      if (hash_req->group != PACX_GROUP_NULL)
      {
				PACX_Fint group = PACX_Group_c2f(hash_req->group);
        CtoF77 (ppacx_group_free) (&group, &ret);
				PACX_CHECK(ret, ppacx_group_free);
      }

      /* PACX_ Stats */
      P2P_Communications ++; 
      P2P_Bytes_Recv += size; /* get_Irank_obj above returns size (the number of bytes received) */

      /*
       *   event : IRECVED_EV                 value : ---
       *   target : sender                    size  : received message size
       *   tag : message tag                  commid: communicator identifier
       *   aux : request
       */
      TRACE_MPIEVENT_NOHWC (temps_final, PACX_IRECVED_EV, EMPTY, src_world, size, tag, hash_req->commid, req);

      hash_remove (&requests, req);
    }

  }
  TRACE_MPIEVENT (temps_final, PACX_WAITANY_EV, EVT_END, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY);
}

/*****************************************************************************
 ***  PPACX_WaitSome_Wrapper
 ******************************************************************************/

void PPACX_WaitSome_Wrapper (PACX_Fint *incount, PACX_Fint array_of_requests[],
	PACX_Fint *outcount, PACX_Fint array_of_indices[],
	PACX_Fint array_of_statuses[][SIZEOF_PACX_STATUS], PACX_Fint *ierror)
{
	PACX_Fint my_statuses[MAX_WAIT_REQUESTS][SIZEOF_PACX_STATUS], *ptr_statuses;
	PACX_Request save_reqs[MAX_WAIT_REQUESTS];
  hash_data_t *hash_req;
  int src_world, size, tag, ret, i;
  iotimer_t temps_final;

  /*
   *   event : WAITSOME_EV                     value : EVT_BEGIN
   *   target : ---                            size  : ---
   *   tag : ---
   */
  TRACE_MPIEVENT (TIME, PACX_WAITSOME_EV, EVT_BEGIN, EMPTY, EMPTY, EMPTY, EMPTY,
                  EMPTY);
  /*
   * Arreglar-ho millor de cara a OMP
   */
  if (*incount > MAX_WAIT_REQUESTS)
    fprintf (stderr, "PANIC: too many requests in pacx_waitall\n");
	else
		for (i = 0; i < *incount; i++)
			save_reqs[i] = PACX_Request_f2c(array_of_requests[i]);

	ptr_statuses = (MPI_F_STATUSES_IGNORE == array_of_statuses)?my_statuses:array_of_statuses;

	CtoF77(ppacx_waitsome) (incount, array_of_requests, outcount, array_of_indices,
	  ptr_statuses, ierror);

  temps_final = TIME;

  if (*ierror == MPI_SUCCESS)
  {
    for (i = 1; i <= *outcount; i++)
    {
			PACX_Request req = save_reqs[array_of_indices[i-1]];
      if ((hash_req = hash_search (&requests, req)) != NULL)
      {
				if ((ret = get_Irank_obj (hash_req, &src_world, &size, &tag, &ptr_statuses[(i-1)*SIZEOF_PACX_STATUS])) != MPI_SUCCESS)
				{
					*ierror = ret;
					return;
				}
        if (hash_req->group != PACX_GROUP_NULL)
        {
					PACX_Fint group = PACX_Group_c2f(hash_req->group);
          CtoF77 (ppacx_group_free) (&group, &ret);
					PACX_CHECK(ret, ppacx_group_free);
        }

        /* PACX_ Stats */
        P2P_Communications ++;
        P2P_Bytes_Recv += size; /* get_Irank_obj above returns size (the number of bytes received) */

        /*
         *   event : IRECVED_EV                 value : ---
         *   target : sender                    size  : received message size
         *   tag : message tag                  commid: communicator identifier
         *   aux : request
         */
        TRACE_MPIEVENT_NOHWC (temps_final, PACX_IRECVED_EV, EMPTY, src_world, size, tag, hash_req->commid, req);
        hash_remove (&requests, req);
      }
    }
  }
  /*
   *   event : WAITSOME_EV                value : EVT_END
   *   target : ---                       size  : ---
   *   tag : ---
   */
  TRACE_MPIEVENT (temps_final, PACX_WAITSOME_EV, EVT_END, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY);
}

/******************************************************************************
 ***  PPACX_BCast_Wrapper
 ******************************************************************************/

void PPACX_BCast_Wrapper (void *buffer, PACX_Fint *count, PACX_Fint *datatype,
	PACX_Fint *root, PACX_Fint *comm, PACX_Fint *ierror)
{
  int me, ret, size;
  PACX_Comm c = PACX_Comm_f2c (*comm);

  CtoF77 (ppacx_comm_rank) (comm, &me, &ret);
	PACX_CHECK(ret, ppacx_comm_rank);

  CtoF77 (ppacx_type_size) (datatype, &size, &ret);
	PACX_CHECK(ret, ppacx_type_size);

  size *= *count;

  /* PACX_ Stats */
  GLOBAL_Communications ++;
  if (me == *root)
  {
     GLOBAL_Bytes_Sent += size;
  }
  else
  {
     GLOBAL_Bytes_Recv += size;
  }

  /*
   *   event : BCAST_EV                     value : EVT_BEGIN
   *   target : root_rank                   size  : message size
   *   tag : rank                           commid: communicator identifier
   *   aux : ---
   */

#if defined(IS_BGL_MACHINE)
  BGL_disable_barrier_inside = 1;
#endif

  TRACE_MPIEVENT (TIME, PACX_BCAST_EV, EVT_BEGIN, *root, size, me, c, 
  	PACX_CurrentOpGlobal);

  CtoF77 (ppacx_bcast) (buffer, count, datatype, root, comm, ierror);

  /*
   *   event : BCAST_EV                    value : EVT_END
   *   target : ---                         size  : ---
   *   tag : ---
   */
  TRACE_MPIEVENT (TIME, PACX_BCAST_EV, EVT_END, EMPTY, EMPTY, EMPTY, c,
  	PACX_CurrentOpGlobal);

#if defined(IS_BGL_MACHINE)
  BGL_disable_barrier_inside = 0;
#endif
}

/******************************************************************************
 ***  PPACX_AllToAll_Wrapper
 ******************************************************************************/
/******************************************************************************
 **      Function name : pacx_alltoall stub function
 **
 **      Description : Marks the beginning and ending of the alltoall
 **                    operation.
 **
 **                 0 1 2 3 4 5         0 6 C I O U
 **                 6 7 8 9 A B         1 7 D J P V
 **                 C D E F G H   -->   2 8 E K Q W
 **                 I J K L M N         3 9 F L R X
 **                 O P Q R S T         4 A G M R Y
 **                 U V W X Y Z         5 B H N T Z
 **
 ******************************************************************************/

void PPACX_AllToAll_Wrapper (void *sendbuf, PACX_Fint *sendcount,
	PACX_Fint *sendtype, void *recvbuf, PACX_Fint *recvcount, PACX_Fint *recvtype,
	PACX_Fint *comm, PACX_Fint *ierror)
{
  int me, ret, sendsize, recvsize, nprocs;
  PACX_Comm c = PACX_Comm_f2c (*comm);

  CtoF77 (ppacx_type_size) (sendtype, &sendsize, &ret);
	PACX_CHECK(ret, ppacx_type_size);

  CtoF77 (ppacx_type_size) (recvtype, &recvsize, &ret);
	PACX_CHECK(ret, ppacx_type_size);

  CtoF77 (ppacx_comm_size) (comm, &nprocs, &ret);
	PACX_CHECK(ret, ppacx_comm_size);

  CtoF77 (ppacx_comm_rank) (comm, &me, &ret);
	PACX_CHECK(ret, ppacx_comm_rank);

  /* PACX_ Stats */
  GLOBAL_Communications ++;
  GLOBAL_Bytes_Sent += *sendcount * sendsize;
  GLOBAL_Bytes_Recv += *recvcount * recvsize;

  /*
   *   event : ALLTOALL_EV                  value : EVT_BEGIN
   *   target : received size               size  : sent size
   *   tag : rank                           commid: communicator id
   *   aux : ---
   */
  TRACE_MPIEVENT (TIME, PACX_ALLTOALL_EV, EVT_BEGIN, *recvcount * recvsize,
                  *sendcount * sendsize, me, c, PACX_CurrentOpGlobal);

  CtoF77 (ppacx_alltoall) (sendbuf, sendcount, sendtype, recvbuf, recvcount,
                          recvtype, comm, ierror);

  /*
   *   event : ALLTOALL_EV                  value : EVT_END
   *   target : ---                         size  : ---
   *   tag : ---
   */
  TRACE_MPIEVENT (TIME, PACX_ALLTOALL_EV, EVT_END, EMPTY, EMPTY, EMPTY, c,
                  PACX_CurrentOpGlobal);
}


/******************************************************************************
 ***  PPACX_AllToAllV_Wrapper
 ******************************************************************************/
/******************************************************************************
 **      Function name : pacx_alltoallv stub function
 **
 **      Description : Marks the beginning and ending of the alltoallv
 **                    operation.
 **
 **                 0 1 2 3 4 5         0 6 C I O U
 **                 6 7 8 9 A B         1 7 D J P V
 **                 C D E F G H   -->   2 8 E K Q W
 **                 I J K L M N         3 9 F L R X
 **                 O P Q R S T         4 A G M R Y
 **                 U V W X Y Z         5 B H N T Z
 **
 ******************************************************************************/

void PPACX_AllToAllV_Wrapper (void *sendbuf, PACX_Fint *sendcount,
	PACX_Fint *sdispls, PACX_Fint *sendtype, void *recvbuf, PACX_Fint *recvcount,
	PACX_Fint *rdispls, PACX_Fint *recvtype,	PACX_Fint *comm, PACX_Fint *ierror)
{
  int me, ret, sendsize, recvsize, nprocs;
  int proc, sendc = 0, recvc = 0;
  PACX_Comm c = PACX_Comm_f2c (*comm);

  CtoF77 (ppacx_type_size) (sendtype, &sendsize, &ret);
	PACX_CHECK(ret, ppacx_type_size);

  CtoF77 (ppacx_type_size) (recvtype, &recvsize, &ret);
	PACX_CHECK(ret, ppacx_type_size);

  CtoF77 (ppacx_comm_size) (comm, &nprocs, &ret);
	PACX_CHECK(ret, ppacx_comm_size);

  CtoF77 (ppacx_comm_rank) (comm, &me, &ret);
	PACX_CHECK(ret, ppacx_comm_rank);

  for (proc = 0; proc < nprocs; proc++)
  {
		if (sendcount != NULL)
  	  sendc += sendcount[proc];
		if (recvcount != NULL)
  	  recvc += recvcount[proc];
  }

  /* PACX_ Stats */
  GLOBAL_Communications ++;
  GLOBAL_Bytes_Sent += sendc * sendsize;
  GLOBAL_Bytes_Recv += recvc * recvsize;

  /*
   *   event : ALLTOALLV_EV                  value : EVT_BEGIN
   *   target : received size               size  : sent size
   *   tag : rank                           commid: communicator id
   *   aux : ---
   */
  TRACE_MPIEVENT (TIME, PACX_ALLTOALLV_EV, EVT_BEGIN, recvsize * recvc,
                  sendsize * sendc, me, c, PACX_CurrentOpGlobal);

  CtoF77 (ppacx_alltoallv) (sendbuf, sendcount, sdispls, sendtype,
                           recvbuf, recvcount, rdispls, recvtype,
                           comm, ierror);

  /*
   *   event : ALLTOALLV_EV                  value : EVT_END
   *   target : ---                         size  : ---
   *   tag : ---
   */
  TRACE_MPIEVENT (TIME, PACX_ALLTOALLV_EV, EVT_END, EMPTY, EMPTY, EMPTY, c,
                  PACX_CurrentOpGlobal);
}



/******************************************************************************
 ***  PPACX_Allgather_Wrapper
 ******************************************************************************/
/******************************************************************************
 **      Function name : pacx_allgather stub function
 **
 **      Description : Marks the beginning and ending of the gather
 **                    operation.
 **
 **                 1 - - - - -         1 2 3 4 5 6
 **                 2 - - - - -         1 2 3 4 5 6
 **                 3 - - - - -   -->   1 2 3 4 5 6
 **                 4 - - - - -         1 2 3 4 5 6
 **                 5 - - - - -         1 2 3 4 5 6
 **                 6 - - - - -         1 2 3 4 5 6
 **
 ******************************************************************************/

void PPACX_Allgather_Wrapper (void *sendbuf, PACX_Fint *sendcount,
	PACX_Fint *sendtype, void *recvbuf, PACX_Fint *recvcount, PACX_Fint *recvtype,
	PACX_Fint *comm, PACX_Fint *ierror)
{
  int ret, sendsize, recvsize, me, nprocs;
  PACX_Comm c = PACX_Comm_f2c (*comm);

  CtoF77 (ppacx_type_size) (sendtype, &sendsize, &ret);
	PACX_CHECK(ret, ppacx_type_size);

  CtoF77 (ppacx_type_size) (recvtype, &recvsize, &ret);
	PACX_CHECK(ret, ppacx_type_size);

  CtoF77 (ppacx_comm_size) (comm, &nprocs, &ret);
	PACX_CHECK(ret, ppacx_comm_size);

  CtoF77 (ppacx_comm_rank) (comm, &me, &ret);
	PACX_CHECK(ret, ppacx_comm_rank);

  /* PACX_ Stats */
  GLOBAL_Communications ++;
  GLOBAL_Bytes_Sent += *sendcount * sendsize;
  GLOBAL_Bytes_Recv += *recvcount * recvsize;

  /*
   *   event : ALLGATHER_EV                 value : EVT_BEGIN
   *   target : ---                         size  : bytes sent
   *   tag : rank                           commid: communicator identifier
   *   aux : bytes received
   */
  TRACE_MPIEVENT (TIME, PACX_ALLGATHER_EV, EVT_BEGIN, EMPTY, *sendcount * sendsize,
                  me, c, *recvcount * recvsize * nprocs);

  CtoF77 (ppacx_allgather) (sendbuf, sendcount, sendtype,
                           recvbuf, recvcount, recvtype, comm, ierror);

  /*
   *   event : ALLGATHER_EV                 value : EVT_END
   *   target : ---                         size  : ---
   *   tag : ---
   */
  TRACE_MPIEVENT (TIME, PACX_ALLGATHER_EV, EVT_END, EMPTY, EMPTY, EMPTY, c, EMPTY);
}


/******************************************************************************
 ***  PPACX_Allgatherv_Wrapper
 ******************************************************************************/
/******************************************************************************
 **      Function name : pacx_allgatherv stub function
 **
 **      Description : Marks the beginning and ending of the gather
 **                    operation.
 **
 **                 1 - - - - -         1 2 3 4 5 6
 **                 2 - - - - -         1 2 3 4 5 6
 **                 3 - - - - -   -->   1 2 3 4 5 6
 **                 4 - - - - -         1 2 3 4 5 6
 **                 5 - - - - -         1 2 3 4 5 6
 **                 6 - - - - -         1 2 3 4 5 6
 **
 ******************************************************************************/

void PPACX_Allgatherv_Wrapper (void *sendbuf, PACX_Fint *sendcount,
	PACX_Fint *sendtype, void *recvbuf, PACX_Fint *recvcount, PACX_Fint *displs,
	PACX_Fint *recvtype, PACX_Fint *comm, PACX_Fint *ierror)
{
  int ret, sendsize, me, nprocs;
  int proc, recvsize, recvc = 0;
  PACX_Comm c = PACX_Comm_f2c (*comm);

  CtoF77 (ppacx_type_size) (sendtype, &sendsize, &ret);
	PACX_CHECK(ret, ppacx_type_size);
	
  CtoF77 (ppacx_type_size) (recvtype, &recvsize, &ret);
	PACX_CHECK(ret, ppacx_type_size);

  CtoF77 (ppacx_comm_size) (comm, &nprocs, &ret);
	PACX_CHECK(ret, ppacx_comm_size);

  CtoF77 (ppacx_comm_rank) (comm, &me, &ret);
	PACX_CHECK(ret, ppacx_comm_rank);

	if (recvcount != NULL)
		for (proc = 0; proc < nprocs; proc++)
			recvc += recvcount[proc];

  /* PACX_ Stats */
  GLOBAL_Communications ++;
  GLOBAL_Bytes_Sent += *sendcount * sendsize;
  GLOBAL_Bytes_Recv += recvc * recvsize;

  /*
   *   event : GATHER_EV                    value : EVT_BEGIN
   *   target : ---                         size  : bytes sent
   *   tag : rank                           commid: communicator identifier
   *   aux : bytes received
   */
  TRACE_MPIEVENT (TIME, PACX_ALLGATHERV_EV, EVT_BEGIN, EMPTY,
                  *sendcount * sendsize, me, c, recvsize * recvc);

  CtoF77 (ppacx_allgatherv) (sendbuf, sendcount, sendtype,
                            recvbuf, recvcount, displs, recvtype,
                            comm, ierror);

  /*
   *   event : GATHER_EV                    value : EVT_END
   *   target : ---                         size  : ---
   *   tag : ---
   */
  TRACE_MPIEVENT (TIME, PACX_ALLGATHERV_EV, EVT_END, EMPTY, EMPTY, EMPTY, c, EMPTY);
}


/******************************************************************************
 ***  PPACX_Gather_Wrapper
 ******************************************************************************/
/******************************************************************************
 **      Function name : pacx_gather stub function
 **
 **      Description : Marks the beginning and ending of the gather
 **                    operation.
 **
 **                 X - - - - -         X X X X X X
 **                 X - - - - -         - - - - - -
 **                 X - - - - -   -->   - - - - - -
 **                 X - - - - -         - - - - - -
 **                 X - - - - -         - - - - - -
 **                 X - - - - -         - - - - - -
 **
 ******************************************************************************/

void PPACX_Gather_Wrapper (void *sendbuf, PACX_Fint *sendcount,
	PACX_Fint *sendtype, void *recvbuf, PACX_Fint *recvcount, PACX_Fint *recvtype,
	PACX_Fint *root, PACX_Fint *comm, PACX_Fint *ierror)
{
  int ret, sendsize, recvsize, me, nprocs;
  PACX_Comm c = PACX_Comm_f2c (*comm);

  CtoF77 (ppacx_type_size) (sendtype, &sendsize, &ret);
	PACX_CHECK(ret, ppacx_type_size);

  CtoF77 (ppacx_type_size) (recvtype, &recvsize, &ret);
	PACX_CHECK(ret, ppacx_type_size);

  CtoF77 (ppacx_comm_size) (comm, &nprocs, &ret);
	PACX_CHECK(ret, ppacx_comm_size);

  CtoF77 (ppacx_comm_rank) (comm, &me, &ret);
	PACX_CHECK(ret, ppacx_comm_rank);

  /* PACX_ Stats */
  GLOBAL_Communications ++;
  if (me == *root)
  {
    GLOBAL_Bytes_Recv += *recvcount * recvsize;
  }
  else 
  {
    GLOBAL_Bytes_Sent += *sendcount * sendsize;
  }


  /*
   *   event : GATHER_EV                    value : EVT_BEGIN
   *   target : root rank                   size  : bytes sent
   *   tag : rank                           commid: communicator identifier
   *   aux : bytes received
   */
  if (me == *root)
  {
    TRACE_MPIEVENT (TIME, PACX_GATHER_EV, EVT_BEGIN, *root, *sendcount * sendsize,
                    me, c, *recvcount * recvsize * nprocs);
  }
  else
  {
    TRACE_MPIEVENT (TIME, PACX_GATHER_EV, EVT_BEGIN, *root, *sendcount * sendsize,
                    me, c, 0);
  }

  CtoF77 (ppacx_gather) (sendbuf, sendcount, sendtype,
                        recvbuf, recvcount, recvtype, root, comm, ierror);

  /*
   *   event : GATHER_EV                    value : EVT_END
   *   target : ---                         size  : ---
   *   tag : ---
   */
  TRACE_MPIEVENT (TIME, PACX_GATHER_EV, EVT_END, EMPTY, EMPTY, EMPTY, c, EMPTY);
}



/******************************************************************************
 ***  PPACX_GatherV_Wrapper
 ******************************************************************************/
/******************************************************************************
 **      Function name : pacx_gatherv stub function
 **
 **      Description : Marks the beginning and ending of the gatherv
 **                    operation.
 **
 **                 X - - - - -         X X X X X X
 **                 X - - - - -         - - - - - -
 **                 X - - - - -   -->   - - - - - -
 **                 X - - - - -         - - - - - -
 **                 X - - - - -         - - - - - -
 **                 X - - - - -         - - - - - -
 **
 ******************************************************************************/

void PPACX_GatherV_Wrapper (void *sendbuf, PACX_Fint *sendcount,
	PACX_Fint *sendtype, void *recvbuf, PACX_Fint *recvcount, PACX_Fint *displs,
	PACX_Fint *recvtype, PACX_Fint *root, PACX_Fint *comm, PACX_Fint *ierror)
{
  int ret, sendsize, me, nprocs;
  int proc, recvsize, recvc = 0;
  PACX_Comm c = PACX_Comm_f2c (*comm);

  CtoF77 (ppacx_type_size) (sendtype, &sendsize, &ret);
	PACX_CHECK(ret, ppacx_type_size);

  CtoF77 (ppacx_type_size) (recvtype, &recvsize, &ret);
	PACX_CHECK(ret, ppacx_type_size);

  CtoF77 (ppacx_comm_size) (comm, &nprocs, &ret);
	PACX_CHECK(ret, ppacx_comm_size);

  CtoF77 (ppacx_comm_rank) (comm, &me, &ret);
	PACX_CHECK(ret, ppacx_comm_rank);

	if (recvcount != NULL)
		for (proc = 0; proc < nprocs; proc++)
			recvc += recvcount[proc];

  /* PACX_ Stats */
  GLOBAL_Communications ++;
  if (me == *root)
  {
    GLOBAL_Bytes_Recv += recvc * recvsize;
  }
  else
  {
    GLOBAL_Bytes_Sent += *sendcount * sendsize;
  }

  /*
   *   event : GATHERV_EV                   value : EVT_BEGIN
   *   target : root rank                   size  : bytes sent
   *   tag : rank                           commid: communicator identifier
   *   aux : bytes received
   */
  if (me == *root)
  {
    TRACE_MPIEVENT (TIME, PACX_GATHERV_EV, EVT_BEGIN, *root, *sendcount * sendsize,
                    me, c, recvsize * recvc);
  }
  else
  {
    TRACE_MPIEVENT (TIME, PACX_GATHERV_EV, EVT_BEGIN, *root, *sendcount * sendsize,
                    me, c, 0);
  }

  CtoF77 (ppacx_gatherv) (sendbuf, sendcount, sendtype,
                         recvbuf, recvcount, displs, recvtype,
                         root, comm, ierror);

  /*
   *   event : GATHERV_EV                    value : EVT_END
   *   target : ---                         size  : ---
   *   tag : ---
   */
  TRACE_MPIEVENT (TIME, PACX_GATHERV_EV, EVT_END, EMPTY, EMPTY, EMPTY, c, EMPTY);
}



/******************************************************************************
 ***  PPACX_Scatter_Wrapper
 ******************************************************************************/
/******************************************************************************
 **      Function name : pacx_scatter stub function
 **
 **      Description : Marks the beginning and ending of the scatter
 **                    operation.
 **
 **                 X X X X X X         X - - - - -
 **                 - - - - - -         X - - - - -
 **                 - - - - - -   -->   X - - - - -
 **                 - - - - - -         X - - - - -
 **                 - - - - - -         X - - - - -
 **                 - - - - - -         X - - - - -
 **
 ******************************************************************************/

void PPACX_Scatter_Wrapper (void *sendbuf, PACX_Fint *sendcount,
	PACX_Fint *sendtype, void *recvbuf, PACX_Fint *recvcount, PACX_Fint *recvtype,
	PACX_Fint *root, PACX_Fint *comm, PACX_Fint *ierror)
{
  int ret, sendsize, recvsize, me, nprocs;
  PACX_Comm c = PACX_Comm_f2c (*comm);

  CtoF77 (ppacx_type_size) (sendtype, &sendsize, &ret);
	PACX_CHECK(ret, ppacx_type_size);

  CtoF77 (ppacx_type_size) (recvtype, &recvsize, &ret);
	PACX_CHECK(ret, ppacx_type_size);

  CtoF77 (ppacx_comm_size) (comm, &nprocs, &ret);
	PACX_CHECK(ret, ppacx_comm_size);

  CtoF77 (ppacx_comm_rank) (comm, &me, &ret);
	PACX_CHECK(ret, ppacx_comm_rank);

  /* PACX_ Stats */
  GLOBAL_Communications ++;
  if (me == *root)
  {
    GLOBAL_Bytes_Sent += *sendcount * sendsize;
  }
  else
  {
    GLOBAL_Bytes_Recv += *recvcount * recvsize;
  }

  /*
   *   event : SCATTER_EV                   value : EVT_BEGIN
   *   target : root rank                   size  : bytes sent
   *   tag : rank                           commid: communicator identifier
   *   aux : bytes received
   */
  if (me == *root)
  {
    TRACE_MPIEVENT (TIME, PACX_SCATTER_EV, EVT_BEGIN, *root,
                    *sendcount * sendsize * nprocs, me, c,
                    *recvcount * recvsize);
  }
  else
  {
    TRACE_MPIEVENT (TIME, PACX_SCATTER_EV, EVT_BEGIN, *root, 0, me, c,
                    *recvcount * recvsize);
  }

  CtoF77 (ppacx_scatter) (sendbuf, sendcount, sendtype,
                         recvbuf, recvcount, recvtype, root, comm, ierror);

  /*
   *   event : SCATTER_EV                   value : EVT_END
   *   target : ---                         size  : ---
   *   tag : ---
   */
  TRACE_MPIEVENT (TIME, PACX_SCATTER_EV, EVT_END, EMPTY, EMPTY, EMPTY, c, EMPTY);
}

/******************************************************************************
 ***  PPACX_ScatterV_Wrapper
 ******************************************************************************/
/******************************************************************************
 **      Function name : pacx_scatterv stub function
 **
 **      Description : Marks the beginning and ending of the scatterv
 **                    operation.
 **
 **                 X X X X X X         X - - - - -
 **                 - - - - - -         X - - - - -
 **                 - - - - - -   -->   X - - - - -
 **                 - - - - - -         X - - - - -
 **                 - - - - - -         X - - - - -
 **                 - - - - - -         X - - - - -
 **
 ******************************************************************************/

void PPACX_ScatterV_Wrapper (void *sendbuf, PACX_Fint *sendcount,
	PACX_Fint *displs, PACX_Fint *sendtype, void *recvbuf, PACX_Fint *recvcount,
	PACX_Fint *recvtype, PACX_Fint *root, PACX_Fint *comm, PACX_Fint *ierror)
{
  int ret, recvsize, me, nprocs;
  int proc, sendsize, sendc = 0;
  PACX_Comm c = PACX_Comm_f2c (*comm);

  CtoF77 (ppacx_type_size) (sendtype, &sendsize, &ret);
	PACX_CHECK(ret, ppacx_type_size);

  CtoF77 (ppacx_type_size) (recvtype, &recvsize, &ret);
	PACX_CHECK(ret, ppacx_type_size);

  CtoF77 (ppacx_comm_size) (comm, &nprocs, &ret);
	PACX_CHECK(ret, ppacx_comm_size);

  CtoF77 (ppacx_comm_rank) (comm, &me, &ret);
	PACX_CHECK(ret, ppacx_comm_rank);

	if (sendcount != NULL)
		for (proc = 0; proc < nprocs; proc++)
			sendc += sendcount[proc];

  /* PACX_ Stats */
  GLOBAL_Communications ++;
  if (me == *root)
  {
    GLOBAL_Bytes_Sent += sendc * sendsize;
  }
  else
  {
    GLOBAL_Bytes_Recv += *recvcount * recvsize;
  }

  /*
   *   event :  SCATTERV_EV                 value : EVT_BEGIN
   *   target : root rank                   size  : bytes sent
   *   tag : rank                           commid: communicator identifier
   *   aux : bytes received
   */
  if (me == *root)
  {
    TRACE_MPIEVENT (TIME, PACX_SCATTERV_EV, EVT_BEGIN, *root, sendsize * sendc, me,
                    c, *recvcount * recvsize);
  }
  else
  {
    TRACE_MPIEVENT (TIME, PACX_SCATTERV_EV, EVT_BEGIN, *root, 0, me, c,
                    *recvcount * recvsize);
  }

  CtoF77 (ppacx_scatterv) (sendbuf, sendcount, displs, sendtype,
                          recvbuf, recvcount, recvtype, root, comm, ierror);

  /*
   *   event : SCATTERV_EV                  value : EVT_END
   *   target : ---                         size  : ---
   *   tag : ---
   */
  TRACE_MPIEVENT (TIME, PACX_SCATTERV_EV, EVT_END, EMPTY, EMPTY, EMPTY, c, EMPTY);
}



/******************************************************************************
 ***  PPACX_Comm_Rank_Wrapper
 ******************************************************************************/

void PPACX_Comm_Rank_Wrapper (PACX_Fint *comm, PACX_Fint *rank, PACX_Fint *ierror)
{
  TRACE_MPIEVENT (TIME, PACX_COMM_RANK_EV, EVT_BEGIN, EMPTY, EMPTY, EMPTY, EMPTY,
                  EMPTY);
  CtoF77 (ppacx_comm_rank) (comm, rank, ierror);
  TRACE_MPIEVENT (TIME, PACX_COMM_RANK_EV, EVT_END, EMPTY, EMPTY, EMPTY, EMPTY,
                  EMPTY);
}




/******************************************************************************
 ***  PPACX_Comm_Size_Wrapper
 ******************************************************************************/

void PPACX_Comm_Size_Wrapper (PACX_Fint *comm, PACX_Fint *size, PACX_Fint *ierror)
{
  TRACE_MPIEVENT (TIME, PACX_COMM_SIZE_EV, EVT_BEGIN, EMPTY, EMPTY, EMPTY, EMPTY,
                  EMPTY);
  CtoF77 (ppacx_comm_size) (comm, size, ierror);
  TRACE_MPIEVENT (TIME, PACX_COMM_SIZE_EV, EVT_END, EMPTY, EMPTY, EMPTY, EMPTY,
                  EMPTY);
}

/******************************************************************************
 ***  PPACX_Comm_Create_Wrapper
 ******************************************************************************/

void PPACX_Comm_Create_Wrapper (PACX_Fint *comm, PACX_Fint *group,
	PACX_Fint *newcomm, PACX_Fint *ierror)
{
	PACX_Fint cnull = PACX_Comm_c2f(PACX_COMM_NULL);

	CtoF77 (ppacx_comm_create) (comm, group, newcomm, ierror);

	if (*newcomm != cnull && *ierror == MPI_SUCCESS)
	{	
		PACX_Comm comm_id = PACX_Comm_f2c(*newcomm);
		Trace_PACX_Communicator (PACX_COMM_CREATE_EV, comm_id);
	}
}


/******************************************************************************
 ***  PPACX_Comm_Dup_Wrapper
 ******************************************************************************/

void PPACX_Comm_Dup_Wrapper (PACX_Fint *comm, PACX_Fint *newcomm,
	PACX_Fint *ierror)
{
	PACX_Fint cnull = PACX_Comm_c2f(PACX_COMM_NULL);

	CtoF77 (ppacx_comm_dup) (comm, newcomm, ierror);

	if (*newcomm != cnull && *ierror == MPI_SUCCESS)
	{
		PACX_Comm comm_id = PACX_Comm_f2c (*newcomm);
		Trace_PACX_Communicator (PACX_COMM_DUP_EV, comm_id);
	}
}



/******************************************************************************
 ***  PPACX_Comm_Split_Wrapper
 ******************************************************************************/

void PPACX_Comm_Split_Wrapper (PACX_Fint *comm, PACX_Fint *color, PACX_Fint *key,
	PACX_Fint *newcomm, PACX_Fint *ierror)
{
	PACX_Fint cnull = PACX_Comm_c2f(PACX_COMM_NULL);

	CtoF77 (ppacx_comm_split) (comm, color, key, newcomm, ierror);

	if (*newcomm != cnull && *ierror == MPI_SUCCESS)
	{
		PACX_Comm comm_id = PACX_Comm_f2c (*newcomm);
		Trace_PACX_Communicator (PACX_COMM_SPLIT_EV, comm_id);
	}
}


/******************************************************************************
 ***  PPACX_Reduce_Scatter_Wrapper
 ******************************************************************************/
/******************************************************************************
 **      Function name :  pacx_reduce_scatter stub function
 **
 **      Description : Marks the beginning and ending of the reduce operation.
 ******************************************************************************/

void PPACX_Reduce_Scatter_Wrapper (void *sendbuf, void *recvbuf,
	PACX_Fint *recvcounts, PACX_Fint *datatype, PACX_Fint *op, PACX_Fint *comm,
	PACX_Fint *ierror)
{
	int me, size;
	int i;
	int sendcount = 0;
	int csize;
	PACX_Comm c = PACX_Comm_f2c (*comm);

	CtoF77 (ppacx_comm_rank) (comm, &me, ierror);
	PACX_CHECK(*ierror, ppacx_comm_rank);

	CtoF77 (ppacx_type_size) (datatype, &size, ierror);
	PACX_CHECK(*ierror, ppacx_type_size);

  /* PACX_ Stats */
  GLOBAL_Communications ++;

  CtoF77 (ppacx_comm_size) (comm, &csize, ierror);
	PACX_CHECK(*ierror, ppacx_comm_size);

	if (recvcounts != NULL)
		for (i = 0; i < csize; i++)
			sendcount += recvcounts[i];

  /* Reduce */  
  if (me == 0) 
  {
     GLOBAL_Bytes_Recv += sendcount * size;
  }
  else
  {
     GLOBAL_Bytes_Sent += sendcount * size;
  }

  /* Scatter */
  if (me == 0)
  {
     GLOBAL_Bytes_Sent += sendcount * size;
  }
  else
  {
     GLOBAL_Bytes_Recv += recvcounts[me] * size;
  }

  /*
   *   type : REDUCESCAT_EV                    value : EVT_BEGIN
   *   target : reduce operation ident.    size  : data size
   *   tag : whoami (comm rank)            comm : communicator id
   *   aux : ---
   */
  TRACE_MPIEVENT (TIME, PACX_REDUCESCAT_EV, EVT_BEGIN, *op, size, me, c, EMPTY);

  CtoF77 (ppacx_reduce_scatter) (sendbuf, recvbuf, recvcounts, datatype,
                                op, comm, ierror);

  /*
   *   event : REDUCESCAT_EV                    value : EVT_END
   *   target : ---                         size  : ---
   *   tag : ---
   */
  TRACE_MPIEVENT (TIME, PACX_REDUCESCAT_EV, EVT_END, EMPTY, EMPTY, EMPTY, c, EMPTY);
}



/******************************************************************************
 ***  PPACX_Scan_Wrapper
 ******************************************************************************/
/******************************************************************************
 **      Function name : pacx_scan  stub function
 **
 **      Description : Marks the beginning and ending of the scan operation.
 ******************************************************************************/

void PPACX_Scan_Wrapper (void *sendbuf, void *recvbuf, PACX_Fint *count,
	PACX_Fint *datatype, PACX_Fint *op, PACX_Fint *comm, PACX_Fint *ierror)
{
  int me, size, csize;
	PACX_Comm c = PACX_Comm_f2c(*comm);

  CtoF77 (ppacx_comm_rank) (comm, &me, ierror);
	PACX_CHECK(*ierror, ppacx_comm_rank);

  CtoF77 (ppacx_type_size) (datatype, &size, ierror);
	PACX_CHECK(*ierror, ppacx_type_size);

  /* PACX_ Stats */
  GLOBAL_Communications ++;

  CtoF77 (ppacx_comm_size) (comm, &csize, ierror);
	PACX_CHECK(*ierror, ppacx_comm_size);

  if (me != csize - 1)
  {
    GLOBAL_Bytes_Sent = *count * size; 
  }
  if (me != 0)
  {
    GLOBAL_Bytes_Recv = *count * size;
  }

  /*
   *   type : SCAN_EV                    value : EVT_BEGIN
   *   target : reduce operation ident.    size  : data size
   *   tag : whoami (comm rank)            comm : communicator id
   *   aux : ---
   */
  TRACE_MPIEVENT (TIME, PACX_SCAN_EV, EVT_BEGIN, *op, *count * size, me, c, PACX_CurrentOpGlobal);

  CtoF77 (ppacx_scan) (sendbuf, recvbuf, count, datatype, op, comm, ierror);

  /*
   *   event : SCAN_EV                      value : EVT_END
   *   target : ---                         size  : ---
   *   tag : ---
   */
  TRACE_MPIEVENT (TIME, PACX_SCAN_EV, EVT_END, EMPTY, EMPTY, EMPTY, c, PACX_CurrentOpGlobal);
}

/******************************************************************************
 ***  PPACX_Start_Wrapper
 ******************************************************************************/

void PPACX_Start_Wrapper (PACX_Fint *request, PACX_Fint *ierror)
{
	PACX_Request req;
	iotimer_t temps = TIME;

  /*
   *   type : START_EV                     value : EVT_BEGIN
   *   target : ---                        size  : ----
   *   tag : ---                           comm : ---
   *   aux : ---
   */
	TRACE_MPIEVENT (temps, PACX_START_EV, EVT_BEGIN, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY);

	/* Execute the real function */
	CtoF77 (ppacx_start) (request, ierror);

	/* Store the resulting request */
	req = PACX_Request_f2c(*request);
	Traceja_Persistent_Request (&req, temps);

  /*
   *   type : START_EV                     value : EVT_END
   *   target : ---                        size  : ----
   *   tag : ---                           comm : ---
   *   aux : ---
   */
	TRACE_MPIEVENT (TIME, PACX_START_EV, EVT_END, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY);
}


/******************************************************************************
 ***  PPACX_Startall_Wrapper
 ******************************************************************************/

void PPACX_Startall_Wrapper (PACX_Fint *count, PACX_Fint array_of_requests[],
	PACX_Fint *ierror)
{
  PACX_Fint save_reqs[MAX_WAIT_REQUESTS];
  int ii;
  iotimer_t temps = TIME;

  /*
   *   type : START_EV                     value : EVT_BEGIN
   *   target : ---                        size  : ----
   *   tag : ---                           comm : ---
   *   aux : ---
   */
  TRACE_MPIEVENT (temps, PACX_STARTALL_EV, EVT_BEGIN, EMPTY, EMPTY, EMPTY, EMPTY,
                  EMPTY);

  /*
   * Algunes implementacions es poden carregar aquesta informacio.
   * Cal salvar-la per poder tracejar després de fer la crida pmpi. 
   */
  memcpy (save_reqs, array_of_requests, (*count) * sizeof (PACX_Fint));

  /*
   * Primer cal fer la crida real 
   */
  CtoF77 (ppacx_startall) (count, array_of_requests, ierror);

  /*
   * Es tracejen totes les requests 
   */
	for (ii = 0; ii < (*count); ii++)
	{
		PACX_Request req = PACX_Request_f2c(&(save_reqs[ii]));
		Traceja_Persistent_Request (&req, temps);
	}

  /*
   *   type : START_EV                     value : EVT_END
   *   target : ---                        size  : ----
   *   tag : ---                           comm : ---
   *   aux : ---
   */
  TRACE_MPIEVENT (TIME, PACX_STARTALL_EV, EVT_END, EMPTY, EMPTY, EMPTY, EMPTY,
                  EMPTY);
}



/******************************************************************************
 ***  PPACX_Request_free_Wrapper
 ******************************************************************************/

void PPACX_Request_free_Wrapper (PACX_Fint *request, PACX_Fint *ierror)
{
	PACX_Request req;

  /*
   *   type : START_EV                     value : EVT_BEGIN
   *   target : ---                        size  : ----
   *   tag : ---                           comm : ---
   *   aux : ---
   */
  TRACE_MPIEVENT (TIME, PACX_REQUEST_FREE_EV, EVT_BEGIN, EMPTY, EMPTY, EMPTY,
                  EMPTY, EMPTY);

  /*
   * Cal guardar la request perque algunes implementacions se la carreguen. 
   */
  req = PACX_Request_f2c (*request);

  /*
   * S'intenta alliberar aquesta persistent request 
   */
  PR_Elimina_request (&PR_queue, &req);

  /*
   * Primer cal fer la crida real 
   */
  CtoF77 (ppacx_request_free) (request, ierror);

  /*
   *   type : START_EV                     value : EVT_END
   *   target : ---                        size  : ----
   *   tag : ---                           comm : ---
   *   aux : ---
   */
  TRACE_MPIEVENT (TIME, PACX_REQUEST_FREE_EV, EVT_END, EMPTY, EMPTY, EMPTY, EMPTY,
                  EMPTY);
}



/******************************************************************************
 ***  PPACX_Recv_init_Wrapper
 ******************************************************************************/

void PPACX_Recv_init_Wrapper (void *buf, PACX_Fint *count, PACX_Fint *datatype,
	PACX_Fint *source, PACX_Fint *tag, PACX_Fint *comm, PACX_Fint *request,
	PACX_Fint *ierror)
{
	PACX_Request req;
	PACX_Comm c = PACX_Comm_f2c (*comm);
	PACX_Datatype type = PACX_Type_f2c (*datatype);

  /*
   *   type : RECV_INIT_EV                 value : EVT_BEGIN
   *   target : ---                        size  : ----
   *   tag : ---                           comm : ---
   *   aux : ---
   */
  TRACE_MPIEVENT (TIME, PACX_RECV_INIT_EV, EVT_BEGIN, EMPTY, EMPTY, EMPTY, EMPTY,
                  EMPTY);

  /*
   * Primer cal fer la crida real 
   */
  CtoF77 (ppacx_recv_init) (buf, count, datatype, source, tag,
                           comm, request, ierror);

  /*
   * Es guarda aquesta request 
   */
	req = PACX_Request_f2c (*request);
	PR_NewRequest (PACX_IRECV_EV, buf, *count, type, *source, *tag, c, req, &PR_queue);

  /*
   *   type : RECV_INIT_EV                 value : EVT_END
   *   target : ---                        size  : ----
   *   tag : ---                           comm : ---
   *   aux : ---
   */
  TRACE_MPIEVENT (TIME, PACX_RECV_INIT_EV, EVT_END, EMPTY, EMPTY, EMPTY, EMPTY,
                  EMPTY);
}



/******************************************************************************
 ***  PPACX_Send_init_Wrapper
 ******************************************************************************/

void PPACX_Send_init_Wrapper (void *buf, PACX_Fint *count, PACX_Fint *datatype,
	PACX_Fint *dest, PACX_Fint *tag, PACX_Fint *comm, PACX_Fint *request,
	PACX_Fint *ierror)
{
	PACX_Request req;
	PACX_Datatype type = PACX_Type_f2c(*datatype);
	PACX_Comm c = PACX_Comm_f2c(*comm);

  /*
   *   type : SEND_INIT_EV                 value : EVT_BEGIN
   *   target : ---                        size  : ----
   *   tag : ---                           comm : ---
   *   aux : ---
   */
  TRACE_MPIEVENT (TIME, PACX_SEND_INIT_EV, EVT_BEGIN, EMPTY, EMPTY, EMPTY, EMPTY,
                  EMPTY);

  /*
   * Primer cal fer la crida real 
   */
  CtoF77 (ppacx_send_init) (buf, count, datatype, dest, tag,
                           comm, request, ierror);

  /*
   * Es guarda aquesta request 
   */
	req = PACX_Request_f2c (*request);
	PR_NewRequest (PACX_ISEND_EV, buf, *count, type, *dest, *tag, c, req, &PR_queue);

  /*
   *   type : SEND_INIT_EV                 value : EVT_END
   *   target : ---                        size  : ----
   *   tag : ---                           comm : ---
   *   aux : ---
   */

  TRACE_MPIEVENT (TIME, PACX_SEND_INIT_EV, EVT_END, EMPTY, EMPTY, EMPTY, EMPTY,
                  EMPTY);
}



/******************************************************************************
 ***  PPACX_Bsend_init_Wrapper
 ******************************************************************************/

void PPACX_Bsend_init_Wrapper (void *buf, PACX_Fint *count, PACX_Fint *datatype,
	PACX_Fint *dest, PACX_Fint *tag, PACX_Fint *comm, PACX_Fint *request,
	PACX_Fint *ierror)
{
	PACX_Request req;
	PACX_Datatype type = PACX_Type_f2c(*datatype);
	PACX_Comm c = PACX_Comm_f2c(*comm);

  /*
   *   type : BSEND_INIT_EV                value : EVT_BEGIN
   *   target : ---                        size  : ----
   *   tag : ---                           comm : ---
   *   aux : ---
   */
  TRACE_MPIEVENT (TIME, PACX_BSEND_INIT_EV, EVT_BEGIN, EMPTY, EMPTY, EMPTY, EMPTY,
                  EMPTY);

  /*
   * Primer cal fer la crida real 
   */
  CtoF77 (ppacx_bsend_init) (buf, count, datatype, dest, tag,
                            comm, request, ierror);

  /*
   * Es guarda aquesta request 
   */
	req = PACX_Request_f2c (*request);
	PR_NewRequest (PACX_IBSEND_EV, buf, *count, type, *dest, *tag, c, req, &PR_queue);

  /*
   *   type : BSEND_INIT_EV                value : EVT_END
   *   target : ---                        size  : ----
   *   tag : ---                           comm : ---
   *   aux : ---
   */
  TRACE_MPIEVENT (TIME, PACX_BSEND_INIT_EV, EVT_END, EMPTY, EMPTY, EMPTY, EMPTY,
                  EMPTY);
}


/******************************************************************************
 ***  PPACX_Rsend_init_Wrapper
 ******************************************************************************/

void PPACX_Rsend_init_Wrapper (void *buf, PACX_Fint *count, PACX_Fint *datatype,
	PACX_Fint *dest, PACX_Fint *tag, PACX_Fint *comm, PACX_Fint *request,
	PACX_Fint *ierror)
{
	PACX_Request req;
	PACX_Datatype type = PACX_Type_f2c(*datatype);
	PACX_Comm c = PACX_Comm_f2c(*comm);

  /*
   *   type : RSEND_INIT_EV                value : EVT_BEGIN
   *   target : ---                        size  : ----
   *   tag : ---                           comm : ---
   *   aux : ---
   */
  TRACE_MPIEVENT (TIME, PACX_RSEND_INIT_EV, EVT_BEGIN, EMPTY, EMPTY, EMPTY, EMPTY,
                  EMPTY);

  /*
   * Primer cal fer la crida real 
   */
  CtoF77 (ppacx_rsend_init) (buf, count, datatype, dest, tag,
                            comm, request, ierror);

  /*
   * Es guarda aquesta request 
   */
	req = PACX_Request_f2c (*request);
	PR_NewRequest (PACX_IRSEND_EV, buf, *count, type, *dest, *tag, c, req, &PR_queue);

  /*
   *   type : RSEND_INIT_EV                value : EVT_END
   *   target : ---                        size  : ----
   *   tag : ---                           comm : ---
   *   aux : ---
   */
  TRACE_MPIEVENT (TIME, PACX_RSEND_INIT_EV, EVT_END, EMPTY, EMPTY, EMPTY, EMPTY,
                  EMPTY);
}


/******************************************************************************
 ***  PPACX_Ssend_init_Wrapper
 ******************************************************************************/

void PPACX_Ssend_init_Wrapper (void *buf, PACX_Fint *count, PACX_Fint *datatype,
	PACX_Fint *dest, PACX_Fint *tag, PACX_Fint *comm, PACX_Fint *request,
	PACX_Fint *ierror)
{
	PACX_Request req;
	PACX_Datatype type = PACX_Type_f2c(*datatype);
	PACX_Comm c = PACX_Comm_f2c(*comm);

  /*
   *   type : SSEND_INIT_EV                value : EVT_BEGIN
   *   target : ---                        size  : ----
   *   tag : ---                           comm : ---
   *   aux : ---
   */
  TRACE_MPIEVENT (TIME, PACX_SSEND_INIT_EV, EVT_BEGIN, EMPTY, EMPTY, EMPTY, EMPTY,
                  EMPTY);

  /*
   * Primer cal fer la crida real 
   */
  CtoF77 (ppacx_ssend_init) (buf, count, datatype, dest, tag,
                            comm, request, ierror);

  /*
   * Es guarda aquesta request 
   */
	req = PACX_Request_f2c (*request);
	PR_NewRequest (PACX_ISSEND_EV, buf, *count, type, *dest, *tag, c, req, &PR_queue);

  /*
   *   type : SSEND_INIT_EV                value : EVT_END
   *   target : ---                        size  : ----
   *   tag : ---                           comm : ---
   *   aux : ---
   */
  TRACE_MPIEVENT (TIME, PACX_SSEND_INIT_EV, EVT_END, EMPTY, EMPTY, EMPTY, EMPTY,
                  EMPTY);
}

void PPACX_Cart_sub_Wrapper (PACX_Fint *comm, PACX_Fint *remain_dims,
	PACX_Fint *comm_new, PACX_Fint *ierror)
{
	PACX_Fint comm_null = PACX_Comm_c2f(PACX_COMM_NULL);

  CtoF77 (ppacx_cart_sub) (comm, remain_dims, comm_new, ierror);

  if (*ierror == MPI_SUCCESS && *comm_new != comm_null)
	{
		PACX_Comm comm_id = PACX_Comm_f2c (*comm_new);
    Trace_PACX_Communicator (PACX_CART_SUB_EV, comm_id);
	}
}

void PPACX_Cart_create_Wrapper (PACX_Fint *comm_old, PACX_Fint *ndims,
	PACX_Fint *dims, PACX_Fint *periods, PACX_Fint *reorder, PACX_Fint *comm_cart,
	PACX_Fint *ierror)
{
	PACX_Fint comm_null = PACX_Comm_c2f(PACX_COMM_NULL);

  CtoF77 (ppacx_cart_create) (comm_old, ndims, dims, periods, reorder,
                             comm_cart, ierror);

  if (*ierror == MPI_SUCCESS && *comm_cart != comm_null)
	{
		PACX_Comm comm_id = PACX_Comm_f2c (*comm_cart);
    Trace_PACX_Communicator (PACX_CART_CREATE_EV, comm_id);
	}
}

void PACX_Sendrecv_Fortran_Wrapper (void *sendbuf, PACX_Fint *sendcount,
	PACX_Fint *sendtype, PACX_Fint *dest, PACX_Fint *sendtag, void *recvbuf,
	PACX_Fint *recvcount, PACX_Fint *recvtype, PACX_Fint *source, PACX_Fint *recvtag,
	PACX_Fint *comm, PACX_Fint *status, PACX_Fint *ierr) 
{
	PACX_Fint my_status[SIZEOF_PACX_STATUS], *ptr_status;
	PACX_Comm c = PACX_Comm_f2c (*comm);
	int DataSendSize, DataRecvSize, DataSend, DataSize, ret;
	int sender_src, SourceRank, RecvRank, Count, sender_tag;

	if ((ret = get_rank_obj (comm, dest, &RecvRank)) != MPI_SUCCESS)
		return; 

	CtoF77(ppacx_type_size) (sendtype, &DataSendSize, &ret);
	PACX_CHECK(ret, ppacx_type_size);

	CtoF77(ppacx_type_size) (recvtype, &DataRecvSize, &ret);
	PACX_CHECK(ret, ppacx_type_size);

	DataSend = *sendcount * DataSendSize;

	TRACE_MPIEVENT (TIME, PACX_SENDRECV_EV, EVT_BEGIN, RecvRank, DataSend, *sendtag, c, EMPTY);

	ptr_status = (MPI_F_STATUS_IGNORE == status)?my_status:status;

	CtoF77(ppacx_sendrecv) (sendbuf, sendcount, sendtype, dest, sendtag,
	  recvbuf, recvcount, recvtype, source, recvtag, comm, ptr_status, ierr);

	CtoF77(ppacx_get_count) (status, recvtype, &Count, &ret);
	PACX_CHECK(ret, ppacx_get_count);

	if (Count != MPI_UNDEFINED)
		DataSize = DataRecvSize * Count;
	else
		DataSize = 0;

	sender_src = ptr_status[PACX_SOURCE_OFFSET];
	sender_tag = ptr_status[PACX_TAG_OFFSET];

	/* PACX_ Stats */
	P2P_Communications ++;
	P2P_Bytes_Sent += DataSend;
	P2P_Bytes_Recv += DataSize;

	if ((ret = get_rank_obj (comm, &sender_src, &SourceRank)) != MPI_SUCCESS)
		return; 

	TRACE_MPIEVENT (TIME, PACX_SENDRECV_EV, EVT_END, SourceRank, DataSize, sender_tag, c, EMPTY);
}

void PACX_Sendrecv_replace_Fortran_Wrapper (void *buf, PACX_Fint *count, PACX_Fint *type,
	PACX_Fint *dest, PACX_Fint *sendtag, PACX_Fint *source, PACX_Fint *recvtag,
	PACX_Fint *comm, PACX_Fint *status, PACX_Fint *ierr) 
{
	PACX_Fint my_status[SIZEOF_PACX_STATUS], *ptr_status;
	PACX_Comm c = PACX_Comm_f2c (*comm);
	int DataSendSize, DataRecvSize, DataSend, DataSize, ret;
	int sender_src, SourceRank, RecvRank, Count, sender_tag;

	if ((ret = get_rank_obj (comm, dest, &RecvRank)) != MPI_SUCCESS)
		return;

	CtoF77(ppacx_type_size) (type, &DataSendSize, &ret);
	DataRecvSize = DataSendSize;

	DataSend = *count * DataSendSize;

	TRACE_MPIEVENT (TIME, PACX_SENDRECV_REPLACE_EV, EVT_BEGIN, RecvRank, DataSend, *sendtag, c, EMPTY);

	ptr_status = (MPI_F_STATUS_IGNORE == status)?my_status:status;

	CtoF77(ppacx_sendrecv_replace) (buf, count, type, dest, sendtag, source, recvtag, comm, ptr_status, ierr);

	CtoF77(ppacx_get_count) (status, type, &Count, &ret);
	PACX_CHECK(ret, ppacx_get_count);

	if (Count != MPI_UNDEFINED)
		DataSize = DataRecvSize * Count;
	else
		DataSize = 0;

	sender_src = ptr_status[PACX_SOURCE_OFFSET];
	sender_tag = ptr_status[PACX_TAG_OFFSET];

	/* PACX_ Stats */
	P2P_Communications ++;
	P2P_Bytes_Sent += DataSend;
	P2P_Bytes_Recv += DataSize;

	if ((ret = get_rank_obj (comm, &sender_src, &SourceRank)) != MPI_SUCCESS)
		return;

	TRACE_MPIEVENT (TIME, PACX_SENDRECV_REPLACE_EV, EVT_END, SourceRank, DataSize, sender_tag, c, EMPTY);
}

#if defined (PACX_SUPPORTS_PACX_IO)
/*************************************************************
 **********************      MPIIO      **********************
 *************************************************************/
void PPACX_File_open_Fortran_Wrapper (PACX_Fint *comm, char *filename, PACX_Fint *amode,
	PACX_Fint *info, PACX_File *fh, PACX_Fint *len)
{
    TRACE_MPIEVENT (TIME, PACX_FILE_OPEN_EV, EVT_BEGIN, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY);
    CtoF77 (ppacx_file_open) (comm, filename, amode, info, fh, len);
    TRACE_MPIEVENT (TIME, PACX_FILE_OPEN_EV, EVT_END, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY);
}

void PPACX_File_close_Fortran_Wrapper (PACX_File *fh, PACX_Fint *ierror)
{
    TRACE_MPIEVENT (TIME, PACX_FILE_CLOSE_EV, EVT_BEGIN, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY);
    CtoF77 (ppacx_file_close) (fh, ierror);
    TRACE_MPIEVENT (TIME, PACX_FILE_CLOSE_EV, EVT_END, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY);
}

void PPACX_File_read_Fortran_Wrapper (PACX_File *fh, void *buf, PACX_Fint *count,
	PACX_Fint *datatype, PACX_Status *status, PACX_Fint *ierror)
{
    TRACE_MPIEVENT (TIME, PACX_FILE_READ_EV, EVT_BEGIN, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY);
    CtoF77 (ppacx_file_read) (fh, buf, count, datatype, status, ierror);
    TRACE_MPIEVENT (TIME, PACX_FILE_READ_EV, EVT_END, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY);
}

void PPACX_File_read_all_Fortran_Wrapper (PACX_File *fh, void *buf, PACX_Fint *count,
	PACX_Fint *datatype, PACX_Status *status, PACX_Fint *ierror)
{
    TRACE_MPIEVENT (TIME, PACX_FILE_READ_ALL_EV, EVT_BEGIN, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY);
    CtoF77 (ppacx_file_read_all) (fh, buf, count, datatype, status, ierror);
    TRACE_MPIEVENT (TIME, PACX_FILE_READ_ALL_EV, EVT_END, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY);
}

void PPACX_File_write_Fortran_Wrapper (PACX_File *fh, void *buf, PACX_Fint *count,
	PACX_Fint *datatype, PACX_Status *status, PACX_Fint *ierror)
{
    TRACE_MPIEVENT (TIME, PACX_FILE_WRITE_EV, EVT_BEGIN, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY);
    CtoF77 (ppacx_file_write) (fh, buf, count, datatype, status, ierror);
    TRACE_MPIEVENT (TIME, PACX_FILE_WRITE_EV, EVT_END, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY);
}

void PPACX_File_write_all_Fortran_Wrapper (PACX_File *fh, void *buf, PACX_Fint *count,
	PACX_Fint *datatype, PACX_Status *status, PACX_Fint *ierror)
{
    TRACE_MPIEVENT (TIME, PACX_FILE_WRITE_ALL_EV, EVT_BEGIN, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY);
    CtoF77 (ppacx_file_write_all) (fh, buf, count, datatype, status, ierror);
    TRACE_MPIEVENT (TIME, PACX_FILE_WRITE_ALL_EV, EVT_END, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY);
}

void PPACX_File_read_at_Fortran_Wrapper (PACX_File *fh, PACX_Offset *offset, void* buf,
	PACX_Fint *count, PACX_Fint *datatype, PACX_Status *status, PACX_Fint *ierror)
{
    TRACE_MPIEVENT (TIME, PACX_FILE_READ_AT_EV, EVT_BEGIN, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY);
    CtoF77 (ppacx_file_read_at) (fh, offset, buf, count, datatype, status, ierror);
    TRACE_MPIEVENT (TIME, PACX_FILE_READ_AT_EV, EVT_END, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY);
}

void PPACX_File_read_at_all_Fortran_Wrapper (PACX_File *fh, PACX_Offset *offset, void* buf,
	PACX_Fint *count, PACX_Fint *datatype, PACX_Status *status, PACX_Fint *ierror)
{
    TRACE_MPIEVENT (TIME, PACX_FILE_READ_AT_ALL_EV, EVT_BEGIN, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY);
    CtoF77 (ppacx_file_read_at_all) (fh, offset, buf, count, datatype, status, ierror);
    TRACE_MPIEVENT (TIME, PACX_FILE_READ_AT_ALL_EV, EVT_END, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY);
}

void PPACX_File_write_at_Fortran_Wrapper (PACX_File *fh, PACX_Offset *offset, void* buf,
	PACX_Fint *count, PACX_Fint *datatype, PACX_Status *status, PACX_Fint *ierror)
{
    TRACE_MPIEVENT (TIME, PACX_FILE_WRITE_AT_EV, EVT_BEGIN, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY);
    CtoF77 (ppacx_file_write_at) (fh, offset, buf, count, datatype, status, ierror);
    TRACE_MPIEVENT (TIME, PACX_FILE_WRITE_AT_EV, EVT_END, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY);
}

void PPACX_File_write_at_all_Fortran_Wrapper (PACX_File *fh, PACX_Offset *offset, void* buf,
	PACX_Fint *count, PACX_Fint *datatype, PACX_Status *status, PACX_Fint *ierror)
{
    TRACE_MPIEVENT (TIME, PACX_FILE_WRITE_AT_ALL_EV, EVT_BEGIN, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY);
    CtoF77 (ppacx_file_write_at_all) (fh, offset, buf, count, datatype, status, ierror);
    TRACE_MPIEVENT (TIME, PACX_FILE_WRITE_AT_ALL_EV, EVT_END, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY);
}

#endif /* PACX_SUPPORTS_PACX_IO */

#endif /* defined(FORTRAN_SYMBOLS) */

#if defined(C_SYMBOLS)

/******************************************************************************
 ***  get_Irank_obj_C
 ******************************************************************************/

static int get_Irank_obj_C (hash_data_t * hash_req, int *src_world, int *size,
	int *tag, PACX_Status *status)
{
	int ret, dest, recved_count;

#if defined(DEAD_CODE)
	if (MPI_STATUS_IGNORE != status)
	{
		ret = PPACX_Get_count (status, PACX_BYTE, &recved_count);
		PACX_CHECK(ret, PPACX_Get_count);

		if (recved_count != MPI_UNDEFINED)
			*size = recved_count;
		else
			*size = 0;

		*tag = status[0].PACX_TAG;
		dest = status[0].PACX_SOURCE;
	}
	else
	{
		*tag = hash_req->tag;
		*size = hash_req->size;
		dest = hash_req->partner;
	}
#endif

	ret = PPACX_Get_count (status, PACX_BYTE, &recved_count);
	PACX_CHECK(ret, PPACX_Get_count);

	if (recved_count != MPI_UNDEFINED)
		*size = recved_count;
	else
		*size = 0;

	*tag = status->MPI_TAG;
	dest = status->MPI_SOURCE;

	if (PACX_GROUP_NULL != hash_req->group)
	{
		ret = PPACX_Group_translate_ranks (hash_req->group, 1, &dest, grup_global,
			src_world);
		PACX_CHECK(ret, PPACX_Group_translate_ranks);
	}
	else
		*src_world = dest;

	return MPI_SUCCESS;
}


/******************************************************************************
*******************************************************************************
*******************************************************************************
*****************************  Wrappers versio C ******************************
*******************************************************************************
*******************************************************************************
*******************************************************************************/

/******************************************************************************
 ***  PACX_Init_C_Wrapper
 ******************************************************************************/

int PACX_Init_C_Wrapper (int *argc, char ***argv)
{
	int val = 0, me, ret;
	iotimer_t temps_inici_PACX_Init, temps_final_PACX_Init;
	PACX_Comm comm = PACX_COMM_WORLD;
	char *config_file;

	mptrace_IsMPI = TRUE;

	hash_init (&requests);
	PR_queue_init (&PR_queue);

	val = PPACX_Init (argc, argv);

	ret = PPACX_Comm_rank (comm, &me);
	PACX_CHECK(ret, PPACX_Comm_rank);

	ret = PPACX_Comm_size (comm, &NumOfTasks);
	PACX_CHECK(ret, PPACX_Comm_size);

	InitPACXCommunicators ();

	/* We have to gather the task id */ 
	TaskID_Setup (me);

#if defined(SAMPLING_SUPPORT)
	/* If sampling is enabled, just stop all the processes at the same point
	   and continue */
	PPACX_Barrier (PACX_COMM_WORLD);
#endif

	if (getenv ("MPTRACE_CONFIG_FILE") != NULL)
		/* Obtain a localized copy *except for the master process* */
		config_file = PACX_Distribute_XML_File (TASKID, NumOfTasks, getenv ("MPTRACE_CONFIG_FILE"));
	else
		config_file = NULL;

	/* Initialize the backend (first step) */
	if (!Backend_preInitialize (TASKID, NumOfTasks, config_file))
		return val;

	/* Remove the local copy only if we're not the master */
	if (me != 0)
		unlink (config_file);
	free (config_file);

	Share_Nodes_Info ();
	Generate_Task_File_List (NumOfTasks, TasksNodes);

	/* Take the time now, we can't put MPIINIT_EV before APPL_EV */
	temps_inici_PACX_Init = TIME;

	/* Call a barrier in order to synchronize all tasks using MPIINIT_EV / END*/
	ret = PPACX_Barrier (PACX_COMM_WORLD);

	initTracingTime = temps_final_PACX_Init = TIME;

	/* End initialization of the backend */
	if (!Backend_postInitialize (me, NumOfTasks, temps_inici_PACX_Init, temps_final_PACX_Init, TasksNodes))
		return val;

	/* Annotate topologies (if available) */
	GetTopology();

	/* Annotate already built communicators */
	Trace_PACX_Communicator (PACX_COMM_CREATE_EV, PACX_COMM_WORLD);
	Trace_PACX_Communicator (PACX_COMM_CREATE_EV, PACX_COMM_SELF);

	return val;
}

#if defined(PACX_HAS_INIT_THREAD)
int PACX_Init_thread_C_Wrapper (int *argc, char ***argv, int required, int *provided)
{
	int val = 0, me, ret;
	iotimer_t temps_inici_PACX_Init, temps_final_PACX_Init;
	PACX_Comm comm = PACX_COMM_WORLD;
	char *config_file;

	mptrace_IsMPI = TRUE;

	hash_init (&requests);
	PR_queue_init (&PR_queue);

	if (required == PACX_THREAD_MULTIPLE || required == PACX_THREAD_SERIALIZED)
		fprintf (stderr, "mpitrace: WARNING! Instrumentation library does not support PACX_THREAD_MULTIPLE and PACX_THREAD_SERIALIZED modes\n");

	val = PPACX_Init_thread (argc, argv, required, provided);

	ret = PPACX_Comm_rank (comm, &me);
	PACX_CHECK(ret, PPACX_Comm_rank);

	ret = PPACX_Comm_size (comm, &NumOfTasks);
	PACX_CHECK(ret, PPACX_Comm_size);

	InitPACXCommunicators ();

	/* We have to gather the task id */ 
	TaskID_Setup (me);

#if defined(SAMPLING_SUPPORT)
	/* If sampling is enabled, just stop all the processes at the same point
	   and continue */
	PPACX_Barrier (PACX_COMM_WORLD);
#endif

	if (getenv ("MPTRACE_CONFIG_FILE") != NULL)
		/* Obtain a localized copy *except for the master process* */
		config_file = PACX_Distribute_XML_File (TASKID, NumOfTasks, getenv ("MPTRACE_CONFIG_FILE"));
	else
		config_file = NULL;

	/* Initialize the backend (first step) */
	if (!Backend_preInitialize (TASKID, NumOfTasks, config_file))
		return val;

	/* Remove the local copy only if we're not the master */
	if (me != 0)
		unlink (config_file);
	free (config_file);

	Share_Nodes_Info ();
	Generate_Task_File_List (NumOfTasks, TasksNodes);

	/* Take the time now, we can't put MPIINIT_EV before APPL_EV */
	temps_inici_PACX_Init = TIME;

	/* Call a barrier in order to synchronize all tasks using MPIINIT_EV / END*/
	ret = PPACX_Barrier (PACX_COMM_WORLD);

	initTracingTime = temps_final_PACX_Init = TIME;

	/* End initialization of the backend */
	if (!Backend_postInitialize (me, NumOfTasks, temps_inici_PACX_Init, temps_final_PACX_Init, TasksNodes))
		return val;

	/* Annotate topologies (if available) */
	GetTopology();

	/* Annotate already built communicators */
	Trace_PACX_Communicator (PACX_COMM_CREATE_EV, PACX_COMM_WORLD);
	Trace_PACX_Communicator (PACX_COMM_CREATE_EV, PACX_COMM_SELF);

	return val;
}
#endif /* PACX_HAS_INIT_THREAD */


/******************************************************************************
 ***  PACX_Finalize_C_Wrapper
 ******************************************************************************/

int PACX_Finalize_C_Wrapper (void)
{
	int ierror = 0;

	if (!mpitrace_on)
		return 0;

#if defined(IS_BGL_MACHINE)
	BGL_disable_barrier_inside = 1;
#endif

	TRACE_MPIEVENT (TIME, PACX_FINALIZE_EV, EVT_BEGIN, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY);

	TRACE_MYRINET_HWC();

	TRACE_MPIEVENT (TIME, PACX_FINALIZE_EV, EVT_END, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY);

#if defined(IS_BGL_MACHINE)
	BGL_disable_barrier_inside = 0;
#endif

#if HAVE_MRNET
    if (MRNet_isEnabled())
    {
	    Quit_MRNet(TASKID);
	}
#endif

   /* fprintf(stderr, "[T: %d] Invoking Thread_Finalization\n", TASKID); */

	/* Es tanca la llibreria de traceig */
	Thread_Finalization ();

	if (mpit_gathering_enabled)
		Gather_MPITS();

	ierror = PPACX_Finalize ();

	return ierror;
}

/******************************************************************************
 ***  PACX_Bsend_C_Wrapper
 ******************************************************************************/

int PACX_Bsend_C_Wrapper (void *buf, int count, PACX_Datatype datatype, int dest,
                         int tag, PACX_Comm comm)
{
  int size, receiver, ret;

  ret = PPACX_Type_size (datatype, &size);
	PACX_CHECK(ret, PPACX_Type_size);

  if ((ret = get_rank_obj_C (comm, dest, &receiver)) != MPI_SUCCESS)
    return ret;

  size *= count;

  /* PACX_ Stats */
  P2P_Communications ++;
  P2P_Bytes_Sent += size;

  /*
   *   event : BSEND_EV                      value : EVT_BEGIN
   *   target : receiver                     size  : send message size
   *   tag : message tag
   */
  TRACE_MPIEVENT (TIME, PACX_BSEND_EV, EVT_BEGIN, receiver, size, tag, comm,
                  EMPTY);

  ret = PPACX_Bsend (buf, count, datatype, dest, tag, comm);

  /*
   *   event : BSEND_EV                      value : EVT_END
   *   target : receiver                     size  : send message size
   *   tag : message tag
   */
  TRACE_MPIEVENT (TIME, PACX_BSEND_EV, EVT_END, receiver, size, tag, comm, EMPTY);

  return ret;
}


/******************************************************************************
 ***  PACX_Ssend_C_Wrapper
 ******************************************************************************/

int PACX_Ssend_C_Wrapper (void *buf, int count, PACX_Datatype datatype, int dest,
                         int tag, PACX_Comm comm)
{
  int size, receiver, ret;

  ret = PPACX_Type_size (datatype, &size);
	PACX_CHECK(ret, PPACX_Type_size);

  if ((ret = get_rank_obj_C (comm, dest, &receiver)) != MPI_SUCCESS)
    return ret;

  size *= count;

  /* PACX_ Stats */
  P2P_Communications ++;
  P2P_Bytes_Sent += size;

  /*
   *   event : SSEND_EV                      value : EVT_BEGIN
   *   target : receiver                     size  : send message size
   *   tag : message tag
   */
  TRACE_MPIEVENT (TIME, PACX_SSEND_EV, EVT_BEGIN, receiver, size, tag, comm,
                  EMPTY);

  ret = PPACX_Ssend (buf, count, datatype, dest, tag, comm);

  /*
   *   event : SSEND_EV                      value : EVT_END
   *   target : receiver                     size  : send message size
   *   tag : message tag
   */
  TRACE_MPIEVENT (TIME, PACX_SSEND_EV, EVT_END, receiver, size, tag, comm, EMPTY);

  return ret;
}



/******************************************************************************
 ***  PACX_Rsend_C_Wrapper
 ******************************************************************************/

int PACX_Rsend_C_Wrapper (void *buf, int count, PACX_Datatype datatype, int dest,
                         int tag, PACX_Comm comm)
{
  int size, receiver, ret;

  ret = PPACX_Type_size (datatype, &size);
	PACX_CHECK(ret, PPACX_Type_size);

  if ((ret = get_rank_obj_C (comm, dest, &receiver)) != MPI_SUCCESS)
    return ret;

  size *= count;

  /* PACX_ Stats */
  P2P_Communications ++;
  P2P_Bytes_Sent += size;

  /*
   *   event : RSEND_EV                      value : EVT_BEGIN
   *   target : receiver                     size  : send message size
   *   tag : message tag
   */
  TRACE_MPIEVENT (TIME, PACX_RSEND_EV, EVT_BEGIN, receiver, size, tag, comm,
                  EMPTY);

  ret = PPACX_Rsend (buf, count, datatype, dest, tag, comm);

  /*
   *   event : RSEND_EV                      value : EVT_END
   *   target : receiver                     size  : send message size
   *   tag : message tag
   */
  TRACE_MPIEVENT (TIME, PACX_RSEND_EV, EVT_END, receiver, size, tag, comm, EMPTY);

  return ret;
}



/******************************************************************************
 ***  PACX_Send_C_Wrapper
 ******************************************************************************/

int PACX_Send_C_Wrapper (void *buf, int count, PACX_Datatype datatype, int dest,
                        int tag, PACX_Comm comm)
{
  int size, receiver, ret;

  ret = PPACX_Type_size (datatype, &size);
	PACX_CHECK(ret, PPACX_Type_size);

  if ((ret = get_rank_obj_C (comm, dest, &receiver)) != MPI_SUCCESS)
    return ret;

  size *= count;

  /* PACX_ Stats */
  P2P_Communications ++;
  P2P_Bytes_Sent += size;

  /*
   *   event : SEND_EV                       value : EVT_BEGIN
   *   target : receiver                     size  : send message size
   *   tag : message tag
   */
  TRACE_MPIEVENT (TIME, PACX_SEND_EV, EVT_BEGIN, receiver, size, tag, comm, EMPTY);

  ret = PPACX_Send (buf, count, datatype, dest, tag, comm);
  
  /*
   *   event : SEND_EV                       value : EVT_END
   *   target : receiver                     size  : send message size
   *   tag : message tag
   */
  TRACE_MPIEVENT (TIME, PACX_SEND_EV, EVT_END, receiver, size, tag, comm, EMPTY);

  return ret;
}



/******************************************************************************
 ***  PACX_Ibsend_C_Wrapper
 ******************************************************************************/

int PACX_Ibsend_C_Wrapper (void *buf, int count, PACX_Datatype datatype, int dest,
                          int tag, PACX_Comm comm, PACX_Request *request)
{
  int size, receiver, ret;

  ret = PPACX_Type_size (datatype, &size);
	PACX_CHECK(ret, PPACX_Type_size);

  if ((ret = get_rank_obj_C (comm, dest, &receiver)) != MPI_SUCCESS)
    return ret;

  size *= count;

  /* PACX_ Stats */
  P2P_Communications ++;
  P2P_Bytes_Sent += size;

  /*
   *   event : IBSEND_EV                     value : EVT_BEGIN
   *   target : ---                          size  : ---
   *   tag : ---                             commid: ---
   *   aux : ---
   */
  TRACE_MPIEVENT (TIME, PACX_IBSEND_EV, EVT_BEGIN, receiver, size, tag, comm, EMPTY);

  ret = PPACX_Ibsend (buf, count, datatype, dest, tag, comm, request);

  /*
   *   event : IBSEND_EV                     value : EVT_END
   *   target : ---                          size  : ---
   *   tag : ---                             commid: ---
   *   aux : ---
   */
  TRACE_MPIEVENT (TIME, PACX_IBSEND_EV, EVT_END, receiver, size, tag, comm, EMPTY);

  return ret;
}



/******************************************************************************
 ***  PACX_Isend_C_Wrapper
 ******************************************************************************/

int PACX_Isend_C_Wrapper (void *buf, int count, PACX_Datatype datatype, int dest,
                         int tag, PACX_Comm comm, PACX_Request *request)
{
  int size, receiver, ret;

  ret = PPACX_Type_size (datatype, &size);
	PACX_CHECK(ret, PPACX_Type_size);

  if ((ret = get_rank_obj_C (comm, dest, &receiver)) != MPI_SUCCESS)
    return ret;

  size *= count;

  /* PACX_ Stats */
  P2P_Communications ++;
  P2P_Bytes_Sent += size;

  /*
   *   event : ISEND_EV                      value : EVT_BEGIN
   *   target : ---                          size  : ---
   *   tag : ---                             commid: ---
   *   aux : ---
   */
  TRACE_MPIEVENT (TIME, PACX_ISEND_EV, EVT_BEGIN, receiver, size, tag, comm, EMPTY);

  ret = PPACX_Isend (buf, count, datatype, dest, tag, comm, request);

  /*
   *   event : ISEND_EV                      value : EVT_END
   *   target : ---                          size  : ---
   *   tag : ---                             commid: ---
   *   aux : ---
   */
  TRACE_MPIEVENT (TIME, PACX_ISEND_EV, EVT_END, receiver, size, tag, comm, EMPTY);

  return ret;
}



/******************************************************************************
 ***  PACX_Issend_C_Wrapper
 ******************************************************************************/

int PACX_Issend_C_Wrapper (void *buf, int count, PACX_Datatype datatype, int dest,
                          int tag, PACX_Comm comm, PACX_Request *request)
{
  int size, receiver, ret;

  ret = PPACX_Type_size (datatype, &size);
	PACX_CHECK(ret, PPACX_Type_size);

  if ((ret = get_rank_obj_C (comm, dest, &receiver)) != MPI_SUCCESS)
    return ret;

  size *= count;

  /* PACX_ Stats */
  P2P_Communications ++;
  P2P_Bytes_Sent += size;

  /*
   *   event : ISSEND_EV                     value : EVT_BEGIN
   *   target : ---                          size  : ---
   *   tag : ---                             commid: ---
   *   aux : ---
   */
  TRACE_MPIEVENT (TIME, PACX_ISSEND_EV, EVT_BEGIN, receiver, size, tag, comm, EMPTY);

  ret = PPACX_Issend (buf, count, datatype, dest, tag, comm, request);

  /*
   *   event : ISSEND_EV                     value : EVT_END
   *   target : ---                          size  : ---
   *   tag : ---                             commid: ---
   *   aux : ---
   */
  TRACE_MPIEVENT (TIME, PACX_ISSEND_EV, EVT_END, receiver, size, tag, comm, EMPTY);

  return ret;
}



/******************************************************************************
 ***  PACX_Irsend_C_Wrapper
 ******************************************************************************/

int PACX_Irsend_C_Wrapper (void *buf, int count, PACX_Datatype datatype, int dest,
                          int tag, PACX_Comm comm, PACX_Request *request)
{
  int size, receiver, ret;

  ret = PPACX_Type_size (datatype, &size);
	PACX_CHECK(ret, PPACX_Type_size);

  if ((ret = get_rank_obj_C (comm, dest, &receiver)) != MPI_SUCCESS)
    return ret;

  size *= count;

  /* PACX_ Stats */
  P2P_Communications ++;
  P2P_Bytes_Sent += size;

  /*
   *   event : IRSEND_EV                     value : EVT_BEGIN
   *   target : ---                          size  : ---
   *   tag : ---                             commid: ---
   *   aux : ---
   */
  TRACE_MPIEVENT (TIME, PACX_IRSEND_EV, EVT_BEGIN, receiver, size, tag, comm, EMPTY);

  ret = PPACX_Irsend (buf, count, datatype, dest, tag, comm, request);

  /*
   *   event : IRSEND_EV                     value : EVT_END
   *   target : ---                          size  : ---
   *   tag : ---                             commid: ---
   *   aux : ---
   */
  TRACE_MPIEVENT (TIME, PACX_IRSEND_EV, EVT_END, receiver, size, tag, comm, EMPTY);

  return ret;
}



/******************************************************************************
 ***  PACX_Recv_C_Wrapper
 ******************************************************************************/

int PACX_Recv_C_Wrapper (void *buf, int count, PACX_Datatype datatype, int source,
                        int tag, PACX_Comm comm, PACX_Status *status)
{
	PACX_Status my_status, *ptr_status;
  int size, src_world, sender_src, ret, recved_count, sended_tag, ierror;

  ret = PPACX_Type_size (datatype, &size);
	PACX_CHECK(ret, PPACX_Type_size);

	if ((ret = get_rank_obj_C (comm, source, &src_world)) != MPI_SUCCESS)
		return ret;

  /*
   *   event : RECV_EV                      value : EVT_BEGIN    
   *   target : MPI_ANY_SOURCE or sender    size  : receive buffer size    
   *   tag : message tag or PACX_ANY_TAG     commid: Communicator identifier
   *   aux: ---
   */
  TRACE_MPIEVENT (TIME, PACX_RECV_EV, EVT_BEGIN, src_world, count * size, tag,
                  comm, EMPTY);

	ptr_status = (MPI_STATUS_IGNORE == status)?&my_status:status; 
 
  ierror = PPACX_Recv (buf, count, datatype, source, tag, comm, ptr_status);

	ret = PPACX_Get_count (ptr_status, datatype, &recved_count);
	PACX_CHECK(ret, PPACX_Get_count);

	if (recved_count != MPI_UNDEFINED)
		size *= recved_count;
	else
		size = 0;

	sender_src = ptr_status->MPI_SOURCE;
	sended_tag = ptr_status->MPI_TAG;

	/* PACX_ Stats */
	P2P_Communications ++;
	P2P_Bytes_Recv += size;

	if ((ret = get_rank_obj_C (comm, sender_src, &src_world)) != MPI_SUCCESS)
		return ret;

  /*
   *   event : RECV_EV                      value : EVT_END
   *   target : sender                      size  : received message size    
   *   tag : message tag
   */
	TRACE_MPIEVENT (TIME, PACX_RECV_EV, EVT_END, src_world, size, sended_tag, comm,
		EMPTY);

  return ierror;
}



/******************************************************************************
 ***  PACX_Irecv_C_Wrapper
 ******************************************************************************/

int PACX_Irecv_C_Wrapper (void *buf, int count, PACX_Datatype datatype,
	int source, int tag, PACX_Comm comm, PACX_Request *request)
{
  hash_data_t hash_req;
  int inter, ret, ierror, size, src_world;

  ret = PPACX_Type_size (datatype, &size);
	PACX_CHECK(ret, PPACX_Type_size);

	if ((ret = get_rank_obj_C (comm, source, &src_world)) != MPI_SUCCESS)
		return ret;

  /*
   *   event : IRECV_EV                     value : EVT_BEGIN
   *   target : ---                         size  : ---
   *   tag : ---
   */
  TRACE_MPIEVENT (TIME, PACX_IRECV_EV, EVT_BEGIN, src_world, count * size, tag,
                  comm, EMPTY);

  ierror = PPACX_Irecv (buf, count, datatype, source, tag, comm, request);

  hash_req.key = *request;
  hash_req.commid = comm;
	hash_req.partner = source;
	hash_req.tag = tag;
	hash_req.size = count * size;

	if (comm == PACX_COMM_WORLD)
	{
		hash_req.group = PACX_GROUP_NULL;
	}
	else
	{
		ret = PPACX_Comm_test_inter (comm, &inter);
		PACX_CHECK(ret,PPACX_Comm_test_inter);

		if (inter)
		{
			ret = PPACX_Comm_remote_group (comm, &hash_req.group);
			PACX_CHECK(ret,PPACX_Comm_remote_group);
		}
		else
		{
			ret = PPACX_Comm_group (comm, &hash_req.group);
			PACX_CHECK(ret,PPACX_Comm_group);
		}
	}

  hash_add (&requests, &hash_req);

  /*
   *   event : IRECV_EV                     value : EVT_END
   *   target : partner                     size  : ---
   *   tag : ---                            comm  : communicator
   *   aux: request
   */
	TRACE_MPIEVENT (TIME, PACX_IRECV_EV, EVT_END, src_world, count * size, tag, comm, hash_req.key);

  return ierror;
}



/******************************************************************************
 ***  PACX_Reduce_C_Wrapper
 ******************************************************************************/

int PACX_Reduce_C_Wrapper (void *sendbuf, void *recvbuf, int count,
                          PACX_Datatype datatype, PACX_Op op, int root, PACX_Comm comm)
{
	int me, ret, size;

	ret = PPACX_Comm_rank (comm, &me);
	PACX_CHECK(ret, PPACX_Comm_rank);

	ret = PPACX_Type_size (datatype, &size);
	PACX_CHECK(ret, PPACX_Type_size);

	size *= count;

	/* PACX_ Stats */
	GLOBAL_Communications ++;
	if (me == root)
		GLOBAL_Bytes_Recv += size;
	else
		GLOBAL_Bytes_Sent += size;

  /*
   *   event : REDUCE_EV                    value : EVT_BEGIN
   *   target: reduce operation ident.      size : bytes send (non root) /received (root)
   *   tag : rank                           commid: communicator Id
   *   aux : root rank
   */
	TRACE_MPIEVENT (TIME, PACX_REDUCE_EV, EVT_BEGIN, op, size, me, comm, root);

	ret = PPACX_Reduce (sendbuf, recvbuf, count, datatype, op, root, comm);

  /*
   *   event : REDUCE_EV                    value : EVT_END
   *   target : ---                         size  : ---
   *   tag : ---
   */
	TRACE_MPIEVENT (TIME, PACX_REDUCE_EV, EVT_END, EMPTY, EMPTY, EMPTY, comm, EMPTY);

	return ret;
}



/******************************************************************************
 ***  PACX_Allreduce_C_Wrapper
 ******************************************************************************/

int PACX_Allreduce_C_Wrapper (void *sendbuf, void *recvbuf, int count,
                             PACX_Datatype datatype, PACX_Op op, PACX_Comm comm)
{
  int me, ret, size;

  ret = PPACX_Comm_rank (comm, &me);
	PACX_CHECK(ret, PPACX_Comm_rank);

  ret = PPACX_Type_size (datatype, &size);
	PACX_CHECK(ret, PPACX_Type_size);

  size *= count;

  /* PACX_ Stats */
  GLOBAL_Communications ++;
  GLOBAL_Bytes_Sent += size;
  GLOBAL_Bytes_Recv += size;

  /*
   *   event : ALLREDUCE_EV                 value : EVT_BEGIN
   *   target: reduce operation ident.      size : bytes send and received
   *   tag : rank                           commid: communicator Id
   *   aux : ---
   */
  TRACE_MPIEVENT (TIME, PACX_ALLREDUCE_EV, EVT_BEGIN, op, size, me, comm,
  	PACX_CurrentOpGlobal);

  ret = PPACX_Allreduce (sendbuf, recvbuf, count, datatype, op, comm);

  /*
   *   event : ALLREDUCE_EV                 value : EVT_END
   *   target : ---                         size  : ---
   *   tag : ---
   */
  TRACE_MPIEVENT (TIME, PACX_ALLREDUCE_EV, EVT_END, EMPTY, EMPTY, EMPTY, comm,
		PACX_CurrentOpGlobal);

  return ret;
}



/******************************************************************************
 ***  PACX_Probe_C_Wrapper
 ******************************************************************************/

int PACX_Probe_C_Wrapper (int source, int tag, PACX_Comm comm, PACX_Status *status)
{
  int ierror;

  /*
   *   event : PROBE_EV                     value : EVT_BEGIN
   *   target : ---                         size  : ---
   *   tag : ---
   */
  TRACE_MPIEVENT (TIME, PACX_PROBE_EV, EVT_BEGIN, EMPTY, EMPTY, EMPTY, comm,
                  EMPTY);

  ierror = PPACX_Probe (source, tag, comm, status);

  /*
   *   event : PROBE_EV                     value : EVT_END
   *   target : ---                         size  : ---
   *   tag : ---
   */
  TRACE_MPIEVENT (TIME, PACX_PROBE_EV, EVT_END, EMPTY, EMPTY, EMPTY, comm, EMPTY);

  return ierror;
}



/******************************************************************************
 ***  PACX_Iprobe_C_Wrapper
 ******************************************************************************/

int Bursts_PACX_Iprobe_C_Wrapper (int source, int tag, PACX_Comm comm, int * flag, PACX_Status *status)
{
	int ierror;

     /*
      *   event : IPROBE_EV                     value : EVT_BEGIN
      *   target : ---                          size  : ---
      *   tag : ---
      */
	TRACE_MPIEVENT (TIME, PACX_IPROBE_EV, EVT_BEGIN, EMPTY, EMPTY, EMPTY, comm, EMPTY);

	ierror = PPACX_Iprobe (source, tag, comm, flag, status);

     /*
      *   event : IPROBE_EV                    value : EVT_END
      *   target : ---                         size  : ---
      *   tag : ---
      */
	TRACE_MPIEVENT (TIME, PACX_IPROBE_EV, EVT_END, EMPTY, EMPTY, EMPTY, comm, EMPTY);

	return ierror;
}

int Normal_PACX_Iprobe_C_Wrapper (int source, int tag, PACX_Comm comm, int *flag,
                          PACX_Status *status)
{
  static int IProbe_C_Software_Counter = 0;
  iotimer_t begin_time, end_time;
  static iotimer_t elapsed_time_outside_iprobes_C = 0, last_iprobe_C_exit_time = 0; 
  int ierror;

  begin_time = TIME;

  if (IProbe_C_Software_Counter == 0) {
    /* Primer Iprobe */
	elapsed_time_outside_iprobes_C = 0;
  }
  else {
	elapsed_time_outside_iprobes_C += (begin_time - last_iprobe_C_exit_time);
  }
  ierror = PPACX_Iprobe (source, tag, comm, flag, status);
  end_time = TIME;
  last_iprobe_C_exit_time = end_time;

	if (tracejant_mpi)
	{
    if (*flag)
    {
       /*
        *   event : IPROBE_EV                     value : EVT_BEGIN
        *   target : ---                          size  : ---
        *   tag : ---
        */
      if (IProbe_C_Software_Counter != 0) {
        TRACE_EVENT (begin_time, PACX_TIME_OUTSIDE_IPROBES_EV, elapsed_time_outside_iprobes_C);
        TRACE_EVENT (begin_time, PACX_IPROBE_COUNTER_EV, IProbe_C_Software_Counter);
      }

      TRACE_MPIEVENT (begin_time, PACX_IPROBE_EV, EVT_BEGIN, EMPTY, EMPTY, EMPTY, comm, EMPTY);
    
      /*
       *   event : IPROBE_EV                    value : EVT_END
       *   target : ---                         size  : ---
       *   tag : ---
       */
      TRACE_MPIEVENT (end_time, PACX_IPROBE_EV, EVT_END, EMPTY, EMPTY, EMPTY, comm, EMPTY);
      IProbe_C_Software_Counter = 0;
    } 
    else
    {
      if (IProbe_C_Software_Counter == 0) {
        /* El primer iprobe que falla */
        TRACE_EVENTANDCOUNTERS (begin_time, PACX_IPROBE_COUNTER_EV, 0, TRUE);
      }
      IProbe_C_Software_Counter ++;
    }
  }
  return ierror;
}

int PACX_Iprobe_C_Wrapper (int source, int tag, PACX_Comm comm, int * flag, PACX_Status *status)
{
   int ret;

   if (CURRENT_TRACE_MODE(THREADID) == TRACE_MODE_BURSTS)
   { 
      ret = Bursts_PACX_Iprobe_C_Wrapper (source, tag, comm, flag, status);
   } 
   else
   {
      ret = Normal_PACX_Iprobe_C_Wrapper (source, tag, comm, flag, status);
   }
   return ret;
}

/******************************************************************************
 ***  PACX_Barrier_C_Wrapper
 ******************************************************************************/

int PACX_Barrier_C_Wrapper (PACX_Comm comm)
{
  int me, ret;

  ret = PPACX_Comm_rank (comm, &me);
	PACX_CHECK(ret, PPACX_Comm_rank);

  /* PACX_ Stats */
  GLOBAL_Communications ++;

  /*
   *   event : BARRIER_EV                    value : EVT_BEGIN
   *   target : ---                          size  : ---
   *   tag : rank                            commid: comunicator id
   *   aux : ---
   */

#if defined(IS_BGL_MACHINE)
  if (!BGL_disable_barrier_inside)
  {
    TRACE_MPIEVENT (TIME, PACX_BARRIER_EV, EVT_BEGIN, EMPTY, EMPTY, me, comm,
                    PACX_CurrentOpGlobal);
  }
#else
  TRACE_MPIEVENT (TIME, PACX_BARRIER_EV, EVT_BEGIN, EMPTY, EMPTY, me, comm, 
  	PACX_CurrentOpGlobal);
#endif

  ret = PPACX_Barrier (comm);

  /*
   *   event : BARRIER_EV                   value : EVT_END
   *   target : ---                         size  : ---
   *   tag : ---
   */

#if defined(IS_BGL_MACHINE)
  if (!BGL_disable_barrier_inside)
  {
    TRACE_MPIEVENT (TIME, PACX_BARRIER_EV, EVT_END, EMPTY, EMPTY, EMPTY, comm,
                    PACX_CurrentOpGlobal);
  }
#else
  TRACE_MPIEVENT (TIME, PACX_BARRIER_EV, EVT_END, EMPTY, EMPTY, EMPTY, comm,
                  PACX_CurrentOpGlobal);
#endif

  return ret;
}



/******************************************************************************
 ***  PACX_Cancel_C_Wrapper
 ******************************************************************************/

int PACX_Cancel_C_Wrapper (PACX_Request *request)
{
  int ierror;

  /*
   *   event : CANCEL_EV                    value : EVT_BEGIN
   *   target : request to cancel           size  : ---
   *   tag : ---
   */
  TRACE_MPIEVENT (TIME, PACX_CANCEL_EV, EVT_BEGIN, *request, EMPTY, EMPTY, EMPTY, EMPTY);

	if (hash_search (&requests, *request) != NULL)
		hash_remove (&requests, *request);

  ierror = PPACX_Cancel (request);

  /*
   *   event : CANCEL_EV                    value : EVT_END
   *   target : request to cancel           size  : ---
   *   tag : ---
   */
  TRACE_MPIEVENT (TIME, PACX_CANCEL_EV, EVT_END, *request, EMPTY, EMPTY, EMPTY, EMPTY);

  return ierror;
}


/******************************************************************************
 ***  PACX_Test_C_Wrapper
 ******************************************************************************/

int Bursts_PACX_Test_C_Wrapper (PACX_Request *request, int *flag, PACX_Status *status)
{
	PACX_Request req;
	hash_data_t *hash_req;
	int src_world, size, tag, ret, ierror;
	iotimer_t temps_final;

  /*
   *   event : TEST_EV                      value : EVT_BEGIN
   *   target : request to test             size  : ---
   *   tag : ---
   */
  TRACE_MPIEVENT (TIME, PACX_TEST_EV, EVT_BEGIN, *request, EMPTY, EMPTY, EMPTY,
                  EMPTY);

	req = *request;

  ierror = PPACX_Test (request, flag, status);

  temps_final = TIME;

  if (ierror == MPI_SUCCESS && *flag && ((hash_req = hash_search (&requests, req)) != NULL))
  {
		if ((ret = get_Irank_obj_C (hash_req, &src_world, &size, &tag, status)) != MPI_SUCCESS)
			return ret;
    if (hash_req->group != PACX_GROUP_NULL)
    {
      ret = PPACX_Group_free (&hash_req->group);
			PACX_CHECK(ret, PPACX_Group_free);
    }

    P2P_Communications ++;
    P2P_Bytes_Recv += size; /* get_Irank_obj_C above returns size (number of bytes received) */

    /*
     *   event : IRECVED_EV                 value : ---
     *   target : sender                    size  : received message size
     *   tag : message tag                  commid: communicator identifier
     *   aux : request
     */
    TRACE_MPIEVENT_NOHWC (temps_final, PACX_IRECVED_EV, EMPTY, src_world, size, tag, hash_req->commid, req);
    hash_remove (&requests, req);
  }
  /*
   *   event : TEST_EV                    value : EVT_END
   *   target : ---                       size  : ---
   *   tag : ---
   */
  TRACE_MPIEVENT (temps_final, PACX_TEST_EV, EVT_END, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY);

  return ierror;
}

int Normal_PACX_Test_C_Wrapper (PACX_Request *request, int *flag, PACX_Status *status)
{
	PACX_Request req;
	hash_data_t *hash_req;
	int src_world, size, tag, ret, ierror;
	iotimer_t temps_inicial, temps_final;
	static int Test_C_Software_Counter = 0;

  temps_inicial = TIME;

	req = *request;

  ierror = PPACX_Test (request, flag, status);

  temps_final = TIME;

  if (ierror == MPI_SUCCESS && *flag && ((hash_req = hash_search (&requests, req)) != NULL))
  {
    /*
     *   event : TEST_EV                      value : EVT_BEGIN
     *   target : request to test             size  : ---
     *   tag : ---
     */
    if (Test_C_Software_Counter != 0) {
       TRACE_EVENT    (temps_inicial, PACX_TEST_COUNTER_EV, Test_C_Software_Counter);
    }
    TRACE_MPIEVENT (temps_inicial, PACX_TEST_EV, EVT_BEGIN, hash_req->key, EMPTY, EMPTY, EMPTY, EMPTY);
    Test_C_Software_Counter = 0;

		if ((ret = get_Irank_obj_C (hash_req, &src_world, &size, &tag, status)) != MPI_SUCCESS)
			return ret;
    if (hash_req->group != PACX_GROUP_NULL)
    {
      ret = PPACX_Group_free (&hash_req->group);
			PACX_CHECK(ret, PPACX_Group_free);
    }

    /*
     *   event : IRECVED_EV                 value : ---
     *   target : sender                    size  : received message size
     *   tag : message tag                  commid: communicator identifier
     *   aux : request
     */
    TRACE_MPIEVENT_NOHWC (temps_final, PACX_IRECVED_EV, EMPTY, src_world, size, tag, hash_req->commid, hash_req->key);
    hash_remove (&requests, req);
  
    /*
     *   event : TEST_EV                    value : EVT_END
     *   target : ---                       size  : ---
     *   tag : ---
     */
    TRACE_MPIEVENT (temps_final, PACX_TEST_EV, EVT_END, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY);
  }
  else {
    if (Test_C_Software_Counter == 0) {
      /* El primer test que falla */
      TRACE_EVENTANDCOUNTERS    (temps_inicial, PACX_TEST_COUNTER_EV, 0, TRUE);
    }     
    Test_C_Software_Counter ++;
  }
  return ierror;
}

int PACX_Test_C_Wrapper (PACX_Request *request, int *flag, PACX_Status *status)
{
	PACX_Status my_status, *ptr_status;
   int ret;

	ptr_status = (status == MPI_STATUS_IGNORE)?&my_status:status;
   
   if (CURRENT_TRACE_MODE(THREADID) == TRACE_MODE_BURSTS)
   {
      ret = Bursts_PACX_Test_C_Wrapper (request, flag, ptr_status);
   }
   else
   {
      ret = Normal_PACX_Test_C_Wrapper (request, flag, ptr_status);
   }
   return ret;
}

/******************************************************************************
 ***  PACX_Wait_C_Wrapper
 ******************************************************************************/

int PACX_Wait_C_Wrapper (PACX_Request *request, PACX_Status *status)
{
	PACX_Status my_status, *ptr_status;
	PACX_Request req;
  hash_data_t *hash_req;
  int src_world, size, tag, ret, ierror;
  iotimer_t temps_final;

  /*
   *   event : WAIT_EV                      value : EVT_BEGIN
   *   target : request to test             size  : ---
   *   tag : ---
   */
  TRACE_MPIEVENT (TIME, PACX_WAIT_EV, EVT_BEGIN, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY);

	req = *request;

	ptr_status = (MPI_STATUS_IGNORE == status)?&my_status:status;

  ierror = PPACX_Wait (request, ptr_status);

  temps_final = TIME;

  if (ierror == MPI_SUCCESS && ((hash_req = hash_search (&requests, req)) != NULL))
  {
		if ((ret = get_Irank_obj_C (hash_req, &src_world, &size, &tag, ptr_status)) != MPI_SUCCESS)
			return ret;
    if (hash_req->group != PACX_GROUP_NULL)
    {
      ret = PPACX_Group_free (&hash_req->group);
			PACX_CHECK(ret,PPACX_Group_free);
    }

    /* PACX_ Stats */
    P2P_Communications ++;
    P2P_Bytes_Recv += size; /* get_Irank_obj_C above returns size (number of bytes received) */

    /*
     *   event : IRECVED_EV                 value : ---
     *   target : sender                    size  : received message size
     *   tag : message tag                  commid: communicator identifier
     *   aux : request
     */
    TRACE_MPIEVENT_NOHWC (temps_final, PACX_IRECVED_EV, EMPTY, src_world, size, tag, hash_req->commid, hash_req->key);
    hash_remove (&requests, req);
  }

  /*
   *   event : WAIT_EV                    value : EVT_END
   *   target : ---                       size  : ---
   *   tag : ---
   */
  TRACE_MPIEVENT (temps_final, PACX_WAIT_EV, EVT_END, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY);

  return ierror;
}



/******************************************************************************
 ***  PACX_Waitall_C_Wrapper
 ******************************************************************************/

int PACX_Waitall_C_Wrapper (int count, PACX_Request *array_of_requests,
                           PACX_Status *array_of_statuses)
{
  PACX_Status my_statuses[MAX_WAIT_REQUESTS], *ptr_array_of_statuses;
  PACX_Request save_reqs[MAX_WAIT_REQUESTS];
  hash_data_t *hash_req;
  int src_world, size, tag, ret, ireq, ierror;
  iotimer_t temps_final;
#if defined(DEBUG_MPITRACE)
	int index;
#endif

  /*
   *   event : WAITALL_EV                      value : EVT_BEGIN
   *   target : ---                            size  : ---
   *   tag : ---
   */

  TRACE_MPIEVENT (TIME, PACX_WAITALL_EV, EVT_BEGIN, EMPTY, EMPTY, EMPTY, EMPTY,
                  EMPTY);

  /*
   * Arreglar-ho millor de cara a OMP
   */
  if (count > MAX_WAIT_REQUESTS)
    fprintf (stderr, "PANIC: too many requests in pacx_waitall\n");
  memcpy (save_reqs, array_of_requests, count * sizeof (PACX_Request));

#if defined(DEBUG_MPITRACE)
	fprintf (stderr, "MPITRACE %d: WAITALL summary\n", TASKID);
	for (index = 0; index < count; index++)
# if SIZEOF_LONG == 8
		fprintf (stderr, "%d: position %d -> request %lu\n", TASKID, index, (UINT64) array_of_requests[index]);
# elif SIZEOF_LONG == 4
		fprintf (stderr, "%d: position %d -> request %llu\n", TASKID, index, (UINT64) array_of_requests[index]);
# endif
#endif

  ptr_array_of_statuses = (MPI_STATUSES_IGNORE == array_of_statuses)?my_statuses:array_of_statuses;

  ierror = PPACX_Waitall (count, array_of_requests, ptr_array_of_statuses);

  temps_final = TIME;

  if (ierror == MPI_SUCCESS)
  {
    for (ireq = 0; ireq < count; ireq++)
    {
      if ((hash_req = hash_search (&requests, save_reqs[ireq])) != NULL)
      {
				if ((ret = get_Irank_obj_C (hash_req, &src_world, &size, &tag, &(ptr_array_of_statuses[ireq]))) != MPI_SUCCESS)
					return ret;

        if (hash_req->group != PACX_GROUP_NULL)
        {
          ret = PPACX_Group_free (&hash_req->group);
					PACX_CHECK(ret, PPACX_Group_free);
        }

        /* PACX_ Stats */
        P2P_Communications ++;
        P2P_Bytes_Recv += size; /* get_Irank_obj_C above returns size (number of bytes received) */

        /*
         *   event : IRECVED_EV                 value : ---
         *   target : sender                    size  : received message size
         *   tag : message tag                  commid: communicator identifier
         *   aux : request
         */
        TRACE_MPIEVENT_NOHWC (temps_final, PACX_IRECVED_EV, EMPTY, src_world, size, tag, hash_req->commid, hash_req->key);
        hash_remove (&requests, save_reqs[ireq]);
      }
    }
  }
  /*
   *   event : WAIT_EV                    value : EVT_END
   *   target : ---                       size  : ---
   *   tag : ---
   */
  TRACE_MPIEVENT (temps_final, PACX_WAITALL_EV, EVT_END, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY);

  return ierror;
}



/******************************************************************************
 ***  PACX_Waitany_C_Wrapper
 ******************************************************************************/

int PACX_Waitany_C_Wrapper (int count, PACX_Request *array_of_requests,
                           int *index, PACX_Status *status)
{
	PACX_Status my_status, *ptr_status;
  PACX_Request save_reqs[MAX_WAIT_REQUESTS];
  hash_data_t *hash_req;
  int src_world, size, tag, ret, ierror;
#if defined(DEBUG_MPITRACE)
  int i;
#endif
  iotimer_t temps_final;

  TRACE_MPIEVENT (TIME, PACX_WAITANY_EV, EVT_BEGIN, EMPTY, EMPTY, EMPTY, EMPTY,
                  EMPTY);

  if (count > MAX_WAIT_REQUESTS)
    fprintf (stderr, "PANIC: too many requests in pacx_waitany\n");
  memcpy (save_reqs, array_of_requests, count * sizeof (PACX_Request));

#if defined(DEBUG_MPITRACE)
	fprintf (stderr, "MPITRACE %d: WAITANY summary\n", TASKID);
	for (i = 0; i < count; i++)
# if SIZEOF_LONG == 8
		fprintf (stderr, "%d: position %d -> request %lu\n", TASKID, i, (UINT64) array_of_requests[i]);
# elif SIZEOF_LONG == 4
		fprintf (stderr, "%d: position %d -> request %llu\n", TASKID, i, (UINT64) array_of_requests[i]);
# endif
#endif

	ptr_status = (MPI_STATUS_IGNORE == status)?&my_status:status;

  ierror = PPACX_Waitany (count, array_of_requests, index, ptr_status);

  temps_final = TIME;

  if (*index != MPI_UNDEFINED && ierror == MPI_SUCCESS)
  {
    if ((hash_req = hash_search (&requests, save_reqs[*index])) != NULL)
    {
			if ((ret = get_Irank_obj_C (hash_req, &src_world, &size, &tag, ptr_status)) != MPI_SUCCESS)
				return ret;

      if (hash_req->group != PACX_GROUP_NULL)
      {
        ret = PPACX_Group_free (&hash_req->group);
				PACX_CHECK(ret, PPACX_Group_free);
      }

      /* PACX_ Stats */
      P2P_Communications ++;
      P2P_Bytes_Recv += size; /* get_Irank_obj_C above returns size (number of bytes received) */

      /*
       *   event : IRECVED_EV                 value : ---
       *   target : sender                    size  : received message size
       *   tag : message tag                  commid: communicator identifier
       *   aux : request
       */
      TRACE_MPIEVENT_NOHWC (temps_final, PACX_IRECVED_EV, EMPTY, src_world, size, tag, hash_req->commid, hash_req->key);
      hash_remove (&requests, save_reqs[*index]);
    }
  }

  TRACE_MPIEVENT (temps_final, PACX_WAITANY_EV, EVT_END, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY);

  return ierror;
}


/******************************************************************************
 ***  PACX_Waitsome_C_Wrapper
 ******************************************************************************/

int PACX_Waitsome_C_Wrapper (int incount, PACX_Request *array_of_requests,
                            int *outcount, int *array_of_indices,
                            PACX_Status *array_of_statuses)
{
  PACX_Status my_statuses[MAX_WAIT_REQUESTS], *ptr_array_of_statuses;
  PACX_Request save_reqs[MAX_WAIT_REQUESTS];
	UINT64 iireq;
  hash_data_t *hash_req;
  int src_world, size, tag, ret, ierror, ii;
  iotimer_t temps_final;
#if defined(DEBUG_MPITRACE)
	int index;
#endif

  /*
   * event : WAITSOME_EV                     value : EVT_BEGIN
   * target : ---                            size  : ---
   * tag : ---
   */
  TRACE_MPIEVENT (TIME, PACX_WAITSOME_EV, EVT_BEGIN, EMPTY, EMPTY, EMPTY, EMPTY,
                  EMPTY);

  /*
   * Arreglar-ho millor de cara a OMP
   */
  if (incount > MAX_WAIT_REQUESTS)
    fprintf (stderr, "PANIC: too many requests in pacx_waitsome\n");

  memcpy (save_reqs, array_of_requests, incount * sizeof (PACX_Request));

#if defined(DEBUG_MPITRACE)
	fprintf (stderr, "MPITRACE %d: WAITSOME summary\n", TASKID);
	for (index = 0; index < incount; index++)
# if SIZEOF_LONG == 8
		fprintf (stderr, "%d: position %d -> request %lu\n", TASKID, index, (UINT64) array_of_requests[index]);
# elif SIZEOF_LONG == 4
		fprintf (stderr, "%d: position %d -> request %llu\n", TASKID, index, (UINT64) array_of_requests[index]);
# endif
#endif

  ptr_array_of_statuses = (MPI_STATUSES_IGNORE == array_of_statuses)?my_statuses:array_of_statuses;

  ierror = PPACX_Waitsome (incount, array_of_requests, outcount, 
    array_of_indices, ptr_array_of_statuses);

  temps_final = TIME;

  if (ierror == MPI_SUCCESS)
  {
    for (ii = 0; ii < (*outcount); ii++)
    {
    	iireq = (long) save_reqs[array_of_indices[ii]];

      if ((hash_req = hash_search (&requests, save_reqs[array_of_indices[ii]])) != NULL)
      {
				if ((ret = get_Irank_obj_C (hash_req, &src_world, &size, &tag, &(ptr_array_of_statuses[ii]))) != MPI_SUCCESS)
					return ret;
        if (hash_req->group != PACX_GROUP_NULL)
        {
          ret = PPACX_Group_free (&hash_req->group);
					PACX_CHECK(ret, PPACX_Group_free);
        }

        /* PACX_ Stats */
        P2P_Communications ++;
        P2P_Bytes_Recv += size; /* get_Irank_obj_C above returns size (number of bytes received) */

        /*
         * event : IRECVED_EV                 value : ---
         * target : sender                    size  : received message size
         * tag : message tag                  commid: communicator identifier
         * aux : request
         */
        TRACE_MPIEVENT_NOHWC (temps_final, PACX_IRECVED_EV, EMPTY, src_world, size, tag, hash_req->commid, save_reqs[array_of_indices[ii]]);
        hash_remove (&requests, save_reqs[array_of_indices[ii]]);
      }
    }
  }
  /*
   * event : WAITSOME_EV                value : EVT_END
   * target : ---                       size  : ---
   * tag : ---
   */
  TRACE_MPIEVENT (temps_final, PACX_WAITSOME_EV, EVT_END, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY);

  return ierror;
}



/******************************************************************************
 ***  PACX_BCast_C_Wrapper
 ******************************************************************************/

int PACX_BCast_C_Wrapper (void *buffer, int count, PACX_Datatype datatype, int root,
                         PACX_Comm comm)
{
  int me, ret, size;

  ret = PPACX_Type_size (datatype, &size);
	PACX_CHECK(ret, PPACX_Type_size);

  size *= count;

  ret = PPACX_Comm_rank (comm, &me);
	PACX_CHECK(ret, PPACX_Comm_rank);

  /* PACX_ Stats */
  GLOBAL_Communications ++;
  if (me == root)
  {
     GLOBAL_Bytes_Sent += size;
  }
  else
  {
     GLOBAL_Bytes_Recv += size;
  }

  /*
   *   event : BCAST_EV                     value : EVT_BEGIN
   *   target : root_rank                   size  : message size
   *   tag : rank                           commid: communicator identifier
   *   aux : ---
   */

#if defined(IS_BGL_MACHINE)
  BGL_disable_barrier_inside = 1;
#endif

  TRACE_MPIEVENT (TIME, PACX_BCAST_EV, EVT_BEGIN, root, size, me, comm, 
  	PACX_CurrentOpGlobal);

  ret = PPACX_Bcast (buffer, count, datatype, root, comm);

  /*
   *   event : BCAST_EV                    value : EVT_END
   *   target : ---                         size  : ---
   *   tag : ---
   */
  TRACE_MPIEVENT (TIME, PACX_BCAST_EV, EVT_END, EMPTY, EMPTY, EMPTY, comm, 
  	PACX_CurrentOpGlobal);

#if defined(IS_BGL_MACHINE)
  BGL_disable_barrier_inside = 0;
#endif

  return ret;
}



/******************************************************************************
 ***  PACX_Alltoall_C_Wrapper
 ******************************************************************************/
/******************************************************************************
 **      Function name : pacx_alltoall stub function
 **
 **      Description : Marks the beginning and ending of the alltoall
 **                    operation.
 **
 **                 0 1 2 3 4 5         0 6 C I O U
 **                 6 7 8 9 A B         1 7 D J P V
 **                 C D E F G H   -->   2 8 E K Q W
 **                 I J K L M N         3 9 F L R X
 **                 O P Q R S T         4 A G M R Y
 **                 U V W X Y Z         5 B H N T Z
 **
 ******************************************************************************/

int PACX_Alltoall_C_Wrapper (void *sendbuf, int sendcount, PACX_Datatype sendtype,
  void *recvbuf, int recvcount, PACX_Datatype recvtype, PACX_Comm comm)
{
  int me, ret, sendsize, recvsize, nprocs;

  ret = PPACX_Type_size (sendtype, &sendsize);
	PACX_CHECK(ret, PPACX_Type_size);

  ret = PPACX_Type_size (recvtype, &recvsize);
	PACX_CHECK(ret, PPACX_Type_size);

  ret = PPACX_Comm_size (comm, &nprocs);
	PACX_CHECK(ret, PPACX_Comm_size);

  ret = PPACX_Comm_rank (comm, &me);
	PACX_CHECK(ret, PPACX_Comm_rank);

  /* PACX_ Stats */
  GLOBAL_Communications ++;
  GLOBAL_Bytes_Sent += sendcount * sendsize;
  GLOBAL_Bytes_Recv += recvcount * recvsize;

  /*
   *   event : ALLTOALL_EV                  value : EVT_BEGIN
   *   target : received size               size  : sent size
   *   tag : rank                           commid: communicator id
   *   aux : ---
   */
  TRACE_MPIEVENT (TIME, PACX_ALLTOALL_EV, EVT_BEGIN, recvcount * recvsize,
                  sendcount * sendsize, me, comm, PACX_CurrentOpGlobal);

  ret = PPACX_Alltoall (sendbuf, sendcount, sendtype, recvbuf, recvcount,
                       recvtype, comm);

  /*
   *   event : ALLTOALL_EV                  value : EVT_END
   *   target : ---                         size  : ---
   *   tag : ---
   */
  TRACE_MPIEVENT (TIME, PACX_ALLTOALL_EV, EVT_END, EMPTY, EMPTY, EMPTY, comm,
                  PACX_CurrentOpGlobal);

  return ret;
}



/******************************************************************************
 ***  PACX_Alltoallv_C_Wrapper
 ******************************************************************************/
/******************************************************************************
 **      Function name : pacx_alltoallv stub function
 **
 **      Description : Marks the beginning and ending of the alltoallv
 **                    operation.
 **
 **                 0 1 2 3 4 5         0 6 C I O U
 **                 6 7 8 9 A B         1 7 D J P V
 **                 C D E F G H   -->   2 8 E K Q W
 **                 I J K L M N         3 9 F L R X
 **                 O P Q R S T         4 A G M R Y
 **                 U V W X Y Z         5 B H N T Z
 **
 ******************************************************************************/

int PACX_Alltoallv_C_Wrapper (void *sendbuf, int *sendcounts, int *sdispls,
  PACX_Datatype sendtype, void *recvbuf, int *recvcounts, int *rdispls,
  PACX_Datatype recvtype, PACX_Comm comm)
{
  int me, ret, sendsize, recvsize, nprocs;
  int proc, sendc = 0, recvc = 0;

  ret = PPACX_Type_size (sendtype, &sendsize);
	PACX_CHECK(ret, PPACX_Type_size);

  ret = PPACX_Type_size (recvtype, &recvsize);
	PACX_CHECK(ret, PPACX_Type_size);

  ret = PPACX_Comm_size (comm, &nprocs);
	PACX_CHECK(ret, PPACX_Comm_size);

  ret = PPACX_Comm_rank (comm, &me);
	PACX_CHECK(ret, PPACX_Comm_rank);

	for (proc = 0; proc < nprocs; proc++)
	{
		if (sendcounts != NULL)
			sendc += sendcounts[proc];
		if (recvcounts != NULL)
			recvc += recvcounts[proc];
	}

  /* PACX_ Stats */
  GLOBAL_Communications ++;
  GLOBAL_Bytes_Sent += sendc * sendsize;
  GLOBAL_Bytes_Recv += recvc * recvsize;

  /*
   *   event : ALLTOALLV_EV                  value : EVT_BEGIN
   *   target : received size               size  : sent size
   *   tag : rank                           commid: communicator id
   *   aux : ---
   */
  TRACE_MPIEVENT (TIME, PACX_ALLTOALLV_EV, EVT_BEGIN, recvsize * recvc,
                  sendsize * sendc, me, comm, PACX_CurrentOpGlobal);

  ret = PPACX_Alltoallv (sendbuf, sendcounts, sdispls, sendtype,
                        recvbuf, recvcounts, rdispls, recvtype, comm);

  /*
   *   event : ALLTOALLV_EV                  value : EVT_END
   *   target : ---                         size  : ---
   *   tag : ---
   */
  TRACE_MPIEVENT (TIME, PACX_ALLTOALLV_EV, EVT_END, EMPTY, EMPTY, EMPTY, comm,
                  PACX_CurrentOpGlobal);


  return ret;
}



/******************************************************************************
 ***  PACX_Allgather_C_Wrapper
 ******************************************************************************/
/******************************************************************************
 **      Function name : pacx_allgather stub function
 **
 **      Description : Marks the beginning and ending of the gather
 **                    operation.
 **
 **                 1 - - - - -         1 2 3 4 5 6
 **                 2 - - - - -         1 2 3 4 5 6
 **                 3 - - - - -   -->   1 2 3 4 5 6
 **                 4 - - - - -         1 2 3 4 5 6
 **                 5 - - - - -         1 2 3 4 5 6
 **                 6 - - - - -         1 2 3 4 5 6
 **
 ******************************************************************************/

int PACX_Allgather_C_Wrapper (void *sendbuf, int sendcount, PACX_Datatype sendtype,
  void *recvbuf, int recvcount, PACX_Datatype recvtype, PACX_Comm comm)
{
  int ret, sendsize, recvsize, me, nprocs;

  ret = PPACX_Type_size (sendtype, &sendsize);
	PACX_CHECK(ret, PPACX_Type_size);

  ret = PPACX_Type_size (recvtype, &recvsize);
	PACX_CHECK(ret, PPACX_Type_size);

  ret = PPACX_Comm_size (comm, &nprocs);
	PACX_CHECK(ret, PPACX_Comm_size);

  ret = PPACX_Comm_rank (comm, &me);
	PACX_CHECK(ret, PPACX_Comm_rank);

  /* PACX_ Stats */
  GLOBAL_Communications ++;
  GLOBAL_Bytes_Sent += sendcount * sendsize;
  GLOBAL_Bytes_Recv += recvcount * recvsize;

  /*
   *   event : GATHER_EV                    value : EVT_BEGIN
   *   target : ---                         size  : bytes sent
   *   tag : rank                           commid: communicator identifier
   *   aux : bytes received
   */
  TRACE_MPIEVENT (TIME, PACX_ALLGATHER_EV, EVT_BEGIN, EMPTY, sendcount * sendsize,
                  me, comm, recvcount * recvsize * nprocs);

  ret = PPACX_Allgather (sendbuf, sendcount, sendtype,
                        recvbuf, recvcount, recvtype, comm);

  /*
   *   event : GATHER_EV                    value : EVT_END
   *   target : ---                         size  : ---
   *   tag : ---
   */
  TRACE_MPIEVENT (TIME, PACX_ALLGATHER_EV, EVT_END, EMPTY, EMPTY, EMPTY, EMPTY,
                  EMPTY);

  return ret;
}



/******************************************************************************
 ***  PACX_Allgatherv_C_Wrapper
 ******************************************************************************/
/******************************************************************************
 **      Function name : pacx_allgatherv stub function
 **
 **      Description : Marks the beginning and ending of the gather
 **                    operation.
 **
 **                 1 - - - - -         1 2 3 4 5 6
 **                 2 - - - - -         1 2 3 4 5 6
 **                 3 - - - - -   -->   1 2 3 4 5 6
 **                 4 - - - - -         1 2 3 4 5 6
 **                 5 - - - - -         1 2 3 4 5 6
 **                 6 - - - - -         1 2 3 4 5 6
 **
 ******************************************************************************/

int PACX_Allgatherv_C_Wrapper (void *sendbuf, int sendcount, PACX_Datatype sendtype,
  void *recvbuf, int *recvcounts, int *displs, PACX_Datatype recvtype, PACX_Comm comm)
{
  int ret, sendsize, me, nprocs;
  int proc, recvsize, recvc = 0;

  ret = PPACX_Type_size (sendtype, &sendsize);
	PACX_CHECK(ret, PPACX_Type_size);

  ret = PPACX_Type_size (recvtype, &recvsize);
	PACX_CHECK(ret, PPACX_Type_size);

  ret = PPACX_Comm_size (comm, &nprocs);
	PACX_CHECK(ret, PPACX_Comm_size);

  ret = PPACX_Comm_rank (comm, &me);
	PACX_CHECK(ret, PPACX_Comm_rank);

	if (recvcounts != NULL)
		for (proc = 0; proc < nprocs; proc++)
			recvc += recvcounts[proc];

  /* PACX_ Stats */
  GLOBAL_Communications ++;
  GLOBAL_Bytes_Sent += sendcount * sendsize;
  GLOBAL_Bytes_Recv += recvc * recvsize;

  /*
   *   event : GATHER_EV                    value : EVT_BEGIN
   *   target : ---                         size  : bytes sent
   *   tag : rank                           commid: communicator identifier
   *   aux : bytes received
   */
  TRACE_MPIEVENT (TIME, PACX_ALLGATHERV_EV, EVT_BEGIN, EMPTY, sendcount * sendsize,
                  me, comm, recvsize * recvc);

  ret = PPACX_Allgatherv (sendbuf, sendcount, sendtype,
                         recvbuf, recvcounts, displs, recvtype, comm);

  /*
   *   event : GATHER_EV                    value : EVT_END
   *   target : ---                         size  : ---
   *   tag : ---
   */
  TRACE_MPIEVENT (TIME, PACX_ALLGATHERV_EV, EVT_END, EMPTY, EMPTY, EMPTY, comm, EMPTY);

  return ret;
}



/******************************************************************************
 ***  PACX_Gather_C_Wrapper
 ******************************************************************************/
/******************************************************************************
 **      Function name : pacx_gather stub function
 **
 **      Description : Marks the beginning and ending of the gather
 **                    operation.
 **
 **                 X - - - - -         X X X X X X
 **                 X - - - - -         - - - - - -
 **                 X - - - - -   -->   - - - - - -
 **                 X - - - - -         - - - - - -
 **                 X - - - - -         - - - - - -
 **                 X - - - - -         - - - - - -
 **
 ******************************************************************************/

int PACX_Gather_C_Wrapper (void *sendbuf, int sendcount, PACX_Datatype sendtype,
  void *recvbuf, int recvcount, PACX_Datatype recvtype, int root, PACX_Comm comm)
{
  int ret, sendsize, recvsize, me, nprocs;

  ret = PPACX_Type_size (sendtype, &sendsize);
	PACX_CHECK(ret, PPACX_Type_size);

  ret = PPACX_Type_size (recvtype, &recvsize);
	PACX_CHECK(ret, PPACX_Type_size);

  ret = PPACX_Comm_size (comm, &nprocs);
	PACX_CHECK(ret, PPACX_Comm_size);

  ret = PPACX_Comm_rank (comm, &me);
	PACX_CHECK(ret, PPACX_Comm_rank);

  /* PACX_ Stats */
  GLOBAL_Communications ++;
  if (me == root)
  {
    GLOBAL_Bytes_Recv += recvcount * recvsize;
  }
  else
  {
    GLOBAL_Bytes_Sent += sendcount * sendsize;
  }

  /*
   *   event : GATHER_EV                    value : EVT_BEGIN
   *   target : root rank                   size  : bytes sent
   *   tag : rank                           commid: communicator identifier
   *   aux : bytes received
   */
  if (me == root)
  {
    TRACE_MPIEVENT (TIME, PACX_GATHER_EV, EVT_BEGIN, root, sendcount * sendsize,
                    me, comm, recvcount * recvsize * nprocs);
  }
  else
  {
    TRACE_MPIEVENT (TIME, PACX_GATHER_EV, EVT_BEGIN, root, sendcount * sendsize,
                    me, comm, 0);
  }

  ret = PPACX_Gather (sendbuf, sendcount, sendtype,
                     recvbuf, recvcount, recvtype, root, comm);

  /*
   *   event : GATHER_EV                    value : EVT_END
   *   target : ---                         size  : ---
   *   tag : ---
   */
  TRACE_MPIEVENT (TIME, PACX_GATHER_EV, EVT_END, EMPTY, EMPTY, EMPTY, comm, EMPTY);

  return ret;
}



/******************************************************************************
 ***  PACX_Gatherv_C_Wrapper
 ******************************************************************************/
/******************************************************************************
 **      Function name : pacx_gatherv stub function
 **
 **      Description : Marks the beginning and ending of the gatherv
 **                    operation.
 **
 **                 X - - - - -         X X X X X X
 **                 X - - - - -         - - - - - -
 **                 X - - - - -   -->   - - - - - -
 **                 X - - - - -         - - - - - -
 **                 X - - - - -         - - - - - -
 **                 X - - - - -         - - - - - -
 **
 ******************************************************************************/

int PACX_Gatherv_C_Wrapper (void *sendbuf, int sendcount, PACX_Datatype sendtype,
  void *recvbuf, int *recvcounts, int *displs, PACX_Datatype recvtype, int root,
  PACX_Comm comm)
{
  int ret, sendsize, me, nprocs;
  int proc, recvsize, recvc = 0;

  ret = PPACX_Type_size (sendtype, &sendsize);
	PACX_CHECK(ret, PPACX_Type_size);

  ret = PPACX_Type_size (recvtype, &recvsize);
	PACX_CHECK(ret, PPACX_Type_size);

  ret = PPACX_Comm_size (comm, &nprocs);
	PACX_CHECK(ret, PPACX_Comm_size);

  ret = PPACX_Comm_rank (comm, &me);
	PACX_CHECK(ret, PPACX_Comm_rank);

	if (recvcounts != NULL)
		for (proc = 0; proc < nprocs; proc++)
			recvc += recvcounts[proc];

  /* PACX_ Stats */
  GLOBAL_Communications ++;
  if (me == root)
  {
    GLOBAL_Bytes_Recv += recvc * recvsize;
  }
  else
  {
    GLOBAL_Bytes_Sent += sendcount * sendsize;
  }

  /*
   *   event : GATHERV_EV                   value : EVT_BEGIN
   *   target : root rank                   size  : bytes sent
   *   tag : rank                           commid: communicator identifier
   *   aux : bytes received
   */
  if (me == root)
  {
    TRACE_MPIEVENT (TIME, PACX_GATHERV_EV, EVT_BEGIN, root, sendcount * sendsize,
                    me, comm, recvsize * recvc);
  }
  else
  {
    TRACE_MPIEVENT (TIME, PACX_GATHERV_EV, EVT_BEGIN, root, sendcount * sendsize,
                    me, comm, 0);
  }

  ret = PPACX_Gatherv (sendbuf, sendcount, sendtype,
                      recvbuf, recvcounts, displs, recvtype, root, comm);

  /*
   *   event : GATHERV_EV                    value : EVT_END
   *   target : ---                         size  : ---
   *   tag : ---
   */
  TRACE_MPIEVENT (TIME, PACX_GATHERV_EV, EVT_END, EMPTY, EMPTY, EMPTY, comm, EMPTY);

  return ret;
}



/******************************************************************************
 ***  PACX_Scatter_C_Wrapper
 ******************************************************************************/
/******************************************************************************
 **      Function name : pacx_scatter stub function
 **
 **      Description : Marks the beginning and ending of the scatter
 **                    operation.
 **
 **                 X X X X X X         X - - - - -
 **                 - - - - - -         X - - - - -
 **                 - - - - - -   -->   X - - - - -
 **                 - - - - - -         X - - - - -
 **                 - - - - - -         X - - - - -
 **                 - - - - - -         X - - - - -
 **
 ******************************************************************************/

int PACX_Scatter_C_Wrapper (void *sendbuf, int sendcount, PACX_Datatype sendtype,
  void *recvbuf, int recvcount, PACX_Datatype recvtype, int root, PACX_Comm comm)
{
  int ret, sendsize, recvsize, me, nprocs;

  ret = PPACX_Type_size (sendtype, &sendsize);
	PACX_CHECK(ret, PPACX_Type_size);

  ret = PPACX_Type_size (recvtype, &recvsize);
	PACX_CHECK(ret, PPACX_Type_size);

  ret = PPACX_Comm_size (comm, &nprocs);
	PACX_CHECK(ret, PPACX_Comm_size);

  ret = PPACX_Comm_rank (comm, &me);
	PACX_CHECK(ret, PPACX_Comm_rank);

  /* PACX_ Stats */
  GLOBAL_Communications ++;
  if (me == root)
  {
    GLOBAL_Bytes_Sent += sendcount * sendsize;
  }
  else
  {
    GLOBAL_Bytes_Recv += recvcount * recvsize;
  }

  /*
   *   event : SCATTER_EV                   value : EVT_BEGIN
   *   target : root rank                   size  : bytes sent
   *   tag : rank                           commid: communicator identifier
   *   aux : bytes received
   */
  if (me == root)
  {
    TRACE_MPIEVENT (TIME, PACX_SCATTER_EV, EVT_BEGIN, root,
                    sendcount * sendsize * nprocs, me, comm,
                    recvcount * recvsize);
  }
  else
  {
    TRACE_MPIEVENT (TIME, PACX_SCATTER_EV, EVT_BEGIN, root, 0, me, comm,
                    recvcount * recvsize);
  }

  ret = PPACX_Scatter (sendbuf, sendcount, sendtype,
                      recvbuf, recvcount, recvtype, root, comm);

  /*
   *   event : SCATTER_EV                   value : EVT_END
   *   target : ---                         size  : ---
   *   tag : ---
   */
  TRACE_MPIEVENT (TIME, PACX_SCATTER_EV, EVT_END, EMPTY, EMPTY, EMPTY, comm, EMPTY);

  return ret;
}



/******************************************************************************
 ***  PACX_Scatterv_C_Wrapper
 ******************************************************************************/
/******************************************************************************
 **      Function name : pacx_scatterv stub function
 **
 **      Description : Marks the beginning and ending of the scatterv
 **                    operation.
 **
 **                 X X X X X X         X - - - - -
 **                 - - - - - -         X - - - - -
 **                 - - - - - -   -->   X - - - - -
 **                 - - - - - -         X - - - - -
 **                 - - - - - -         X - - - - -
 **                 - - - - - -         X - - - - -
 **
 ******************************************************************************/

int PACX_Scatterv_C_Wrapper (void *sendbuf, int *sendcounts, int *displs,
  PACX_Datatype sendtype, void *recvbuf, int recvcount, PACX_Datatype recvtype,
  int root, PACX_Comm comm)
{
  int ret, recvsize, me, nprocs;
  int proc, sendsize, sendc = 0;

  ret = PPACX_Type_size (sendtype, &sendsize);
	PACX_CHECK(ret, PPACX_Type_size);

  ret = PPACX_Type_size (recvtype, &recvsize);
	PACX_CHECK(ret, PPACX_Type_size);

  ret = PPACX_Comm_size (comm, &nprocs);
	PACX_CHECK(ret, PPACX_Comm_size);

  ret = PPACX_Comm_rank (comm, &me);
	PACX_CHECK(ret, PPACX_Comm_rank);

	if (sendcounts != NULL)
		for (proc = 0; proc < nprocs; proc++)
			sendc += sendcounts[proc];

  /* PACX_ Stats */
  GLOBAL_Communications ++;
  if (me == root)
  {
    GLOBAL_Bytes_Sent += sendc * sendsize;
  }
  else
  {
    GLOBAL_Bytes_Recv += recvcount * recvsize;
  }

  /*
   *   event :  SCATTERV_EV                 value : EVT_BEGIN
   *   target : root rank                   size  : bytes sent
   *   tag : rank                           commid: communicator identifier
   *   aux : bytes received
   */
  if (me == root)
  {
    TRACE_MPIEVENT (TIME, PACX_SCATTERV_EV, EVT_BEGIN, root, sendsize * sendc, me,
                    comm, recvcount * recvsize);
  }
  else
  {
    TRACE_MPIEVENT (TIME, PACX_SCATTERV_EV, EVT_BEGIN, root, 0, me, comm,
                    recvcount * recvsize);
  }

  ret = PPACX_Scatterv (sendbuf, sendcounts, displs, sendtype,
                       recvbuf, recvcount, recvtype, root, comm);

  /*
   *   event : SCATTERV_EV                  value : EVT_END
   *   target : ---                         size  : ---
   *   tag : ---
   */
  TRACE_MPIEVENT (TIME, PACX_SCATTERV_EV, EVT_END, EMPTY, EMPTY, EMPTY, comm, EMPTY);

  return ret;
}


/******************************************************************************
 ***  PACX_Comm_rank_C_Wrapper
 ******************************************************************************/

int PACX_Comm_rank_C_Wrapper (PACX_Comm comm, int *rank)
{
  int ierror;

  TRACE_MPIEVENT (TIME, PACX_COMM_RANK_EV, EVT_BEGIN, EMPTY, EMPTY, EMPTY, EMPTY,
                  EMPTY);
  ierror = PPACX_Comm_rank (comm, rank);
  TRACE_MPIEVENT (TIME, PACX_COMM_RANK_EV, EVT_END, EMPTY, EMPTY, EMPTY, EMPTY,
                  EMPTY);

  return ierror;
}



/******************************************************************************
 ***  PACX_Comm_size_C_Wrapper
 ******************************************************************************/

int PACX_Comm_size_C_Wrapper (PACX_Comm comm, int *size)
{
  int ierror;

  TRACE_MPIEVENT (TIME, PACX_COMM_SIZE_EV, EVT_BEGIN, EMPTY, EMPTY, EMPTY, EMPTY,
                  EMPTY);
  ierror = PPACX_Comm_size (comm, size);
  TRACE_MPIEVENT (TIME, PACX_COMM_SIZE_EV, EVT_END, EMPTY, EMPTY, EMPTY, EMPTY,
                  EMPTY);

  return ierror;
}


/******************************************************************************
 ***  PACX_Comm_create_C_Wrapper
 ******************************************************************************/

int PACX_Comm_create_C_Wrapper (PACX_Comm comm, PACX_Group group, PACX_Comm *newcomm)
{
  int ierror;

  ierror = PPACX_Comm_create (comm, group, newcomm);
  if (*newcomm != PACX_COMM_NULL && ierror == MPI_SUCCESS)
    Trace_PACX_Communicator (PACX_COMM_CREATE_EV, *newcomm);

  return ierror;
}


/******************************************************************************
 ***  PACX_Comm_dup_C_Wrapper
 ******************************************************************************/

int PACX_Comm_dup_C_Wrapper (PACX_Comm comm, PACX_Comm *newcomm)
{
  int ierror;

  ierror = PPACX_Comm_dup (comm, newcomm);
  if (*newcomm != PACX_COMM_NULL && ierror == MPI_SUCCESS)
    Trace_PACX_Communicator (PACX_COMM_DUP_EV, *newcomm);

  return ierror;
}


/******************************************************************************
 ***  PACX_Comm_split_C_Wrapper
 ******************************************************************************/

int PACX_Comm_split_C_Wrapper (PACX_Comm comm, int color, int key, PACX_Comm *newcomm)
{
  int ierror;

  ierror = PPACX_Comm_split (comm, color, key, newcomm);
  if (*newcomm != PACX_COMM_NULL && ierror == MPI_SUCCESS)
    Trace_PACX_Communicator (PACX_COMM_SPLIT_EV, *newcomm);

  return ierror;
}


/******************************************************************************
 ***  PACX_Reduce_Scatter_C_Wrapper
 ******************************************************************************/
/******************************************************************************
 **      Function name :  pacx_reduce_scatter stub function
 **
 **      Description : Marks the beginning and ending of the reduce operation.
 ******************************************************************************/

int PACX_Reduce_Scatter_C_Wrapper (void *sendbuf, void *recvbuf,
	int *recvcounts, PACX_Datatype datatype, PACX_Op op, PACX_Comm comm)
{
  int me, size, ierror;
  int i;
  int sendcount = 0;
  int csize;

  ierror = PPACX_Comm_rank (comm, &me);
	PACX_CHECK(ierror, PPACX_Comm_rank);

  ierror = PPACX_Type_size (datatype, &size);
	PACX_CHECK(ierror, PPACX_Type_size);

  ierror = PPACX_Comm_size (comm, &csize);
	PACX_CHECK(ierror, PPACX_Comm_size);

	if (recvcounts != NULL)
		for (i=0; i<csize; i++)
			sendcount += recvcounts[i];

  /* PACX_ Stats */
  GLOBAL_Communications ++;

  /* Reduce */
  if (me == 0)
  {
     GLOBAL_Bytes_Recv += sendcount * size;
  }
  else
  {
     GLOBAL_Bytes_Sent += sendcount * size;
  }

  /* Scatter */
  if (me == 0)
  {
     GLOBAL_Bytes_Sent += sendcount * size;
  }
  else
  {
     GLOBAL_Bytes_Recv += recvcounts[me] * size;
  }

  /*
   *   type : REDUCESCAT_EV                    value : EVT_BEGIN
   *   target : reduce operation ident.    size  : data size
   *   tag : whoami (comm rank)            comm : communicator id
   *   aux : ---
   */
  TRACE_MPIEVENT (TIME, PACX_REDUCESCAT_EV, EVT_BEGIN, op, size, me, comm, EMPTY);

  ierror = PPACX_Reduce_scatter (sendbuf, recvbuf, recvcounts, datatype,
                                op, comm);

  /*
   *   event : REDUCESCAT_EV                    value : EVT_END
   *   target : ---                         size  : ---
   *   tag : ---
   */
  TRACE_MPIEVENT (TIME, PACX_REDUCESCAT_EV, EVT_END, EMPTY, EMPTY, EMPTY, comm, EMPTY);
  return ierror;
}


/******************************************************************************
 ***  PACX_Scan_C_Wrapper
 ******************************************************************************/
/******************************************************************************
 **      Function name : pacx_scan  stub function
 **
 **      Description : Marks the beginning and ending of the scan operation.
 ******************************************************************************/

int PACX_Scan_C_Wrapper (void *sendbuf, void *recvbuf, int count,
                        PACX_Datatype datatype, PACX_Op op, PACX_Comm comm)
{
  int me, ierror, size;
  int csize;

  ierror = PACX_Comm_rank (comm, &me);
	PACX_CHECK(ierror, PACX_Comm_rank);

  ierror = PPACX_Type_size (datatype, &size);
	PACX_CHECK(ierror, PPACX_Type_size);

  /* PACX_ Stats */
  GLOBAL_Communications ++;

  ierror = PPACX_Comm_size (comm, &csize);
	PACX_CHECK(ierror, PPACX_Comm_size);

  if (me != csize - 1)
  {
    GLOBAL_Bytes_Sent = count * size;
  }
  if (me != 0)
  {
    GLOBAL_Bytes_Recv = count * size;
  }

  /*
   *   type : SCAN_EV                    value : EVT_BEGIN
   *   target : reduce operation ident.    size  : data size
   *   tag : whoami (comm rank)            comm : communicator id
   *   aux : ---
   */
  TRACE_MPIEVENT (TIME, PACX_SCAN_EV, EVT_BEGIN, op, count * size, me, comm,
                  PACX_CurrentOpGlobal);

  ierror = PPACX_Scan (sendbuf, recvbuf, count, datatype, op, comm);

  /*
   *   event : REDUCESCAT_EV                    value : EVT_END
   *   target : ---                         size  : ---
   *   tag : ---
   */
  TRACE_MPIEVENT (TIME, PACX_SCAN_EV, EVT_END, EMPTY, EMPTY, EMPTY, comm, 
  	PACX_CurrentOpGlobal);

  return ierror;
}

/******************************************************************************
 ***  PACX_Cart_create
 ******************************************************************************/
int PACX_Cart_create_C_Wrapper (PACX_Comm comm_old, int ndims, int *dims,
                               int *periods, int reorder, PACX_Comm *comm_cart)
{
  int ierror = PPACX_Cart_create (comm_old, ndims, dims, periods, reorder,
                                 comm_cart);

  if (ierror == MPI_SUCCESS && *comm_cart != PACX_COMM_NULL)
    Trace_PACX_Communicator (PACX_CART_CREATE_EV, *comm_cart);

  return ierror;
}

/* -------------------------------------------------------------------------
   PACX_Cart_sub
   ------------------------------------------------------------------------- */
int PACX_Cart_sub_C_Wrapper (PACX_Comm comm, int *remain_dims, PACX_Comm *comm_new)
{
  int ierror = PPACX_Cart_sub (comm, remain_dims, comm_new);

  if (ierror == MPI_SUCCESS && *comm_new != PACX_COMM_NULL)
    Trace_PACX_Communicator (PACX_CART_SUB_EV, *comm_new);

  return ierror;
}



/******************************************************************************
 ***  PACX_Start_C_Wrapper
 ******************************************************************************/

int PACX_Start_C_Wrapper (PACX_Request *request)
{
  int ierror;
  iotimer_t temps = TIME;

  /*
   *   type : START_EV                     value : EVT_BEGIN
   *   target : ---                        size  : ----
   *   tag : ---                           comm : ---
   *   aux : ---
   */
  TRACE_MPIEVENT (temps, PACX_START_EV, EVT_BEGIN, EMPTY, EMPTY, EMPTY, EMPTY,
                  EMPTY);

  /* Primer cal fer la crida real */
  ierror = PPACX_Start (request);

  /* S'intenta tracejar aquesta request */
  Traceja_Persistent_Request (request, temps);

  /*
   *   type : START_EV                     value : EVT_END
   *   target : ---                        size  : ----
   *   tag : ---                           comm : ---
   *   aux : ---
   */
  TRACE_MPIEVENT (TIME, PACX_START_EV, EVT_END, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY);
  return ierror;
}


/******************************************************************************
 ***  PACX_Startall_C_Wrapper
 ******************************************************************************/

int PACX_Startall_C_Wrapper (int count, PACX_Request *array_of_requests)
{
  PACX_Request save_reqs[MAX_WAIT_REQUESTS];
  int ii, ierror;
  iotimer_t temps = TIME;

  /*
   *   type : START_EV                     value : EVT_BEGIN
   *   target : ---                        size  : ----
   *   tag : ---                           comm : ---
   *   aux : ---
   */
  TRACE_MPIEVENT (temps, PACX_STARTALL_EV, EVT_BEGIN, EMPTY, EMPTY, EMPTY, EMPTY,
                  EMPTY);

  /*
   * Algunes implementacions es poden carregar aquesta informacio.
   * Cal salvar-la per poder tracejar després de fer la crida pmpi. 
   */
  memcpy (save_reqs, array_of_requests, count * sizeof (PACX_Request));

  /* Primer cal fer la crida real */
  ierror = PPACX_Startall (count, array_of_requests);

  /* Es tracejen totes les requests */
  for (ii = 0; ii < count; ii++)
    Traceja_Persistent_Request (&(save_reqs[ii]), temps);

  /*
   *   type : START_EV                     value : EVT_END
   *   target : ---                        size  : ----
   *   tag : ---                           comm : ---
   *   aux : ---
   */
  TRACE_MPIEVENT (TIME, PACX_STARTALL_EV, EVT_END, EMPTY, EMPTY, EMPTY, EMPTY,
                  EMPTY);
  return ierror;
}


/******************************************************************************
 ***  PACX_Request_free_C_Wrapper
 ******************************************************************************/
int PACX_Request_free_C_Wrapper (PACX_Request *request)
{
  int ierror;

  /*
   *   type : START_EV                     value : EVT_BEGIN
   *   target : ---                        size  : ----
   *   tag : ---                           comm : ---
   *   aux : ---
   */
  TRACE_MPIEVENT (TIME, PACX_REQUEST_FREE_EV, EVT_BEGIN, EMPTY, EMPTY, EMPTY,
                  EMPTY, EMPTY);

  /* Free from our structures */
  PR_Elimina_request (&PR_queue, request);

  /* Perform the real call */
  ierror = PPACX_Request_free (request);

  /*
   *   type : START_EV                     value : EVT_END
   *   target : ---                        size  : ----
   *   tag : ---                           comm : ---
   *   aux : ---
   */
  TRACE_MPIEVENT (TIME, PACX_REQUEST_FREE_EV, EVT_END, EMPTY, EMPTY, EMPTY, EMPTY,
                  EMPTY);
  return ierror;
}


/******************************************************************************
 ***  PACX_Recv_init_C_Wrapper
 ******************************************************************************/

int PACX_Recv_init_C_Wrapper (void *buf, int count, PACX_Datatype datatype, int source,
                             int tag, PACX_Comm comm, PACX_Request *request)
{
  int ierror;

  /*
   *   type : RECV_INIT_EV                 value : EVT_BEGIN
   *   target : ---                        size  : ----
   *   tag : ---                           comm : ---
   *   aux : ---
   */
  TRACE_MPIEVENT (TIME, PACX_RECV_INIT_EV, EVT_BEGIN, EMPTY, EMPTY, EMPTY, EMPTY,
                  EMPTY);

  /*
   * Primer cal fer la crida real 
   */
  ierror = PPACX_Recv_init (buf, count, datatype, source, tag, comm,
    request);

  /*
   * Es guarda aquesta request 
   */
	PR_NewRequest (PACX_IRECV_EV, buf, count, datatype, source, tag, comm, *request,
                 &PR_queue);

  /*
   *   type : RECV_INIT_EV                 value : EVT_END
   *   target : ---                        size  : ----
   *   tag : ---                           comm : ---
   *   aux : ---
   */
  TRACE_MPIEVENT (TIME, PACX_RECV_INIT_EV, EVT_END, EMPTY, EMPTY, EMPTY, EMPTY,
                  EMPTY);
  return ierror;
}


/******************************************************************************
 ***  PACX_Send_init_C_Wrapper
 ******************************************************************************/

int PACX_Send_init_C_Wrapper (void *buf, int count, PACX_Datatype datatype, int dest,
                             int tag, PACX_Comm comm, PACX_Request *request)
{
  int ierror;

  /*
   *   type : SEND_INIT_EV                 value : EVT_BEGIN
   *   target : ---                        size  : ----
   *   tag : ---                           comm : ---
   *   aux : ---
   */
  TRACE_MPIEVENT (TIME, PACX_SEND_INIT_EV, EVT_BEGIN, EMPTY, EMPTY, EMPTY, EMPTY,
                  EMPTY);

  /*
   * Primer cal fer la crida real 
   */
  ierror = PPACX_Send_init (buf, count, datatype, dest, tag, comm,
    request);

  /*
   * Es guarda aquesta request 
   */
	PR_NewRequest (PACX_ISEND_EV, buf, count, datatype, dest, tag, comm, *request,
                 &PR_queue);

  /*
   *   type : SEND_INIT_EV                 value : EVT_END
   *   target : ---                        size  : ----
   *   tag : ---                           comm : ---
   *   aux : ---
   */
  TRACE_MPIEVENT (TIME, PACX_SEND_INIT_EV, EVT_END, EMPTY, EMPTY, EMPTY, EMPTY,
                  EMPTY);
  return ierror;
}


/******************************************************************************
 ***  PACX_Bsend_init_C_Wrapper
 ******************************************************************************/

int PACX_Bsend_init_C_Wrapper (void *buf, int count, PACX_Datatype datatype, int dest,
                              int tag, PACX_Comm comm, PACX_Request *request)
{
  int ierror;

  /*
   *   type : BSEND_INIT_EV                value : EVT_BEGIN
   *   target : ---                        size  : ----
   *   tag : ---                           comm : ---
   *   aux : ---
   */
  TRACE_MPIEVENT (TIME, PACX_BSEND_INIT_EV, EVT_BEGIN, EMPTY, EMPTY, EMPTY, EMPTY,
                  EMPTY);

  /*
   * Primer cal fer la crida real 
   */
  ierror = PPACX_Bsend_init (buf, count, datatype, dest, tag, comm,
    request);

  /*
   * Es guarda aquesta request 
   */
	PR_NewRequest (PACX_IBSEND_EV, buf, count, datatype, dest, tag, comm, *request,
                 &PR_queue);

  /*
   *   type : BSEND_INIT_EV                value : EVT_END
   *   target : ---                        size  : ----
   *   tag : ---                           comm : ---
   *   aux : ---
   */
  TRACE_MPIEVENT (TIME, PACX_BSEND_INIT_EV, EVT_END, EMPTY, EMPTY, EMPTY, EMPTY,
                  EMPTY);
  return ierror;
}


/******************************************************************************
 ***  PACX_Rsend_init_C_Wrapper
 ******************************************************************************/

int PACX_Rsend_init_C_Wrapper (void *buf, int count, PACX_Datatype datatype, int dest,
                              int tag, PACX_Comm comm, PACX_Request *request)
{
  int ierror;

  /*
   *   type : RSEND_INIT_EV                value : EVT_BEGIN
   *   target : ---                        size  : ----
   *   tag : ---                           comm : ---
   *   aux : ---
   */
  TRACE_MPIEVENT (TIME, PACX_RSEND_INIT_EV, EVT_BEGIN, EMPTY, EMPTY, EMPTY, EMPTY,
                  EMPTY);

  /*
   * Primer cal fer la crida real 
   */
  ierror = PPACX_Rsend_init (buf, count, datatype, dest, tag, comm,
    request);

  /*
   * Es guarda aquesta request 
   */
	PR_NewRequest (PACX_IRSEND_EV, buf, count, datatype, dest, tag, comm, *request,
                 &PR_queue);

  /*
   *   type : RSEND_INIT_EV                value : EVT_END
   *   target : ---                        size  : ----
   *   tag : ---                           comm : ---
   *   aux : ---
   */
  TRACE_MPIEVENT (TIME, PACX_RSEND_INIT_EV, EVT_END, EMPTY, EMPTY, EMPTY, EMPTY,
                  EMPTY);
  return ierror;
}


/******************************************************************************
 ***  PACX_Ssend_init_C_Wrapper
 ******************************************************************************/

int PACX_Ssend_init_C_Wrapper (void *buf, int count, PACX_Datatype datatype, int dest,
                              int tag, PACX_Comm comm, PACX_Request *request)
{
	int ierror;

	/*
	*   type : SSEND_INIT_EV                value : EVT_BEGIN
	*   target : ---                        size  : ----
	*   tag : ---                           comm : ---
	*   aux : ---
	*/
	TRACE_MPIEVENT (TIME, PACX_SSEND_INIT_EV, EVT_BEGIN, EMPTY, EMPTY, EMPTY, EMPTY,
	  EMPTY);

	/*
	* Primer cal fer la crida real 
	*/
	ierror = PPACX_Ssend_init (buf, count, datatype, dest, tag, comm,
	  request);

	/*
	 * Es guarda aquesta request 
	 */
	PR_NewRequest (PACX_ISSEND_EV, buf, count, datatype, dest, tag, comm, *request,
                 &PR_queue);

	/*
	 *   type : SSEND_INIT_EV                value : EVT_END
	 *   target : ---                        size  : ----
	 *   tag : ---                           comm : ---
	 *   aux : ---
	 */
	TRACE_MPIEVENT (TIME, PACX_SSEND_INIT_EV, EVT_END, EMPTY, EMPTY, EMPTY, EMPTY,
	  EMPTY);

	return ierror;
}


int PACX_Sendrecv_C_Wrapper (void *sendbuf, int sendcount, PACX_Datatype sendtype,
	int dest, int sendtag, void *recvbuf, int recvcount, PACX_Datatype recvtype,
	int source, int recvtag, PACX_Comm comm, PACX_Status * status) 
{
	PACX_Status my_status, *ptr_status;
	PACX_Datatype DataSendType = sendtype, DataRecvType = recvtype;
	int ierror, ret;
	int DataSendSize, DataRecvSize, DataSend, DataSize;
	int SendRank, SourceRank, RecvRank, Count, Tag;

	if ((ret = get_rank_obj_C (comm, dest, &RecvRank)) != MPI_SUCCESS)
		return ret;

	ret = PPACX_Type_size (DataSendType, (int *) &DataSendSize);
	PACX_CHECK(ret, PPACX_Type_size);

	ret = PPACX_Type_size (DataRecvType, (int *) &DataRecvSize);
	PACX_CHECK(ret, PPACX_Type_size);

	DataSend = sendcount * DataSendSize;

	TRACE_MPIEVENT (TIME, PACX_SENDRECV_EV, EVT_BEGIN, RecvRank, DataSend, sendtag,
		comm, EMPTY);

	ptr_status = (status == MPI_STATUS_IGNORE)?&my_status:status;

	ierror = PPACX_Sendrecv (sendbuf, sendcount, sendtype, dest, sendtag,
		recvbuf, recvcount, recvtype, source, recvtag, comm, ptr_status);

	ret = PPACX_Get_count (ptr_status, DataRecvType, &Count);
	PACX_CHECK(ret, PPACX_Get_count);

	if (Count != MPI_UNDEFINED)
		DataSize = DataRecvSize * Count;
	else
		DataSize = 0;

	SendRank = ptr_status->MPI_SOURCE;
	Tag = ptr_status->MPI_TAG;

	/* PACX_ Stats */
	P2P_Communications ++;
	P2P_Bytes_Sent += DataSend;
	P2P_Bytes_Recv += DataSize;

	if ((ret = get_rank_obj_C (comm, SendRank, &SourceRank)) != MPI_SUCCESS)
		return ret;

	TRACE_MPIEVENT (TIME, PACX_SENDRECV_EV, EVT_END, SourceRank, DataSize, Tag, comm,
	  EMPTY);

	return ierror;
}

int PACX_Sendrecv_replace_C_Wrapper (void *buf, int count, PACX_Datatype type,
  int dest, int sendtag, int source, int recvtag, PACX_Comm comm,
  PACX_Status * status) 
{
	PACX_Status my_status, *ptr_status;
	PACX_Datatype DataSendType = type, DataRecvType = type;
	int ierror, ret;
	int DataSendSize, DataRecvSize, DataSend, DataSize;
	int SendRank, SourceRank, RecvRank, Count, Tag;

	if ((ret = get_rank_obj_C (comm, dest, &RecvRank)) != MPI_SUCCESS)
		return ret;

	ret = PPACX_Type_size (DataSendType, (int *) &DataSendSize);
	PACX_CHECK(ret, PPACX_Type_size);

	DataRecvSize = DataSendSize;

	DataSend = count * DataSendSize;

	TRACE_MPIEVENT (TIME, PACX_SENDRECV_REPLACE_EV, EVT_BEGIN, RecvRank, DataSend,
	  sendtag, comm, EMPTY);

	ptr_status = (status == MPI_STATUS_IGNORE)?&my_status:status;

	ierror = PPACX_Sendrecv_replace (buf, count, type, dest, sendtag, source,
	  recvtag, comm, ptr_status);

	ret = PPACX_Get_count (status, DataRecvType, &Count);
	PACX_CHECK(ret, PPACX_Get_count);

	if (Count != MPI_UNDEFINED)
		DataSize = DataRecvSize * Count;
	else
		DataSize = 0;

	SendRank = ptr_status->MPI_SOURCE;
	Tag = ptr_status->MPI_TAG;

	/* PACX_ Stats */
	P2P_Communications ++;
	P2P_Bytes_Sent += DataSend;
	P2P_Bytes_Recv += DataSize;

	if ((ret = get_rank_obj_C (comm, SendRank, &SourceRank)) != MPI_SUCCESS)
		return ret;

	TRACE_MPIEVENT (TIME, PACX_SENDRECV_REPLACE_EV, EVT_END, SourceRank, DataSize,
	  Tag, comm, EMPTY);

	return ierror;
}

#if defined(PACX_SUPPORTS_PACX_IO)

/*************************************************************
 **********************      MPIIO      **********************
 *************************************************************/

int PACX_File_open_C_Wrapper (PACX_Comm comm, char * filename, int amode, PACX_Info info, PACX_File *fh)
{
	int ierror;

	TRACE_MPIEVENT (TIME, PACX_FILE_OPEN_EV, EVT_BEGIN, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY); 
	ierror = PPACX_File_open (comm, filename, amode, info, fh);
	TRACE_MPIEVENT (TIME, PACX_FILE_OPEN_EV, EVT_END, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY);

	return ierror;
}

int PACX_File_close_C_Wrapper (PACX_File *fh)
{
	int ierror;

	TRACE_MPIEVENT (TIME, PACX_FILE_CLOSE_EV, EVT_BEGIN, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY);
	ierror = PPACX_File_close (fh);
	TRACE_MPIEVENT (TIME, PACX_FILE_CLOSE_EV, EVT_END, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY);

	return ierror;
}

int PACX_File_read_C_Wrapper (PACX_File fh, void * buf, int count, PACX_Datatype datatype, PACX_Status *status)
{
	int ierror;

	TRACE_MPIEVENT (TIME, PACX_FILE_READ_EV, EVT_BEGIN, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY);
	ierror = PPACX_File_read (fh, buf, count, datatype, status);
	TRACE_MPIEVENT (TIME, PACX_FILE_READ_EV, EVT_END, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY);

	return ierror;
}

int PACX_File_read_all_C_Wrapper (PACX_File fh, void * buf, int count, PACX_Datatype datatype, PACX_Status *status)
{
	int ierror;

	TRACE_MPIEVENT (TIME, PACX_FILE_READ_ALL_EV, EVT_BEGIN, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY);
	ierror = PPACX_File_read_all (fh, buf, count, datatype, status);
	TRACE_MPIEVENT (TIME, PACX_FILE_READ_ALL_EV, EVT_END, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY);

	return ierror;
}

int PACX_File_write_C_Wrapper (PACX_File fh, void * buf, int count, PACX_Datatype datatype, PACX_Status *status)
{
	int ierror;

	TRACE_MPIEVENT (TIME, PACX_FILE_WRITE_EV, EVT_BEGIN, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY);
	ierror = PPACX_File_write (fh, buf, count, datatype, status);
	TRACE_MPIEVENT (TIME, PACX_FILE_WRITE_EV, EVT_END, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY);

	return ierror;
}

int PACX_File_write_all_C_Wrapper (PACX_File fh, void * buf, int count, PACX_Datatype datatype, PACX_Status *status)
{
	int ierror;

	TRACE_MPIEVENT (TIME, PACX_FILE_WRITE_ALL_EV, EVT_BEGIN, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY);
	ierror = PPACX_File_write_all (fh, buf, count, datatype, status);
	TRACE_MPIEVENT (TIME, PACX_FILE_WRITE_ALL_EV, EVT_END, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY);

	return ierror;
}

int PACX_File_read_at_C_Wrapper (PACX_File fh, PACX_Offset offset, void * buf, int count, PACX_Datatype datatype, PACX_Status *status)
{
	int ierror;

	TRACE_MPIEVENT (TIME, PACX_FILE_READ_AT_EV, EVT_BEGIN, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY);
	ierror = PPACX_File_read_at (fh, offset, buf, count, datatype, status);
	TRACE_MPIEVENT (TIME, PACX_FILE_READ_AT_EV, EVT_END, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY);

	return ierror;
}

int PACX_File_read_at_all_C_Wrapper (PACX_File fh, PACX_Offset offset, void * buf, int count, PACX_Datatype datatype, PACX_Status *status)
{
	int ierror;

	TRACE_MPIEVENT (TIME, PACX_FILE_READ_AT_ALL_EV, EVT_BEGIN, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY);
	ierror = PPACX_File_read_at_all (fh, offset, buf, count, datatype, status);
	TRACE_MPIEVENT (TIME, PACX_FILE_READ_AT_ALL_EV, EVT_END, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY);

	return ierror;
}

int PACX_File_write_at_C_Wrapper (PACX_File fh, PACX_Offset offset, void * buf, int count, PACX_Datatype datatype, PACX_Status* status)
{
	int ierror;

	TRACE_MPIEVENT (TIME, PACX_FILE_WRITE_AT_EV, EVT_BEGIN, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY);
	ierror = PPACX_File_write_at (fh, offset, buf, count, datatype, status);
	TRACE_MPIEVENT (TIME, PACX_FILE_WRITE_AT_EV, EVT_END, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY);

	return ierror;
}

int PACX_File_write_at_all_C_Wrapper (PACX_File fh, PACX_Offset offset, void * buf, int count, PACX_Datatype datatype, PACX_Status* status)
{
	int ierror;

	TRACE_MPIEVENT (TIME, PACX_FILE_WRITE_AT_ALL_EV, EVT_BEGIN, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY);
	ierror = PPACX_File_write_at_all (fh, offset, buf, count, datatype, status);
	TRACE_MPIEVENT (TIME, PACX_FILE_WRITE_AT_ALL_EV, EVT_END, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY);

	return ierror;
}

#endif /* PACX_SUPPORTS_PACX_IO */

#endif /* defined(C_SYMBOLS) */

/****************************************************************************
 *** Gather_MPITS
 ****************************************************************************/

enum
{
	WAKE_UP_TAG,
	MPIT_NAME_LENGTH_TAG,
	MPIT_NAME_TAG,
	MPIT_SIZE_TAG,
	START_SENDING_TAG,
	MPIT_CONTENT_TAG,
	DELETE_MPIT_TAG
};

#define MPIT_CHUNK_SIZE 1024 * 1024


static void Gather_MPITS(void) 
{
	int wake_up = 0;
	int start_sending;
	int confirm_delete = 0;
	int remaining_bytes;
	int mpit_fd;
	int mpit_name_len;
	int mpit_size;
	int pacx_err;
	int hostname_length;
	char hostname[MPI_MAX_PROCESSOR_NAME];
	char mpit_content[MPIT_CHUNK_SIZE];
	PACX_Status sts;

	/* Synchronize all tasks */
	pacx_err = PPACX_Barrier(PACX_COMM_WORLD);
	PACX_CHECK(pacx_err, PPACX_Barrier);

	/* Retrieve the host name */
	pacx_err = PMPI_Get_processor_name (hostname, &hostname_length);
	PACX_CHECK(pacx_err, PMPI_Get_processor_name);

	if (TASKID == 0) /* MASTER side */
	{
		int slave;

		fprintf (stdout, "mpitrace: Gathering mpits in master node %s (%s)\n", hostname, final_dir);
		
		wake_up = 1;
		for (slave = 1; slave < NumOfTasks; slave ++)
		{
			char * mpit_name;
		
			/* Wake up slave */
			pacx_err = PPACX_Send(&wake_up, 1, PACX_INT, slave, WAKE_UP_TAG, PACX_COMM_WORLD);
			PACX_CHECK(pacx_err, PPACX_Send);

			/* Retrieve the MPIT name and size from the slave */
			pacx_err = PPACX_Recv(&mpit_name_len, 1, PACX_INT, slave, MPIT_NAME_LENGTH_TAG, PACX_COMM_WORLD, &sts);
			PACX_CHECK(pacx_err, PPACX_Recv);

			mpit_name = (char *)malloc((mpit_name_len+1)*sizeof(char));
			if (mpit_name == NULL)
			{
				fprintf (stderr, "mpitrace: Error while allocating memory for mpit_name (requested: %u bytes)\n", (mpit_name_len+1)*sizeof(char));
				exit(-1);
			}
			pacx_err = PPACX_Recv(mpit_name, mpit_name_len, PACX_CHAR, slave, MPIT_NAME_TAG, PACX_COMM_WORLD, &sts);
			PACX_CHECK(pacx_err, PPACX_Recv);
			mpit_name[mpit_name_len] = '\0';

			pacx_err = PPACX_Recv(&mpit_size, 1, PACX_INT, slave, MPIT_SIZE_TAG, PACX_COMM_WORLD, &sts);
			PACX_CHECK(pacx_err, PPACX_Recv);

			fprintf (stdout, "mpitrace: Asking task %d for %s (%d bytes)\n", slave, mpit_name, mpit_size);

			/* Check whether this mpit is already at the master node */
			mpit_fd = open(mpit_name, O_RDONLY);
			if ((mpit_fd != -1) && (mpit_size == lseek(mpit_fd, 0, SEEK_END)))
			{
				/* The mpit is already at the master node (FS is GPFS or slave was running in the same node as master) */
				start_sending = 0;
				fprintf (stdout, "mpitrace: This MPIT is already at the master node and doesn't need to be moved.\n");
			}
			else
			{
				/* The mpit does not exists, or exists from a previous execution. Ask the slave for it */
				if (mpit_fd != -1) 
				{
					if (close(mpit_fd) == -1)
					{
						fprintf(stderr, "mpitrace: Error while closing MPIT %s\n", mpit_name);
						/* Since we don't need to open this file again, try to continue */
					}
				}
				start_sending = 1;
				fprintf(stdout, "mpitrace: Transferring MPIT... ");
				fflush (stdout);
			}

			/* Inform the slave whether the mpit has to be transferred */
			pacx_err = PPACX_Send (&start_sending, 1, PACX_INT, slave, START_SENDING_TAG, PACX_COMM_WORLD);
			PACX_CHECK(pacx_err, PPACX_Send);
			if (start_sending)
			{
				/* Transference has started */

				/* Create (or truncate) the mpit */
				mpit_fd = open(mpit_name, O_WRONLY | O_TRUNC | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
				if (mpit_fd != -1) 
				{
					remaining_bytes = mpit_size;
					while (remaining_bytes > 0)
					{
						int recv_bytes;
						int written_bytes;
						
						/* Receive the mpit in chunks of MPIT_CHUNK_SIZE and write it to disk */
						recv_bytes = MIN(MPIT_CHUNK_SIZE, remaining_bytes);
						pacx_err = PPACX_Recv(mpit_content, recv_bytes, PACX_BYTE, slave, MPIT_CONTENT_TAG, PACX_COMM_WORLD, &sts);
						PACX_CHECK(pacx_err, PPACX_Recv);
						written_bytes = write(mpit_fd, mpit_content, recv_bytes);
						if ((written_bytes == -1) || (written_bytes != recv_bytes))
						{
							fprintf(stderr, "mpitrace: Error while writing %d bytes in MPIT %s (%d written)\n", 
								recv_bytes, mpit_name, written_bytes);
							exit(-1);
						}
						remaining_bytes -= MPIT_CHUNK_SIZE;
					}
					/* MPIT has been successfully transferred */
					fprintf(stdout, "done!\n");

					confirm_delete = 1;

					/* Flush data to disk and close the mpit. In an error arises, 
					   the mpit will not be deleted in the slave side
					   and the execution will continue 
					 */
					if (fsync(mpit_fd) == -1)
					{
						fprintf(stderr, "mpitrace: Error while flushing MPIT %s data into disk (fsync failed)\n", mpit_name);
						confirm_delete = 0;
					}
					if (close(mpit_fd) == -1) 
					{
						fprintf(stderr, "mpitrace: Error while closing MPIT %s\n", mpit_name);
						confirm_delete = 0;
					}

					/* Inform the slave whether the mpit has to be deleted */
					pacx_err = PPACX_Send(&confirm_delete, 1, PACX_INT, slave, DELETE_MPIT_TAG, PACX_COMM_WORLD);
					PACX_CHECK(pacx_err, PPACX_Send);
				}
				else
				{
					fprintf(stderr, "mpitrace: Error while opening MPIT %s for writing\n", mpit_name);
					exit(-1);
				}
			}
			free(mpit_name);
		}
	}
	else /* SLAVE side */
	{
		/* Each task is blocked until master wakes them up */
		pacx_err = PPACX_Recv(&wake_up, 1, PACX_INT, 0, WAKE_UP_TAG, PACX_COMM_WORLD, &sts);
		PACX_CHECK(pacx_err, PPACX_Recv);

		if (wake_up)
		{
			char mpit_name[TRACE_FILE];
			int master_pid = getpid();

			/* Send the mpit name and size to the master */
			//Tracefile_Name (mpit_name, final_dir, appl_name, master_pid, TASKID, 0);
			FileName_PTT(mpit_name, final_dir, appl_name, master_pid, TASKID, 0, EXT_MPIT);

			mpit_name_len = strlen(mpit_name);
			mpit_fd = open(mpit_name, O_RDONLY);
			if (mpit_fd == -1) 
			{
				fprintf(stderr, "mpitrace: Task %d: Error while opening MPIT %s for reading.\n", TASKID, mpit_name);
				exit(-1);
			}
			mpit_size = lseek(mpit_fd, 0, SEEK_END);
			if (mpit_size == -1) 
			{
				fprintf(stderr, "mpitrace: Task %d: Error while checking MPIT %s file size (lseek failed)\n", 
					TASKID, mpit_name);
				exit(-1);
			}
			if (lseek(mpit_fd, 0, SEEK_SET) == -1) 
			{
				fprintf(stderr, "mpitrace: Task %d: Error while rewinding MPIT %s file descriptor (lseek failed)\n", 
					TASKID, mpit_name);
				exit(-1);
			}
			pacx_err = PPACX_Send(&mpit_name_len, 1, PACX_INT, 0, MPIT_NAME_LENGTH_TAG, PACX_COMM_WORLD);
			PACX_CHECK(pacx_err, PPACX_Send);
			pacx_err = PPACX_Send(mpit_name, mpit_name_len, PACX_CHAR, 0, MPIT_NAME_TAG, PACX_COMM_WORLD);
			PACX_CHECK(pacx_err, PPACX_Send);
			pacx_err = PPACX_Send(&mpit_size, 1, PACX_INT, 0, MPIT_SIZE_TAG, PACX_COMM_WORLD);
			PACX_CHECK(pacx_err, PPACX_Send);

			/* Wait for confirmation to start sending the mpit */
			pacx_err = PPACX_Recv(&start_sending, 1, PACX_INT, 0, START_SENDING_TAG, PACX_COMM_WORLD, &sts);
			PACX_CHECK(pacx_err, PPACX_Recv);
			if (start_sending) 
			{
				remaining_bytes = mpit_size;
				while (remaining_bytes > 0)
				{
					int send_bytes;
					int read_bytes;
				
					/* Send the mpit in chunks of MPIT_CHUNK_SIZE */	
					send_bytes = MIN(MPIT_CHUNK_SIZE, remaining_bytes);
					read_bytes = read(mpit_fd, mpit_content, send_bytes);
					if ((read_bytes == -1) || (read_bytes != send_bytes))
					{
						fprintf(stderr, "mpitrace: Task %d: Error while reading %d bytes from MPIT %s (%d read)\n",
							TASKID, send_bytes, mpit_name, read_bytes);
						exit(-1);
					}
					pacx_err = PPACX_Send(mpit_content, send_bytes, PACX_BYTE, 0, MPIT_CONTENT_TAG, PACX_COMM_WORLD);
					PACX_CHECK(pacx_err, PPACX_Send);
					remaining_bytes -= MPIT_CHUNK_SIZE;
				}
				/* MPIT has been successfuly sent to the master task */
				if (close (mpit_fd) == -1)
				{
					fprintf(stderr, "mpitrace: Task %d: Error while closing MPIT %s\n", TASKID, mpit_name);
					/* Anyway, try to continue */
				}
				
				/* Wait for confirmation to delete the mpit */
				PPACX_Recv(&confirm_delete, 1, PACX_INT, 0, DELETE_MPIT_TAG, PACX_COMM_WORLD, &sts);
				if (confirm_delete)
				{
					/* Everything went OK in the master side, delete the mpit in the slave node */
					if (unlink(mpit_name) == -1) 
					{
						fprintf(stderr, "mpitrace: Task %d, Error deleting MPIT %s in node %s\n", 
							TASKID, mpit_name, hostname);
					}
				}
				else
				{
					fprintf(stderr, "mpitrace: Warning: MPIT %s in node %s will not be deleted due to previous errors.\n", mpit_name, hostname);
				}
			}
		}
	}
}

/* HSG
   This should be called _PACX_stats_ ... but it's hardcoded in some macros
   in trace_macros.h -- should be solved in the future
*/
static void OMPItrace_MPI_stats_Wrapper (iotimer_t timestamp)
{
	unsigned int vec_types[7] =
		{ PACX_STATS_EV, PACX_STATS_EV, PACX_STATS_EV, PACX_STATS_EV, PACX_STATS_EV,
		PACX_STATS_EV, PACX_STATS_EV };
	unsigned int vec_values[7] =
		{ PACX_STATS_P2P_COMMS_EV, PACX_STATS_P2P_BYTES_SENT_EV,
		PACX_STATS_P2P_BYTES_RECV_EV, PACX_STATS_GLOBAL_COMMS_EV,
		PACX_STATS_GLOBAL_BYTES_SENT_EV, PACX_STATS_GLOBAL_BYTES_RECV_EV,
		PACX_STATS_TIME_IN_PACX_EV };
	unsigned int vec_params[7] = 
		{ P2P_Communications, P2P_Bytes_Sent, P2P_Bytes_Recv, 
		GLOBAL_Communications, GLOBAL_Bytes_Sent, GLOBAL_Bytes_Recv,
		Elapsed_Time_In_MPI };

	if (TRACING_PACX_STATISTICS)
		TRACE_N_MISCEVENT (timestamp, 7, vec_types, vec_values, vec_params);

	/* Reset the counters */
	P2P_Communications = 0;
	P2P_Bytes_Sent = 0;
	P2P_Bytes_Recv = 0;
	GLOBAL_Communications = 0;
	GLOBAL_Bytes_Sent = 0;
	GLOBAL_Bytes_Recv = 0;
	Elapsed_Time_In_MPI = 0;
}

void OMPItrace_network_counters_Wrapper (void)
{
	TRACE_MYRINET_HWC();
}

void OMPItrace_network_routes_Wrapper (int pacx_rank)
{
	TRACE_MYRINET_ROUTES(pacx_rank);
}

/******************************************************************************
 **      Function name : OMPItrace_tracing_tasks_Wrapper
 **      Author: HSG
 **      Description : Let the user choose which tasks must be traced
 ******************************************************************************/
void OMPItrace_tracing_tasks_Wrapper (int from, int to)
{
	int i, tmp;

	if (NumOfTasks > 1)
	{
		if (tracejant && TracingBitmap != NULL)
		{
			/*
			 * Interchange them if limits are bad given 
			 */
			if (from > to)
			{
				tmp = from;
				from = to;
				to = tmp;
			}

			if (from < 0)
				from = 0;
			if (to >= NumOfTasks)
				to = NumOfTasks - 1;

			TracingBitmap[TASKID] = TRUE;

			/*
			 * If I'm not in the bitmask, disallow me tracing! 
			 */
			TRACE_EVENT (TIME, SET_TRACE_EV, (from <= TASKID) && (TASKID <= to));

			for (i = 0; i < NumOfTasks; i++)
				TracingBitmap[i] = FALSE;

			/*
			 * Build the bitmask 
			 */
			for (i = from; i <= to; i++)
				TracingBitmap[i] = TRUE;
		}
	}
}

static char * PACX_Distribute_XML_File (int rank, int world_size, char *origen)
{
	char hostname[1024];
	char *result_file = NULL;
	off_t file_size;
	int fd;
	char *storage;
	int has_hostname = FALSE;

	has_hostname = gethostname(hostname, 1024 - 1) == 0;

	/* If no other tasks are running, just return the same file */
	if (world_size == 1)
	{
		/* Copy the filename */
		result_file = strdup (origen);
		if (result_file == NULL)
		{
			fprintf (stderr, "mpitrace: Cannot obtain memory for the XML file!\n");
			exit (0);
		}
		return result_file;
	}

	if (rank == 0)
	{
		/* Copy the filename */
		result_file = (char*) malloc ((strlen(origen)+1)*sizeof(char));
		if (result_file == NULL)
		{
			fprintf (stderr, "mpitrace: Cannot obtain memory for the XML file!\n");
			exit (0);
		}
		memset (result_file, 0, (strlen(origen)+1)*sizeof(char));
		strncpy (result_file, origen, strlen(origen));

		/* Open the file */
		fd = open (result_file, O_RDONLY);

		/* If open fails, just return the same fail... XML parsing will fail too! */
		if (fd < 0)
		{
			fprintf (stderr, "mpitrace: Cannot open XML configuration file (%s)!\n", result_file);
			exit (0);
		}

		file_size = lseek (fd, 0, SEEK_END);
		lseek (fd, 0, SEEK_SET);

		/* Send the size */
		PPACX_Bcast (&file_size, 1, MPI_LONG_LONG, 0, PACX_COMM_WORLD);

		/* Allocate & Read the file */
		storage = (char*) malloc ((file_size)*sizeof(char));
		if (storage == NULL)
		{
			fprintf (stderr, "mpitrace: Cannot obtain memory for the XML distribution!\n");
			exit (0);
		}
		if (file_size != read (fd, storage, file_size))
		{
			fprintf (stderr, "mpitrace: Unable to read XML file for its distribution on host %s\n", has_hostname?hostname:"unknown");
			exit (0);
		}

		/* Send the file */
		PPACX_Bcast (storage, file_size, PACX_CHARACTER, 0, PACX_COMM_WORLD);

		/* Close the file */
		close (fd);
		free (storage);

		return result_file;
	}
	else
	{
		/* Receive the size */
		PPACX_Bcast (&file_size, 1, MPI_LONG_LONG, 0, PACX_COMM_WORLD);
		storage = (char*) malloc ((file_size)*sizeof(char));
		if (storage == NULL)
		{
			fprintf (stderr, "mpitrace: Cannot obtain memory for the XML distribution!\n");
			exit (0);
		}

		/* Build the temporal file pattern */
		if (getenv("TMPDIR"))
		{
			/* 14 is the length from /XMLFileXXXXXX */
			result_file = (char*) malloc (14+strlen(getenv("TMPDIR")+1)*sizeof(char));
			sprintf (result_file, "%s/XMLFileXXXXXX", getenv ("TMPDIR"));
		}
		else
		{
			/* 13 is the length from XMLFileXXXXXX */
			result_file = (char*) malloc ((13+1)*sizeof(char));
			sprintf (result_file, "XMLFileXXXXXX");
		}

		/* Create the temporal file */
		fd = mkstemp (result_file);

		/* Receive the file */
		PPACX_Bcast (storage, file_size, PACX_CHARACTER, 0, PACX_COMM_WORLD);

		if (file_size != write (fd, storage, file_size))
		{
			fprintf (stderr, "mpitrace: Unable to write XML file for its distribution (%s) - host %s\n", result_file, has_hostname?hostname:"unknown");
			perror("write");
			exit (0);
		}

		/* Close the file, free and return it! */
		close (fd);
		free (storage);

		return result_file;
	}
}

#if defined(DEAD_CODE)
/**
 * Checks whether a given communicator is a PACX_COMM_WORLD alias.
 * We consider them synonyms if both have the same number of members.
 * \param comm The communicator.
 * \return 1 if is PACX_COMM_WORLD alias, 0 otherwise.
 */
int is_PACX_World_Comm (PACX_Comm comm)
{
	static int world_size = 0;
	int comm_size;
	
	/* Trick to check this once */
	if (world_size == 0)
		PPACX_Comm_size (PACX_COMM_WORLD, &world_size);
	PPACX_Comm_size (comm, &comm_size);

	return (comm_size == world_size);
}
#endif

/******************************************************************************
 ***  Trace_PACX_Communicator
 ******************************************************************************/
static void Trace_PACX_Communicator (int tipus_event, PACX_Comm newcomm)
{
	/* Store in the tracefile the definition of the communicator.
	   If the communicator is self/world, store an alias, otherwise store the
	   involved tasks
	*/

	int i, num_tasks, ierror;
	PACX_Group group;
	iotimer_t temps;

	temps = TIME;

	if (newcomm != PACX_COMM_WORLD && newcomm != PACX_COMM_SELF)
	{
		/* Obtain the group of the communicator */
		ierror = PPACX_Comm_group (newcomm, &group);
		PACX_CHECK(ierror, PPACX_Comm_group);

		/* Calculate the number of involved tasks */
		ierror = PPACX_Group_size (group, &num_tasks);
		PACX_CHECK(ierror, PPACX_Group_size);

		/* Obtain task id of each element */
		ierror = PPACX_Group_translate_ranks (group, num_tasks, ranks_global, grup_global, ranks_aux);
		PACX_CHECK(ierror, PPACX_Group_translate_ranks);

		FORCE_TRACE_MPIEVENT (temps, tipus_event, EVT_BEGIN, EMPTY, num_tasks, EMPTY, newcomm, EMPTY);

		/* Dump each of the task ids */
		for (i = 0; i < num_tasks; i++)
			FORCE_TRACE_MPIEVENT (temps, PACX_RANK_CREACIO_COMM_EV, ranks_aux[i], EMPTY,
				EMPTY, EMPTY, EMPTY, EMPTY);

		/* Free the group */
		if (group != PACX_GROUP_NULL)
		{
			ierror = PPACX_Group_free (&group);
			PACX_CHECK(ierror, PPACX_Group_free);
		}
	}
	else if (newcomm == PACX_COMM_WORLD)
	{
		FORCE_TRACE_MPIEVENT (temps, tipus_event, EVT_BEGIN, PACX_COMM_WORLD_ALIAS,
			NumOfTasks, EMPTY, newcomm, EMPTY);
	}
	else if (newcomm == PACX_COMM_SELF)
	{
		FORCE_TRACE_MPIEVENT (temps, tipus_event, EVT_BEGIN, PACX_COMM_SELF_ALIAS,
			1, EMPTY, newcomm, EMPTY);
	}

	FORCE_TRACE_MPIEVENT (temps, tipus_event, EVT_END, EMPTY, EMPTY, EMPTY, newcomm, EMPTY);
}

