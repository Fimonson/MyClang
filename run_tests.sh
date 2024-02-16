#!/bin/bash

set -e

S=$1
Q=$2

./check_names -p . $S/tests/no-dict/*.cpp $S/check_names.cpp > /tmp/no-dict-result.txt 2> /dev/null
./check_names -p . $S/tests/dict/*.cpp -dict $S/tests/dict/dict.txt > /tmp/dict-result.txt 2> /dev/null

diff $S/tests/no-dict/result.txt /tmp/no-dict-result.txt $Q
diff $S/tests/dict/result.txt /tmp/dict-result.txt $Q
