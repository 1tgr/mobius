/* Copyright (c) 1998 Shevek
 * All rights reserved
 *
 * GNU fortune 1.0
 *
 * From an idea in BSD fortune.c - I think that this file has been redesigned
 * sufficiently including major flow control modifications and a brand new
 * memory model. I have tried to maintain the BSD style of the code.  However
 * it has been ENTIRELY rewritten and typed from scratch and is NOT a
 * modified copy of fortune.c.
 *
 * This file is Copyright (c) to Shevek <shevek@anarres.org> and
 * is distributed under the GNU Public License version 2, or such later
 * version as may come into effect.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/* View this file with tabstops set to 4 spaces.
 * less -x4 gfortune.c		for less
 * :set ts=4				for vi
 * expand -t4 gfortune.c	for expand
 */


#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <strings.h>
#include <dirent.h>		/* For opendir(3) et al */
#include <assert.h>		/* For assert(3) */
#include <ctype.h>		/* For character classification */
#include <regex.h>		/* For regcomp, regexec */
#include <time.h>		/* For getting a random number seed */
#include <sys/types.h>	/* For char class et al */
#include <sys/stat.h>	/* For stat(3) */
#include <netinet/in.h>	/* For ntohl(3) */

# include       "strfile.h"

/* Declarations */
#ifndef FORTDIR
#define	FORTDIR	"/var/lib/games/fortunes"
#endif

#ifdef DEBUG
#	undef NDEBUG
	int Debug = 0;
#	define     DPRINTF(l,x)    if (Debug >= l) { fprintf (stderr, "<%d>", l); fprintf x; }
#else  /* DEBUG */
#	define NDEBUG
#	define        DPRINTF(l,x)
#endif /* DEBUG */

/* All these are in the wrong or non-canonical order. SEP. */

/* General declarations */
# define        TRUE    1
# define        FALSE   0

# define	bool	short

/* Environment declarations */
/* Need to define min wait, short length, chars per second etc */
# define	NO_PROB	(-1)	/* No probability for file */
				/* Check this one out... WTF is it? */
# define	POS_UNKNOWN	((unsigned long) -1)	/* pos for file unknown */
#define CPERS	40
#define MIN_WAIT	6

/* System-dependent declarations */
#ifdef  SYSV
# define	index	strchr
# define	rindex	strrchr
#endif /* SYSV */
/* I don't know anything about SYSV anyhow. This is Linux software. */
/* I don't care. */

typedef struct fd {
        int             percent;
        int             fd, datfd;
        unsigned long   pos;
        FILE            *inf;
        char            *name;
        char            *path;
        char            *datfile, *posfile;
        bool            read_tbl;
        bool            was_pos_file;
        STRFILE         tbl;
        int             num_children;
        struct fd       *child, *parent;
        struct fd       *next, *prev;
        	unsigned long	num_files;
        	unsigned long	is_offensive;
} FILEDESC;

typedef struct _args {
	bool	all_fortunes;
	int		technique;
	bool	print_list;
	bool	ignore_case;
	bool	long_only;
	bool	match_pattern;
	bool	offensive;
	bool	no_recurse;
	bool	short_only;
	bool	pause;

	char	*reg_pattern;

	int		long_length;
	int		short_length;
	int		max_lines;

	char	*prog_name;
} ARGS;

/* Globals */
ARGS args;

/* Prototypes */
int add_dir (FILEDESC	*fp);

int
max (i, j)
int	i;
int	j;
{
	return (i >= j ? i : j);
}

char *
do_malloc(size)
unsigned int    size;
{
        char    *new;

        if ((new = (char *)malloc(size)) == NULL) {
                fprintf(stderr, "%s: out of memory\n", args.prog_name);
                exit(1);
        }
        return new;
}

char *
copy(str, len)
char            *str;
unsigned int    len;
{
        char    *new, *sp;

        new = do_malloc(len + 1);
        sp = new;
		strcpy (new, str);
        return new;
}

inline void
zero_tbl (tblp)
STRFILE		*tblp;
{
	tblp->str_numstr = 0;
	tblp->str_longlen = 0;
	tblp->str_shortlen = -1;
}

FILEDESC *
new_fp()
{
        register FILEDESC       *fp;

        fp = (FILEDESC *) do_malloc(sizeof *fp);
        fp->datfd = -1;
        fp->pos = POS_UNKNOWN;
        fp->inf = NULL;
        fp->fd = -1;
        fp->percent = NO_PROB;
        fp->read_tbl = FALSE;
        fp->next = NULL;
        fp->prev = NULL;
        fp->child = NULL;
        fp->parent = NULL;
        fp->datfile = NULL;
        fp->posfile = NULL;

        fp->num_files = 0; /* We need to set this to 1 in add_file */
		fp->num_children = 0; /* Surely this should be here? */

		fp->is_offensive = 0;

#ifdef DEBUG
		zero_tbl (&fp->tbl);
#endif /* DEBUG */

        return fp;
}

int
is_dir(file)
char    *file;
{
        struct stat		sbuf;

        if (stat(file, &sbuf) < 0)
                return FALSE;
        return (sbuf.st_mode & S_IFDIR);
}

