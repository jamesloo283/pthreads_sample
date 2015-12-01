#!/bin/bash
#
# archives current dir for backup
# srcdir: required, cannot be current dir
# targetdir: optional, if not provided, create in current dir
#
# if curr dir is a git repo, this is a git backuup, the following
# is performed
#  * a new directory named after srcdir is created in destdir
#  * git.status file is created with current git status
#  * backup archive of srcdir is created in destdir
#
# if not a git repo, standard tar archive is created in destdir

#GOVERBOSE=1
if [ "$GOVERBOSE" = "1" ]; then
	set -x
fi

usage()
{
	msg="$1"
	if [ ! -z "$msg" ]; then
		echo ""
		echo "Error: $msg"
	fi
	echo "Usage: `basename $0` srcdir [destdir]"
	echo ""
	exit 1
}

check_backup()
{
	if [ ! -f "${1}" ]; then
		echo "Error with backup"
	else
		echo "backup created in ${1}"
	fi
}

gitrepobackup()
{
	src=$1
	outdir=$2
	outfile=$3
	gitfile="git.status"
	mkdir "${outdir}"
	git status > "${outdir}"/"${gitfile}"
	tar cjf "${outdir}"/"${outfile}" "${src}"
	check_backup "${outdir}"/"${outfile}"
}

stdbackup()
{
	src="${1}"
	outfile="${2}"
	tar cjf "${outfile}" "${src}"
	check_backup "${outfile}"
}

srcdir=${1}
destdir=${2}
git=${3}

if [ -z "$srcdir" ]; then
	usage "source dir is required"
elif [ $srcdir = "." ]; then
	usage "src dir cannot be current dir"
elif [ ! -d "$srcdir" ]; then
	usage "src dir not found"
fi

# remove trailing / if there's one
srcdir=$(echo ${srcdir%?})

if [ "${destdir}" = "git" ]; then
	destdir=""
	git="git"
fi

if [ -z "$destdir" ]; then
	destdir=$(pwd)
elif [ $destdir = "." ]; then
	# resolve the . (dot) directory
	destdir=$(cd $destdir && pwd)
fi


if [ "$srcdir" = "$destdir" ]; then
	usage "target dir cannot equal source dir"
fi

date=$(date +"%m%d%y-%H%M%S")
OUTPUTDIR="${destdir}"/"${srcdir}"_"${date}"
OUTPUTFILE="${srcdir}"_"${date}".tar.bz2

git status > /dev/null 2>&1
stdbk=$?

if [ "$stdbk" -eq 0 ]; then
	gitrepobackup "${srcdir}" "${OUTPUTDIR}" "${OUTPUTFILE}"
else
	stdbackup "${srcdir}" "$destdir"/"${OUTPUTFILE}"
fi
