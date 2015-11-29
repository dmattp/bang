#!/bin/bash

for i in `ls -1 test/*.bang`; do bn=`basename $i`;./bang $i > /tmp/out1x; echo $i; diff /tmp/out1x ./test-ref/$bn.out; done
