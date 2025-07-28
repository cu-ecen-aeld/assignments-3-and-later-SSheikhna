#include <stdio.h>
#include <syslog.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <signal.h>
#include <errno.h>
#include <stdbool.h>


#define PORT 9000
#define TMP_FILE "/var/tmp/aesdsocketdata"
#define BUFF_SIZE 1024



volatile bool flag_exit = false;
static void signal_handler(int sig_num)
{
  flag_exit=true;
}

int receive_and_write_to_file(int client)
{
  char buffer[BUFF_SIZE] = {0};
  int my_file;

  while(1)
  {
    memset(buffer, 0, sizeof(buffer));
    int bytes_read = recv(client, buffer, sizeof(buffer), 0);
    if (bytes_read <= 0)
    {
      if (bytes_read == 0)
      {
        syslog(LOG_DEBUG, "Client disconnected");
      }
      else
      {
        syslog(LOG_ERR, "Error receiving data from client");
      }
      return -1;
    }
    else
    {
      my_file = open(TMP_FILE, O_WRONLY | O_APPEND | O_CREAT, 0644);
      if (my_file < 0)
      {
        syslog(LOG_ERR, "Error opening file %s", TMP_FILE);
      }

      int bytes_written = write(my_file, buffer, bytes_read);
      if (bytes_written < 0)
      {
        syslog(LOG_ERR, "Error writing to %s", TMP_FILE);
      }
      close(my_file);
      if ((bytes_read <= BUFF_SIZE) && (buffer[bytes_read-1] == '\n'))
      {
        return 0;
      }
    }
  }
}

int read_file_and_send_to_client(int client, struct sockaddr_in address)
{
  char buffer[BUFF_SIZE] = {0};
  int my_file;
  
  my_file = open(TMP_FILE, O_RDONLY);
  while (1)
  {
    ssize_t bytesRead = read(my_file, buffer, sizeof(buffer));
    if (bytesRead <= 0)
    {
      break;
    }
    ssize_t bytesSent = send(client, buffer, bytesRead, 0);
    if (bytesSent == -1)
    {
      syslog(LOG_ERR, "Error sending %s to %d", TMP_FILE, address.sin_addr.s_addr);
      return -1;
    }
  }
  close(my_file);
  return 0;
}

void check_and_switch_to_daemon(int argc, char *argv[])
{
  if (argc == 2)
  {
    if (!strcmp(argv[1], "-d"))
    {
      pid_t fpid = fork();
      if (fpid < 0)
      {
        syslog(LOG_ERR, "Error forking");
        exit(EXIT_FAILURE);
      }
      if (fpid > 0)
      {
        exit(EXIT_SUCCESS);
      }
    }
  }
}



int main(int argc, char *argv[])
{
  int my_server, my_client, my_file, ret, opt = 1;
  struct sigaction my_action;
  struct sockaddr_in my_server_addr;
  struct sockaddr_in my_client_addr;
  socklen_t my_addrlen = (socklen_t)sizeof(my_server_addr);
  char buffer[BUFF_SIZE] = {0};


  openlog("aesdsocket", LOG_NDELAY, LOG_USER);
  memset(&my_action, 0, sizeof(struct sigaction));
  my_action.sa_handler = signal_handler;
  sigaction(SIGINT, &my_action, NULL);
  sigaction(SIGTERM, &my_action, NULL);
  
  my_server = socket(AF_INET, SOCK_STREAM, 0);
  if (my_server == -1)
  {
    syslog(LOG_ERR, "socket call failed");
    return -1;
  }

  my_server_addr.sin_family = AF_INET;
  my_server_addr.sin_addr.s_addr = INADDR_ANY;
  my_server_addr.sin_port = htons(PORT);
  setsockopt(my_server, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

  ret = bind(my_server, (struct sockaddr *)&my_server_addr, my_addrlen);
  if (ret != 0)
  {
    syslog(LOG_ERR, "socket bind call failed");
    return -1;
  }

  check_and_switch_to_daemon(argc, argv);
  
  ret = listen(my_server, 10);
  if (ret != 0)
  {
    syslog(LOG_ERR, "socket listen call failed");
    return -1;
  }

  int restart = 0;
  while(1)
  {
    my_client = accept(my_server, (struct sockaddr *)&my_client_addr, &my_addrlen);
    if (!flag_exit)
    {
      if (my_client == -1)
      {
        syslog(LOG_ERR, "socket accept client call failed");
        return -1;
      }
      syslog(LOG_DEBUG, "Accepted connection from %d", my_client_addr.sin_addr.s_addr);
      ret = receive_and_write_to_file(my_client);
      if (ret == 0)
      {
        ret = read_file_and_send_to_client(my_client, my_client_addr);
        if (!restart && !ret)
        {
          close(my_client);
          syslog(LOG_DEBUG, "Closed connection from %d", my_client_addr.sin_addr.s_addr);
          restart = 1;
        }
      }
    }
    else
    {
      syslog(LOG_DEBUG, "Caught signal, exiting");
      break;
    }
  }

  syslog(LOG_DEBUG, "Closed connection from %d", my_client_addr.sin_addr.s_addr);
  ret= close(my_server);
  syslog(LOG_DEBUG, "Closed connection from %d", my_server_addr.sin_addr.s_addr);
  remove(TMP_FILE);
  syslog(LOG_DEBUG, "Removed %s", TMP_FILE);
  closelog();
  return 0;
}
