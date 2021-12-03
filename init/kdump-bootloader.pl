#! /usr/bin/perl

use Bootloader::Tools;

Bootloader::Tools::InitLibrary();

my $grub2;
my $section;
if (Bootloader::Tools::GetBootloader() =~ /^(grub2|grub2-efi)$/) {
    $grub2 = true;
    $section = Bootloader::Tools::GetGlobals();
} else {
    $grub2 = false;
    $section = Bootloader::Tools::GetDefaultSection();
}

if ($ARGV[0] eq "--get") {
    print $section->{"append"};
} elsif ($ARGV[0] eq "--update") {
    my $input = $section->{"append"};
    my $result;
    while (length($input)) {
    	$input =~ s/^[[:space:]]+//;
	if ($input =~ s/^("[^"]*"?|[^"[:space:]]+)+//) {
	    my $rawparam = $&;
	    my $param = $rawparam;
	    $param =~ s/"//g;
	    $param =~ s/=(.*)//;
	    if (! ($param =~ /^fadump$/)) {
		$result .= " " if length($result);
		$result .= $rawparam;
	    }
	}
    }

    shift @ARGV;
    $result .= " " if length($result);
    $result .= join(" ", @ARGV);
    if ($grub2) {
	Bootloader::Tools::SetGlobals("append" => $result);
    } else {
	$section->{"append"} = $result;
	$section->{"__modified"} = 1;
	Bootloader::Tools::SetGlobals();
    }
} else {
    die "Need an action (--get or --update)";
}
