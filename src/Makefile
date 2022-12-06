obj-m += rndu4000.o
rndu4000-objs := lcd.o led.o buttons.o module.o


PWD := $(CURDIR)

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
