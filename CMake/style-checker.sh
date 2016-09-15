#!/bin/bash

LANG=C

ASTYLE="astyle --style=google \
               --indent-switches \
               --indent-col1-comments \
               --min-conditional-indent=0 \
               --max-instatement-indent=40 \
               --pad-oper \
               --pad-header \
               --align-pointer=name \
               --align-reference=name \
               --remove-brackets \
               --convert-tabs \
               --mode=c \
               --break-after-logical "

TOTAL=0
BAD=0

if [ "$2" == "-f" ]; then
    CHECK_FILES=1
fi

cd $1

for file in $(find . -name "*.[c|h]"); do
    (( TOTAL=$TOTAL+1 ))
    if ! ${ASTYLE} < "${file}" | diff ${file} - > /dev/null; then
        (( BAD=$BAD+1 ))
        echo -e "File $file \e[31mFAILED\e[0m"
        if [ $CHECK_FILES ]; then
            echo -n "Show diff? [y/N] "
            read input
            if [ "$input" == "y" ]; then
                ${ASTYLE} < "${file}" | diff --color=always ${file} -
            fi
        fi
    else
        echo -e "File $file \e[32mSUCCEED\e[0m"
    fi
done

echo "Bad formatted files: $BAD/$TOTAL"

