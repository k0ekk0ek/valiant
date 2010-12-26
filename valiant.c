/* system includes */
#include <confuse.h>
#include <glib.h>
#include <glib/gthread.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>

/* valiant includes */
#include "check.h"


/* command line options */

char *config_file;
char *pid_file;

/* configuration file options */

//pid_t
//daemonize (void)
//{
//  // IMPLEMENT
//}
//
//void
//write_pid_file (pid_t pid)
//{
//  // IMPLEMENT
//}


void
worker (gpointer data, gpointer user_data)
{
  // right... this is the thread, so data is the filehandle
  // use request function to read the request from the socket... filehandle

  checklist_t *chks = (checklist_t *) user_data;

  request_t *request;
  gint sock;

  sock = *(gint *) data;

  g_printf ("got filehandle %d\n", sock);


  request = request_from_socket (sock);

  // will print this as debug
  g_printf ("got request\n");
  g_printf ("helo_name: %s, sender: %s, client_address: %s, client_name: %s, reverse_client_name: %s\n",
            request->helo_name,
            request->sender,
            request->client_address,
            request->client_name,
            request->reverse_client_name);

  /* so far, so good! */
  /* not... lets get those checks working! */

  // lets create nice new score
  score_t *score = score_new ();


  check_t *check;
  checklist_t *cur;

  for (cur=chks; cur; cur=cur->next) {
    check = cur->check;

    gint bla = check->check_fn (check, request, score);

    g_printf ("%s: got returned %d\n", __func__, bla);
  }


  write (sock, "DUNNO\n", 6);

  close (sock);

/*
  char *helo_name;
  char *sender;
  char *sender_domain;
  char *client_address;
  char *client_name;
  char *reverse_client_name;
*/
}


int
main (int argc, char *argv[])
{
  g_thread_init (NULL);
  config_file = NULL;
  pid_file = NULL;


  int c;

  /* parse command line options */
  while ((c = getopt (argc, argv, "c:p:")) != -1)
    switch (c)
    {
      case 'c':
        config_file = optarg;
        break;
      case 'p':
        pid_file = optarg;
        break;
      case '?':
        if (optopt == 'c'
         || optopt == 'p')
          fprintf (stderr, "Option -%c requires an argument.\n", optopt);
        else if (isprint (optopt))
          fprintf (stderr, "Unknown option `-%c'.\n", optopt);
        else
          fprintf (stderr, "Unknown option character `\\x%x'.\n", optopt);
          return 1;
      default:
        abort ();
    }

  printf ("config_file: %s, pid_file: %s\n", config_file, pid_file);

  /* check configuration options... later */
  cfg_opt_t check_opts[] = {
    CFG_STR ("type", 0, CFGF_NODEFAULT),
    CFG_STR ("attribute", 0, CFGF_NODEFAULT),
    CFG_STR ("format", 0, CFGF_NODEFAULT),
    CFG_FLOAT ("positive", CHECK_POSITIVE_SCORE, CFGF_NONE),
    CFG_FLOAT ("negative", CHECK_NEGATIVE_SCORE, CFGF_NONE),
    CFG_BOOL ("case-sensitive", cfg_false, CFGF_NONE),
    CFG_END ()
  };

  cfg_opt_t opts[] =
  {
    CFG_SEC("check", check_opts, CFGF_MULTI | CFGF_TITLE),
    CFG_END()
  };

  cfg_t *cfg = cfg_init (opts, CFGF_NONE);
  cfg_parse (cfg, config_file);

  checklist_t *chks = cfg_to_checklist (cfg);

  // now daemonize if not debug
  // bind to socket
  // create nice new thread pool

  GError *error = NULL;

  GThreadPool *pool = g_thread_pool_new (&worker, chks, 10, TRUE, &error);

  // XXX: start working on thread pool here!


     int sockfd, newsockfd, clilen;
     //char buffer[256];
     struct sockaddr_in serv_addr, cli_addr;
     int n;
     //if (argc < 2) {
     //    fprintf(stderr,"ERROR, no port provided\n");
     //    exit(1);
     //}
     sockfd = socket(AF_INET, SOCK_STREAM, 0);
     if (sockfd < 0) 
        g_error("ERROR opening socket");
     bzero((char *) &serv_addr, sizeof(serv_addr));
     //portno = atoi("1234");
     serv_addr.sin_family = AF_INET;
     serv_addr.sin_addr.s_addr = INADDR_ANY;
     serv_addr.sin_port = htons(1234);
     if (bind(sockfd, (struct sockaddr *) &serv_addr,
              sizeof(serv_addr)) < 0) 
              g_error("ERROR on binding");
     listen(sockfd,5);
     clilen = sizeof(cli_addr);
     
     while ((newsockfd = accept(sockfd, 
                 (struct sockaddr *) &cli_addr, 
                 &clilen))) {

      gint *ptr = g_malloc0 (sizeof (gint));
      *ptr = newsockfd;

      g_printf ("got connection\n");

      g_thread_pool_push (pool, (void *) ptr, &error);

     //if (newsockfd < 0) 
     //     error("ERROR on accept");
     //bzero(buffer,256);
     //n = read(newsockfd,buffer,255);
     //if (n < 0) error("ERROR reading from socket");
     //printf("Here is the message: %s\n",buffer);
     //n = write(newsockfd,"I got your message",18);
     //if (n < 0) error("ERROR writing to socket");
     //return 0; 

  }

  return 0;
}

