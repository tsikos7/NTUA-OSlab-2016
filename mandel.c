/*
 * pthread-test.c
 *
 * A test program for POSIX Threads.
 *
 * Vangelis Koukis <vkoukis@cslab.ece.ntua.gr>
 * Operating Systems course, ECE, NTUA
 *
 */


#include <stdio.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include <semaphore.h>
#include <errno.h>
#include <pthread.h>
#include <math.h>

#include "mandel-lib.h"

#define MANDEL_MAX_ITERATION 100000


/* 
 * POSIX thread functions do not return error numbers in errno,
 * but in the actual return value of the function call instead.
 * This macro helps with error reporting in this case.
 */
#define perror_pthread(ret, msg) \
	do { errno = ret; perror(msg); } while (0)



/* 
 * POSIX thread functions do not return error numbers in errno,
 * but in the actual return value of the function call instead.
 * This macro helps with error reporting in this case.
 */
#define perror_pthread(ret, msg) \
	do { errno = ret; perror(msg); } while (0)


/*
 * A (distinct) instance of this structure
 * is passed to each thread
 */
struct thread_info_struct {
	pthread_t tid; /* POSIX thread id, as returned by the library */

	int *color_val; /* Pointer to array to manipulate */
	int len; 
	//double mul;
	
	int thrid; /* Application-defined thread id */
	int thrcnt;
};


int safe_atoi(char *s, int *val)
{
	long l;
	char *endp;

	l = strtol(s, &endp, 10);
	if (s != endp && *endp == '\0') {
		*val = l;
		return 0;
	} else
		return -1;
}

void *safe_malloc(size_t size)
{
	void *p;

	if ((p = malloc(size)) == NULL) {
		fprintf(stderr, "Out of memory, failed to allocate %zd bytes\n",
			size);
		exit(1);
	}

	return p;
}

void usage(char *argv0)
{
	fprintf(stderr, "Usage: %s thread_count\n\n"
		"Exactly 1 argument required:\n"
		"    thread_count: The number of threads to create.\n",
		argv0);
	exit(1);
}


/******************
Semaphore functions
******************/
typedef sem_t Semaphore;

/*
 *A safe implementation of sem_init
 */
Semaphore *make_semaphore(int capacity){
	Semaphore *sem = safe_malloc(sizeof(Semaphore));
	int n = sem_init(sem, 0, capacity);
  	if (n != 0){
  		perror("sem_init failed");
  		exit(1);
  	} 
  	return sem;
}

/*
 *A safe implementation of sem_wait
 */
void semaphore_wait(Semaphore *sem)
{
	int n = sem_wait(sem);
  	if (n != 0) {
  		perror("sem_wait failed");
  		exit(2);
  	}
}

/*
 *A safe implemantation of sem_post
 */
void semaphore_post(Semaphore *sem)
{
  	int n = sem_post(sem);
  	if (n != 0) {
  		perror("sem_post failed");
  		exit(3);
  	}
}


Semaphore *print_sem ;
//Semaphore *compute_sem ;
int pr_line = 0; //current thread waiting to print each assigned line





/********************
 *Mandel Staff
 *******************/


/*
 * Output at the terminal is is x_chars wide by y_chars long
*/
int y_chars = 50;
int x_chars = 90;

/*
 * The part of the complex plane to be drawn:
 * upper left corner is (xmin, ymax), lower right corner is (xmax, ymin)
*/
double xmin = -1.8, xmax = 1.0;
double ymin = -1.0, ymax = 1.0;
	
/*
 * Every character in the final output is
 * xstep x ystep units wide on the complex plane.
 */
double xstep;
double ystep;

/*
 * This function computes a line of output
 * as an array of x_char color values.
 */