// S_IROTH to test for world readability
inline void
open_fp (fp)
FILEDESC	*fp;
{
	if (fp->inf == NULL && (fp->inf = fopen(fp->path, "r")) == NULL) {
		perror (fp->path);
		exit(1);
	}
}

inline void
open_dat (fp)
FILEDESC	*fp;
{
	if ((fp->datfd = open (fp->datfile, O_RDONLY)) < 0) {
		perror (fp->datfile);
		exit(1);
	}
}

int
is_fortfile (file, datp, posp, check_for_offend)
char	*file;
char	**datp, **posp;
int		check_for_offend;
{
	int		i;
	char	*sp;
	char	*datfile;

	static char	*suflist[] = { /* List of illegal suffices */
								/* Is this still true? */
						"dat", "pos",
#ifdef DEBUG /* I don't think these are relevant nowadays */
						"c", "h", "p", "i", "f",
						"pas", "ftn", "ins", "pas", "sml",
#endif /* DEBUG */
						NULL
				};

	DPRINTF (3, (stderr, "  Running is_fortfile on \"%s\"\n", file));
	
	/* We no longer check for offensiveness here */
	
	/* Point sp to the first char of the filename  a la` basename(1) */
	if ((sp = rindex (file, '/')) == NULL)
		sp = file;
	else
		sp++;

	/* Extension check */
	if ((sp = rindex(sp, '.')) != NULL) {
		sp++;
		for (i=0; suflist[i] != NULL; i++)
			if (strcmp(sp, suflist[i]) == 0) {
				DPRINTF (4, (stderr, "  FALSE (file has extension \".%s\")\n", sp));
				return FALSE;
			 }
	}

	DPRINTF (4, (stderr, "  Extension OK \"%s\"\n", file));
	
	datfile = copy (file, (unsigned int)(strlen(file) + 4));
	strcat (datfile, ".dat");

	DPRINTF (4, (stderr, "  Datfile name is \"%s\"\n", datfile));

	/* Check permissions and existence of .dat file via access(2) */
	if (access(datfile, R_OK) <0) {
		DPRINTF (2, (stderr, "  is_fortfile() returns FALSE (\"%s\" is unreadable)\n", datfile));
		free (datfile);
		return FALSE;
	}
	
	/* I don't know what this lot does yet, maybe I'll find out later.
	   For now I'll just copy and paster verbatim */

	if (datp != NULL) /* If the pointer exists? */
		*datp = datfile;	/* Presumably this is a part of *fp */
	else
		free(datfile);
		/* Something's up here... this shouldn't happen IMHO */

	/* I'm not going to do a disk writing version, so IMHO no posp */

	DPRINTF (2, (stderr, "  is_fortfile() returns TRUE\n"));
	return TRUE;
}

int
add_file(percent, file, dir, head, tail, parent)
/* This function is now * DIFFERENT to before, previous fortune
 * coders BEWARE. */
