#define log_error(msg) \
		fprintf(stderr, "%s: %s: %d: %s: %s\n", __progname, \
				__FILE__, __LINE__, msg, strerror(errno))

typedef struct
  {
  	int timerfd;
  	int timerpipe[2];
  } thread_data;