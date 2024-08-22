@echo off

if not exist "vcpkg" (
    git clone https://github.com/capric8416/vcpkg.git
)

cd vcpkg

git reset --hard 1263aa9173ae55eab936f05877f93da4333ddb25

if not exist "vcpkg.exe" (
    bootstrap-vcpkg.bat
)

