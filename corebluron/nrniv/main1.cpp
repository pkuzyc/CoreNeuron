#include <string.h>

#include "corebluron/nrnconf.h"
#include "corebluron/nrnoc/multicore.h"
#include "corebluron/nrnoc/nrnoc_decl.h"
#include "corebluron/nrnmpi/nrnmpi.h"
#include "corebluron/nrniv/nrniv_decl.h"
#include "corebluron/nrniv/output_spikes.h"

#define HAVE_MALLINFO 1
#if HAVE_MALLINFO
#include <malloc.h>
int nrn_mallinfo() {
  struct mallinfo m = mallinfo();
  return m.hblkhd + m.uordblks;
  return 0;
}
#else
int nrn_mallinfo() { return 0; }
#endif

void Get_MPIReduceInfo(long long int input, long long int &max, long long int &min, long long int &avg)
{
  avg = nrnmpi_int_allreduce(input, 1);
  avg = (long long int)(avg / nrnmpi_numprocs);
  max = nrnmpi_int_allreduce(input, 2);
  min = nrnmpi_int_allreduce(input, 3);
}

int main1(int argc, char** argv, char** env) {
  (void)env; /* unused */

  long long int memMax, memMin, memAvg;
//  printf("enter main1 mallinfo %d\n", nrn_mallinfo());
  nrnmpi_init(1, &argc, &argv);

  Get_MPIReduceInfo(nrn_mallinfo(), memMax, memMin, memAvg);
  if (nrnmpi_myid == 0)
    printf("after nrnmpi_init mallinfo: max %lld, min %lld, avg %lld\n", memMax, memMin, memAvg);

  mk_mech("bbcore_mech.dat");

  Get_MPIReduceInfo(nrn_mallinfo(), memMax, memMin, memAvg);
  if (nrnmpi_myid == 0)
    printf("after mk_mech mallinfo: max %lld, min %lld, avg %lld\n", memMax, memMin, memAvg);
  mk_netcvode();

  /// PatterStim option
  int need_patternstim = 0;
//  if (nrnmpi_numprocs == 1 && argc > 1 && strcmp(argv[1], "-pattern") == 0) {
  if (argc > 1 && strcmp(argv[1], "-pattern") == 0) {
    // One part done before call to nrn_setup. Other part after.
    need_patternstim = 1;
  }
  if (need_patternstim) {
    nrn_set_extra_thread0_vdata();
  }

  /// Reading essential inputs
  double tstop, maxdelay, voltage;
  int iSpikeBuf;
  char *str = new char[128];
  FILE *fp = fopen("inputs.dat","r");
  if (!fp)
  {
    if (nrnmpi_myid == 0)
      printf("\nWARNING! No input data, applying defaults...\n\n");

    t = 0.;
    tstop = 100.;
    dt = 0.025;
    celsius = 34.;
    voltage = -65.;
    maxdelay = 10.;
    iSpikeBuf = 400000;  
  }
  else
  {
    str = fgets(str, 128, fp);
    assert(fscanf(fp, "  StartTime\t%lf\n", &t) == 1);
    assert(fscanf(fp, "  EndTime\t%lf\n", &tstop) == 1);
    assert(fscanf(fp, "  Dt\t\t%lf\n\n", &dt) == 1);
    str = fgets(str, 128, fp);
    assert(fscanf(fp, "  Celsius\t%lf\n", &celsius) == 1);
    assert(fscanf(fp, "  Voltage\t%lf\n", &voltage) == 1);
    assert(fscanf(fp, "  MaxDelay\t%lf\n", &maxdelay) == 1);
    assert(fscanf(fp, "  SpikeBuf\t%d\n", &iSpikeBuf) == 1);
    fclose(fp);
  }
  delete [] str;

  /// Assigning threads to a specific task by the first gid written in the file
  fp = fopen("files.dat","r");
  if (!fp)
  {
    if (nrnmpi_myid == 0)
      printf("\nERROR! No input file with nrnthreads, exiting...\n\n");

    nrnmpi_barrier();
    return -1;
  }
  
  int iNumFiles;
  assert(fscanf(fp, "%d\n", &iNumFiles) == 1);

  int ngrp = 0;
  int* grp = new int[iNumFiles / nrnmpi_numprocs + 1];

  /// For each file written in bluron
  for (int iNum = 0; iNum < iNumFiles; ++iNum)
  {    
    int iFile;
    assert(fscanf(fp, "%d\n", &iFile) == 1);
    if ((iNum % nrnmpi_numprocs) == nrnmpi_myid)
    {
      grp[ngrp] = iFile;
      ngrp++;
    }
  }
  fclose(fp);

  /// Reading the files and setting up the data structures
  Get_MPIReduceInfo(nrn_mallinfo(), memMax, memMin, memAvg);
  if (nrnmpi_myid == 0)
    printf("before nrn_setup mallinfo: max %lld, min %lld, avg %lld\n", memMax, memMin, memAvg);
  nrn_setup(ngrp, grp, ".");

  Get_MPIReduceInfo(nrn_mallinfo(), memMax, memMin, memAvg);
  if (nrnmpi_myid == 0)
    printf("after nrn_setup mallinfo: max %lld, min %lld, avg %lld\n", memMax, memMin, memAvg);

  delete [] grp;


  /// Invoke PatternStim
  if (need_patternstim) {
    nrn_mkPatternStim("out.std");
  }


  double mindelay = BBS_netpar_mindelay(maxdelay);
  if (nrnmpi_myid == 0)
    printf("mindelay = %g\n", mindelay);

  mk_spikevec_buffer(iSpikeBuf);

  nrn_finitialize(1, voltage);
  Get_MPIReduceInfo(nrn_mallinfo(), memMax, memMin, memAvg);
  if (nrnmpi_myid == 0)
    printf("after finitialize mallinfo: max %lld, min %lld, avg %lld\n", memMax, memMin, memAvg);

  /// Solver execution
  double time = nrnmpi_wtime();
  BBS_netpar_solve(tstop);//17.015625);
  nrnmpi_barrier();

  if (nrnmpi_myid == 0)
    printf("Time to solution: %g\n", nrnmpi_wtime() - time);

  /// Outputting spikes
  output_spikes();

  nrnmpi_barrier();

  nrnmpi_finalize();

  return 0;
}

const char* nrn_version(int) {
  return "version id unimplemented";
}