int		percent;
char		*file;
char		*dir;
FILEDESC	**head, **tail;
FILEDESC	*parent;
{
	bool		was_malloc;
	char		*path;
	FILEDESC	*fp;
	bool		is_offensive;

	if (*file == '.') {
		DPRINTF(3, (stderr, "  File is a dot file, ignored\n"));
		/* Is this exhaustive if *file == NULL? */
		/* *file != NULL for all *file */
		return FALSE;
	}

	/* Offensiveness test has moved house down 30 lines */

	/* Get the full filename in *path */
	if (dir == NULL) {
		path = file;
		was_malloc = FALSE;
	} else {
		/* Cast the size_t to an unsigned int */
		path = do_malloc ((unsigned int)(strlen(dir) + strlen(file) + 2));
		strcat (strcat (strcpy (path, dir), "/"), file);
		was_malloc = TRUE;
	}

	DPRINTF (3, (stderr, "  add_file got file \"%s\" with \"%s\"\n", path, file));

	/* If file isn't readable */
	if (access (path, R_OK) <0) {
		/* Ignore the hack for the -o default suffix, BYEBYE GOTO! */

		/* Now if the file is not absolute, read FORTDIR instead */
		if ((dir == NULL) && (file[0] != '/'))
			return add_file (percent, file, FORTDIR, head, tail, parent);

		DPRINTF (4, (stderr, "Returning false at point 0\n"));
		/* Single file given and that dosen't exist. */
		if (parent->parent == NULL)
			perror (path);
			/* XXX Now we have a master object, this won't work.
			 * However parent->parent should still give NULL. */
		
		if (was_malloc)
			free(path);
		
		return FALSE;
	}

	DPRINTF (3, (stderr, "  Path = \"%s\"\n", path));

	/* Offensiveness check is here, before we allocate memory */
	/* This is our offensive test. Unless *file is a complete absolute
	 * reference in which case we've been passed it as a command line
	 * argument and the user is perverted anyhow, this will work. */
	/* It catches both FILES and DIRECTORIES
	 * Needs to match first 9 characters ONLY - this relies on a
	 * version of strncmp that OKs if strlen(str1) < n */
	if (!(is_offensive = parent->is_offensive))
		if (*file == 'o')
			is_offensive = (strncmp(file, "offensive", (size_t)(9)) == 0);

	DPRINTF (2, (stderr, "is_offensive on \"%s\" gives %d\n", file, is_offensive));
	if (is_offensive) {
		DPRINTF (1, (stderr, "Offensive file \"%s\" detected. TAKE COVER.\n", file));
		/* XXX Need to fix this */
	}
	if (!args.all_fortunes && parent->parent != NULL) {
		DPRINTF (4, (stderr, "Parent->parent test passed for \"%s\"\n", path));
		if ((!args.offensive && is_offensive) ||
				(args.offensive && !is_dir (path) && !is_offensive)) {
			DPRINTF (3, (stderr, "File \"%s\" rejected: wrong offensiveness.\n", file));
			return FALSE;
		}
	}
	else {
		DPRINTF (3, (stderr, "Parent test failed for \"%s\"\n", path));
	}

	/* Create a new file thingy. This might have to be messed around
	   with to allow the program to open N+1 fortune files. */
	fp = new_fp();
	fp->percent = percent;
	fp->name = file;
	fp->path = path;
	fp->parent = parent;
	fp->is_offensive = is_offensive;

	/* We need to check for dir first I think, not sure if it matters */
	if (is_dir(path)) {
		/* XXX Really we want full recursion to be a command line option */
		if (!add_dir (fp)) {
			DPRINTF (2, (stderr, "Discarding bad directory \"%s\"\n", fp->path));
			free (fp);
			return FALSE;
		}
	}
	/* XXX This can be cleaned up later, I don't care again */
	else if (!is_fortfile(path, &fp->datfile, &fp->posfile, NULL)) {
		DPRINTF(2, (stderr, "  File \"%s\" is not a fortune file or directory.\n", path));
		return FALSE;
	}
	else
		fp->num_files = 1; /* The file actually exists and needs counting */
	/* vi is behaving today. :-) */

	/* We can't run this here, so we MUST run get_tbl for technique 2 */
	// parent->num_files += fp->num_files;

	if (*head == NULL) {
		*head = *tail = fp;
	}
	else if (fp->percent == NO_PROB) {
		(*tail)->next = fp;
		fp->prev = *tail;
		*tail = fp;
	}
	else {
		(*head)->prev = fp;
		fp->next = *head;
		*head = fp;
	}

	/* We now trap the bad conclusions above */
	return TRUE;
}

int
add_dir (fp)
FILEDESC	*fp;
{
/* I suppose an ifdef SYSV would be appropriate here XXX */
	struct dirent	*dirent;	/* NIH? WTF is that? */
	DIR				*dir;		/* File stream thingy for the dir */
	char			*name;		/* Filename in question */
	FILEDESC		*tailp;		/* Pointer to last tail in list */

	/* I'm gonna leave out the rest of the junk till I know it does
	 * anything */
	
	/* The obvious snafu */
	if ((dir = opendir (fp->path)) == NULL) {
		perror (fp->path);
		return FALSE;
	}
	if (args.no_recurse)
		if (fp->parent != NULL) /* Play this safe, rely on optimiser */
			if (fp->parent->parent != NULL) {
				/* We are lower level subdir below master's child. */
				DPRINTF (2, (stderr, "Stopping recursion at \"%s\"\n", fp->name));
				return FALSE;
			}

	tailp = NULL;
	DPRINTF (1, (stderr, "Adding dir \"%s\"\n", fp->path));

	while ((dirent = readdir(dir)) != NULL) {
		if (dirent->d_reclen == 0)
			continue; /* Wot? */
		name = copy (dirent->d_name, dirent->d_reclen);
		DPRINTF(2, (stderr, "Trying file \"%s\"\n", name));
		if (add_file (NO_PROB, name, fp->path, &fp->child, &tailp, fp)) {
			DPRINTF(2, (stderr, "File \"%s\" added from dir.\n", name));
			fp->num_children++;
		} else
			free(name);
	}
	closedir (dir);		/* Har har the original forgot this *smug* */

	/* Now what happens if we have no files in the dir? */
	if (fp->num_children == 0) {
		if (fp->parent->parent == NULL) /* If it was specified, report. */
			fprintf (stderr, "%s: %s: No fortune files in directory\n", args.prog_name, fp->path);

		// free (fp); /* XXX This might fubar. */
		/* This free is now done in add_file and the optimiser can sort it out */
		return FALSE;
	}

	return TRUE;
}

inline void
sum_tbl (t1, t2)
STRFILE	*t1;
STRFILE	*t2;
{
	t1->str_numstr += t2->str_numstr;
	if (t1->str_longlen < t2->str_longlen)
		t1->str_longlen = t2->str_longlen;
	if (t1->str_shortlen > t2->str_shortlen)
		t1->str_shortlen = t2->str_shortlen;
}

