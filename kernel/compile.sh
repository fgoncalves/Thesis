#!/bin/bash

cores="-j2"
kernel_path="/lib/modules/`uname -r`/build"
out_dir=`pwd`"/modules/"
on_error_resume=0

CLEAN=0

while getopts k:o:cr OPTION
do
    case ${OPTION} in
        k) kernel_path=$OPTARG;;
        o) out_dir=$OPTARG;;
        c) CLEAN=1;;
	r) on_error_resume=1;;
      	\?) echo "Usage: ${PROG_NAME} [ -k <kernel_path> -c -o <module_directory> -r]"
	    echo "-k    Alternate kernel source."
	    echo "-c    Clean modules."
	    echo "-o    Directory where to store compiled modules."
	    echo "-r    On error resume."
           exit 2;;
    esac
done

shift $(($OPTIND - 1))

if [ ! -e $out_dir ]
then
    echo ">> Creating directory $out_dir"
    mkdir $out_dir
fi

function check_error {
    if [ $? -ne 0 ] && [ $on_error_resume -eq 0 ]
    then
	exit 1
    fi
}

function compile {
    make KERNEL_PATH=$kernel_path clean
    check_error
    make $cores KERNEL_PATH=$kernel_path
    check_error
    mv *.ko $out_dir
    check_error
    for dir in ./interceptors/*
    do
	if  test -d $dir 
	then
	    cd $dir
	    make KERNEL_PATH=$kernel_path clean
	    check_error
	    cp ../../Module.symvers .
	    check_error
	    make $cores KERNEL_PATH=$kernel_path
	    check_error
	    mv *.ko $out_dir
	    check_error
	    cd ../../
	fi
    done
}

function clean {
    make KERNEL_PATH=$kernel_path clean
    check_error
    for dir in ./interceptors/*
    do
	if  test -d $dir 
	then
	    cd $dir
	    make KERNEL_PATH=$kernel_path clean
	    check_error
	    cd ../../
	fi
    done
    rm -rf $out_dir
}

if [ $CLEAN -eq 1 ]
then
    clean
else
    compile
fi