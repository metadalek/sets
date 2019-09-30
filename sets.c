/*
 * The original idea of sets comes from Christopher Tweed, EdCAAD, University of Edinburgh, Scotland.
 *
 * This version is written by Peter D. Gray.
 *
 * There is no code shared with the original version.
 *
*/

/*

Copyright 2019 Peter D. Gray

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.

*/


#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <libgen.h>
#include <stdint.h>

#define	VERSION		"1.3"

#define	TRUE	(1)
#define	FALSE	(0)

#define	OP_NONE			(0)
#define	OP_UNION		(1)
#define	OP_DIFFERENCE		(2)
#define	OP_INTERSECTION		(3)
#define	OP_SYMMETRIC_DIFFERENCE	(4)

#define	SORT_BY_POSITION	(1)
#define	SORT_BY_VALUE		(2)

typedef struct
{
	char *	str;
	char *	abbrev;
	int64_t	position;
} member_t;

typedef struct
{
	char *		name;
	int64_t		size;
	int64_t		num_members;
	member_t *	members;
} set_t;

static void	out_of_memory	(void);
static void	usage		(int, char *[]);
static void	print_set	(set_t *);
static set_t *	new_set		(char *);
static void	add_entry	(set_t *, char *);
static int	is_a_member	(member_t *, set_t *);
static void *	safe_malloc	(int64_t);
static void *	safe_realloc	(void *, size_t);
static char *	safe_strdup	(char *);
static void	rm_duplicates	(set_t *);
static void	my_sort		(set_t *, int);

static int	compare_entries_by_string	(const void *, const void *);
static int	compare_entries_by_position	(const void *, const void *);

static char *	get_abbrev_string	(char *);
static void	read_set_from_file	(set_t *, char *);

static int	use_basenames = FALSE;
static int	ignore_extensions = FALSE;
static char	separator = '\0';

static int	verbose = 0;
static int64_t	buffer_size = 0;
static char *	buffer = NULL;

static int	maintain_order = FALSE;

#define		is_not_a_member(X,Y)	(is_a_member((X),(Y)) == FALSE)

static char *	nl = "\n";

static set_t *	set1;
static set_t *	set2;

static
void
add(register int number, register char * s)
{
	if (number == 1) add_entry(set1, s);
	else add_entry(set2, s);
	return;
}


