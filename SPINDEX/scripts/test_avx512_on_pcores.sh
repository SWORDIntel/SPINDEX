#!/bin/bash

# This script manually tests AVX-512 on P-cores if a Meteor Lake processor is detected.

# First, we need to build the hardware_detection tool.
gcc -o SPINDEX/core/hardware_detection SPINDEX/core/hardware_detection.c -I SPINDEX/include

# Next, we need to run the hardware_detection tool and check if the processor is a Meteor Lake processor.
is_meteor_lake=$(SPINDEX/core/hardware_detection | grep "Meteor Lake" | awk '{print $3}')

if [ "$is_meteor_lake" == "Yes" ]; then
    echo "Meteor Lake processor detected."

    # Now, we need to determine the P-cores and E-cores.
    # This is a simplified way to do this. A more robust solution would be to parse /proc/cpuinfo.
    p_cores=$(lscpu | grep "On-line CPU(s) list:" | awk '{print $4}' | cut -d'-' -f1)
    e_cores=$(lscpu | grep "On-line CPU(s) list:" | awk '{print $4}' | cut -d'-' -f2)

    echo "P-cores: 0-$p_cores"
    echo "E-cores: $(($p_cores + 1))-$e_cores"

    # Now, we need to build the AVX-512 test.
    gcc -o SPINDEX/tests/test_avx512_phrase_search SPINDEX/plugins/search_algorithms/test_avx512_phrase_search.c SPINDEX/plugins/search_algorithms/avx512_phrase_search.c -I SPINDEX/include -mavx512f -mavx512bw

    # Now, we need to run the AVX-512 test on the P-cores.
    echo "Running AVX-512 test on P-cores..."
    taskset -c 0-$p_cores SPINDEX/tests/test_avx512_phrase_search

    echo "AVX-512 test complete."
else
    echo "Meteor Lake processor not detected."
fi
