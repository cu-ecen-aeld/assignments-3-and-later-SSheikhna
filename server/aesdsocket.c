#include <stdio.h>
#include <syslog.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/queue.h>
#include <sys/epoll.h>
#include <arpa/inet.h>
#include <signal.h>
#include <errno.h>
#include <stdbool.h>
#include <pthread.h>
#include <time.h>


#define PORT 9000
#define TMP_FILE "/var/tmp/aesdsocketdata"
#define BUFF_SIZE 1024
#define MAX_EVENTS 100



typedef struct thread_data{
  pthread_t  thread_id;
  pthread_mutex_t thread_mutex;
  int thread_client;
  bool flag;
  struct sockaddr_in thread_client_addr;
}thread_data_t;

pthread_mutex_t threads_general_mutex;

typedef struct thread_data2{
  pthread_t  thread_id;
  pthread_mutex_t thread_mutex;
}thread_data_t2;

struct socket_init_data{
  int *serverfd;
  int *epollfd;
  struct epoll_event *event;
  struct sockaddr_in *serverfd_addr;
  int argc;
  char *argv[];
};

typedef struct node
{
  thread_data_t *thread;
  TAILQ_ENTRY(node) nodes;
}node_t;

TAILQ_HEAD(head_s, node) head;
volatile bool flag_exit = false;


static void signal_handler(int sig_num)
{
  flag_exit=true;
}

bool lock_unlock_mutex(bool lock)
{
  int ret = 0;
  if (lock)
  {
    ret = pthread_mutex_lock(&threads_general_mutex);
    if (ret != 0)
    {
      syslog(LOG_ERR, "pthread_mutex_lock failed with %d", ret);
      return false;
    }
  }
  else
  {
    ret = pthread_mutex_unlock(&threads_general_mutex);
    if (ret != 0)
    {
      syslog(LOG_ERR, "pthread_mutex_lock failed with %d", ret);
      return false;
    }
  }
  return true;
}

bool lock_unlock_mutex_tail(bool lock)
{
  int ret = 0;
  if (lock)
  {
    ret = pthread_mutex_lock(&threads_general_mutex);
    if (ret != 0)
    {
      syslog(LOG_ERR, "pthread_mutex_lock failed with %d", ret);
      return false;
    }
  }
  else
  {
    ret = pthread_mutex_unlock(&threads_general_mutex);
    if (ret != 0)
    {
      syslog(LOG_ERR, "pthread_mutex_lock failed with %d", ret);
      return false;
    }
  }
  return true;
}

bool write_to_file(char buffer[], int length)
{
  lock_unlock_mutex(true);
  int my_file = open(TMP_FILE, O_WRONLY | O_APPEND | O_CREAT, 0644);
  if (my_file < 0)
  {
    syslog(LOG_ERR, "Error opening file %s", TMP_FILE);
    lock_unlock_mutex(false);
    return false;
  }

  int bytes_written = write(my_file, buffer, length);
  if (bytes_written < 0)
  {
    syslog(LOG_ERR, "Error writing to %s", TMP_FILE);
    lock_unlock_mutex(false);
    return false;
  }
  close(my_file);
  lock_unlock_mutex(false);
  return true;
}

int receive_and_write_to_file(thread_data_t *thread)
{
  char buffer[BUFF_SIZE] = {0};
  int bytes_read;
  while(1)
  {
    memset(buffer, 0, BUFF_SIZE);
    lock_unlock_mutex(true);
    bytes_read = recv(thread->thread_client, buffer, BUFF_SIZE, 0);
    lock_unlock_mutex(false);
    if (bytes_read <= 0)
    {
      if (bytes_read == 0) syslog(LOG_DEBUG, "Client disconnected");
      else syslog(LOG_ERR, "Error receiving data from client");
      close(thread->thread_client);
      return -1;
    }
    else
    {
      bool ret = write_to_file(buffer, bytes_read);
      if ((bytes_read <= BUFF_SIZE) && (buffer[bytes_read-1] == '\n') && ret)
      {
        return 0;
      }
    }
  }
}

