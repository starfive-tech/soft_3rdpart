# Comment/uncomment the	following line to disable/enable debugging
#DEBUG = y

# Add your debugging flag (or not) to CFLAGS
ifeq ($(DEBUG),y)
  DEBFLAGS = -O	-g # "-O" is needed to expand inlines
else
  DEBFLAGS = -O2
endif

export CC="$(CROSS_COMPILE)gcc"
export AR="$(CROSS_COMPILE)ar"
export CXX="${CROSS_COMPILE}g++"
export AS="${CROSS_COMPILE}as"
export LD="${CROSS_COMPILE}ld"
export RANLIB="${CROSS_COMPILE}ranlib"
export READELF="${CROSS_COMPILE}readelf"
export STRIP="${CROSS_COMPILE}strip"


EXTRA_CFLAGS +=	$(DEBFLAGS) -I$(LDDINCDIR)

ifneq ($(KERNELRELEASE),)
# call from kernel build system

obj-m	:= jpu.o

else

KERNELDIR := ../../../../../work/linux/
PWD	  := $(shell pwd)

default:
	$(MAKE)	-C $(KERNELDIR)	M=$(PWD) LDDINCDIR=$(PWD)/../include modules

endif



clean:
	rm -rf *.o *~ core .depend .*.cmd *.ko *.mod.c .tmp_versions

depend .depend dep:
	$(CC) $(CFLAGS)	-M *.c > .depend


ifeq (.depend,$(wildcard .depend))
include	.depend
endif
