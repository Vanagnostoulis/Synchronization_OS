#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <assert.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include "mandel-lib.h"
#include <semaphore.h>

//////////////////////////////////////////////////////////////////////
////////////////////// START OF GIVEN CODE ///////////////////////////
//////////////////////////////////////////////////////////////////////
#define perror_pthread(ret, msg) \
	do { errno = ret; perror(msg); } while (0)
#define MANDEL_MAX_ITERATION 100000
// Output at the terminal is is x_chars wide by y_chars long
int y_chars = 50;
int x_chars = 90;
// The part of the complex plane to be drawn:
//upper left corner is (xmin, ymax), lower right corner is (xmax, ymin)
double xmin = -1.8, xmax = 1.0;
double ymin = -1.0, ymax = 1.0;
//  Every character in the final output is
//  xstep x ystep units wide on the complex plane.
double xstep;
double ystep;
void usage(char *argv0) {
	fprintf(stderr, "Usage: %s thread_count \n\n"
	        "Exactly one argument required:\n"
	        "    thread_count: The number of threads to create.\n",
	        argv0);
	exit(1);
}
int safe_atoi(char *s, int *val) {
	long l;
	char *endp;
	l = strtol(s, &endp, 10);
	if (s != endp && *endp == '\0') {
		*val = l;
		return 0;
	} else
		return -1;
}

void *safe_malloc(size_t size) {
	void *p;
	if ((p = malloc(size)) == NULL) {
		fprintf(stderr, "Out of memory, failed to allocate %zd bytes\n",
		        size);
		exit(1);
	}
	return p;
}
// * This function computes a line of output
// * as an array of x_char color values.
void compute_mandel_line(int line, int color_val[]) {
	double x, y;
	int n;
	int val;
	/* Find out the y value corresponding to this line */
	y = ymax - ystep * line;
	/* and iterate for all points on this line */
	for (x = xmin, n = 0; n < x_chars; x += xstep, n++) {
		/* Compute the point's color value */
		val = mandel_iterations_at_point(x, y, MANDEL_MAX_ITERATION);
		if (val > 255)
			val = 255;
		/* And store it in the color_val[] array */
		val = xterm_color(val);
		color_val[n] = val;
	}
}
/*
 * This function outputs an array of x_char color values
 * to a 256-color xterm.
 */
void output_mandel_line(int fd, int color_val[]) {
	int i;
	char point = '@';
	char newline = '\n';
	for (i = 0; i < x_chars; i++) {
		/* Set the current color, then output the point */
		set_xterm_color(fd, color_val[i]);
		if (write(fd, &point, 1) != 1) {
			perror("Thread_compute_and_output_mandel_line: write point");
			exit(1);
		}
	}
	/* Now that the line is done, output a newline character */
	if (write(fd, &newline, 1) != 1) {
		perror("Thread_compute_and_output_mandel_line: write newline");
		exit(1);
	}
}
//////////////////////////////////////////////////////////////////////
//////////////////////// END OF GIVEN CODE ///////////////////////////
//////////////////////////////////////////////////////////////////////
sem_t *lock;
struct ThreadStruct
{
	pthread_t tid; /* POSIX thread id, as returned by the library */
	int ThreadId; /* Application-defined thread id */
	int ThreadNumber;
};

void* Thread_compute_and_output_mandel_line(void* arg)
{
	struct ThreadStruct *arg_struct = (struct ThreadStruct*) arg; //type cast from (void*) to (struct*)
	int line, color_val[x_chars];
	for (line = arg_struct->ThreadId; line < y_chars; line = line + arg_struct->ThreadNumber)
	{
		compute_mandel_line(line, color_val);
		sem_wait(&lock[line % (arg_struct->ThreadNumber)]);
		output_mandel_line(1, color_val);
		sem_post(&lock[(line + 1)%(arg_struct->ThreadNumber)]);
	}
	return NULL;
}

int main(int argc, char *argv[])
{

	xstep = (xmax - xmin) / x_chars;
	ystep = (ymax - ymin) / y_chars;
	int ThreadNumber, ret, i;
	// read thread number
	if (argc != 2)
		usage(argv[0]);
	if (safe_atoi(argv[1], &ThreadNumber) < 0 || ThreadNumber <= 0) {
		fprintf(stderr, "`%s' is not valid for `thread_count'\n", argv[1]);
		exit(1);
	}
	struct ThreadStruct *ThrStrPtr;
	ThrStrPtr = safe_malloc(ThreadNumber * sizeof(*ThrStrPtr));
	lock = safe_malloc(y_chars * sizeof(sem_t));
	sem_init(&lock[0], 0, 1);
	for (i = 1; i < ThreadNumber; ++i)
	{
		sem_init(&lock[i], 0, 0);
	}
	// create NTHREADS threads
	for (i = 0; i < ThreadNumber; ++i)
	{
		ThrStrPtr[i].ThreadId = i;
		ThrStrPtr[i].ThreadNumber = ThreadNumber;
		ret = pthread_create(&ThrStrPtr[i].tid, NULL, Thread_compute_and_output_mandel_line, &ThrStrPtr[i]);
		if (ret) {
			perror_pthread(ret, "pthread_create");
			exit(1);
		}
	}
	//wait all threads
	for (i = 0; i < ThreadNumber; ++i)
	{
		ret = pthread_join(ThrStrPtr[i].tid, NULL);
		if (ret) {
			perror_pthread(ret, "pthread_join");
			exit(1);
		}
	}
	reset_xterm_color(1);
	return 0;
}

