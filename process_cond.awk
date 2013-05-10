#! /usr/bin/awk -f
/^@if\>/ {
    while (match($0, /@(\w+)@/, var))
	$0 = substr($0, 1, RSTART - 1) \
	    ENVIRON[var[1]] \
	    substr($0, RSTART + RLENGTH)
    stack[sp++] = remove
    remove = remove || ($2 != $3)
    skip = 1
}
/^@else\>/ {
    remove = stack[sp-1] || !remove
    skip = 1
}
/^@endif\>/ {
    remove = stack[--sp]
    skip = 1
}
!remove && !skip { print }
skip { --skip }
