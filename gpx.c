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

		printf("%f,%f\n", lat, lon);
	} else if (strcmp(element, "trk") == 0) {
		printf("--\n");
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

int main(int argc, char *argv[]) {
	if (argc == 1) {
		parse(stdin);
	} else {
		int i;

		for (i = 1; i < argc; i++) {
			FILE *f = fopen(argv[i], "r");
			if (f == NULL) {
				perror(argv[i]);
				exit(EXIT_FAILURE);
			}

			parse(f);
			fclose(f);
		}
	}
}