int read_file_and_send_to_client(thread_data_t *thread)
{
  char buffer[BUFF_SIZE] = {0};
  int my_file;
  
  lock_unlock_mutex(true);
  my_file = open(TMP_FILE, O_RDONLY);
  while (1)
  {
    ssize_t bytesRead = read(my_file, buffer, BUFF_SIZE);
    if (bytesRead <= 0) break;
    ssize_t bytesSent = send(thread->thread_client, buffer, bytesRead, 0);
    if (bytesSent == -1)
    {
      syslog(LOG_ERR, "Error sending %s to %d", TMP_FILE, thread->thread_client_addr.sin_addr.s_addr);
      lock_unlock_mutex(false);
      return -1;
    }
  }
  close(my_file);
  lock_unlock_mutex(false);
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


int set_to_non_blocking(int socket)
{
  int ret = fcntl(socket, F_GETFL, 0);
  if (ret >= 0) ret = fcntl(socket, F_SETFL, ret | O_NONBLOCK);
  return ret;
}



int initialize_server(int *serverfd, struct sockaddr_in *serverfd_addr, int *epollfd, struct epoll_event *event, int argc, char *argv[])
{
  int ret, opt = 1;
  struct sigaction my_action;
  struct epoll_event events[MAX_EVENTS];
  socklen_t addrlen = (socklen_t)sizeof(*serverfd_addr);

  flag_exit=false;
  memset(&my_action, 0, sizeof(struct sigaction));
  my_action.sa_handler = signal_handler;
  sigaction(SIGINT, &my_action, NULL);
  sigaction(SIGTERM, &my_action, NULL);
  
  *serverfd = socket(AF_INET, SOCK_STREAM, 0);
  if (*serverfd == -1)
  {
    syslog(LOG_ERR, "socket call failed");
    return -1;
  }
  set_to_non_blocking(*serverfd);

  memset(serverfd_addr, 0, sizeof(*serverfd_addr));
  serverfd_addr->sin_family = AF_INET;
  serverfd_addr->sin_addr.s_addr = INADDR_ANY;
  serverfd_addr->sin_port = htons(PORT);
  setsockopt(*serverfd, SOL_SOCKET, SO_REUSEADDR|SO_REUSEPORT, &opt, sizeof(opt));

  ret = bind(*serverfd, (struct sockaddr *)serverfd_addr, addrlen);
  if (ret != 0)
  {
    syslog(LOG_ERR, "socket bind call failed");
    return -1;
  }

  check_and_switch_to_daemon(argc, argv);
  
  ret = listen(*serverfd, 100);
  if (ret != 0)
  {
    syslog(LOG_ERR, "socket listen call failed");
    return -1;
  }

  *epollfd = epoll_create1(0);
  if (ret != 0)
  {
    syslog(LOG_ERR, "epoll_create1 call failed");
    return -1;
  }
  
  event->events = EPOLLIN;
  event->data.fd = *serverfd;
  ret = epoll_ctl(*epollfd, EPOLL_CTL_ADD, *serverfd, event);
  if (ret != 0)
  {
    syslog(LOG_ERR, "epoll_ctl call failed");
    return -1;
  }

  return 0;
}

void* thread_hanlder(void* thread_param)
{
  thread_data_t *my_thread_data = (thread_data_t *) thread_param;
  int ret = receive_and_write_to_file(my_thread_data);
  if (ret == 0)
  {
    ret = read_file_and_send_to_client(my_thread_data);
    if (ret == 0) my_thread_data->flag = true;
  }
  if (my_thread_data->flag)
  {
    ret = close(my_thread_data->thread_client);
    if (!ret) syslog(LOG_DEBUG, "Closed client connection from %d", my_thread_data->thread_client_addr.sin_addr.s_addr);
  }
  pthread_exit(NULL);
}

void *thread_time_hanlder(void*)
{
  int ret;
  struct timespec ts;
  time_t     prec_time=0;
  time_t     cur_time=0;
  char buffer[BUFF_SIZE] = "timestamp:";
  struct tm *timeinfo;
  
  while(1)
  {
    if (!flag_exit)
    {
      clock_gettime(CLOCK_REALTIME, &ts);
      if (ts.tv_sec - prec_time >= 10)
      {
        cur_time = ts.tv_sec;
        timeinfo = localtime(&cur_time);
        strftime(buffer, BUFF_SIZE, "%F - %H:%M:%S%n", timeinfo);
        char timestamp_msg[BUFF_SIZE] = "timestamp:";
        strcat(timestamp_msg,buffer);
        int bytes_to_write = 32;
        bool ret = write_to_file(timestamp_msg, bytes_to_write);
        if (!ret) pthread_exit(NULL);
        prec_time = ts.tv_sec;
      }
    }
    else
    {
      syslog(LOG_DEBUG, "Caught signal in timestamp thread, exiting");
      break;
    }
  }
  pthread_exit(NULL);
}

bool start_thread(int *client, struct sockaddr_in *client_addr)
{
  int ret;
  thread_data_t *my_thread_data = malloc(sizeof(thread_data_t));
  if (!my_thread_data)
  {
    syslog(LOG_ERR, "thread_data_t malloc failed");
    return false;
  }
  my_thread_data->thread_client = *client;
  my_thread_data->thread_client_addr = *client_addr;
  my_thread_data->flag = false;
  ret = pthread_create(&my_thread_data->thread_id, NULL, thread_hanlder, my_thread_data);
  if (ret != 0)
  {
    syslog(LOG_ERR, "pthread_create failed");
    free(my_thread_data);
    return false;
  }
  node_t *my_node = malloc(sizeof(*my_node));
  if (my_node == NULL)
  {
    syslog(LOG_ERR, "node_t malloc failed");
    return false;
  }
  my_node->thread = my_thread_data;
  if (!lock_unlock_mutex(true)) return false;
  TAILQ_INSERT_TAIL(&head, my_node, nodes);
  if (!lock_unlock_mutex(false)) return false;
  node_t *node_obj;
  TAILQ_FOREACH(node_obj, &head, nodes)
  {
    if (node_obj->thread->flag == false) return false;
  }
  ret = pthread_join(my_node->thread->thread_id, NULL);
  return true;
}

bool start_thread2(int *client, struct sockaddr_in *client_addr)
{
  int ret;
  thread_data_t *my_thread_data = malloc(sizeof(thread_data_t));
  if (!my_thread_data)
  {
    syslog(LOG_ERR, "thread_data_t malloc failed");
    return false;
  }
  my_thread_data->thread_client = *client;
  my_thread_data->thread_client_addr = *client_addr;
  my_thread_data->flag = false;
  ret = pthread_create(&my_thread_data->thread_id, NULL, thread_hanlder, my_thread_data);
  if (ret != 0)
  {
    syslog(LOG_ERR, "pthread_create failed");
    free(my_thread_data);
    return false;
  }
  node_t *my_node = malloc(sizeof(*my_node));
  if (my_node == NULL)
  {
    syslog(LOG_ERR, "node_t malloc failed");
    return false;
  }
  my_node->thread = my_thread_data;
  if (!lock_unlock_mutex(true)) return false;
  TAILQ_INSERT_TAIL(&head, my_node, nodes);
  if (!lock_unlock_mutex(false)) return false;
  node_t *node_obj;
  TAILQ_FOREACH(node_obj, &head, nodes)
  {
    if (node_obj->thread->flag == false) return false;
  }
  ret = pthread_join(my_node->thread->thread_id, NULL);
  *client = 0; //tmp
  return true;
}


int main(int argc, char *argv[])
{
  openlog("aesdsocket", LOG_NDELAY, LOG_USER);

  int my_server=0, my_client=0, my_file=0, my_epoll=0, ret=0, opt = 1;
  pthread_t  thread_id;
  struct epoll_event my_event, events[MAX_EVENTS];
  struct sockaddr_in my_server_addr;
  struct sockaddr_in my_client_addr;
  socklen_t my_addrlen = (socklen_t)sizeof(my_server_addr);
  
  TAILQ_INIT(&head);
  pthread_mutex_init(&threads_general_mutex, NULL);
  ret = initialize_server(&my_server, &my_server_addr, &my_epoll, &my_event, argc, argv);
  if (ret == -1) return -1;
  
  if (pthread_create(&thread_id, NULL, thread_time_hanlder, NULL) != 0)
  {
    syslog(LOG_ERR, "pthread_create failed");
    return -1;
  }
  while(1)
  {
    int fd_numbers = epoll_wait(my_epoll, events, MAX_EVENTS, -1);
    if (!flag_exit)
    {
      for (int i=0; i<fd_numbers; i++)
      {
        if (events[i].data.fd == my_server)
        {
          my_client = accept(my_server, (struct sockaddr *)&my_client_addr, &my_addrlen);
          if (my_client == -1)
          {
            syslog(LOG_ERR, "socket accept client call failed");
            return -1;
          }
          syslog(LOG_DEBUG, "Accepted connection from %d", my_client_addr.sin_addr.s_addr);
          //printf("New connection: socket fd %d, IP %s, port %d\n", my_client, inet_ntoa(my_client_addr.sin_addr), ntohs(my_client_addr.sin_port));
          set_to_non_blocking(my_client);
          my_event.events = EPOLLIN | EPOLLET;
          my_event.data.fd = my_client;
          epoll_ctl(my_epoll, EPOLL_CTL_ADD, my_client, &my_event);
        }
        else
        {
          my_client = events[i].data.fd;
          start_thread(&my_client, &my_client_addr);
        }
      }
    }
    else
    {
      syslog(LOG_DEBUG, "Caught signal, exiting");
      break;
    }
  }
  
  node_t *my_node;
  while (!TAILQ_EMPTY(&head))
  {
    my_node = TAILQ_FIRST(&head);
    if (!pthread_join(my_node->thread->thread_id, NULL)) free(my_node->thread);
    TAILQ_REMOVE(&head, my_node, nodes);
    free(my_node);
  }
  if (!pthread_join(thread_id, NULL)) pthread_mutex_destroy(&threads_general_mutex);
  if (!close(my_server)) syslog(LOG_DEBUG, "Closed server connection from %d", my_server_addr.sin_addr.s_addr);
  if (!close(my_epoll)) syslog(LOG_DEBUG, "Closed epoll from %d", my_epoll);
  if (!remove(TMP_FILE)) syslog(LOG_DEBUG, "Removed %s", TMP_FILE);
  flag_exit=false;
  closelog();
  return 0;
}

