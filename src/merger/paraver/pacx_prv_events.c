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
 | @file: $HeadURL$
 | @last_commit: $Date$
 | @version:     $Revision$
\* -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=- */
#include "common.h"

static char UNUSED rcsid[] = "$Id$";

#ifdef HAVE_STDLIB_H
# include <stdlib.h>
#endif

#include "pacx_prv_events.h"
#include "events.h"

struct t_event_mpit2prv
{
  int tipus_mpit;
  int tipus_prv;
  int valor_prv;
  int utilitzada;               /* Boolea que indica si apareix a la trac,a */
};

struct t_prv_type_info
{
  int type;
  char *label;
  int flag_color;
};

struct t_prv_val_label
{
  int value;
  char *label;
};

static struct t_event_mpit2prv event_mpit2prv[NUM_PACX_PRV_ELEMENTS] = {
	{PACX_ALLGATHER_EV, PACXTYPE_COLLECTIVE, PACX_ALLGATHER_VAL, FALSE}, /*   1 */
	{PACX_ALLGATHERV_EV, PACXTYPE_COLLECTIVE, PACX_ALLGATHERV_VAL, FALSE},       /*   2 */
	{PACX_ALLREDUCE_EV, PACXTYPE_COLLECTIVE, PACX_ALLREDUCE_VAL, FALSE}, /*   3 */
	{PACX_ALLTOALL_EV, PACXTYPE_COLLECTIVE, PACX_ALLTOALL_VAL, FALSE},   /*   4 */
	{PACX_ALLTOALLV_EV, PACXTYPE_COLLECTIVE, PACX_ALLTOALLV_VAL, FALSE}, /*   5 */
	{PACX_BARRIER_EV, PACXTYPE_COLLECTIVE, PACX_BARRIER_VAL, FALSE},     /*   6 */
	{PACX_BCAST_EV, PACXTYPE_COLLECTIVE, PACX_BCAST_VAL, FALSE}, /*   7 */
	{PACX_GATHER_EV, PACXTYPE_COLLECTIVE, PACX_GATHER_VAL, FALSE},       /*   8 */
	{PACX_GATHERV_EV, PACXTYPE_COLLECTIVE, PACX_GATHERV_VAL, FALSE},     /*   9 */
	{-1, PACXTYPE_OTHER, PACX_OP_CREATE_VAL, FALSE},        /*  10 */
	{-1, PACXTYPE_OTHER, PACX_OP_FREE_VAL, FALSE},  /*  11 */
	{PACX_REDUCESCAT_EV, PACXTYPE_COLLECTIVE, PACX_REDUCE_SCATTER_VAL, FALSE},   /*  12 */
	{PACX_REDUCE_EV, PACXTYPE_COLLECTIVE, PACX_REDUCE_VAL, FALSE},       /*  13 */
	{PACX_SCAN_EV, PACXTYPE_COLLECTIVE, PACX_SCAN_VAL, FALSE},   /*  14 */
	{PACX_SCATTER_EV, PACXTYPE_COLLECTIVE, PACX_SCATTER_VAL, FALSE},     /*  15 */
	{PACX_SCATTERV_EV, PACXTYPE_COLLECTIVE, PACX_SCATTERV_VAL, FALSE},   /*  16 */
	{-1, PACXTYPE_OTHER, PACX_ATTR_DELETE_VAL, FALSE},      /*  17 */
	{-1, PACXTYPE_OTHER, PACX_ATTR_GET_VAL, FALSE}, /*  18 */
	{-1, PACXTYPE_OTHER, PACX_ATTR_PUT_VAL, FALSE}, /*  19 */
	{PACX_COMM_CREATE_EV, PACXTYPE_COMM, PACX_COMM_CREATE_VAL, FALSE},   /*  20 */
	{PACX_COMM_DUP_EV, PACXTYPE_COMM, PACX_COMM_DUP_VAL, FALSE}, /*  21 */
	{-1, PACXTYPE_COMM, PACX_COMM_FREE_VAL, FALSE}, /*  22 */
	{-1, PACXTYPE_COMM, PACX_COMM_GROUP_VAL, FALSE},        /*  23 */
	{PACX_COMM_RANK_EV, PACXTYPE_COMM, PACX_COMM_RANK_VAL, FALSE},       /*  24 */
	{-1, PACXTYPE_COMM, PACX_COMM_REMOTE_GROUP_VAL, FALSE}, /*  25 */
	{-1, PACXTYPE_COMM, PACX_COMM_REMOTE_SIZE_VAL, FALSE},  /*  26 */
	{PACX_COMM_SIZE_EV, PACXTYPE_COMM, PACX_COMM_SIZE_VAL, FALSE},       /*  27 */
	{PACX_COMM_SPLIT_EV, PACXTYPE_COMM, PACX_COMM_SPLIT_VAL, FALSE},     /*  28 */
	{-1, PACXTYPE_COMM, PACX_COMM_TEST_INTER_VAL, FALSE},   /*  29 */
	{-1, PACXTYPE_COMM, PACX_COMM_COMPARE_VAL, FALSE},      /*  30 */
	{-1, PACXTYPE_GROUP, PACX_GROUP_DIFFERENCE_VAL, FALSE}, /*  31 */
	{-1, PACXTYPE_GROUP, PACX_GROUP_EXCL_VAL, FALSE},       /*  32 */
	{-1, PACXTYPE_GROUP, PACX_GROUP_FREE_VAL, FALSE},       /*  33 */
	{-1, PACXTYPE_GROUP, PACX_GROUP_INCL_VAL, FALSE},       /*  34 */
	{-1, PACXTYPE_GROUP, PACX_GROUP_INTERSECTION_VAL, FALSE},       /*  35 */
	{-1, PACXTYPE_GROUP, PACX_GROUP_RANK_VAL, FALSE},       /*  36 */
	{-1, PACXTYPE_GROUP, PACX_GROUP_RANGE_EXCL_VAL, FALSE}, /*  37 */
	{-1, PACXTYPE_GROUP, PACX_GROUP_RANGE_INCL_VAL, FALSE}, /*  38 */
	{-1, PACXTYPE_GROUP, PACX_GROUP_SIZE_VAL, FALSE},       /*  39 */
	{-1, PACXTYPE_GROUP, PACX_GROUP_TRANSLATE_RANKS_VAL, FALSE},    /*  40 */
	{-1, PACXTYPE_GROUP, PACX_GROUP_UNION_VAL, FALSE},      /*  41 */
	{-1, PACXTYPE_GROUP, PACX_GROUP_COMPARE_VAL, FALSE},    /*  42 */
	{-1, PACXTYPE_COMM, PACX_INTERCOMM_CREATE_VAL, FALSE},  /*  43 */
	{-1, PACXTYPE_COMM, PACX_INTERCOMM_MERGE_VAL, FALSE},   /*  44 */
	{-1, PACXTYPE_OTHER, PACX_KEYVAL_FREE_VAL, FALSE},      /*  45 */
	{-1, PACXTYPE_OTHER, PACX_KEYVAL_CREATE_VAL, FALSE},    /*  46 */
	{-1, PACXTYPE_OTHER, PACX_ABORT_VAL, FALSE},    /*  47 */
	{-1, PACXTYPE_OTHER, PACX_ERROR_CLASS_VAL, FALSE},      /*  48 */
	{-1, PACXTYPE_OTHER, PACX_ERRHANDLER_CREATE_VAL, FALSE},        /*  49 */
	{-1, PACXTYPE_OTHER, PACX_ERRHANDLER_FREE_VAL, FALSE},  /*  50 */
	{-1, PACXTYPE_OTHER, PACX_ERRHANDLER_GET_VAL, FALSE},   /*  51 */
	{-1, PACXTYPE_OTHER, PACX_ERROR_STRING_VAL, FALSE},     /*  52 */
	{-1, PACXTYPE_OTHER, PACX_ERRHANDLER_SET_VAL, FALSE},   /*  53 */
	{PACX_FINALIZE_EV, PACXTYPE_OTHER, PACX_FINALIZE_VAL, FALSE},        /*  54 */
	{-1, PACXTYPE_OTHER, PACX_GET_PROCESSOR_NAME_VAL, FALSE},       /*  55 */
	{PACX_INIT_EV, PACXTYPE_OTHER, PACX_INIT_VAL, FALSE},     /*  56 */
	{-1, PACXTYPE_OTHER, PACX_INITIALIZED_VAL, FALSE},      /*  57 */
	{-1, PACXTYPE_OTHER, PACX_WTICK_VAL, FALSE},    /*  58 */
	{-1, PACXTYPE_OTHER, PACX_WTIME_VAL, FALSE},    /*  59 */
	{-1, PACXTYPE_OTHER, PACX_ADDRESS_VAL, FALSE},  /*  60 */
	{PACX_BSEND_EV, PACXTYPE_PTOP, PACX_BSEND_VAL, FALSE},       /*  61 */
	{PACX_BSEND_INIT_EV, PACXTYPE_PTOP, PACX_BSEND_INIT_VAL, FALSE},     /*  62 */
	{-1, PACXTYPE_OTHER, PACX_BUFFER_ATTACH_VAL, FALSE},    /*  63 */
	{-1, PACXTYPE_OTHER, PACX_BUFFER_DETACH_VAL, FALSE},    /*  64 */
	{PACX_CANCEL_EV, PACXTYPE_PTOP, PACX_CANCEL_VAL, FALSE},     /*  65 */
	{PACX_REQUEST_FREE_EV, PACXTYPE_OTHER, PACX_REQUEST_FREE_VAL, FALSE},        /*  66 */
	{PACX_RECV_INIT_EV, PACXTYPE_PTOP, PACX_RECV_INIT_VAL, FALSE},       /*  67 */
	{PACX_SEND_INIT_EV, PACXTYPE_PTOP, PACX_SEND_INIT_VAL, FALSE},       /*  68 */
	{-1, PACXTYPE_OTHER, PACX_GET_COUNT_VAL, FALSE},        /*  69 */
	{-1, PACXTYPE_OTHER, PACX_GET_ELEMENTS_VAL, FALSE},     /*  70 */
	{PACX_IBSEND_EV, PACXTYPE_PTOP, PACX_IBSEND_VAL, FALSE},     /*  71 */
	{PACX_IPROBE_EV, PACXTYPE_PTOP, PACX_IPROBE_VAL, FALSE},     /*  72 */
	{PACX_IRECV_EV, PACXTYPE_PTOP, PACX_IRECV_VAL, FALSE},       /*  73 */
	{PACX_IRSEND_EV, PACXTYPE_PTOP, PACX_IRSEND_VAL, FALSE},     /*  74 */
	{PACX_ISEND_EV, PACXTYPE_PTOP, PACX_ISEND_VAL, FALSE},       /*  75 */
	{PACX_ISSEND_EV, PACXTYPE_PTOP, PACX_ISSEND_VAL, FALSE},     /*  76 */
	{-1, PACXTYPE_OTHER, PACX_PACK_VAL, FALSE},     /*  77 */
	{-1, PACXTYPE_OTHER, PACX_PACK_SIZE_VAL, FALSE},        /*  78 */
	{PACX_PROBE_EV, PACXTYPE_PTOP, PACX_PROBE_VAL, FALSE},       /*  79 */
	{PACX_RECV_EV, PACXTYPE_PTOP, PACX_RECV_VAL, FALSE}, /*  80 */
	{PACX_RSEND_EV, PACXTYPE_PTOP, PACX_RSEND_VAL, FALSE},       /*  81 */
	{PACX_RSEND_INIT_EV, PACXTYPE_PTOP, PACX_RSEND_INIT_VAL, FALSE},     /*  82 */
	{PACX_SEND_EV, PACXTYPE_PTOP, PACX_SEND_VAL, FALSE}, /*  83 */
	{PACX_SENDRECV_EV, PACXTYPE_PTOP, PACX_SENDRECV_VAL, FALSE},  /*  84 */
	{PACX_SENDRECV_REPLACE_EV, PACXTYPE_PTOP, PACX_SENDRECV_REPLACE_VAL, FALSE},  /*  85 */
	{PACX_SSEND_EV, PACXTYPE_PTOP, PACX_SSEND_VAL, FALSE},       /*  86 */
	{PACX_SSEND_INIT_EV, PACXTYPE_PTOP, PACX_SSEND_INIT_VAL, FALSE},     /*  87 */
	{PACX_START_EV, PACXTYPE_OTHER, PACX_START_VAL, FALSE},      /*  88 */
	{PACX_STARTALL_EV, PACXTYPE_OTHER, PACX_STARTALL_VAL, FALSE},        /*  89 */
	{PACX_TEST_EV, PACXTYPE_OTHER, PACX_TEST_VAL, FALSE},        /*  90 */
	{-1, PACXTYPE_OTHER, PACX_TESTALL_VAL, FALSE},  /*  91 */
	{-1, PACXTYPE_OTHER, PACX_TESTANY_VAL, FALSE},  /*  92 */
	{-1, PACXTYPE_OTHER, PACX_TEST_CANCELLED_VAL, FALSE},   /*  93 */
	{-1, PACXTYPE_OTHER, PACX_TESTSOME_VAL, FALSE}, /*  94 */
	{-1, PACXTYPE_TYPE, PACX_TYPE_COMMIT_VAL, FALSE},       /*  95 */
	{-1, PACXTYPE_TYPE, PACX_TYPE_CONTIGUOUS_VAL, FALSE},   /*  96 */
	{-1, PACXTYPE_TYPE, PACX_TYPE_EXTENT_VAL, FALSE},       /*  97 */
	{-1, PACXTYPE_TYPE, PACX_TYPE_FREE_VAL, FALSE}, /*  98 */
	{-1, PACXTYPE_TYPE, PACX_TYPE_HINDEXED_VAL, FALSE},     /*  99 */
	{-1, PACXTYPE_TYPE, PACX_TYPE_HVECTOR_VAL, FALSE},      /* 100 */
	{-1, PACXTYPE_TYPE, PACX_TYPE_INDEXED_VAL, FALSE},      /* 101 */
	{-1, PACXTYPE_TYPE, PACX_TYPE_LB_VAL, FALSE},   /* 102 */
	{-1, PACXTYPE_TYPE, PACX_TYPE_SIZE_VAL, FALSE}, /* 103 */
	{-1, PACXTYPE_TYPE, PACX_TYPE_STRUCT_VAL, FALSE},       /* 104 */
	{-1, PACXTYPE_TYPE, PACX_TYPE_UB_VAL, FALSE},   /* 105 */
	{-1, PACXTYPE_TYPE, PACX_TYPE_VECTOR_VAL, FALSE},       /* 106 */
	{-1, PACXTYPE_OTHER, PACX_UNPACK_VAL, FALSE},   /* 107 */
	{PACX_WAIT_EV, PACXTYPE_PTOP, PACX_WAIT_VAL, FALSE}, /* 108 */
	{PACX_WAITALL_EV, PACXTYPE_PTOP, PACX_WAITALL_VAL, FALSE},   /* 109 */
	{PACX_WAITANY_EV, PACXTYPE_PTOP, PACX_WAITANY_VAL, FALSE},   /* 110 */
	{PACX_WAITSOME_EV, PACXTYPE_PTOP, PACX_WAITSOME_VAL, FALSE}, /* 111 */
	{PACX_CART_COORDS_EV, PACXTYPE_TOPOLOGIES, PACX_CART_COORDS_VAL, FALSE}, /* 112 */
	{PACX_CART_CREATE_EV, PACXTYPE_TOPOLOGIES, PACX_CART_CREATE_VAL, FALSE}, /* 113 */
	{-1, PACXTYPE_TOPOLOGIES, PACX_CART_GET_VAL, FALSE},    /* 114 */
	{-1, PACXTYPE_TOPOLOGIES, PACX_CART_MAP_VAL, FALSE},    /* 115 */
	{-1, PACXTYPE_TOPOLOGIES, PACX_CART_RANK_VAL, FALSE},   /* 116 */
	{-1, PACXTYPE_TOPOLOGIES, PACX_CART_SHIFT_VAL, FALSE},  /* 117 */
	{PACX_CART_SUB_EV, PACXTYPE_TOPOLOGIES, PACX_CART_SUB_VAL, FALSE},       /* 118 */
	{-1, PACXTYPE_TOPOLOGIES, PACX_CARTDIM_GET_VAL, FALSE}, /* 119 */
	{-1, PACXTYPE_TOPOLOGIES, PACX_DIMS_CREATE_VAL, FALSE}, /* 120 */
	{-1, PACXTYPE_TOPOLOGIES, PACX_GRAPH_GET_VAL, FALSE},   /* 121 */
	{-1, PACXTYPE_TOPOLOGIES, PACX_GRAPH_MAP_VAL, FALSE},   /* 122 */
	{-1, PACXTYPE_TOPOLOGIES, PACX_GRAPH_NEIGHBORS_VAL, FALSE},     /* 123 */
	{-1, PACXTYPE_TOPOLOGIES, PACX_GRAPH_CREATE_VAL, FALSE},        /* 124 */
	{-1, PACXTYPE_TOPOLOGIES, PACX_GRAPHDIMS_GET_VAL, FALSE},       /* 125 */
	{-1, PACXTYPE_TOPOLOGIES, PACX_GRAPH_NEIGHBORS_COUNT_VAL, FALSE},       /* 126 */
	{-1, PACXTYPE_TOPOLOGIES, PACX_TOPO_TEST_VAL, FALSE},   /* 127 */
	{-1, PACXTYPE_RMA, PACX_WIN_CREATE_VAL, FALSE}, /* 128 */
	{-1, PACXTYPE_RMA, PACX_WIN_FREE_VAL, FALSE},   /* 129 */
	{-1, PACXTYPE_RMA, PACX_PUT_VAL, FALSE},        /* 130 */
	{-1, PACXTYPE_RMA, PACX_GET_VAL, FALSE},        /* 131 */
	{-1, PACXTYPE_RMA, PACX_ACCUMULATE_VAL, FALSE}, /* 132 */
	{-1, PACXTYPE_RMA, PACX_WIN_FENCE_VAL, FALSE},  /* 133 */
	{-1, PACXTYPE_RMA, PACX_WIN_START_VAL, FALSE},  /* 134 */
	{-1, PACXTYPE_RMA, PACX_WIN_COMPLETE_VAL, FALSE},       /* 135 */
	{-1, PACXTYPE_RMA, PACX_WIN_POST_VAL, FALSE},   /* 136 */
	{-1, PACXTYPE_RMA, PACX_WIN_WAIT_VAL, FALSE},   /* 137 */
	{-1, PACXTYPE_RMA, PACX_WIN_TEST_VAL, FALSE},   /* 138 */
	{-1, PACXTYPE_RMA, PACX_WIN_LOCK_VAL, FALSE},   /* 139 */
	{-1, PACXTYPE_RMA, PACX_WIN_UNLOCK_VAL, FALSE},  /* 140 */
	{PACX_FILE_OPEN_EV, PACXTYPE_IO, PACX_FILE_OPEN_VAL, FALSE}, /* 141 */
	{PACX_FILE_CLOSE_EV, PACXTYPE_IO, PACX_FILE_CLOSE_VAL, FALSE}, /* 142 */
	{PACX_FILE_READ_EV, PACXTYPE_IO, PACX_FILE_READ_VAL, FALSE}, /* 143 */
	{PACX_FILE_READ_ALL_EV, PACXTYPE_IO, PACX_FILE_READ_ALL_VAL, FALSE}, /* 144 */
	{PACX_FILE_WRITE_EV, PACXTYPE_IO, PACX_FILE_WRITE_VAL, FALSE}, /* 145 */
	{PACX_FILE_WRITE_ALL_EV, PACXTYPE_IO, PACX_FILE_WRITE_ALL_VAL, FALSE}, /* 146 */
	{PACX_FILE_READ_AT_EV, PACXTYPE_IO, PACX_FILE_READ_AT_VAL, FALSE}, /* 147 */
	{PACX_FILE_READ_AT_ALL_EV, PACXTYPE_IO, PACX_FILE_READ_AT_ALL_VAL, FALSE}, /* 148 */
	{PACX_FILE_WRITE_AT_EV, PACXTYPE_IO, PACX_FILE_WRITE_AT_VAL, FALSE}, /* 149 */
	{PACX_FILE_WRITE_AT_ALL_EV, PACXTYPE_IO, PACX_FILE_WRITE_AT_ALL_VAL, FALSE} /* 150 */
};



