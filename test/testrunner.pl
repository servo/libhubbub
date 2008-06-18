#!/bin/perl
#
# Testcase runner for libhubbub
#
# Usage: testrunner <directory> [<executable extension>]
#
# Operates upon INDEX files described in the README.
# Locates and executes testcases, feeding data files to programs 
# as appropriate.
# Logs testcase output to file.
# Aborts test sequence on detection of error.
#

use warnings;
use strict;
use File::Spec;
use IO::Select;
use IPC::Open3;

if (@ARGV < 1) {
	print "Usage: testrunner.pl <directory> [<exeext>]\n";
	exit;
}

# Get directory
my $directory = shift @ARGV;

# Get EXE extension (if any)
my $exeext = "";
$exeext = shift @ARGV if (@ARGV > 0);

# Open log file and /dev/null
open(LOG, ">$directory/log") or die "Failed opening test log";
open(NULL, "+<", File::Spec->devnull) or die "Failed opening /dev/null";

# Open testcase index
open(TINDEX, "<$directory/INDEX") or die "Failed opening test INDEX";

# Parse testcase index, looking for testcases
while (my $line = <TINDEX>) {
	next if ($line =~ /^(#.*)?$/);

	# Found one; decompose
	(my $test, my $desc, my $data) = split /\t+/, $line;

	# Strip whitespace
	$test =~ s/^\s+|\s+$//g;
	$desc =~ s/^\s+|\s+$//g;
	$data =~ s/^\s+|\s+$//g if ($data);

	# Append EXE extension to binary name
	$test = $test . $exeext;

	print "Test: $desc\n";

	if ($data) {
		# Testcase has external data files

		# Open datafile index
		open(DINDEX, "<$directory/data/$data/INDEX") or 
			die "Failed opening $directory/data/$data/INDEX";

		# Parse datafile index, looking for datafiles
		while (my $dentry = <DINDEX>) {
			next if ($dentry =~ /^(#.*)?$/);

			# Found one; decompose
			(my $dtest, my $ddesc) = split /\t+/, $dentry;

			# Strip whitespace
			$dtest =~ s/^\s+|\s+$//g;
			$ddesc =~ s/^\s+|\s+$//g;

			print LOG "Running $directory/$test " .
					"$directory/data/Aliases " .
					"$directory/data/$data/$dtest\n";

			# Make message fit on an 80 column terminal
			my $msg = "    ==> $test [$data/$dtest]";
			$msg = $msg . "." x (80 - length($msg) - 8);

			print $msg;

			# Run testcase
			run_test("$directory/$test", "$directory/data/Aliases", 
					"$directory/data/$data/$dtest");
                }

		close(DINDEX);
	} else {
		# Testcase has no external data files
		print LOG "Running $directory/$test $directory/data/Aliases\n";

		# Make message fit on an 80 column terminal
		my $msg = "    ==> $test";
		$msg = $msg . "." x (80 - length($msg) - 8);

		print $msg;

		# Run testcase
		run_test("$directory/$test", "$directory/data/Aliases");
	}

	print "\n";
}

# Clean up
close(TINDEX);

close(NULL);
close(LOG);

sub run_test
{
	my @errors;

	my $pid = open3("&<NULL", \*OUT, \*ERR, @_);

	$SIG{CHLD} = sub { };

	my $selector = IO::Select->new();
	$selector->add(*OUT, *ERR);

	my $last = "FAIL";

	# Marshal testcase output to log file
	while (my @ready = $selector->can_read) {
		foreach my $fh (@ready) {
			if (fileno($fh) == fileno(OUT)) {
				while (my $output = <OUT>) {
					print LOG "    $output";
					$last = $output;
				}
			} else {
				my @tmp = <ERR>;
				push(@errors, @tmp);
			}

			$selector->remove($fh) if eof($fh);
		}
	}

	waitpid($pid, 0);

	# Catch non-zero exit status and turn it into failure
	if ($? != 0) {
		my $status = $? & 127;

		if ($status != 0) {
			print LOG "    FAIL: Exit status $status\n";
		}
		$last = "FAIL";
	}

	print substr($last, 0, 4) . "\n";

	# Bail, noisily, on failure
	if (substr($last, 0, 4) eq "FAIL") {
		# Write any stderr output to the log
		foreach my $error (@errors) {
			print LOG "    $error";
		}

		print "\n\nFailure detected: consult log file\n\n\n";

		exit(1);
	}

	close(OUT);
	close(ERR);
}

