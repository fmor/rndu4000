all: build

.PHONY: build
build: clean
	meson setup build
	cd build && meson compile

.PHONY: clean
clean:
	rm -rf build
	make -C src clean


load:
	rmmod -f  rndu4000 || true
	dmesg -C
	insmod build/local__debug/rndu4000.ko || insmod build/rndu4000.ko
	dmesg
	@echo " "
	ls -l /sys/devices/rndu4000
	@echo " "
	ls -l /sys/devices/rndu4000/lcd
	@echo " "
	ls -l /sys/devices/rndu4000/leds
	@echo " "
	cat /proc/devices | grep lcd
	@echo " "
	ls -l  /sys/class/lcd/
	@echo " "
	ls -l /dev/lcd


unload:
	rmmod -f rndu4000
	dmesg

led_on:
	echo 1 > /sys/devices/rndu4000/leds/backup
	echo 1 > /sys/devices/rndu4000/leds/power
	echo 1 > /sys/devices/rndu4000/leds/disk1
	echo 1 > /sys/devices/rndu4000/leds/disk2
	echo 1 > /sys/devices/rndu4000/leds/disk3
	echo 1 > /sys/devices/rndu4000/leds/disk4

led_blinks:
	echo 2 > /sys/devices/rndu4000/leds/backup
	echo 2 > /sys/devices/rndu4000/leds/power
	echo 2 > /sys/devices/rndu4000/leds/disk1
	echo 2 > /sys/devices/rndu4000/leds/disk2
	echo 2 > /sys/devices/rndu4000/leds/disk3
	echo 2 > /sys/devices/rndu4000/leds/disk4

led_off:
	echo 0 > /sys/devices/rndu4000/leds/backup
	echo 0 > /sys/devices/rndu4000/leds/power
	echo 0 > /sys/devices/rndu4000/leds/disk1
	echo 0 > /sys/devices/rndu4000/leds/disk2
	echo 0 > /sys/devices/rndu4000/leds/disk3
	echo 0 > /sys/devices/rndu4000/leds/disk4

test_lcd:
	@clear

	@dmesg -C
	@echo "Clear screen"
	echo -n '\e[2J' > /dev/lcd

	@echo "Return to home"
	echo -n '\e[H' > /dev/lcd
	@dmesg

	@echo "Test backlight"
	@echo -n '\e[L-' > /dev/lcd
	@sleep 1
	echo -n '\e[L+' > /dev/lcd

	@echo "Return to home and write hello world"
	@echo -n '\e[H' > /dev/lcd
	@echo -n 'Hello world' > /dev/lcd