/* Dels 12, de moment nomes 8 son diferents */
static struct t_prv_type_info prv_block_groups[NUM_PACX_BLOCK_GROUPS] = {
	{PACXTYPE_PTOP, PACXTYPE_PTOP_LABEL, PACXTYPE_FLAG_COLOR},
	{PACXTYPE_COLLECTIVE, PACXTYPE_COLLECTIVE_LABEL, PACXTYPE_FLAG_COLOR},
	{PACXTYPE_OTHER, PACXTYPE_OTHER_LABEL, PACXTYPE_FLAG_COLOR},
	{PACXTYPE_RMA, PACXTYPE_RMA_LABEL, PACXTYPE_FLAG_COLOR}
#if defined(DEAD_CODE)
	{ PACXTYPE_COMM,        PACXTYPE_COMM_LABEL,        PACXTYPE_FLAG_COLOR},
	{ PACXTYPE_GROUP,       PACXTYPE_GROUP_LABEL,	  PACXTYPE_FLAG_COLOR},
	{ PACXTYPE_TOPOLOGIES,  PACXTYPE_TOPOLOGIES_LABEL,  PACXTYPE_FLAG_COLOR},
	{ PACXTYPE_TYPE,        PACXTYPE_TYPE_LABEL,	  PACXTYPE_FLAG_COLOR},
	{PACXTYPE_IO, PACXTYPE_IO_LABEL, PACXTYPE_FLAG_COLOR},
	{USER_FUNCTION, USER_FUNCTION_LABEL, PACXTYPE_FLAG_COLOR},
	{USER_CALL, USER_CALL_LABEL, PACXTYPE_FLAG_COLOR},
	{USER_BLOCK, USER_BLOCK_LABEL, PACXTYPE_FLAG_COLOR}
#endif
};



