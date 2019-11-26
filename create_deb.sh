#!/usr/bin/env bash

function create_deb {
    local builddir='build_deb'
    rm -rf $builddir
    mkdir $builddir
    cd $builddir
    cmake \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX=/usr \
        ..
    cd ..
    cmake --build $builddir --target package -- -j$(nproc)
}

create_deb