int /* XXX Can't this be a void? */
get_tbl (fp)
FILEDESC	*fp;
{
	FILEDESC	*list;

	/* Getting all the wossnames is a bit inefficient but will do ATM */

	DPRINTF (3, (stderr, "    Testing %s in get_tbl.\n", fp->name));
	if (fp->read_tbl)
		return TRUE;

	DPRINTF (3, (stderr, "    Non-trivial %s in get_tbl.\n", fp->name));
	if (fp->child == NULL) {
		DPRINTF (3, (stderr, "    %s has no children.\n", fp->name));
		DPRINTF (4, (stderr, "    About to open %s\n", fp->datfile));
		open_dat (fp);
		DPRINTF (4, (stderr, "    Dat file open for %s.\n", fp->name));
		if (read (fp->datfd, (char *) &fp->tbl, sizeof (fp->tbl)) != sizeof (fp->tbl)) {
			fprintf (stderr, "%s: %s corrupted\n", args.prog_name, fp->datfile);
			exit(1); /* ^^ hee hee another bug... they said fp->path */
		}
		fp->tbl.str_numstr = ntohl(fp->tbl.str_numstr);
		fp->tbl.str_longlen = ntohl(fp->tbl.str_longlen);
		fp->tbl.str_shortlen = ntohl(fp->tbl.str_shortlen);
		fp->tbl.str_flags = ntohl(fp->tbl.str_flags);
		close (fp->datfd);
	}
	else {
		zero_tbl (&fp->tbl);
		for (list = fp->child; list != NULL; list = list->next) {
			get_tbl (list);
			sum_tbl (&fp->tbl, &list->tbl);
			/* XXX We need to do this here when we build tables. */
			fp->num_files += list->num_files;
		}
	}

	fp->read_tbl = TRUE;
	/* We can just return TRUE from here as all errors are exit(3)ed */
	return TRUE;
}

#ifdef OLD_REGEX /* For old style regex pattern matching. */
char *
conv_pat (orig)
char	*orig;
{
	char		*sp;
	char		*new;
	unsigned int	cnt = 1;

	for (sp = orig; *sp != '\0'; sp++)
		if (isalpha (*sp))
			cnt += 4;
		else
			cnt++;

	if ((new = malloc(cnt)) == NULL) {
		fprintf(stderr, "%s: pattern too long to ignore case\n", args.prog_name);
		exit(1);
	}

	for (sp = new; *orig != '\0'; orig++) {
		if (islower(*orig)) {
			/* I know it's weird but it knocks 64 bytes off the code size */
			// sprintf (sp, "[%c%c]", toupper (*orig), *orig);
			// sp+=4;
			*sp++='[';
			*sp++=toupper(*orig);
			*sp++=*orig;
			*sp++=']';
		}
		else if (isupper(*orig)) {
			//sprintf (sp, "[%c%c]", *orig, tolower (*orig));
			//sp+=4;
			*sp++='[';
			*sp++=*orig;
			*sp++=tolower(*orig);
			*sp++=']';
		}
		else
			*sp++ = *orig;
	}
	*sp = '\0';
	return new;
}
#endif /* OLD_REGEX */

bool
matches_in_list (object, preg, fortbuf, max_length)
FILEDESC	*object;
regex_t		*preg;
char		*fortbuf;
int			max_length;
/* I can ditch max_length as soon as I work out what that +10 hack does. */
{
	FILEDESC	*fp;
	char		*sp;
	bool		in_file;
	bool		found_one = FALSE;
	long		num_lines;
	long		length;

	/* Here I've renamed my variables to match the original a bit,
	 * normally I have used list as the variable and fp as the object. */
	for (fp = object; fp != NULL; fp = fp->next) {
		if (fp->child != NULL) {
			matches_in_list (fp->child, preg, fortbuf, max_length);
			continue;	/* Not my style, but if it works, don't fix it. */
		}
		DPRINTF (1, (stderr, "Searching for pattern in \"%s\"\n", fp->path));
		open_fp (fp);
		sp = fortbuf;
		num_lines = 0;
		length = 0;
		in_file = FALSE;
		/* We've assigned Fort_len to be the max fortune size in the
		 * original. This is OK as fgets stops at newline or EOF */
		while (fgets(sp, max_length, fp->inf) != NULL) {
			if (!STR_ENDSTRING(sp, fp->tbl)) {
				sp += strlen(sp);
				num_lines++;
			}
			else {
				*sp = '\0';
				/* XXX Here we need our RX compiled and ready */
				if (regexec (preg, fortbuf, (size_t) (0), NULL, 0) == 0) {
					/* Messy... */
					length = sp - fortbuf;

					/* This is a bit messy but should work. I think I'm
					 * reverting to LISP layout here. Sorry BSD! */
					if (!(
						(args.short_only &&
							(length > args.short_length))
							||
						(args.long_only &&
							(length < args.long_length))
							||
						((args.max_lines < 999) &&
							(num_lines > args.max_lines))
						))
					{
						printf ("%c%c", fp->tbl.str_delim, fp->tbl.str_delim);
						/* Can I optimise this with %*s? */
						if (!in_file) {
							printf ("  (%s)", fp->path);
							found_one = TRUE;
							in_file = TRUE;
						}
						putchar('\n');
						fwrite (fortbuf, 1, (sp-fortbuf), stdout);
						/* Pointer arith. Yuk. */
					}
				}
				sp = fortbuf;
				num_lines = 0;
				length = 0;
			}	/* if */
		}		/* while */
	}			/* for */
	return found_one;
}				/* function */