static struct t_prv_val_label pacx_prv_val_label[NUM_PACX_PRV_ELEMENTS] = {
	{PACX_SEND_VAL, PACX_SEND_LABEL},
	{PACX_RECV_VAL, PACX_RECV_LABEL},
	{PACX_ISEND_VAL, PACX_ISEND_LABEL},
	{PACX_IRECV_VAL, PACX_IRECV_LABEL},
	{PACX_WAIT_VAL, PACX_WAIT_LABEL},
	{PACX_WAITALL_VAL, PACX_WAITALL_LABEL},
	{PACX_BCAST_VAL, PACX_BCAST_LABEL},
	{PACX_BARRIER_VAL, PACX_BARRIER_LABEL},
	{PACX_REDUCE_VAL, PACX_REDUCE_LABEL},
	{PACX_ALLREDUCE_VAL, PACX_ALLREDUCE_LABEL},
	{PACX_ALLTOALL_VAL, PACX_ALLTOALL_LABEL},
	{PACX_ALLTOALLV_VAL, PACX_ALLTOALLV_LABEL},
	{PACX_GATHER_VAL, PACX_GATHER_LABEL},
	{PACX_GATHERV_VAL, PACX_GATHERV_LABEL},
	{PACX_SCATTER_VAL, PACX_SCATTER_LABEL},
	{PACX_SCATTERV_VAL, PACX_SCATTERV_LABEL},
	{PACX_ALLGATHER_VAL, PACX_ALLGATHER_LABEL},
	{PACX_ALLGATHERV_VAL, PACX_ALLGATHERV_LABEL},
	{PACX_COMM_RANK_VAL, PACX_COMM_RANK_LABEL},
	{PACX_COMM_SIZE_VAL, PACX_COMM_SIZE_LABEL},
	{PACX_COMM_CREATE_VAL, PACX_COMM_CREATE_LABEL},
	{PACX_COMM_DUP_VAL, PACX_COMM_DUP_LABEL},
	{PACX_COMM_SPLIT_VAL, PACX_COMM_SPLIT_LABEL},
	{PACX_COMM_GROUP_VAL, PACX_COMM_GROUP_LABEL},
	{PACX_COMM_FREE_VAL, PACX_COMM_FREE_LABEL},
	{PACX_COMM_REMOTE_GROUP_VAL, PACX_COMM_REMOTE_GROUP_LABEL},
	{PACX_COMM_REMOTE_SIZE_VAL, PACX_COMM_REMOTE_SIZE_LABEL},
	{PACX_COMM_TEST_INTER_VAL, PACX_COMM_TEST_INTER_LABEL},
	{PACX_COMM_COMPARE_VAL, PACX_COMM_COMPARE_LABEL},
	{PACX_SCAN_VAL, PACX_SCAN_LABEL},
	{PACX_INIT_VAL, PACX_INIT_LABEL},
	{PACX_FINALIZE_VAL, PACX_FINALIZE_LABEL},
	{PACX_BSEND_VAL, PACX_BSEND_LABEL},
	{PACX_SSEND_VAL, PACX_SSEND_LABEL},
	{PACX_RSEND_VAL, PACX_RSEND_LABEL},
	{PACX_IBSEND_VAL, PACX_IBSEND_LABEL},
	{PACX_ISSEND_VAL, PACX_ISSEND_LABEL},
	{PACX_IRSEND_VAL, PACX_IRSEND_LABEL},
	{PACX_TEST_VAL, PACX_TEST_LABEL},
	{PACX_CANCEL_VAL, PACX_CANCEL_LABEL},
	{PACX_SENDRECV_VAL, PACX_SENDRECV_LABEL},
	{PACX_SENDRECV_REPLACE_VAL, PACX_SENDRECV_REPLACE_LABEL},
	{PACX_CART_CREATE_VAL, PACX_CART_CREATE_LABEL},
	{PACX_CART_SHIFT_VAL, PACX_CART_SHIFT_LABEL},
	{PACX_CART_COORDS_VAL, PACX_CART_COORDS_LABEL},
	{PACX_CART_GET_VAL, PACX_CART_GET_LABEL},
	{PACX_CART_MAP_VAL, PACX_CART_MAP_LABEL},
	{PACX_CART_RANK_VAL, PACX_CART_RANK_LABEL},
	{PACX_CART_SUB_VAL, PACX_CART_SUB_LABEL},
	{PACX_CARTDIM_GET_VAL, PACX_CARTDIM_GET_LABEL},
	{PACX_DIMS_CREATE_VAL, PACX_DIMS_CREATE_LABEL},
	{PACX_GRAPH_GET_VAL, PACX_GRAPH_GET_LABEL},
	{PACX_GRAPH_MAP_VAL, PACX_GRAPH_MAP_LABEL},
	{PACX_GRAPH_CREATE_VAL, PACX_GRAPH_CREATE_LABEL},
	{PACX_GRAPH_NEIGHBORS_VAL, PACX_GRAPH_NEIGHBORS_LABEL},
	{PACX_GRAPHDIMS_GET_VAL, PACX_GRAPHDIMS_GET_LABEL},
	{PACX_GRAPH_NEIGHBORS_COUNT_VAL, PACX_GRAPH_NEIGHBORS_COUNT_LABEL},
	{PACX_TOPO_TEST_VAL, PACX_TOPO_TEST_LABEL},
	{PACX_WAITANY_VAL, PACX_WAITANY_LABEL},
	{PACX_WAITSOME_VAL, PACX_WAITSOME_LABEL},
	{PACX_PROBE_VAL, PACX_PROBE_LABEL},
	{PACX_IPROBE_VAL, PACX_IPROBE_LABEL},
	{PACX_WIN_CREATE_VAL, PACX_WIN_CREATE_LABEL},
	{PACX_WIN_FREE_VAL, PACX_WIN_FREE_LABEL},
	{PACX_PUT_VAL, PACX_PUT_LABEL},
	{PACX_GET_VAL, PACX_GET_LABEL},
	{PACX_ACCUMULATE_VAL, PACX_ACCUMULATE_LABEL},
	{PACX_WIN_FENCE_VAL, PACX_WIN_FENCE_LABEL},
	{PACX_WIN_START_VAL, PACX_WIN_START_LABEL},
	{PACX_WIN_COMPLETE_VAL, PACX_WIN_COMPLETE_LABEL},
	{PACX_WIN_POST_VAL, PACX_WIN_POST_LABEL},
	{PACX_WIN_WAIT_VAL, PACX_WIN_WAIT_LABEL},
	{PACX_WIN_TEST_VAL, PACX_WIN_TEST_LABEL},
	{PACX_WIN_LOCK_VAL, PACX_WIN_LOCK_LABEL},
	{PACX_WIN_UNLOCK_VAL, PACX_WIN_UNLOCK_LABEL},
	{PACX_PACK_VAL, PACX_PACK_LABEL},
	{PACX_UNPACK_VAL, PACX_UNPACK_LABEL},
	{PACX_OP_CREATE_VAL, PACX_OP_CREATE_LABEL},
	{PACX_OP_FREE_VAL, PACX_OP_FREE_LABEL},
	{PACX_REDUCE_SCATTER_VAL, PACX_REDUCE_SCATTER_LABEL},
	{PACX_ATTR_DELETE_VAL, PACX_ATTR_DELETE_LABEL},
	{PACX_ATTR_GET_VAL, PACX_ATTR_GET_LABEL},
	{PACX_ATTR_PUT_VAL, PACX_ATTR_PUT_LABEL},
	{PACX_GROUP_DIFFERENCE_VAL, PACX_GROUP_DIFFERENCE_LABEL},
	{PACX_GROUP_EXCL_VAL, PACX_GROUP_EXCL_LABEL},
	{PACX_GROUP_FREE_VAL, PACX_GROUP_FREE_LABEL},
	{PACX_GROUP_INCL_VAL, PACX_GROUP_INCL_LABEL},
	{PACX_GROUP_INTERSECTION_VAL, PACX_GROUP_INTERSECTION_LABEL},
	{PACX_GROUP_RANK_VAL, PACX_GROUP_RANK_LABEL},
	{PACX_GROUP_RANGE_EXCL_VAL, PACX_GROUP_RANGE_EXCL_LABEL},
	{PACX_GROUP_RANGE_INCL_VAL, PACX_GROUP_RANGE_INCL_LABEL},
	{PACX_GROUP_SIZE_VAL, PACX_GROUP_SIZE_LABEL},
	{PACX_GROUP_TRANSLATE_RANKS_VAL, PACX_GROUP_TRANSLATE_RANKS_LABEL},
	{PACX_GROUP_UNION_VAL, PACX_GROUP_UNION_LABEL},
	{PACX_GROUP_COMPARE_VAL, PACX_GROUP_COMPARE_LABEL},
	{PACX_INTERCOMM_CREATE_VAL, PACX_INTERCOMM_CREATE_LABEL},
	{PACX_INTERCOMM_MERGE_VAL, PACX_INTERCOMM_MERGE_LABEL},
	{PACX_KEYVAL_FREE_VAL, PACX_KEYVAL_FREE_LABEL},
	{PACX_KEYVAL_CREATE_VAL, PACX_KEYVAL_CREATE_LABEL},
	{PACX_ABORT_VAL, PACX_ABORT_LABEL},
	{PACX_ERROR_CLASS_VAL, PACX_ERROR_CLASS_LABEL},
	{PACX_ERRHANDLER_CREATE_VAL, PACX_ERRHANDLER_CREATE_LABEL},
	{PACX_ERRHANDLER_FREE_VAL, PACX_ERRHANDLER_FREE_LABEL},
	{PACX_ERRHANDLER_GET_VAL, PACX_ERRHANDLER_GET_LABEL},
	{PACX_ERROR_STRING_VAL, PACX_ERROR_STRING_LABEL},
	{PACX_ERRHANDLER_SET_VAL, PACX_ERRHANDLER_SET_LABEL},
	{PACX_GET_PROCESSOR_NAME_VAL, PACX_GET_PROCESSOR_NAME_LABEL},
	{PACX_INITIALIZED_VAL, PACX_INITIALIZED_LABEL},
	{PACX_WTICK_VAL, PACX_WTICK_LABEL},
	{PACX_WTIME_VAL, PACX_WTIME_LABEL},
	{PACX_ADDRESS_VAL, PACX_ADDRESS_LABEL},
	{PACX_BSEND_INIT_VAL, PACX_BSEND_INIT_LABEL},
	{PACX_BUFFER_ATTACH_VAL, PACX_BUFFER_ATTACH_LABEL},
	{PACX_BUFFER_DETACH_VAL, PACX_BUFFER_DETACH_LABEL},
	{PACX_REQUEST_FREE_VAL, PACX_REQUEST_FREE_LABEL},
	{PACX_RECV_INIT_VAL, PACX_RECV_INIT_LABEL},
	{PACX_SEND_INIT_VAL, PACX_SEND_INIT_LABEL},
	{PACX_GET_COUNT_VAL, PACX_GET_COUNT_LABEL},
	{PACX_GET_ELEMENTS_VAL, PACX_GET_ELEMENTS_LABEL},
	{PACX_PACK_SIZE_VAL, PACX_PACK_SIZE_LABEL},
	{PACX_RSEND_INIT_VAL, PACX_RSEND_INIT_LABEL},
	{PACX_SSEND_INIT_VAL, PACX_SSEND_INIT_LABEL},
	{PACX_START_VAL, PACX_START_LABEL},
	{PACX_STARTALL_VAL, PACX_STARTALL_LABEL},
	{PACX_TESTALL_VAL, PACX_TESTALL_LABEL},
	{PACX_TESTANY_VAL, PACX_TESTANY_LABEL},
	{PACX_TEST_CANCELLED_VAL, PACX_TEST_CANCELLED_LABEL},
	{PACX_TESTSOME_VAL, PACX_TESTSOME_LABEL},
	{PACX_TYPE_COMMIT_VAL, PACX_TYPE_COMMIT_LABEL},
	{PACX_TYPE_CONTIGUOUS_VAL, PACX_TYPE_CONTIGUOUS_LABEL},
	{PACX_TYPE_EXTENT_VAL, PACX_TYPE_EXTENT_LABEL},
	{PACX_TYPE_FREE_VAL, PACX_TYPE_FREE_LABEL},
	{PACX_TYPE_HINDEXED_VAL, PACX_TYPE_HINDEXED_LABEL},
	{PACX_TYPE_HVECTOR_VAL, PACX_TYPE_HVECTOR_LABEL},
	{PACX_TYPE_INDEXED_VAL, PACX_TYPE_INDEXED_LABEL},
	{PACX_TYPE_LB_VAL, PACX_TYPE_LB_LABEL},
	{PACX_TYPE_SIZE_VAL, PACX_TYPE_SIZE_LABEL},
	{PACX_TYPE_STRUCT_VAL, PACX_TYPE_STRUCT_LABEL},
	{PACX_TYPE_UB_VAL, PACX_TYPE_UB_LABEL},
	{PACX_TYPE_VECTOR_VAL, PACX_TYPE_VECTOR_LABEL},
	{PACX_FILE_OPEN_VAL, PACX_FILE_OPEN_LABEL},
	{PACX_FILE_CLOSE_VAL, PACX_FILE_CLOSE_LABEL},
	{PACX_FILE_READ_VAL, PACX_FILE_READ_LABEL},
	{PACX_FILE_READ_ALL_VAL, PACX_FILE_READ_ALL_LABEL},
	{PACX_FILE_WRITE_VAL, PACX_FILE_WRITE_LABEL},
	{PACX_FILE_WRITE_ALL_VAL, PACX_FILE_WRITE_ALL_LABEL},
	{PACX_FILE_READ_AT_VAL, PACX_FILE_READ_AT_LABEL},
	{PACX_FILE_READ_AT_ALL_VAL, PACX_FILE_READ_AT_ALL_LABEL},
	{PACX_FILE_WRITE_AT_VAL, PACX_FILE_WRITE_AT_LABEL},
	{PACX_FILE_WRITE_AT_ALL_VAL, PACX_FILE_WRITE_AT_ALL_LABEL}
};


