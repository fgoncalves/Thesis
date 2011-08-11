include Makefile.inc

all:
	mkdir -p $(BUILD_DIR)
	make -C sax_parser/
	make -C localregistry/user/
	make -C localregistry/daemon/
	make -C configuration/ objs
	make -C configuration/user/
	make -C datastructures/
	make -C handler/

lib: all
	$(CC) -fPIC -shared -Wl,-soname,lib$(LIB_NAME).so -o lib$(LIB_NAME).so.1.0.1 -lrt -lpthread $(BUILD_DIR)*.o

install: lib
	cp -r include/ /usr/include/$(LIB_NAME)
	cp lib$(LIB_NAME).so.1.0.1 /usr/lib/
	ln -s /usr/lib/lib$(LIB_NAME).so.1.0.1 /usr/lib/lib$(LIB_NAME).so

uninstall:
	rm -rf /usr/include/$(LIB_NAME) /usr/lib/lib$(LIB_NAME).so.1.0.1 /usr/lib/lib$(LIB_NAME).so

install_scripts:
	make -C localregistry/daemon/ install
	make -C localregistry/shell/ install
	make -C interceptor_shell/ install
	make -C configuration/ install

uninstall_scripts:
	make -C localregistry/daemon/ uninstall
	make -C localregistry/shell/ uninstall
	make -C interceptor_shell/ uninstall
	make -C configuration/ uninstall

redo: clean uninstall all install uninstall_scripts install_scripts

deploy: clean lib
	rm -rf deploy/
	./deploy.sh
	rm -f deploy.tgz
	tar cvzf deploy.tgz deploy/

clean:
	rm -rf $(BUILD_DIR) lib$(LIB_NAME).so.1.0.1
	make -C sax_parser/ clean
	make -C localregistry/user/ clean
	make -C localregistry/daemon/ clean
	make -C localregistry/shell/ clean
	make -C datastructures/ clean
	make -C handler/ clean
	make -C configuration/ clean
	make -C configuration/user clean
	make -C interceptor_shell/ clean