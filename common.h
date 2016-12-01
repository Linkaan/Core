#define log_error(msg) \
		fprintf(stderr, "%s: %s: %d: %s: %s\n", __progname, \
				__FILE__, __LINE__, msg, strerror(errno))

typedef struct
  {
  	int 			timerfd;
  	int 			timerpipe[2];
  	pthread_t 		timer_t;
	pthread_attr_t 	attr;
  } thread_data;