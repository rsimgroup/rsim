
/* includes exit and a few other simple functions that are one level
   away from traps */

#define MAX_ATEXITS 50
static int ATEXITCTR = 0;

static void (*atexitarr[MAX_ATEXITS])(void);

int atexit(void (*func)(void))
{
  if (ATEXITCTR >= MAX_ATEXITS)
    return -1;
  atexitarr[ATEXITCTR++] = func;
  return 0;
}

void exit(int status)
{
  int i;
  for (i=ATEXITCTR-1; i>=0; i--)
    {
      (*(atexitarr[i]))();
    }
  _exit(status);
}

int _mutex_lock()
{
  return 0;
}
int mutex_lock()
{
  return 0;
}
int _mutex_unlock()
{
  return 0;
}
int mutex_unlock()
{
  return 0;
}
int brk(unsigned endds)
{
  unsigned c;
  int i;
  c = (unsigned)sbrk(0);
  i = endds - c;
  if (i >= 0)
    sbrk(i);
  return 0;
}

int _brk(unsigned endds)
{
  unsigned c;
  int i;
  c = (unsigned)sbrk(0);
  i = endds - c;
  if (i >= 0)
    sbrk(i);
  return 0;
}

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

int creat(const char *path, mode_t mode)
{
  return open(path, O_WRONLY | O_CREAT | O_TRUNC, mode);
}

int _creat(const char *path, mode_t mode)
{
  return open(path, O_WRONLY | O_CREAT | O_TRUNC, mode);
}

int __creat(const char *path, mode_t mode)
{
  return open(path, O_WRONLY | O_CREAT | O_TRUNC, mode);
}

int ___errno(void)
{
  extern int errno;
  return errno;
}

