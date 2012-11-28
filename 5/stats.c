#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>

#define STAT_DAYS    10 /* days in statistical window */
#define INTRO_DAYS   10 /* Amount of days the assistant fills up the can */
#define WORKER_COUNT 15 /* number of worker in the company */
#define WORKER_MASK 0xf /* mask is needed for array shuffling */
#define CAN_CAPACITY 10 /* Cups per can */
#define SERVINGS      3 /* Coffee servings per worker per day */

/* data for one worker */
typedef struct {
	int stat_cups[STAT_DAYS];
	int stat_brew[STAT_DAYS];
} worker_t;

/* data for the whole company at a given point */
typedef struct {
	double threshold;
	int day_number;
	int total_cups; /* total number of cups drunk by the workers */
	int can_state;
	worker_t workers[WORKER_COUNT];
} company_t;

/* xorshift random number generator for random number generation with good
 * statistical properties. See http://en.wikipedia.org/wiki/Xorshift */
static uint32_t xor_state[4];

static uint32_t xor128(void) {
	uint32_t t;

	t = xor_state[0] ^ (xor_state[1] << 11);
	xor_state[0] = xor_state[1];
	xor_state[1] = xor_state[2];
	xor_state[2] = xor_state[3];
	return xor_state[3] = xor_state[3] ^ (xor_state[3] >> 19) ^ (t^(t>>8));
}

/* initialize the random number generator. Return 0 on success. */
static int init_xor128(void) {
	FILE *entropy = fopen("/dev/urandom","rb");
	size_t items_read;

	if (entropy == NULL) {
		perror("Can't open /dev/random");
		return 1;
	}

	items_read = fread(xor_state,sizeof xor_state[0],4,entropy);

	fclose(entropy);

	if (items_read != sizeof(xor_state[0]*4)) {
		fprintf(stderr,"Can't initialize random number generator.\n");
		return 1;
	}

	return 0;
}

/* Generate a permutated array of pointers to members of an array of workers.
 * Algorithm taken from http://en.wikipedia.org/wiki/Fisher–Yates_shuffle */
static void shuffle_array(
    worker_t workers[WORKER_COUNT], worker_t *out[WORKER_COUNT]) {
	int i,j;

	out[0] = workers;

	for (i = 1; i < WORKER_COUNT; i++) {
		/* get random numbers till you get one in range */
		while (j = xor128() & WORKER_MASK, j > i);
		out[i] = out[j];
		out[j] = workers + i;
	}
}

/* Is a worker happy? */
static bool happy(const worker_t *worker,double threshold) {
	int i;
	int cup_count = 0, can_count = 0;

	/* get amount of cups drunk, cans brewed */
	for (i = 0; i < STAT_DAYS; i++) cup_count += worker->stat_cups[i];
	for (i = 0; i < STAT_DAYS; i++) can_count += worker->stat_brew[i];

	/* I interpretate n >= 10 as "n >= sliding window" */
	return  cup_count >= STAT_DAYS && 1.0L*can_count/cup_count < threshold;
}

/* simulate one round of coffee for all workers */
static void coffee_round(company_t *company) {
	worker_t *order[WORKER_COUNT];
	int i, day_mod = company->day_number % STAT_DAYS;

	shuffle_array(company->workers,order);

	for (i = 0; i < WORKER_COUNT; i++) {
		if (!company->can_state && company->day_number >= INTRO_DAYS) {
			if (!happy(order[i],company->threshold)) continue;

			company->can_state = CAN_CAPACITY - 1;
			order[i]->stat_brew[day_mod]++;
		}

		company->can_state--;
		order[i]->stat_cups[day_mod]++;
		company->total_cups++;
	}
}

/* simulate one day of coffee */
static void day(company_t *company) {
	int i, day_mod = company->day_number % STAT_DAYS;

	/* zero out statistics entry for today */
	for (i = 0;i < WORKER_COUNT; i++) {
		company->workers[i].stat_cups[day_mod] = 0;
		company->workers[i].stat_brew[day_mod] = 0;
	}

	company->can_state = 0; /* empty can */

	for (i = 0;i < SERVINGS; i++) coffee_round(company);

	company->day_number++; /* A day has passed */
}

/* Main function */
int main(int argc,char **argv) {
	company_t *company;
	int day_count;

	if (argc != 3) {
		fprintf(stderr,"Usage: %s days threshold\n",argv[0]);
		return EXIT_FAILURE;
	}

	/* allocate and set to zero */
	company = calloc(sizeof *company,1);

	if (!sscanf(argv[1],"%d",&day_count) || day_count <= 0) {
		fprintf(stderr,"%s is not a valid number of days.\n",argv[1]);
		return EXIT_FAILURE;
	}

	if (!sscanf(argv[2],"%lf",&company->threshold)) {
		fprintf(stderr,"%s is not a valid threshold.\n",argv[2]);
		return EXIT_FAILURE;
	}

	if (init_xor128()) return EXIT_FAILURE;

	while (company->day_number < day_count) day(company);

	/* print output */
	printf("%.6lf\t%.6lf\n",
	  company->threshold,
	  ((double)company->total_cups/day_count)/WORKER_COUNT);

	free(company);

	return EXIT_SUCCESS;
}