void compute_mandel_line(int line, int color_val[])
{
	/*
	 * x and y traverse the complex plane.
	 */
	double x, y;

	int n;
	int val;

	/* Find out the y value corresponding to this line */
	y = ymax - ystep * line;

	/* and iterate for all points on this line */
	for (x = xmin, n = 0; n < x_chars; x+= xstep, n++) {

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
void output_mandel_line(int fd, int color_val[])
{
	int i;
	
	char point ='@';
	char newline='\n';

	for (i = 0; i < x_chars; i++) {
		/* Set the current color, then output the point */
		set_xterm_color(fd, color_val[i]);
		if (write(fd, &point, 1) != 1) {
			perror("compute_and_output_mandel_line: write point");
			exit(1);
		}
	}

	/* Now that the line is done, output a newline character */
	if (write(fd, &newline, 1) != 1) {
		perror("compute_and_output_mandel_line: write newline");
		exit(1);
	}
}

void compute_and_output_mandel_line(int fd, int line, void *arg)
{
	/*
	 * A temporary array, used to hold color values for the line being drawn
	 */
	struct thread_info_struct *thr = arg;

	//semaphore_wait(compute_sem); //a thread starts to compute 
	//printf("%d thread enter compute:\n",thr->thrid);
	compute_mandel_line(line, thr->color_val);
	//semaphore_post(compute_sem);//a thread finished coputation and it 's ready to print
	//printf("%d thread Exit compute:\n",thr->thrid);
	while (1){; //wait until it is this thread 's time to print
		semaphore_wait(print_sem); //only one thread can print each time
		if(pr_line == thr->thrid){
			pr_line++;
			//printf("%d thread printing:\n",thr->thrid);
			output_mandel_line(fd, thr->color_val);
			if(pr_line == thr->thrcnt) pr_line=0;
			//printf("Next Line: %d\n",pr_line);
			//printf("%d thread posting print_sem:\n",thr->thrid);
			semaphore_post(print_sem);
		}
		else {
			semaphore_post(print_sem);
			continue;
		}
		break;
	}
	//printf("%d thread Exit compute & print:\n",thr->thrid);
}


/* Start function for each thread */
void *thread_start_fn(void *arg)
{
	int i;

	/* We know arg points to an instance of thread_info_struct */
	struct thread_info_struct *thr = arg;
	//fprintf(stderr, "Thread %d of %d. START.\n", thr->thrid, thr->thrcnt);

	for (i = thr->thrid; i < thr->len; i += thr->thrcnt)
		compute_and_output_mandel_line(1,i,thr);

	//fprintf(stderr, "Thread %d of %d. END.\n", thr->thrid, thr->thrcnt);
	return NULL;
}


int main(int argc, char *argv[])
{
	//int *arr;
	int thrcnt,i,ret;
	struct thread_info_struct *thr;

	xstep = (xmax - xmin) / x_chars;
	ystep = (ymax - ymin) / y_chars;

	if (argc != 2)
		usage(argv[0]);
	if (safe_atoi(argv[1], &thrcnt) < 0 || thrcnt <= 0) {
		fprintf(stderr, "`%s' is not valid for `thread_count'\n", argv[1]);
		exit(1);
	}

	/*
	 * Parse the command line
	 */

	print_sem = make_semaphore(1); //only one thread will print each time
	//compute_sem = make_semaphore(thrcnt);


	thr = safe_malloc(thrcnt * sizeof(*thr));

	for (i = 0; i < thrcnt; i++) {
		/* Initialize per-thread structure */
		thr[i].color_val = safe_malloc(x_chars * sizeof(*thr[i].color_val));
		thr[i].len = y_chars;
		thr[i].thrid = i;
		thr[i].thrcnt = thrcnt;
		
		/* Spawn new thread */
		ret = pthread_create(&thr[i].tid, NULL, thread_start_fn, &thr[i]);
		if (ret) {
			perror_pthread(ret, "pthread_create");
			exit(1);
		}
	}

	/*
	 * Wait for all threads to terminate
	 */
	for (i = 0; i < thrcnt; i++) {
		ret = pthread_join(thr[i].tid, NULL);
		if (ret) {
			perror_pthread(ret, "pthread_join");
			exit(1);
		}
	}


	printf("OK.\n");

	return 0;
}
