#
# Define kdump directories here						     {{{
TESTKDUMP=(	"2014-03-27-10:37"
		"2013-12-31-23:59"
		"2014-03-05-12:15"
		"2014-01-12-09:22"
)									   # }}}

#
# Define non-kdump directories here					     {{{
TESTDIRS=(	"2014-03-26-18:15"
		"unrelated"
		"app-dump"
)									   # }}}

#
# Define test files here						     {{{
TESTFILES=(	"beta"
		"99-last"
		"gamma"
		"UpperCase"
		"123-digit"
)									   # }}}

#
# Set up the test directory
#									     {{{
function setup_testdir()
{
    local dest="$1"
    local tdir f

    rm -rf "$dest"
    mkdir "$dest" || return 1

    for tdir in "${TESTKDUMP[@]}" "${TESTDIRS[@]}"; do
	mkdir -p "$dest/$tdir" || return 1
    done

    for tdir in "${TESTKDUMP[@]}"; do
	touch "$dest/$tdir/vmcore" || return 1
    done

    for f in "${TESTFILES[@]}"; do
	touch "$dest/$f" || return 1
    done
}									   # }}}