bool
find_matches (fp, preg)
FILEDESC	*fp;
regex_t		*preg;
{
	int			max_length;
	char		*fortbuf;

	get_tbl (fp);
	/* This looks like a nasty hack, we don't know how long the fortune
	 * is, we just guess at 10 extra characters, surely we should add
	 * the newlines in when we do a get_tbl? XXX */
	max_length = fp->tbl.str_longlen;
	fortbuf = do_malloc((unsigned int) max_length + 10);
	/* Actually I don't need this either as I can put it on the stack?
	 * or is that inefficient to keep messing around with memory? I
	 * figure I'll just pass the buffer down the stack instead of
	 * creating a new minimal buffer at each level. */

	/* And thus we don't need maxlen_in_list */
	return (matches_in_list (fp, preg, fortbuf, max_length));
}

/* Pick child selects a file to print an epigram from. */
/* I suppose also that we need three techniques here. One based on
 * file size and num_str, one based on children per parent
 * having equal probability, and so children of children having less
 * probablility, and one where all children get the same whack at
 * the wotsit. */

FILEDESC *
pick_child (parent)
FILEDESC	*parent;
{
	FILEDESC	*fp;
	int	choice;
	int			technique = args.technique;


	if (parent->num_children == 0)
		return parent;	/* Textbook base case */
	
	DPRINTF(1, (stderr, "Starting technique %d on %s\n", technique, parent->name));

	if (technique == 1) { /* Each file in dir is equal */
		choice = random() % parent->num_children;
		DPRINTF(1, (stderr, "    Choice = %d (of %d)\n",
				choice+1, parent->num_children));
		for (fp = parent->child; choice--; fp = fp->next)
			continue;
		DPRINTF(1, (stderr, "    Using %s\n", fp->name));
		return pick_child (fp);
	}
	else if (technique == 2) { /* Each file everywhere is equal */
		/* XXX Needs get_tbl to be run on the root node */
		choice = random() % parent->num_files;
		DPRINTF(1, (stderr, "    Choice is %d of %ld\n", choice,
				parent->num_files));
		for (fp = parent->child; choice > fp->num_files; fp = fp->next)
			choice -= fp->num_files;
		DPRINTF (1, (stderr, "    File number is %d\n", choice));
		DPRINTF (1, (stderr, "    File is %s\n", fp->name));
		return pick_child (fp); /* I think */
	}
	else if (technique == 3) { /* Each fortune is equal (ish) */
		/* XXX We MUST get_tbl() before running technique 3 */
		/* Actually on consideration, we've already ditched all percentage
		 * considerations, so we might as well use num_str here, hence
		 * get shot of virtual entirely. Wonder if it works? */
		/* We now need get_tbl for technique 3 */
		choice = random() % parent->tbl.str_numstr;
		DPRINTF (1, (stderr, "    Choice is %d of %ld\n", choice,
				parent->tbl.str_numstr));
		for (fp = parent->child; choice > fp->tbl.str_numstr; fp = fp->next)
			choice -= fp->tbl.str_numstr;
		DPRINTF (1, (stderr, "    Offset is %d\n", choice));
		DPRINTF (1, (stderr, "    File is %s\n", fp->name));
		return pick_child (fp); /* I think */
	}
#ifdef DEBUG
	else {
		fprintf (stderr, "%s: invalid selection technique\n", args.prog_name);
		exit(1);
	}
#endif /* DEBUG */
}

inline void
get_pos (fp)
FILEDESC	*fp;
{
#ifdef DEBUG
	assert (fp->read_tbl); /* XXX Do I need this?? */
#endif	/* DEBUG */
	/* Only if technique 1 was used. */
	if (fp->pos == POS_UNKNOWN)
		fp->pos = random() % fp->tbl.str_numstr;
	/* We don't need to increment here we already have a value in
	 * the range [0, tbl.str_numstr - 1] */
	DPRINTF (1, (stderr, "pos for file %s is %ld of %ld\n",
			fp->name, fp->pos, fp->tbl.str_numstr));
}

