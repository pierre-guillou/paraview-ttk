#!/bin/sh

curr_dir="$(pwd)"
target_file=${curr_dir}/.git/hooks/pre-commit
source_file=${curr_dir}/.git-hooks/pre-commit
if [ -f $target_file ]; then
   echo "Please move current $target_file out of the way first."
else
   ln -s $source_file $target_file
fi
