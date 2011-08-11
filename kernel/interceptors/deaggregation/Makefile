obj-m += deaggregate.o
deaggregate-objs := deaggregation.o spin_lock_queue.o injection_thread.o

EXTRA_CFLAGS += -I$(PWD)/../../ -I$(PWD)/../../../include/ -I$(PWD)/../

KERNEL_PATH=/lib/modules/$(shell uname -r)/build

all:	
	make -C $(KERNEL_PATH) M=$(PWD) modules

clean:
	make -C $(KERNEL_PATH) M=$(PWD) clean
	rm -f *~