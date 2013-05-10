#! /usr/bin/perl

use Bootloader::Tools;

Bootloader::Tools::InitLibrary();

if ($ARGV[0] eq "--get") {
    my $section = Bootloader::Tools::GetDefaultSection ();
    print $section->{"append"};
} elsif ($ARGV[0] eq "--update") {
    my $section = Bootloader::Tools::GetDefaultSection ();
    my $input = $section->{"append"};
    my $result;
    while (length($input)) {
    	$input =~ s/^[[:space:]]+//;
	if ($input =~ s/^("[^"]*"?|[^"[:space:]]+)+//) {
	    my $rawparam = $&;
	    my $param = $rawparam;
	    $param =~ s/"//g;
	    $param =~ s/=(.*)//;
	    if (! ($param =~ /^(KDUMP|MAKEDUMPFILE)_|^fadump$/)) {
		$result .= " " if length($result);
		$result .= $rawparam;
	    }
	}
    }

    shift @ARGV;
    $result .= " " if length($result);
    $result .= join(" ", @ARGV);
    $section->{"append"} = $result;
    $section->{"__modified"} = 1;
    Bootloader::Tools::SetGlobals();
} else {
    die "Need an action (--get or --update)";
}
