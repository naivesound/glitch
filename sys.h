#ifndef SYS_H
#define SYS_H

extern "C" {

#ifdef _WIN32
#include <windows.h>
#include <stdint.h>

int gettimeofday(struct timeval *tv, struct timezone *tz) {
  static const uint64_t EPOCH = ((uint64_t) 116444736000000000ULL);
  SYSTEMTIME st;
  FILETIME ft;
  uint64_t time;
  GetSystemTime(&st);
  SystemTimeToFileTime(&st, &ft);
  time = ((uint64_t) ft.dwLowDateTime) + (((uint64_t) ft.dwHighDateTime) << 32);
  tv->tv_sec = (long) ((time - EPOCH) / 10000000L);
  tv->tv_usec = (long) (st.wMilliseconds * 1000);
  return 0;
}

typedef HANDLE mutex_t;
static inline void mutex_create(mutex_t *m) { *m = CreateMutex(NULL, FALSE, NULL); }
static inline void mutex_destroy(mutex_t *m) { CloseHandle(*m); }
static inline void mutex_lock(mutex_t *m) { WaitForSingleObject(*m, INFINITE); }
static inline void mutex_unlock(mutex_t *m) { ReleaseMutex(*m); }
static inline void sleep_ms(long ms) { Sleep((DWORD) ms); }
#else
#include <sys/types.h>
#include <sys/time.h>
#include <pthread.h>
typedef pthread_mutex_t mutex_t;
static inline void mutex_create(mutex_t *m) { pthread_mutex_init(m, NULL); }
static inline void mutex_destroy(mutex_t *m) { pthread_mutex_destroy(m); }
static inline void mutex_lock(mutex_t *m) { pthread_mutex_lock(m); }
static inline void mutex_unlock(mutex_t *m) { pthread_mutex_unlock(m); }
static inline void sleep_ms(long ms) { usleep((unsigned long) (ms * 1000.0)); }
#endif

#include <sys/stat.h>
static inline time_t mtime(char *s) {
  struct stat st;
  if (stat(s, &st) < 0) {
    return -1;
  }
  return st.st_mtime;
}

}

#endif /* SYS_H */
