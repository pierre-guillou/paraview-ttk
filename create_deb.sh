#!/usr/bin/env bash

function install_build_deb {
    sudo apt install -y \
         ccache \
         cmake-curses-gui \
         dpkg-dev \
         g++ \
         git \
         libegl1-mesa-dev \
         libpython3-dev \
         libqt5x11extras5-dev \
         libxt-dev \
         openssh-server \
         qt5-default \
         qttools5-dev \
         qtxmlpatterns5-dev-tools \
         vim
}

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
