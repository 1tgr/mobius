#! /bin/sh
# Replaces addresses ($3 on '-' and $4 on '+' with unique block numbers
# to minimize the number of blocks presently allocated.
# For trace driven simulation of mallocs.
# Mark Moraes, University of Toronto.
gawk 'BEGIN {
	nextfree = 0;
}
/^[?!]/ {
	print
	next
}
/^\+ / {
	if (nfree > 0) {
		# if we have some existing free blocks numbers, pick one
		for(i in freenums) {
			delete freenums[i];
			nfree--;
			break;
		}
	} else {
		# new block number
		i = nextfree;
		nextfree++;
	}
	if ($4 in blks) {
		print "! block allocated again without free:", $0;
	}
	blks[$4] = i;
	print $1, $2, $3, $4, i;
	next;
}
/^\+\+ / {
	if ($4 in blks) {
		print $0, blks[$4]
	} else {
		print "! realloc on unallocated block:", $0;
	}
	next
}
/^- / {
	if (newblk != 0) {
		print "--", $2, $3;
		newblk = 0;
	} else if ($3 in blks) {
		i = blks[$3];
		delete blks[$3];
		freenums[i] = i;
		nfree++;
		print $1, $2, $3, i;
	} else {
		print "! freeing block that wasnt allocated:", $0;
	}
	next;
}
/^sbrk / {
	newblk = $2;
	heap += newblk;
	next;
}
/^heapstart / {
	print;
	heapstart = $2;
	next;
}
$0 ~ /^allocheader / || $0 ~ /^freeheader / || $0 ~ /^heapend / || $0 ~ /^no-op/ {
	print;
	next;
}
/^wordsize / {
	wdsize = $2;
	print;
	next;
}
{
	print "?", $0;
}
END {
	printf "! %d blocks used\n", nextfree;
	printf "! heap grew to %d bytes\n", wdsize * heap;
}' $@