#define IPROBE_CNT_INDEX							0
#define TIME_OUTSIDE_IPROBES_INDEX		1
#define TEST_CNT_INDEX								2

#define MAX_SOFTCNT										3

int PACX_SoftCounters_used[MAX_SOFTCNT] = { FALSE, FALSE, FALSE };

void Enable_PACX_Soft_Counter (unsigned int EvType)
{
	if (EvType == PACX_IPROBE_COUNTER_EV)
		PACX_SoftCounters_used[IPROBE_CNT_INDEX] = TRUE;
	else if (EvType == PACX_TIME_OUTSIDE_IPROBES_EV)
		PACX_SoftCounters_used[TIME_OUTSIDE_IPROBES_INDEX] = TRUE;
	else if (EvType == PACX_TEST_COUNTER_EV)
		PACX_SoftCounters_used[TEST_CNT_INDEX] = TRUE;
}


/******************************************************************************
 **      Function name : busca_event_mpit
 **      
 **      Description : 
 ******************************************************************************/

static int busca_event_mpit (int tmpit)
{
  int i;

  for (i = 0; i < NUM_PACX_PRV_ELEMENTS; i++)
    if (event_mpit2prv[i].tipus_mpit == tmpit)
      break;
  if (i < NUM_PACX_PRV_ELEMENTS)
    return i;
  return -1;
}




