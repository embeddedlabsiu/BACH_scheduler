#ifndef UTILS_H__
#define UTILS_H__

#define MIN(x,y) ((x < y) ? (x) : (y))

#define emsg(fmt,args...) \
  fprintf(stderr, "\033[31mERROR %s @%s:%d =>" fmt "\033[0m" , __FUNCTION__, __FILE__, __LINE__, ##args); \

// set -rdynamic for this to work
#include <execinfo.h>
static inline void
print_trace(void)
{
#if 0
#define static
	void *array[128];
	size_t size;
	char **strings;
	size_t i;

	size = backtrace (array, 128);
	strings = backtrace_symbols (array, size);
	if (strings == NULL){
		perror("backtrace_symbols");
		exit(1);
	}

	printf ("Obtained %zd stack frames.\n", size);
	for (i = 0; i < size; i++) {
		printf ("%s\n", strings[i]);
	}
	free (strings);
#endif
}

#define die(fmt,args...)      \
	do {                      \
		fprintf(stderr, "\033[31mERROR %s @%s:%d => " fmt "\033[0m" , __FUNCTION__, __FILE__, __LINE__, ##args); \
		print_trace();        \
		exit(1);              \
	} while (0)

#define perrdie(pmsg, fmt,args...) \
	do {                           \
		fprintf(stderr, "\033[31mERROR %s @%s:%d => " fmt "\033[0m", __FUNCTION__, __FILE__, __LINE__, ##args); \
		print_trace();             \
		perror(pmsg);              \
		exit(1);                   \
	} while (0)

#endif /* UTILS_H__ */
