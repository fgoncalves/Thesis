obj-m += interceptor_framework.o
interceptor_framework-objs := main.o interceptor_manager.o klist.o rule_manager.o rule.o filter.o chains.o hooks.o proc_entry.o proc_registry.o   

EXTRA_CFLAGS += -I$(PWD) -I$(PWD)/../include

KERNEL_PATH=/lib/modules/$(shell uname -r)/build

all:	
	make -C $(KERNEL_PATH) M=$(PWD) modules

clean:
	make -C $(KERNEL_PATH) M=$(PWD) clean
	rm -f *~