/******************************************************************************
 **      Function name : Enable_MPI_Operation
 **      
 **      Description : 
 ******************************************************************************/

void Enable_PACX_Operation (int Op)
{
  int index;

  index = busca_event_mpit (Op);
  if (index >= 0)
    event_mpit2prv[index].utilitzada = TRUE;
}



/******************************************************************************
 **      Function name : get_pacx_prv_val_label
 **      
 **      Description : 
 ******************************************************************************/

static char *get_pacx_prv_val_label (int val)
{
  int i;

  /*
   * Cal buscar aquest valor 
   */
  for (i = 0; i < NUM_PACX_PRV_ELEMENTS; i++)
    if (pacx_prv_val_label[i].value == val)
      break;
  if (i < NUM_PACX_PRV_ELEMENTS)
    return pacx_prv_val_label[i].label;
  return NULL;
}

#if defined(PARALLEL_MERGE)

#include <mpi.h>
#include "mpi-aux.h"

void Share_PACX_Softcounter_Operations (void)
{
	int res, i, tmp_in[MAX_SOFTCNT], tmp_out[MAX_SOFTCNT];
  
	for (i = 0; i < MAX_SOFTCNT; i++)
		tmp_in[i] = PACX_SoftCounters_used[i];
    
	res = MPI_Reduce (tmp_in, tmp_out, MAX_SOFTCNT, MPI_INT, MPI_BOR, 0, MPI_COMM_WORLD);
	MPI_CHECK(res, MPI_Reduce, "While sharing MPI enabled operations");

	for (i = 0; i < MAX_SOFTCNT; i++)
		PACX_SoftCounters_used[i] = tmp_out[i];
}

