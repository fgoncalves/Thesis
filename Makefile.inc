INCLUDE_DIR=include
#CC=arm-unknown-linux-gnu-gcc
CC=gcc
CFLAGS=-fPIC -g -DDEBUG -DBASH

#We can only change the name of this folder. We cannot put it into another directory 
#and it must end with a slash '/'
BUILD_DIR=build/

LIB_NAME=fred