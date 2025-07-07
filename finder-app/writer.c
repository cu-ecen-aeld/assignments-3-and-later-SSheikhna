#include <stdio.h>
#include <syslog.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>


int main(int argc, char *argv[])
{
  openlog("writer", LOG_NDELAY, LOG_USER);
  if (argc != 3)
  {
    syslog(LOG_ERR, "2 args required (file, string)\n");
    closelog();
    return 1;
  }
  const char *_file = argv[1];
  const char *_string = argv[2];
  syslog(LOG_DEBUG, "Opening %s\n", _file);
  int fd = open(_file, O_RDWR|O_CREAT,S_IRWXU|S_IRWXG|S_IRWXO);
  if (fd==-1)
  {
    syslog(LOG_ERR, "Unable to open %s\n", _file);
    closelog();
    return 1;
  }
  syslog(LOG_DEBUG, "Writing %s to %s\n", _string, _file);
  ssize_t wr = write(fd, _string, strlen(_string));
  if (wr==-1)
  {
    syslog(LOG_ERR, "Unable to write %s to %s\n", _string, _file);
    closelog();
    return 1;
  }
  syslog(LOG_DEBUG, "Closing %s\n", _file);
  close(fd);
  closelog();
  return 0;
}
