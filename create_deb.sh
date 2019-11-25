#!/usr/bin/env bash

function create_deb {
    local builddir='build_deb'
    rm -rf $builddir
    mkdir $builddir
    cmake \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX=/usr \
        -B $builddir
    cmake --build $builddir --target package
}

create_deb
