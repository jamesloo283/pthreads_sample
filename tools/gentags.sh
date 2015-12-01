#!/bin/bash
#
# Generate cscope and ctags files
# Usage gentags.sh mode topdir
#  mode: required argument
#   all: generates cscope and ctags files in all dirs found in topdir
#   clean: delete cscope and ctags files in topdir
# topdir: optional argument
#   the top directoy to start, if not provided, current directory is used
#
# cscope and ctags are required

#GENTAGSVERBOSE=1
if [ "$GENTAGSVERBOSE" = "1" ]; then
	set -x
fi

# This is a duplicate of RCS_FIND_IGNORE without escaped '()'
ignore="( -name SCCS -o -name BitKeeper -o -name .svn -o \
          -name CVS  -o -name .pc       -o -name .hg  -o \
          -name .git )                                   \
          -prune -o"

usage()
{
	msg="$1"
	if [ ! -z "$msg" ]; then
		echo ""
		echo "Error: $msg"
	fi
	echo "Usage: `basename $0` [all | clean] [topdir]"
	echo ""
	exit 1
}

dotags()
{
	mode=$1

	# validity of $2 must be checked by caller
	topd=$2
	if [ -f $topd/tags ]; then
		rm -f $topd/tags
	fi
	all_sources | xargs $mode -a
}

doclean()
{
	# validity of $1 must be checked by caller
	topd=$1
	if [ ! -z $topd ]; then
		rm -f $topd/tags
		rm -f $topd/cscope*
	fi
}

find_all_dirs()
{
	# validity of $1 must be checked by caller
	topd=$1
	# patch in current top dir, it could possibly contain c files
	ALLSOURCE_DIRS="$topd"
	for dir in `ls ${topd}`; do
		if [ -d ${dir} ]; then
			ALLSOURCE_DIRS="${ALLSOURCE_DIRS} "${dir##\/}
		fi
	done
}

docscope()
{
	# validity of $1 must be checked by caller
	topd=$1
	(echo \-k; echo \-q; all_sources) > $topd/cscope.files
	cscope -b -f $topd/cscope.out
}

all_sources()
{
	for dir in $ALLSOURCE_DIRS
	do
		find_sources $dir '*.[chS]'
	done
}

find_sources()
{
	find "$1" $ignore $prune -name "$2" -print;
}

if [ -z $(which cscope) ]; then
	echo "Error: cscope is not installed, exiting!"
	exit 1
fi

if [ -z $(which ctags) ]; then
	echo "Error: ctags is not installed, continuing with cscope only!"
	noctags=1
fi

mode=${1}
topdir=${2}

# missing mode
if [ -z $mode ]; then
	usage "mode is required"
fi

# if topdir was not provided, use current dir
if [ -z $topdir ]; then
	topdir=$(pwd)
elif [ $topdir = "." ]; then
	# resolve the . (dot) directory
	topdir=$(cd $topdir && pwd)
fi

if [ ! -d $topdir ]; then
	usage "$topdir is not a directory"
fi

case "$1" in
	"all")
		doclean "$topdir"
		find_all_dirs "$topdir"
		docscope "$topdir"
		if [ "$noctags" != "1" ]; then
			dotags ctags
		fi
		;;
	"clean")
		doclean "$topdir"
		;;
	*)
		usage "invalid mode"
		;;
esac