FILEDESC *
get_fort_fp (fp)
FILEDESC	*fp;	/* As ever, the top of the tree */
{
	FILEDESC	*list;
	int			choice;
	/* I don't know why we have 2 here, we only need 0 and 1 AFAIK */

	/* Actually, as we now have a master object, we can ask for it's
	 * number of children and hence if anything was specified on
	 * command line it will be either first child or there will be a
	 * list thereof. Thus. */
	/* I wonder how much I can cane off the code if I lose the assert(3) */

#ifdef DEBUG
	assert (fp->num_children >= 1); /* This is for safety XXX */
#endif	/* DEBUG */

	if (fp->num_children == 1 || fp->child->percent == NO_PROB) {
		DPRINTF (2, (stderr, "No probability or no next (v2)\n"));
		/* Must select a random object here, we need the master object */
		/* list = fp ; and then select child from list */
		/* Maybe I'll assign it to fp instead. l8r. */
		list = fp;
	}
	else {
		choice = random() % 100;
		DPRINTF (1, (stderr, "get_fort: Choice = %d\n", choice));
		for (list = fp->child; list->percent != NO_PROB; list = list->next) {
			if (choice < list->percent)
				break;
			else {
				choice -= list->percent;
				DPRINTF (1, (stderr, "    skip \"%s\", %d%%\n",
						list->name, list->percent));
			}
		}
		DPRINTF (1, (stderr, "    Using \"%s\", %d%%\n",
				list->name, list->percent));
	}
	/* Now we build techniques etc etc and get tables */
	DPRINTF (1, (stderr, "Picking child from %s\n", list->name));

	get_tbl(list); /* For tech 2 and 3 */
	if (list->tbl.str_numstr == 0) {
		fprintf (stderr, "%s: no fortunes found\n", args.prog_name);
		exit(1);
	}
	list = pick_child(list);
	DPRINTF(1, (stderr, "Using file \"%s\" for fortune.\n", list->name));
	return list;
}

long
fort_len (fp, seekpts)
FILEDESC	*fp;
off_t *		seekpts;
{
	char	line[BUFSIZ];
	long	length = 0;

	if (!(fp->tbl.str_flags & (STR_RANDOM | STR_ORDERED)))
		length = (seekpts[1] - seekpts[0]);
	else {
		open_fp (fp);
		fseek (fp->inf, seekpts[0], SEEK_SET);
		while (fgets (line, sizeof (line), fp->inf) != NULL &&
				!STR_ENDSTRING(line, fp->tbl))
			length += strlen (line);
	}
	return length;
}

long
fort_lines (fp, seekpts)
FILEDESC	*fp;
off_t *		seekpts;
{
	char	line[BUFSIZ];
	long	numlines = 0;

	open_fp (fp);
	fseek (fp->inf, seekpts[0], SEEK_SET);
	while (fgets (line, sizeof (line), fp->inf) != NULL &&
			!STR_ENDSTRING(line, fp->tbl))
		numlines++;

	return numlines;
}

void
display (fp, seekpts)
FILEDESC	*fp;
off_t *		seekpts;
{
	char	*p, ch;
	char	line[BUFSIZ];
	long	length;

	open_fp (fp);
	fseek (fp->inf, seekpts[0], SEEK_SET);
	/* Length seems to be number of lines here. But we must already
	 * have evaluated fort_len so why not use it? and also number
	 * of lines. Lessee... as soon as I write a decent -S and -L */
	/* XXX Mustn't evaluate this twice */
	for  (length = 0; fgets (line, sizeof (line), fp->inf) != NULL &&
			!STR_ENDSTRING(line, fp->tbl); length++) {
		if (fp->tbl.str_flags & STR_ROTATED)
			for (p=line; (ch=*p); ++p)
				if (isupper(ch))
					*p = 'A' + (ch - 'A' + 13) % 26;
				else if (islower(ch))
					*p = 'a' + (ch - 'a' + 13) % 26;
		fputs(line, stdout);
	}
	fflush (stdout);
}

bool
get_fort (fp)
FILEDESC	*fp;
{
	off_t		seekpts[2];

	get_pos(fp);
	open_dat(fp);
	lseek (fp->datfd,
			(off_t) (sizeof (fp->tbl) + fp->pos * sizeof (seekpts [0])),
			SEEK_SET);
	read (fp->datfd, seekpts, sizeof (seekpts));
	close (fp->datfd); /* >:-) */
	seekpts[0] = ntohl(seekpts[0]);
	seekpts[1] = ntohl(seekpts[1]);

	if (args.short_only &&
			(fort_len (fp, seekpts) > args.short_length))
		return FALSE;
	else if (args.long_only &&
			(fort_len (fp, seekpts) < args.long_length))
		return FALSE;
	else if ((args.max_lines < 999) &&
			(fort_lines (fp, seekpts) > args.max_lines))
		return FALSE;

	/* Display_fortune needs fp and seekpts */
	display (fp, seekpts);
	if (args.pause)
		sleep ((unsigned int)
				max(fort_len (fp, seekpts) / CPERS, MIN_WAIT));

	return TRUE;
}

void
print_list_format (list, level)
FILEDESC	*list;
int			level;
{
	/* XXX Remove this when the program is finished */
	while (list != NULL) {
		fprintf (stderr, "%*s", (level * 6), "");

		if (level == 1)
			if (list->percent == NO_PROB)
				fprintf (stderr, "---%%");
			else
				fprintf (stderr, "%3d%%", list->percent);
		else
			fprintf (stderr, "    ");

		if (list->child != NULL) {
			fprintf (stderr, " %-16s", strcat(strcat(calloc(strlen(list->name)+1, 1), list->name), "/"));
		} else {
			fprintf (stderr, " %-16s", list->name);
		}

#ifdef DEBUG
		fprintf (stderr, " [%ld %ld %ld]\t {%d %ld}", list->tbl.str_shortlen,
				list->tbl.str_longlen, list->tbl.str_numstr,
				list->num_children, list->num_files);
#endif	/* DEBUG */
		putc ('\n', stderr);

		/* I've evaluated this IF twice :-( Leave it to the optimiser */
		/* XXX More liekly, fix it later because I don't need the above
		 * printf statement, it's just a glob of debugging junk */
		if (list->child != NULL)
			print_list_format (list->child, level+1);

		list = list->next;
	}
}

