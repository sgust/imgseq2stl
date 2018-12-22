#!/usr/bin/perl -w

use strict;
use Getopt::Long;

my $xscale = 17.1;
my $yscale = 17.1;
my $zscale = 2000/50;

GetOptions ('xscale=f' => \$xscale, 'yscale=f' => \$yscale, 'zscale=f' => \$zscale);

while(<>) {
	if (/vertex\s+([0-9.efEF]+)\s+([0-9efEF]+)\s+([0-9efEF]+)/) {
		print "vertex ", $1/$xscale, " ", $2/$yscale, " ", $3/$zscale, "\n";
	} else {
		print $_;
	}
}
