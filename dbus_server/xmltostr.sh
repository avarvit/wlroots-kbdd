#!/bin/sh

awk '
{
    gsub(/\\/, "\\\\", $0);
    gsub(/"/, "\\\"", $0);
    if (NR > 1) {
        printf "\\n\"\n\"";
    }
    printf "%s", $0;
}
END {
    printf "\";\n";
}
BEGIN {
    printf "const char *'$1' = \""
}' $2 > $3