inline void
print_file_list (start)
FILEDESC	*start;
{
#ifdef DEBUG
	fprintf(stderr, "Percentage, Filename, [short_len, long_len, num_str]"
	"{num_children, num_files}\n");
#endif	/* DEBUG */
	print_list_format (start, 0);
}

void
zero_args(void)
{
	args.all_fortunes = FALSE;
	args.technique = 3;
	args.print_list = FALSE;
	args.ignore_case = FALSE;
	args.long_only = FALSE;
	args.match_pattern = FALSE;
	args.offensive = FALSE;
	args.no_recurse = FALSE;
	args.short_only = FALSE;
	args.pause = FALSE;
	args.reg_pattern = NULL;

	args.long_length = 160;
	args.short_length = 160;
	args.max_lines = 999;
}

void
usage (void)
{
	fprintf (stderr, 
"Usage:
	%s [-a"
#ifdef DEBUG
"D"
#endif /* DEBUG */
"eEfilosrw] [-m pattern] [-L num] [-N num] [-S num]
	%*s [[#%%] file/directory/all]
", args.prog_name, strlen(args.prog_name), "");
}

void
get_args (argc, argv)
int		*argc;
char	**argv[];
{
	int		option;
	char	*temp;

	temp = rindex(*argv[0], '/');
	args.prog_name = (temp? ++temp :*argv[0]);

#ifdef DEBUG
	while ((option = getopt(*argc, *argv, "aefilm:osrwEL:N:S:Dh?")) != EOF)
#else /* DEBUG */
	while ((option = getopt(*argc, *argv, "aefilm:osrwEL:N:S:h?")) != EOF)
#endif /* DEBUG */

	switch (option) {
		case 'a':	/* All fortunes */
			args.all_fortunes = TRUE;
			break;
		case 'e':	/* Files are all equal */
			args.technique = 2;
			break;
		case 'f':	/* Print file list */
			args.print_list = TRUE;
			break;
		case 'i':	/* Ignore case in matches */
			args.ignore_case = TRUE;
			break;
		case 'l':	/* Long fortunes only */
			args.long_only = TRUE;
			args.short_only = FALSE;
			break;
		case 'm':	/* Match pattern */
			args.match_pattern = TRUE;
			args.reg_pattern = optarg;
			break;
		case 'o':	/* Offensive only */
			args.offensive = TRUE;
			break;
		case 'r':
			args.no_recurse = TRUE;
			break;
		case 's':	/* Short fortunes only */
			args.short_only = TRUE;
			args.long_only = FALSE;
			break;
		case 'w':	/* Pause after display */
			args.pause = TRUE;
			break;

		case 'E':	/* Files in directory are equal */
			args.technique = 1;
			break;
		case 'L':	/* Set long length */
			args.long_length = atoi(optarg);
			args.long_only = TRUE;
			args.short_only = FALSE;
			break;
		case 'N':	/* Set max lines */
			args.max_lines = atoi(optarg);
			break;
		case 'S':	/* Set short length */
			args.short_length = atoi(optarg);
			args.short_only = TRUE;
			args.long_only = FALSE;
			break;
		case 'W':	/* Set wait time */
			/* I'm far too lazy to do any half consistent implementation
			 * of wait times, it would need to set min wait, max wait and
			 * cps to wait for fortunes. SEP again. */
			break;

#ifdef DEBUG
		case 'D':	/* Debug mode on */
			Debug++;
			break;
#endif /* DEBUG */

		case '?':
		case 'h':
		default:
			usage();
			exit(1);
	}

	*argc-=optind;
	*argv+=optind;
}

#ifdef DEBUG
void
print_args ()
{
	fprintf(stderr, 
"All fortunes is %d
Offensive is %d

Ignore case is %d
Match pattern is %d
Pattern is \"%s\"

Long only is %d
Long length is %d
Short only is %d
Short length is %d
Max lines is %d

Pause is %d
Technique is %d
Print list is %d
Don't recurse is %d

Debug is %d
",
args.all_fortunes,
args.offensive,
args.ignore_case,
args.match_pattern,
(args.reg_pattern ? args.reg_pattern : "(null)"),
args.long_only,
args.long_length,
args.short_only,
args.short_length,
args.max_lines,
args.pause,
args.technique,
args.print_list,
args.no_recurse,
Debug
);
}
#endif	/* DEBUG */

FILEDESC *
make_list (argc, argv)
int		argc;
char	*argv[];
{
	FILEDESC	*tailp = NULL;
	FILEDESC	*object;
	char		*sp;
	int			i;
	int			percent;
	int			total_percent = 0;

	object = new_fp();
	object->name = copy("Master node", 12);
	object->path = NULL;
	/* fp->name and fp->path are not set to NULL by new_fp() */

	if (argc) {
		for (i=0; i<argc; i++) {
			DPRINTF (3, (stderr, "Parsing argument %d, value %s\n", i, argv[i]));
			if (!isdigit (argv[i][0])) {
				percent = NO_PROB;
				sp = argv[i];
			}
			else {
				percent = 0;
				for (sp = argv[i]; isdigit (*sp); sp++) {
					percent = (10 * percent) + *sp - '0';
				}
				if (percent > 100) {
					fprintf (stderr, "Percentages must be <= 100\n");
					exit(1);
				}
				if (*sp == '.') {
					fprintf (stderr, "%s: percentages must be integers\n", args.prog_name);
					exit(1);
				}
				/* If not followed by a % then it's a filename. */
				if (*sp != '%') {
					percent = NO_PROB;
					sp = argv[i];
				}
				else if (*++sp == '\0') {
					if (++i >= argc) {
						fprintf (stderr, "%s: percentages must precede filenames\n", args.prog_name);
						exit(1);
					}
					total_percent += percent;
					sp = argv[i]; /* Point sp to the file (argv[i++]) */
				}
			}
			if (strcmp(sp, "all") == 0)
				sp = FORTDIR;
			if (!add_file (percent, sp, NULL, &object->child, &tailp, object)) {
				fprintf (stderr, "%s: failed to add file \"%s\"\n", args.prog_name, sp);
				exit(1);
				/* XXX We're about to crash out if we add a dot file manually. */
			}
			object->num_children++;
		}
		/* XXX Fix this */
		if ( (total_percent > 100) ||
			((total_percent < 100) && (tailp->percent != NO_PROB)) ) {
			fprintf (stderr, "%s: percentages must total 100%% or include undefined percentages\n", args.prog_name);
			exit(1);
		}
	}
	else {
		sp = FORTDIR;
		add_file (NO_PROB, sp, NULL, &object->child, &tailp, object);
		object->num_children++;
	}
	return object;
}

#ifdef DEBUG
void
stat_fp (fp)
FILEDESC	*fp;
{
	fprintf (stderr,
"
--- Statting FilePointer ---
Name is %s
Path is %s
Child is %s
Next is %s
Prev is %s
Percent is %d
Number of children is %d
Number of files is %ld
Number of strings is %ld
",
	fp->name,
	fp->path,
	(fp->child==NULL?"NULL":fp->child->path),
	(fp->next==NULL?"NULL":fp->next->path),
	(fp->prev==NULL?"NULL":fp->prev->path),
	fp->percent,
	fp->num_children,
	fp->num_files,
	fp->tbl.str_numstr
	);
}
#endif	/* DEBUG */

regex_t *
make_regex (void)
{
	regex_t		*preg;
	int			re_flags;
	int			re_error;
	char		*errbuf = NULL;
	int			errsize;

	preg = (regex_t *)malloc (sizeof (regex_t));
	re_flags = (args.ignore_case ? REG_ICASE : 0) |
			REG_NOSUB | REG_NEWLINE;

	DPRINTF (2, (stderr, "Regular expression flags are %d\n", re_flags));
	DPRINTF (1, (stderr, "Compiling regular expression buffer\n"));
	re_error = regcomp (preg, args.reg_pattern, re_flags);
	DPRINTF (2, (stderr, "Checking for regexp error\n"));
	if (re_error) {
		errsize = (regerror (re_error, preg, NULL, (size_t)(0)) + 1);
		errbuf = malloc (errsize);
		regerror (re_error, preg, errbuf, errsize);
		fprintf (stderr, "%s: %s\n", args.prog_name, errbuf);
		exit(1);
	}
	return preg;
}

int
main(argc, argv)
int		argc;
char	*argv[];
{
	FILEDESC	*object;
	int			counter;

	/* This is good, why not keep it. I can't get gettimeofday(3) to work */
	srandom((int)(time((time_t *) NULL) + getpid()));

	zero_args();
	get_args (&argc, &argv);
#ifdef DEBUG
	print_args ();
#endif /* DEBUG */

	object = make_list (argc, argv);

	if (args.print_list) {
#ifdef DEBUG
		if (object->child != NULL)
			get_tbl (object);
#endif	/* DEBUG */
		print_file_list (object);
		exit(0);
	}
	if (object->child == NULL) {
		fprintf (stderr, "%s: no files found\n", args.prog_name);
		exit(1);
	}
	if (args.match_pattern) {
		DPRINTF (2, (stderr, "Making regexp buffer\n"));
		/* Compile regex, checking for ignore case. */
		/* find_matches (fp, preg) */
		exit (find_matches (object, make_regex()));
	}
	// stat_fp (object);

	/* We don't need to do either of these till we know (a) a technique
	 * and (b) Which child to generate from (Master if print list) */
	// get_tbl (object);

	// printf("%s\n", conv_pat("ah237\\z^%&*")); /* This is now OLD_REGEX */
	/* Make several tries at this and return FALSE or TRUE from it */

	for (counter = 0; counter < 128; counter++)
		if ( get_fort (get_fort_fp (object)) )
			exit(0);
	fprintf (stderr, "%s: too many tries\n", args.prog_name);
	exit(1);
}
