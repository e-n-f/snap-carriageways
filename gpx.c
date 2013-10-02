#include <stdio.h>
#include <stdlib.h>
#include <expat.h>
#include <limits.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>
#include <math.h>
#include <time.h>

// boilerplate from
// http://marcomaggi.github.io/docs/expat.html#overview-intro
// Copyright 1999, Clark Cooper

#define BUFFSIZE        8192

// -----------------------------------------

struct point {
	float lat;
	float lon;
	float angle;
	float dist;

	int next; // in bucket
};

struct point *points = NULL;
int npoints = 0;
int npalloc = 0;

#define FOOT .00000274
#define BUCKET (100 * FOOT)

#define MIN_DIST (8 * FOOT * 5280 / 3600)

struct bucket {
	long long code;
	int point;

	struct bucket *left;
	struct bucket *right;
};

struct bucket *buckets = NULL;

struct bucket **findbucket(double lat, double lon, int alloc) {
	struct bucket **here = &buckets;

	int a = (lat + 180) / BUCKET;
	int o = (lon + 180) / BUCKET;
	long long code = (((long long) a) << 32) ^ o;

	while (*here != NULL) {
		if (code == (*here)->code) {
			return here;
		}

		if (code < ((*here)->code)) {
			here = &((*here)->left);
		} else {
			here = &((*here)->right);
		}
	}

	if (alloc) {
		*here = malloc(sizeof(struct bucket));
		(*here)->code = code;
		(*here)->left = NULL;
		(*here)->right = NULL;
		(*here)->point = -1;
	}

	return here;
}

void point(double lat, double lon) {
	if (npoints >= npalloc) {
		npalloc = npalloc * 2 + 1024;
		points = realloc(points, npalloc * sizeof(struct point));
		if (points == NULL) {
			perror("realloc");
			exit(EXIT_FAILURE);
		}
	}

	points[npoints].lat = lat;
	points[npoints].lon = lon;

#define UNKNOWN_ANGLE -999

	float angle = UNKNOWN_ANGLE;
	float dist = 0;
	if (npoints - 1 >= 0 && points[npoints - 1].lat != 0 && lat != 0) {
		double rat = cos(lat * M_PI / 180);
		angle = atan2(lat - points[npoints - 1].lat,
			      (lon - points[npoints - 1].lon) * rat);
		double latd = lat - points[npoints - 1].lat;
		double lond = (lon - points[npoints - 1].lon) * rat;
		dist = sqrt(latd * latd + lond * lond);
	}
	points[npoints].angle = angle;
	points[npoints].dist = dist;

	if (npoints - 1 >= 0 && points[npoints - 1].angle == UNKNOWN_ANGLE) {
		points[npoints - 1].angle = angle;
	}

	if (lat != 0) {
		struct bucket **b = findbucket(lat, lon, 1);

		points[npoints].next = (*b)->point;
		(*b)->point = npoints;
	}

	npoints++;
}

void traverse(struct bucket *b) {
	if (b != NULL) {
		traverse(b->left);
		traverse(b->right);

		printf("%lld:\n", b->code);

		int pt = b->point;
		while (pt != -1) {
			printf("   %d %f,%f %f\n", pt, points[pt].lat, points[pt].lon, points[pt].angle);
			pt = points[pt].next;
		}

		printf("\n");
	}
}

static void XMLCALL start(void *data, const char *element, const char **attribute) {
	if (strcmp(element, "trkpt") == 0) {
		double lat = 0;
		double lon = 0;

		int i;
		for (i = 0; attribute[i] != NULL; i += 2) {
			if (strcmp(attribute[i], "lat") == 0) {
				lat = atof(attribute[i + 1]);
			} else if (strcmp(attribute[i], "lon") == 0) {
				lon = atof(attribute[i + 1]);
			}
		}

		point(lat, lon);
	} else if (strcmp(element, "trk") == 0) {
		point(0, 0);
	}
}

static void XMLCALL end(void *data, const char *el) {
}

void parse(FILE *f) {
	XML_Parser p = XML_ParserCreate(NULL);
	if (p == NULL) {
		fprintf(stderr, "Couldn't allocate memory for parser\n");
		exit(EXIT_FAILURE);
	}

	XML_SetElementHandler(p, start, end);

	int done = 0;
	while (!done) {
		int len;
		char Buff[BUFFSIZE];

		len = fread(Buff, 1, BUFFSIZE, f);
		if (ferror(f)) {
       			fprintf(stderr, "Read error\n");
			exit(EXIT_FAILURE);
		}
		done = feof(f);

		if (XML_Parse(p, Buff, len, done) == XML_STATUS_ERROR) {
			fprintf(stderr, "Parse error at line %lld:\n%s\n", (long long) XML_GetCurrentLineNumber(p), XML_ErrorString(XML_GetErrorCode(p)));
			exit(EXIT_FAILURE);
		}
	}

	XML_ParserFree(p);
}

void match() {
	int i;
	int old = 0;

	for (i = 0; i < npoints; i++) {
		if (points[i].lat == 0) {
			printf("--\n");
			continue;
		}

		int percent = 100 * i / npoints;
		if (percent != old) {
			fprintf(stderr, "%d%%\n", percent);
			old = percent;
		}

		double latsum = 0;
		double lonsum = 0;
		double count = 0;
		int reject = 0;

		double rat = cos(points[i].lat * M_PI / 180);

		// Should be good up to 60 degrees latitude
		struct bucket *look[5][3];

		int x, y;
		for (y = -1; y <= 1; y++) {
			for (x = -2; x <= 2; x++) {
				struct bucket **b = findbucket(points[i].lat + y * BUCKET,
							       points[i].lon + x * BUCKET, 0);

				look[x + 2][y + 1] = *b;

				if (*b != NULL) {
					int pt;
					for (pt = (*b)->point; pt != -1; pt = points[pt].next) {
						if (points[pt].dist < MIN_DIST) {
							continue;
						}

						double latd = points[pt].lat - points[i].lat;
						double lond = (points[pt].lon - points[i].lon) * rat;
						double d = sqrt(latd * latd + lond * lond);

						if (d < BUCKET) {
							double weight = cos(points[pt].angle - points[i].angle);

#if 0
							printf("%f %f,%f %f,%f %f %f\n",
								weight,
								points[i].lat, points[i].lon,
								points[pt].lat, points[pt].lon,
								points[i].angle, points[pt].angle);
#endif

							if (weight > 0) {
								latsum += weight * (points[pt].lat - points[i].lat);
								lonsum += weight * (points[pt].lon - points[i].lon);
								count += weight;
							}
						} else {
							reject++;
						}
					}
				}
			}
		}

		printf("%f,%f %f,%f %f %d\n",
			points[i].lat, points[i].lon,
			points[i].lat + latsum / count, points[i].lon + lonsum / count,
			count, reject);
	}
}

int main(int argc, char *argv[]) {
	if (argc == 1) {
		parse(stdin);
	} else {
		int i;

		for (i = 1; i < argc; i++) {
			FILE *f = fopen(argv[i], "r");
			fprintf(stderr, "%s\n", argv[i]);
			if (f == NULL) {
				perror(argv[i]);
				exit(EXIT_FAILURE);
			}

			parse(f);
			fclose(f);
		}
	}

	match();

	//printf("got %d points\n", npoints);
	//traverse(buckets);
	exit(EXIT_SUCCESS);
}