int
main(int argc, char *argv[])
{
	int64_t		i;
	int		op = OP_NONE;
	char *		output_file = "-";
	FILE *		fp;
	int		quoted;
	int		setnumber = 1;

	set1 = new_set("Set 1");
	set2 = new_set("Set 2");
	quoted = FALSE;
	for(i = (int64_t) optind; i < (int64_t) argc; i++)
	{
		if (quoted)
		{
			add(setnumber, argv[i]);
			quoted = FALSE;
			continue;
		}

		if (strcmp(argv[i], "-q") == 0)
		{
			quoted = TRUE;
			continue;
		}

		if (strcmp(argv[i], "-u") == 0)
		{
			op = OP_UNION;
			setnumber = 2;
			continue;
		}

		if (strcmp(argv[i], "-d") == 0)
		{
			op = OP_DIFFERENCE;
			setnumber = 2;
			continue;
		}

		if (strcmp(argv[i], "-i") == 0)
		{
			op = OP_INTERSECTION;
			setnumber = 2;
			continue;
		}

		if (strcmp(argv[i], "-s") == 0)
		{
			op = OP_SYMMETRIC_DIFFERENCE;
			setnumber = 2;
			continue;
		}

		if (strcmp(argv[i], "-F") == 0)
		{
			i++;
			/*LINTED*/
			if (i >= argc) usage(argc, argv);
			if (strcasecmp(argv[i], "null") == 0) separator = '\0';
			else if (strlen(argv[i]) > 1) usage(argc, argv);
			else separator = argv[i][0];
			continue;
		}

		if (strcmp(argv[i], "-n") == 0)
		{
			nl = " ";
			continue;
		}

		if (strcmp(argv[i], "-f") == 0)
		{
			i++;
			/*LINTED*/
			if (i >= argc) usage(argc, argv);
			output_file = argv[i];
			continue;
		}

		if (strcmp(argv[i], "-o") == 0)
		{
			maintain_order = TRUE;
			continue;
		}

		if (strcmp(argv[i], "-v") == 0)
		{
			verbose++;
			continue;
		}

		if (strcmp(argv[i], "-b") == 0)
		{
			use_basenames = TRUE;
			continue;
		}

		if (strcmp(argv[i], "-B") == 0)
		{
			use_basenames = FALSE;
			continue;
		}

		if (strcmp(argv[i], "-e") == 0)
		{
			ignore_extensions = TRUE;
			continue;
		}

		if (strcmp(argv[i], "-E") == 0)
		{
			ignore_extensions = FALSE;
			continue;
		}

		if (strcmp(argv[i], "-1") == 0)
		{
			i++;
			/*LINTED*/
			if (i >= argc) usage(argc, argv);
			read_set_from_file(set1, argv[i]);
			continue;
		}

		if (strcmp(argv[i], "-2") == 0)
		{
			i++;
			/*LINTED*/
			if (i >= argc) usage(argc, argv);
			read_set_from_file(set2, argv[i]);
			continue;
		}

		add(setnumber, argv[i]);
	}

	if (op == OP_NONE) usage(argc, argv);

	my_sort(set1, SORT_BY_VALUE);
	my_sort(set2, SORT_BY_VALUE);
	rm_duplicates(set1);
	rm_duplicates(set2);

	if (verbose)
	{
		print_set(set1);
		print_set(set2);
	}

	if (strcmp(output_file, "-") == 0)
		fp = stdout;
	else
		/*LINTED*/
		fp = fopen(output_file, "w");

	if (fp == NULL)
	{
		(void) fprintf(stderr, "ERROR: unable to open file \"%s\" for writing.\n", output_file);
		exit(1);
	}

	(void) setvbuf(fp, _IOFBF, NULL, 1024*256);
	if (op == OP_UNION)
	{

		if (maintain_order) my_sort(set1, SORT_BY_POSITION);
		for(i=0;i<set1->num_members;i++)
		{
			(void) fprintf(fp, "%s%s", set1->members[i].str, nl);
		}

		if (maintain_order) my_sort(set1, SORT_BY_VALUE);
		for(i=0;i<set2->num_members;i++)
		{
			if (is_not_a_member(&set2->members[i], set1))
			{
				(void) fprintf(fp, "%s%s", set2->members[i].str, nl);
			}
		}
	}
	else if (op == OP_DIFFERENCE)
	{

		if (maintain_order) my_sort(set1, SORT_BY_POSITION);
		for(i=0;i<set1->num_members;i++)
		{
			if (is_not_a_member(&set1->members[i], set2))
			{
				(void) fprintf(fp, "%s%s", set1->members[i].str, nl);
			}
		}
	}
	else if (op == OP_INTERSECTION)
	{
		if (maintain_order) my_sort(set1, SORT_BY_POSITION);
		for(i=0;i<set1->num_members;i++)
		{
			if (is_a_member(&set1->members[i], set2))
			{
				(void) fprintf(fp, "%s%s", set1->members[i].str, nl);
			}
		}
	}
	else if (op == OP_SYMMETRIC_DIFFERENCE) // redundant check I know
	{
		if (maintain_order) my_sort(set1, SORT_BY_POSITION);
		for(i=0;i<set1->num_members;i++)
		{
			if (is_not_a_member(&set1->members[i], set2))
			{
				(void) fprintf(fp, "%s%s", set1->members[i].str, nl);
			}
		}

		if (maintain_order)
		{
			my_sort(set1, SORT_BY_VALUE);
			my_sort(set2, SORT_BY_POSITION);
		}

		for(i=0;i<set2->num_members;i++)
		{
			if (is_not_a_member(&set2->members[i], set1))
			{
				(void) fprintf(fp, "%s%s", set2->members[i].str, nl);
			}
		}
	}

	(void) fflush(fp);
	(void) fclose(fp);
	return(0);
}

static
void *
safe_malloc(int64_t bytes)
{
	void *	rval;

	/*LINTED*/
	rval = malloc((size_t) bytes);
	if (rval == NULL) out_of_memory();
	(void) memset(rval, 0, bytes);
	return(rval);
}

static
char *
get_abbrev_string(char * str)
{
	char *	tp;
	char *	p;
	int64_t	len;

	/*LINTED*/
	len = (int64_t) strlen(str);

	if (len > (buffer_size - 1))
	{
		/*LINTED*/
		if (buffer != NULL) (void) free(buffer);
		buffer_size = len + 256;
		buffer = safe_malloc(buffer_size);
	}

	/*LINTED*/
	(void) strcpy(buffer, str); // safe
	if (use_basenames) p = basename(buffer);
	else p = buffer;

	if (ignore_extensions)
	{
		tp = strrchr(p, '.');
		if (tp != NULL) *tp = '\0';
	}

	if (separator != '\0')
	{
		tp = strchr(p, separator);
		if (tp != NULL) *tp = '\0';
	}

	return(safe_strdup(p));
}

static
int
is_a_member(member_t * member, set_t * target_set)
{
	int	rval;

	if (target_set->num_members <= 0) return(FALSE);

	rval = TRUE;
	if (bsearch(member, target_set->members, target_set->num_members,
		    sizeof(member_t), compare_entries_by_string) == NULL)
		rval = FALSE;

	if (verbose > 1)
	{
		(void) fprintf(stderr,"is_a_member: set %s, member %s(%s), "
			       "returned %d\n",
			       target_set->name, member->str,
			       member->abbrev, rval);
	}

	return(rval);
}

static void
out_of_memory(void)
{
	(void) fprintf(stderr, "Out of memory.\n");
	exit(1);
}

