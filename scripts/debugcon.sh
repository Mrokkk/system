#!/usr/bin/env bash

pid=

function cleanup()
{
    kill ${pid}
}

if [[ ! -p ttyS0.in ]] || [[ ! -p ttyS0.out ]]
then
    echo "No ttyS.{out|in}" >&2
    exit -1
fi

cat ttyS0.out &
pid=$!
cat >ttyS0.in
