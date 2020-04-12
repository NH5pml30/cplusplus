#!/bin/bash -e

tests_passed=0
tests_total=0

for i in sub_tests/test*.txt
do
    if test -f "$i" 
    then
        if ./sub < "$i" | cmp -s - sub_tests/$(basename $i .txt).gold
        then
            (( tests_passed += 1 ))
        else
            echo "failed"
        fi
    fi
    (( tests_total += 1 ))
done

echo "passed: $tests_passed, total: $tests_total"

tests_passed=0
tests_total=0

for i in mult_tests/test*.txt
do
    if test -f "$i" 
    then
        if ./mult < "$i" | cmp -s - mult_tests/$(basename $i .txt).gold
        then
            (( tests_passed += 1 ))
        else
            echo "failed"
        fi
    fi
    (( tests_total += 1 ))
done

echo "passed: $tests_passed, total: $tests_total"

