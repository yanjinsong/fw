#!/bin/sh
#
#cd $PROJ
function run_lint
{
    wine "c:/lint/lint-nt.exe" \
	-e1904 +v  -i"c:\\lint\\lnt" \
	-i"c:\\lint" env-si std \
	-"format=\n%t:%n: ##%f##:%l: %m\n------\n" ./script/gcc_inc.lnt ./script/defs.lnt ./script/CAB.lnt $*
}

cd ..
if [ -n "$2" ]; then
  echo $2 > lint_file.lnt;
  make SHOW_LINT=1 LINT_FILE_LIST=lint_file lint;
else
  make SHOW_LINT=1 lint;
fi

