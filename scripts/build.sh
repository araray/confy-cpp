#!/usr/bin/env bash

p_target_mode="${1:-Release}"

dir_base="${2:-$PWD}"
dir_src="${dir_base}/"
dir_build="${dir_base}/build"

{
    if [[ -d "${dir_build}" ]]; then
        rm -rf "${dir_build}"
    fi

    mkdir -p "${dir_build}"

    cd "${dir_build}"

    cmake -DCMAKE_BUILD_TYPE=${p_target_mode} "${dir_src}"

    cmake --build ${dir_build} -j

    ctest --output-on-failure
} 2>&1