static
set_t *
new_set(char * name)
{
	set_t *	rval;

	/*LINTED*/
	rval = (set_t *) safe_malloc((int64_t) sizeof(set_t));
	if (name != NULL) rval->name = safe_strdup(name);
	return(rval);
}

static
void
add_entry(set_t * set, char * entry)
{
	long	index;

	if (set->num_members >= set->size)
	{
		set->size += 4096;
		set->members = (member_t *) safe_realloc(set->members,
						set->size * sizeof(member_t));
	}

	index = set->num_members;
	set->members[index].str = safe_strdup(entry);
	set->members[index].abbrev = get_abbrev_string(entry);
	set->members[index].position = index;
	set->num_members++;
	if (set->num_members < 0) out_of_memory();
	return;
}

static
void
read_set_from_file(set_t * set, char * filename)
{
	FILE *	stream;
	int	c;
	long	pos;

	if (strcmp(filename, "-") == 0)
		stream = stdin;
	else
		stream = fopen(filename, "r");

	if (stream == NULL)
	{
		(void) fprintf(stderr,"ERROR: Unable to open file "
			       "\"%s\"\n", filename);
		exit(1);
	}

	(void) setvbuf(stream, NULL, _IOFBF, 1024*256);

	pos = 0;
	for(;;)
	{
		if (pos >= (buffer_size-4))
		{
			buffer_size += 256;
			buffer = safe_realloc(buffer, buffer_size);
		}

		c = getc(stream);

		if (c == EOF) break;

		if (c == '\n')
		{
			buffer[pos] = '\0';
			add_entry(set, buffer);
			pos = 0;
		}
		else
		{
			/*LINTED*/
			buffer[pos] = (char) c;
			pos++;
		}
	}

	if (pos > 0)
	{
		buffer[pos] = '\0';
		add_entry(set, buffer);
	}

	(void) fclose(stream);

	return;
}

static void
usage(int argc, char * argv[])
/* ARGSUSED */
{
	(void) fprintf(stderr,"Usage: %s [-benov] [-F sep] [-f output_file] [-1 file] [-2 file]"
		       " [e1 e2 ...] -u|i|d|s [e1 e2 ...] \n",
			argv[0]);
	exit(1);
}

static
void
print_set(set_t * set)
{
	long	i;

	(void) fprintf(stderr, "\n%s\n\n", set->name == NULL ? "NULL"
		       : set->name);
	(void) fprintf(stderr, "%6s\t%32s %32s\n", "Index", "Member",
		       "Compared As");
	for(i=0;i<set->num_members;i++)
	{
		(void) printf("%6ld\t%32s %32s\n", i,
			      set->members[i].str, set->members[i].abbrev);
	}
	(void) fprintf(stderr, "\n\n");
	return;
}


static
void *
safe_realloc(void * old, size_t bytes)
{
	void *	rval;

	rval = realloc(old, bytes);
	if (rval == NULL) out_of_memory();
	return(rval);
}

static
char *
safe_strdup(char * str)
{
	char *	rval;
	int64_t	len;

	/*LINTED*/
	len = (int64_t) strlen(str);
	rval = safe_malloc(len + 1);
	/*LINTED*/
	(void) strcpy(rval, str); // safe
	return(rval);
}

static
int
compare_entries_by_string(const void * p1, const void * p2)
{
	member_t	*	entry_1;
	member_t	*	entry_2;

	entry_1 = (member_t *) p1;
	entry_2 = (member_t *) p2;
	return(strcmp(entry_1->abbrev, entry_2->abbrev));
}

static
int
compare_entries_by_position(const void * p1, const void * p2)
{
	member_t	*	entry_1;
	member_t	*	entry_2;

	entry_1 = (member_t *) p1;
	entry_2 = (member_t *) p2;
	if (entry_1->position == entry_2->position) return(0);
	if (entry_1->position < entry_2->position) return(-1);
	return(1);
}

static
void
rm_duplicates(set_t * set)
{
	int64_t	i;
	int64_t	j;
	int64_t	k;

	if (set->num_members <= 1) return;

	i = 0;
	j = 1;
	for(;;)
	{
		if (j >= set->num_members) break;

		if ((strcmp(set->members[i].abbrev,
			    set->members[j].abbrev) != 0))
		{
			/* They are different, is the top one in the */
			/* correct spot? */
			k = i+1;
			if (j != k)
			{
				/* No, move it down to its correct location */
				set->members[k] = set->members[j];
			}

			i++; j++;
			continue;
		}

		j++;
	}

	set->num_members = i+1;
	return;
}

static
void
my_sort(set_t * set, int algorithm)
{
	if (set->num_members <= 1) return;

	if (algorithm == SORT_BY_POSITION)
		qsort(set->members, set->num_members,
		      sizeof(member_t),
		      compare_entries_by_position);
	else if (algorithm == SORT_BY_VALUE)
		qsort(set->members, set->num_members,
		      sizeof(member_t),
		      compare_entries_by_string);

	return;
}
