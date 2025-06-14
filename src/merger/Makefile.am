include $(top_srcdir)/PATHS

if HAVE_MPI
if WANT_PARALLEL_MERGE
SUBDIRS = parallel
endif
endif

noinst_LTLIBRARIES = libmpi2prv.la

bin_PROGRAMS = mpi2prv

common_FILES = \
 common/fdz.h \
 common/dump.c common/dump.h \
 common/checkoptions.h common/checkoptions.c \
 common/communicators.c common/communicators.h \
 common/intercommunicators.c common/intercommunicators.h \
 common/cpunode.c common/cpunode.h \
 common/mpi2out.c common/mpi2out.h \
 common/options.c common/options.h \
 common/object_tree.h common/object_tree.c \
 common/semantics.c common/semantics.h \
 common/addresses.c common/addresses.h \
 common/vector.c common/vector.h \
 common/stack.c common/stack.h \
 common/thread_dependencies.c common/thread_dependencies.h \
 common/address_space.c common/address_space.h

dimemas_FILES = \
 dimemas/dimemas_generator.c dimemas/dimemas_generator.h \
 dimemas/mpi_trf_semantics.c dimemas/mpi_trf_semantics.h \
 dimemas/misc_trf_semantics.c dimemas/misc_trf_semantics.h \
 dimemas/trace_to_trf.c dimemas/trace_to_trf.h

paraver_FILES = \
 paraver/bswap.c paraver/bswap.h \
 paraver/trace_communication.c paraver/trace_communication.h \
 paraver/paraver_generator.c paraver/paraver_generator.h \
 paraver/paraver_state.c paraver/paraver_state.h \
 paraver/trace_to_prv.c paraver/trace_to_prv.h \
 paraver/mpi_prv_semantics.c paraver/mpi_prv_semantics.h \
 paraver/omp_prv_semantics.c paraver/omp_prv_semantics.h \
 paraver/misc_prv_semantics.c paraver/misc_prv_semantics.h \
 paraver/pthread_prv_semantics.c paraver/pthread_prv_semantics.h \
 paraver/java_prv_semantics.c paraver/java_prv_semantics.h \
 paraver/cuda_prv_semantics.c paraver/cuda_prv_semantics.h \
 paraver/opencl_prv_semantics.c paraver/opencl_prv_semantics.h \
 paraver/openshmem_prv_semantics.c paraver/openshmem_prv_semantics.h \
 paraver/openacc_prv_semantics.c paraver/openacc_prv_semantics.h \
 paraver/gaspi_prv_semantics.c paraver/gaspi_prv_semantics.h \
 paraver/mpi_prv_events.c paraver/mpi_prv_events.h \
 paraver/omp_prv_events.c paraver/omp_prv_events.h \
 paraver/misc_prv_events.c paraver/misc_prv_events.h \
 paraver/pthread_prv_events.c paraver/pthread_prv_events.h \
 paraver/java_prv_events.c paraver/java_prv_events.h \
 paraver/cuda_prv_events.c paraver/cuda_prv_events.h \
 paraver/opencl_prv_events.c paraver/opencl_prv_events.h \
 paraver/openshmem_prv_events.c paraver/openshmem_prv_events.h \
 paraver/openacc_prv_events.c paraver/openacc_prv_events.h \
 paraver/gaspi_prv_events.c paraver/gaspi_prv_events.h \
 paraver/write_file_buffer.c paraver/write_file_buffer.h \
 paraver/paraver_nprintf.c paraver/paraver_nprintf.h \
 paraver/MPI_EventEncoding.h

libmpi2prv_la_SOURCES = \
 paraver/addr2types.c paraver/addr2types.h \
 paraver/addr2info.c paraver/addr2info.h \
 paraver/addr2info_hashcache.c paraver/addr2info_hashcache.h \
 paraver/file_set.c paraver/file_set.h \
 paraver/communication_queues.c paraver/communication_queues.h \
 paraver/mpi_comunicadors.c paraver/mpi_comunicadors.h \
 paraver/labels.c paraver/labels.h \
 paraver/HardwareCounters.c paraver/HardwareCounters.h \
 $(dimemas_FILES) \
 $(paraver_FILES) \
 $(common_FILES) \
 $(top_srcdir)/src/common/events.h \
 $(top_srcdir)/src/common/queue.h \
 $(top_srcdir)/src/common/record.h \
 $(top_srcdir)/src/common/trace_mode.h

mpi2prv_SOURCES = \
 merger.c

if HAVE_PAPI
if !HAVE_PAPI_EMBEDDED
  PAPI_LINKER_FLAGS = @PAPI_LDFLAGS@ @PAPI_RPATH@ @PAPI_LIBS@
endif
endif

libmpi2prv_la_CFLAGS = -I$(top_srcdir)/src/common -I$(top_srcdir)/include -I$(MERGER_INC)/common -I$(PRV_MERGER_INC) -I$(TRF_MERGER_INC) @MPI_CFLAGS@ @LIBADDR2LINE_CFLAGS@

mpi2prv_CFLAGS = -I$(top_srcdir)/src/common -I$(top_srcdir)/include -I$(MERGER_INC)/common -I$(PRV_MERGER_INC) -I$(TRF_MERGER_INC) @MPI_CFLAGS@ @LIBADDR2LINE_CFLAGS@
mpi2prv_LDFLAGS = libmpi2prv.la $(COMMON_LIB)/libcommon.la $(PAPI_LINKER_FLAGS)
if HAVE_LIBADDR2LINE
mpi2prv_LDADD = @LIBADDR2LINE_LDADD@
endif
mpi2prv_DEPENDENCIES = libmpi2prv.la $(COMMON_LIB)/libcommon.la

if HAVE_ZLIB
  libmpi2prv_la_CFLAGS += @LIBZ_CFLAGS@
  mpi2prv_CFLAGS += @LIBZ_CFLAGS@
  mpi2prv_LDFLAGS += @LIBZ_LDFLAGS@ @LIBZ_RPATH@ @LIBZ_LIBS@
endif

if HAVE_PAPI
if !HAVE_PAPI_EMBEDDED
    libmpi2prv_la_CFLAGS += @PAPI_CFLAGS@
    mpi2prv_CFLAGS += @PAPI_CFLAGS@
endif
endif

if HAVE_PMAPI
  mpi2prv_LDFLAGS += -lpmapi
endif

if HAVE_SIONLIB
  libmpi2prv_la_CFLAGS += @SIONLIB_CFLAGS@
  mpi2prv_CFLAGS += @SIONLIB_CFLAGS@
  mpi2prv_LDFLAGS += @SIONLIB_LDFLAGS@ @SIONLIB_RPATH@ -lsionmpi_64 -lsionser_64 -lsioncom_64 -lsioncom_64_lock_none
endif

# Online support
mpi2prv_CFLAGS += -DHAVE_ONLINE -I$(ONLINE_INC)
libmpi2prv_la_CFLAGS += -DHAVE_ONLINE -I$(ONLINE_INC)

install-exec-hook:
	cd $(DESTDIR)$(bindir) ; $(LN_S) -f mpi2prv$(EXEEXT) mpi2dim$(EXEEXT)

uninstall-local:
	rm $(DESTDIR)$(bindir)/mpi2dim$(EXEEXT)
