#! /bin/sh
# takes output of testrun and massages into columnar form for easier
# evaluation and graphing
cat $* | tr ',' ' ' |
    awk 'BEGIN {
	    printf "  Time  Size  Life  Total  Used %%Waste";
	    printf " Real User   Sys  Notes\n";
	}
	$1 == "Sbrked" {
	    sb = $2;
	    i = index(sb, ",")
	    if (i != 0)
		sb = substr(sb, 1, i-1)
	    ma = $4;
	    i = index(ma, ",")
	    if (i != 0)
		ma = substr(ma, 1, i-1)
	    w = $6;
	    next;
	}
	$2 == "real" {
	    if ($1 < mr) mr = $1;
	    if ($3 < mu) mu = $3;
	    if ($5 < ms) ms = $5;
	    next;
	}
	{
	    if (t != 0) {
		printf "%6d %5d %5d %6d %6d %3d", t, s, l, sb/1024, ma/1024, int(w*100);
		printf " %5.1f %5.1f %5.1f %s\n", mr, mu, ms, note;
	    }
	    t = 0
	    # FALLTHROUGH.  Everything after this case wants printing.
	}
	/^[ ]*#/ {
	    note = $0
#	    while (note ~ /^[ #]/) {
		note = substr($0, 2)
#	    }
	    next
	}
	$1 == "Maxtime" || $0 ~ /^\+ \/bin\/time/ {
	    if ($1 == "Maxtime") {
		t = $3;
		s = $6;
		l = $9;
	    } else {
		i = 1
		while (i <= NF) {
		    arg = $i
		    i++
		    if (arg == "-t")
			t = $i
		    else if (arg == "-s")
			s = $i
		    else if (arg == "-l")
			l = $i
		    else if (arg == "-a")
			l = 0
		    else {
#			print "ignoring", arg, $i
			continue
		    }
		    i++
		}
	    }
	    mr = 100000;
	    mu = 100000;
	    ms = 100000;
	    next
        }
        {
#	    print "skipping", $0
	}
	END {
	    if (t != 0) {
		printf "%6d %5d %5d %6d %6d %3d", t, s, l, sb/1024, ma/1024, int(w*100);
		printf " %5.1f %5.1f %5.1f %s\n", mr, mu, ms, note;
	    }
	}
    '
