#
# Makefile for USB Core files and filesystem
#

EXTRA_CFLAGS += -O0

ifdef CONFIG_USB_MT7621_XHCI_HCD
CORE_OBJ_PATH := mtk
else
CORE_OBJ_PATH := .
endif

usbcore-objs	:= $(CORE_OBJ_PATH)/usb.o $(CORE_OBJ_PATH)/hub.o $(CORE_OBJ_PATH)/hcd.o \
		$(CORE_OBJ_PATH)/urb.o $(CORE_OBJ_PATH)/message.o $(CORE_OBJ_PATH)/driver.o \
		$(CORE_OBJ_PATH)/config.o $(CORE_OBJ_PATH)/file.o $(CORE_OBJ_PATH)/buffer.o \
		$(CORE_OBJ_PATH)/sysfs.o mtk/endpoint.o $(CORE_OBJ_PATH)/devio.o \
		$(CORE_OBJ_PATH)/notify.o $(CORE_OBJ_PATH)/generic.o $(CORE_OBJ_PATH)/quirks.o \
		$(CORE_OBJ_PATH)/devices.o

ifeq ($(CONFIG_PCI),y)
	usbcore-objs	+= $(CORE_OBJ_PATH)/hcd-pci.o
endif

ifeq ($(CONFIG_USB_DEVICEFS),y)
	usbcore-objs	+= $(CORE_OBJ_PATH)/inode.o
endif

obj-$(CONFIG_USB)	+= usbcore.o

ifeq ($(CONFIG_USB_DEBUG),y)
EXTRA_CFLAGS += -DDEBUG
endif