void Share_PACX_Operations (void)
{
	int res;
	int i, tmp_in[NUM_PACX_PRV_ELEMENTS], tmp_out[NUM_PACX_PRV_ELEMENTS];

	for (i = 0; i < NUM_PACX_PRV_ELEMENTS; i++)
		tmp_in[i] = event_mpit2prv[i].utilitzada;

	res = MPI_Reduce (tmp_in, tmp_out, NUM_PACX_PRV_ELEMENTS, MPI_INT, MPI_BOR, 0, MPI_COMM_WORLD);
	MPI_CHECK(res, MPI_Reduce, "While sharing MPI enabled operations");

	for (i = 0; i < NUM_PACX_PRV_ELEMENTS; i++)
		event_mpit2prv[i].utilitzada = tmp_out[i];
}
#endif /* PARALLEL_MERGE */



/******************************************************************************
 **      Function name : MPITEvent_WriteEnabledOperations
 **      
 **      Description : 
 ******************************************************************************/

void MPITEvent_WriteEnabled_PACX_Operations (FILE * fd)
{
	int ii, jj;
	int cnt;
	int Type;
	char *etiqueta;

	for (ii = 0; ii < NUM_PACX_BLOCK_GROUPS; ii++)
	{
		Type = prv_block_groups[ii].type;

		/*
		 * Primer comptem si hi ha alguna operacio MPI del grup actual 
		 */
		cnt = 0;
		for (jj = 0; jj < NUM_PACX_PRV_ELEMENTS; jj++)
		{
			if ((Type == event_mpit2prv[jj].tipus_prv) &&
			    (event_mpit2prv[jj].utilitzada))
				cnt++;
		}

		if (cnt)
		{
		  fprintf (fd, "%s\n", "EVENT_TYPE");
		  fprintf (fd, "%d   %d    %s\n", prv_block_groups[ii].flag_color,
		           prv_block_groups[ii].type, prv_block_groups[ii].label);

		  fprintf (fd, "%s\n", "VALUES");
		  for (jj = 0; jj < NUM_PACX_PRV_ELEMENTS; jj++)
		  {
		  	  if ((Type == event_mpit2prv[jj].tipus_prv) &&
		  	      (event_mpit2prv[jj].utilitzada))
		  	  {
		  	  	  etiqueta = get_pacx_prv_val_label (event_mpit2prv[jj].valor_prv);
		  	  	  fprintf (fd, "%d   %s\n", event_mpit2prv[jj].valor_prv, etiqueta);
		  	  }
		  }
		  fprintf (fd, "%d   %s\n", 0, "End");
		  fprintf (fd, "\n\n");
		}
	}
}

