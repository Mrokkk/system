#!/bin/bash

LANG=C

if [ "$2" == "--f" ]; then
    CHECK_FILES=1
fi

ASTYLE="astyle --style=google --indent-switches --indent-col1-comments --pad-oper --align-pointer=name --align-reference=name --remove-brackets --convert-tabs --mode=c "

cd $1

for file in $(find . -name "*.[c|h]"); do
    if ! ${ASTYLE} < "${file}" | diff ${file} - > /dev/null; then
        echo -e "File $file \e[31mFAILED\e[0m"
        if [ $CHECK_FILES ]; then
            echo -n "Show diff? "
            read input
            if [ $input == 'y' ]; then
                ${ASTYLE} < "${file}" | diff --color=always ${file} -
            fi
        fi
    else
        echo -e "File $file \e[32mSUCCEED\e[0m"
    fi
done

