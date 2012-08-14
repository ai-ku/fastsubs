#!/usr/bin/perl -w
# Take fastsubs output and convert the unnormalized log10(p) entries
# to normalized p entries.
use strict;
while(<>) {
    my @a = split;
    my $z;
    for (my $i = 2; $i <= $#a; $i += 2) {
	$a[$i] = 10**$a[$i];
	$z += $a[$i];
    }
    for (my $i = 2; $i <= $#a; $i += 2) {
	$a[$i] /= $z;
    }
    print join("\t", @a)."\n";
}
