#!/bin/bash

if [ ! -d "vcpkg" ]; then
    git clone https://github.com/capric8416/vcpkg.git
fi

cd vcpkg || exit

git reset --hard 1263aa9173ae55eab936f05877f93da4333ddb25

if [ ! -f "vcpkg" ]; then
    ./bootstrap-vcpkg.sh
fi

