#!/bin/perl
$base = $ARGV[0];
while (<stdin>)
{
	if (/(\w+)\t+([\d\w]+)/)
	{
		if ($1 eq $base)
		{
			print "$2\n";
		}
	}
}