#!/usr/bin/perl -w

use strict;

my $xmin = 1e6;
my $ymin = 1e6;
my $zmin = 1e6;

my $xmax = -1;
my $ymax = -1;
my $zmax = -1;

while(<>) {
	if (/vertex\s+([0-9.efEF]+)\s+([0-9efEF]+)\s+([0-9efEF]+)/) {
		if ($1 < $xmin) { $xmin = $1; }
		if ($2 < $ymin) { $ymin = $2; }
		if ($3 < $zmin) { $zmin = $3; }
		if ($1 > $xmax) { $xmax = $1; }
		if ($2 > $ymax) { $ymax = $2; }
		if ($3 > $zmax) { $zmax = $3; }
	}
}

print "X ", $xmin, " - ", $xmax, " size ", $xmax-$xmin, "\n";
print "Y ", $ymin, " - ", $ymax, " size ", $ymax-$ymin, "\n";
print "Z ", $zmin, " - ", $zmax, " size ", $zmax-$zmin, "\n";
