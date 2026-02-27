#include "proc/daemon.h"

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "core/log.h"

np_status_t daemonize(const char *pid_file) {
  pid_t pid = fork();
  if (pid < 0)
    return NP_ERR;
  if (pid > 0)
    _exit(0);

  if (setsid() < 0)
    return NP_ERR;

  pid = fork();
  if (pid < 0)
    return NP_ERR;
  if (pid > 0)
    _exit(0);

  umask(0);

  int devnull = open("/dev/null", O_RDWR);
  if (devnull >= 0) {
    dup2(devnull, STDIN_FILENO);
    dup2(devnull, STDOUT_FILENO);
    dup2(devnull, STDERR_FILENO);
    if (devnull > STDERR_FILENO)
      close(devnull);
  }

  if (pid_file && pid_file[0] != '\0') {
    FILE *fp = fopen(pid_file, "w");
    if (fp) {
      fprintf(fp, "%d\n", (int)getpid());
      fclose(fp);
    }
  }

  return NP_OK;
}

void pid_file_remove(const char *pid_file) {
  if (pid_file && pid_file[0] != '\0')
    unlink(pid_file);
}

pid_t pid_file_read(const char *pid_file) {
  if (!pid_file || pid_file[0] == '\0')
    return -1;

  FILE *fp = fopen(pid_file, "r");
  if (!fp)
    return -1;

  int pid = 0;
  if (fscanf(fp, "%d", &pid) != 1)
    pid = -1;

  fclose(fp);
  return (pid_t)pid;
}
