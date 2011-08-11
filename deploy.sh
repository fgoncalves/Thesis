#!/bin/bash

mkdir -p deploy/
make -C localregistry/daemon/ exec
mv localregistry/daemon/registry_daemon deploy/
make -C localregistry/shell/
mv localregistry/shell/register deploy/
make -C interceptor_shell/
mv interceptor_shell/mkrule deploy/
mv interceptor_shell/rmrule deploy/
make -C configuration/
mv configuration/config_daemon deploy/
cd kernel/
./compile.sh
cd -
cp -r kernel/modules deploy
cp Makefile.deploy deploy/Makefile
cp libfred.so.1.0.1 deploy/libfred.so
cp -r include/ deploy/	