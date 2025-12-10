PWD := $(shell pwd)

all: modules

modules:
	# Build BSP module
	$(MAKE) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) -C $(KERNEL_SRC) M=$(PWD)/BSP \
	EXTRA_CFLAGS="-I$(PWD)/BSP/include" CFG_AML_WIFI_DEVICE_UWE5621=y modules
	$(CROSS_COMPILE)strip --strip-unneeded $(PWD)/BSP/uwe5621_bsp_sdio.ko

	# Build WIFI module, tambahkan include path BSP/include
	$(MAKE) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) -C $(KERNEL_SRC) M=$(PWD)/WIFI \
	EXTRA_CFLAGS="-I$(PWD)/BSP/include -I$(PWD)/WIFI/include" CFG_AML_WIFI_DEVICE_UWE5621=y modules
	$(CROSS_COMPILE)strip --strip-unneeded $(PWD)/WIFI/sprdwl_ng.ko

modules_install:
	$(MAKE) INSTALL_MOD_STRIP=1 M=$(PWD)/BSP -C $(KERNEL_SRC) modules_install
	mkdir -p ${OUT_DIR}/../vendor_lib/modules
	cd ${OUT_DIR}/$(M); find -name "*.ko" -exec cp {} ${OUT_DIR}/../vendor_lib/modules/ \;