/******************************************************************************
 *   Software counters labels
 ******************************************************************************/

#define IPROBE_COUNTER_LBL       "PACX__Iprobe misses"
#define TIME_OUTSIDE_IPROBES_LBL "Elapsed time outside PACX_Iprobe"
#define TEST_COUNTER_LBL         "PACX_Test misses"

void SoftCountersEvent_WriteEnabled_PACX_Operations (FILE * fd)
{
	if (PACX_SoftCounters_used[IPROBE_CNT_INDEX])
	{
		fprintf (fd, "EVENT_TYPE\n");
		fprintf (fd, "%d    %d    %s\n\n", 0, 
			MPI_IPROBE_COUNTER_EV, IPROBE_COUNTER_LBL);
	}
	if (PACX_SoftCounters_used[TIME_OUTSIDE_IPROBES_INDEX])
	{
		fprintf (fd, "EVENT_TYPE\n");
		fprintf (fd, "%d    %d    %s\n\n", 0, 
			MPI_TIME_OUTSIDE_IPROBES_EV, TIME_OUTSIDE_IPROBES_LBL);
	}
	if (PACX_SoftCounters_used[TEST_CNT_INDEX])
	{
		fprintf (fd, "EVENT_TYPE\n");
		fprintf (fd, "%d    %d    %s\n\n", 0, 
			MPI_TEST_COUNTER_EV, TEST_COUNTER_LBL);
	}
}

void Translate_PACX_MPIT2PRV (int typempit, UINT64 valuempit, int *typeprv, UINT64 *valueprv)
{
	int index = busca_event_mpit(typempit);

	if (index >= 0)
	{
		*typeprv = event_mpit2prv[index].tipus_prv;
		*valueprv = (valuempit!=0)?event_mpit2prv[index].valor_prv:0;
	}
	else
	{
		*typeprv = typempit;
		*valueprv = valuempit;
	}
}
