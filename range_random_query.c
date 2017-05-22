/* gcc id_query_loop_test.c -o id_query_loop_test $(pkg-config --cflags --libs
 * libmongoc-1.0) */

/* ./id_query_loop_test [CONNECTION_STRING [COLLECTION_NAME]] */

#include <mongoc.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/time.h>
#include "range_random_query_opts.h"

static volatile sig_atomic_t got_exit_alarm = 0;

void print_usage(FILE* fstr) {
  fprintf(fstr, "Usage: id_query_loop_test [options] <file of ids to query>\n");
}

void print_desc() {
  printf("In a loop will query db.collection.find({ _id: <decimal_id_value>}) \n\
  and output the id, the time, the time elapsed in microsecs, and the matching \n\
  document's byte size.\n\
  The final line of output will be slowest times by several percentiles.\n");
}

static void sigalrmHandler(int sig) {
   got_exit_alarm = 1;
}

void set_process_exit_timer(double timer_secs) {
   struct itimerval new, old;
   struct sigaction sa;

   if (timer_secs <= 0) {
      fprintf(stderr, "set_process_exit_timer() argument must be > 0. Aborting.\n");
      exit(EXIT_FAILURE);
   }

   sigemptyset(&sa.sa_mask);
   sa.sa_flags = 0;
   sa.sa_handler = sigalrmHandler;
   if (sigaction(SIGALRM, &sa, NULL) == -1) {
      fprintf(stderr, "set_process_exit_timer() has failed to set handler for SIGALRM. Aborting.\n");
      exit(EXIT_FAILURE);
   }
   
   new.it_interval.tv_sec = 0;
   new.it_interval.tv_usec = 0;
   new.it_value.tv_sec = (int)timer_secs;
   new.it_value.tv_usec = (int)((timer_secs - new.it_value.tv_sec) * 1000000.0f);
   if (setitimer(ITIMER_REAL, &new, &old) == -1) {
      fprintf(stderr, "set_process_exit_timer() has failed to set interval timer. Aborting.\n");
      exit(EXIT_FAILURE);
   }
}

int
main (int argc, char *argv[])
{
   int opt_err_flag = 0;
   int nonopt_arg_idx = parse_cmd_options(argc, argv, &opt_err_flag);

   if (help_flag) {
      print_usage(stdout);
      printf("\n");
      print_desc();
      printf("\n");
      print_options_help();
      free_options();
      exit(EXIT_SUCCESS);
   } else if (opt_err_flag) {
      print_usage(stderr);
      exit(EXIT_FAILURE);
   }
//dump_cmd_options();
//exit(EXIT_SUCCESS);

   if (!conn_uri || !database_name || !collection_name) {
      fprintf(stderr, "Aborting. One or more of the neccesary --conn-uri, --database and --collection arguments was absent.\n");
      fprintf(stderr, "Try --help for options description\n");
      print_usage(stderr);
      exit(EXIT_FAILURE);
   }
   
   if (run_interval > 0) {
      set_process_exit_timer(run_interval);
   }

   mongoc_client_t *client;
   mongoc_collection_t *collection;
   mongoc_cursor_t *cursor;
   bson_error_t error;
   const bson_t *doc;
   bson_t query;

   mongoc_init();

   client = mongoc_client_new(conn_uri);

   if (!client) {
      fprintf (stderr, "Failed to parse URI, or otherwise establish mongo connection.\n");
      return EXIT_FAILURE;
   }

   mongoc_client_set_error_api (client, 2);

   collection = mongoc_client_get_collection (client, database_name, collection_name);

   size_t i = 0;
   size_t range_sz = max_id + 1 - min_id;
   long sum_rtt_ms = 0;

   while (!got_exit_alarm) {

	  long curr_id = (rand() % range_sz) + min_id;
      bson_init (&query);
      bson_append_int64(&query, fieldname, -1, curr_id);
   
         struct timeval start_tp;
         gettimeofday(&start_tp, NULL);

      cursor = mongoc_collection_find_with_opts (
         collection,
         &query,
         NULL,  /* additional options */
         NULL); /* read prefs, NULL for default */

      bool cursor_next_ret = mongoc_cursor_next (cursor, &doc);
      struct timeval end_tp;
      gettimeofday(&end_tp, NULL);
      sum_rtt_ms += ((end_tp.tv_sec - start_tp.tv_sec) * 1000) + ((end_tp.tv_usec - start_tp.tv_usec) / 1000);
      if (!cursor_next_ret) {
         fprintf (stderr, "No document for { \"%s\":: %ld } was found\n", fieldname, curr_id);
      }

      if (mongoc_cursor_error (cursor, &error)) {
         fprintf (stderr, "Cursor Failure: %s\n", error.message);
         return EXIT_FAILURE;
      }

      bson_destroy (&query);
      mongoc_cursor_destroy (cursor);

      ++i;
   } //end while(true)

   mongoc_collection_destroy (collection);
   mongoc_client_destroy (client);

   mongoc_cleanup ();

   fprintf(stdout, "Count = %lu, Median = %fms\n", i, (double)sum_rtt_ms / i);

   free_options();
   return EXIT_SUCCESS;
}
