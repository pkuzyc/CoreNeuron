#include <nrnconf.h>

double celsius;
double t, dt;
int secondorder;
int stoprun;
int nrn_nthread;
NrnThread* nrn_threads;

char* pnt_name(Point_process* pnt) {
  return memb_func[pnt->type].sym;
}

void hoc_execerror(const char* s1, const char* s2) {
  printf("error: %s %s\n", s1, s2?s2:"");
  abort();
}

void hoc_warning(const char* s1, const char* s2) {
  printf("warning: %s %s\n", s1, s2?s2:"");
}

void* emalloc(size_t size) {
  void* memptr;
  memptr = malloc(size);
  assert(memptr);
  return memptr;
}

void* ecalloc(size_t n, size_t size) {
  void* p;
  if (n == 0) { return (void*)0; }
  p = calloc(n, size);
  assert(p);
  return p;
}

void* erealloc(void* ptr, size_t size) {
  void* p;
  if (!ptr) {
    return emalloc(size);
  }
  p = realloc(ptr, size);
  assert(p);
  return p;
}

void* nrn_cacheline_alloc(void** memptr, size_t size) {
#if HAVE_MEMALIGN
  if (posix_memalign(memptr, 64, size) != 0 {
    fprintf(stderr, "posix_memalign not working\n");
    assert(0);
#else
    *memptr = emalloc(size);
#endif
  return *memptr;
}

void* nrn_cacheline_calloc(void** memptr, size_t nmemb, size_t size) {
  int i, n;
#if HAVE_MEMALIGN
  nrn_cacheline_alloc(memptr, nmemb*size);
  memset(*memptr, 0, nmemb*size);
#else
  *memptr = ecalloc(nmemb, size);
#endif
  return *memptr;
}

