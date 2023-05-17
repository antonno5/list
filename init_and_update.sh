#!/bin/sh


echo "Checking for task updates"
git remote | grep template > /dev/null
has_remote=$?
if [ ! "0" -eq $has_remote ]; then
    echo "adding template repo to git"
    git remote add template git@github.com:cpp-prod-2022/list.git
fi

git fetch --all
git merge template/master --allow-unrelated-histories
git merge origin/master --allow-unrelated-histories

echo "Setting up pre-push hooks"
cp test.sh .git/hooks/pre-push

if [ ! -e compile_commands.json ]; then
    echo "Generating compile_commands.json"
    ./build.sh > /dev/null
    ln -s build/compile_commands.json compile_commands.json
fi

last_line=$(tail -n1 readme.md)

if [ "$last_line" = "# Я невнимательно читал(а) readme и не запустил(а) init\_and\_update.sh" ]; then
    head -n -1 readme.md > ._tmp ; mv ._tmp readme.md
    git add readme.md
fi
