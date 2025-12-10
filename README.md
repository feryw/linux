Linux kernel
============

Repositori ini berisi source code Linux Kernel yang digunakan untuk platform
Amlogic ARM64, termasuk pengembangan, kompilasi, dan integrasi berbagai driver
seperti MT7668 dan UWE5621.

Dokumentasi Kernel
------------------

Kernel Linux menyediakan berbagai panduan untuk developer dan pengguna. Panduan
tersebut dapat dirender dalam beberapa format seperti HTML dan PDF.

Sebelum memulai, baca file berikut:

    Documentation/admin-guide/README.rst

Untuk membangun dokumentasi:

    make htmldocs
    make pdfdocs

Dokumentasi online tersedia di:

    https://www.kernel.org/doc/html/latest/

Direktori `Documentation/` berisi berbagai file teks, termasuk yang menggunakan
format reStructuredText (RST). Pastikan membaca:

    Documentation/process/changes.rst

File tersebut menjelaskan persyaratan build kernel dan potensi masalah ketika
melakukan upgrade kernel.

Compiler yang digunakan
-----------------------

Kernel ini dikompilasi menggunakan toolchain berikut:

    aarch64-linux-gnu-gcc (Ubuntu 11.5.0-1ubuntu1~24.04) 11.5.0

Pastikan toolchain tersedia:

    sudo apt install gcc-aarch64-linux-gnu g++-aarch64-linux-gnu

Build Modules
-------------

Masuk ke direktori kernel, kemudian jalankan:

    make modules

Perintah ini akan membangun seluruh modul (.ko) yang tersedia dalam source
kernel.

Build Kernel
------------

Untuk membangun kernel utama (Image, modul, dan object lain), gunakan:

    make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- KCFLAGS="-Wno-error" -j$(nproc)

Opsi `-Wno-error` digunakan untuk mengabaikan warning tertentu yang dianggap
error oleh compiler.

Build Driver UWE5621 (Unisoc)
-----------------------------

Untuk melakukan build modul driver WiFi/Bluetooth UWE5621, gunakan environment
variable berikut:

    export ARCH=arm64
    export CROSS_COMPILE=aarch64-linux-gnu-
    export KERNEL_SRC=/home/USER/linux-amlogic      # Path ke root kernel
    export PWD=$(pwd)                               # Folder driver unisoc_uwe5621
    export OUT_DIR=/home/USER/linux-amlogic/build   # Output folder opsional

Perintah build modul:

    make -C $KERNEL_SRC M=$PWD modules

Modul hasil kompilasi akan berada di dalam direktori build atau folder driver,
sesuai konfigurasi yang digunakan.

Catatan
-------

Repositori ini ditujukan untuk pengembangan kernel Amlogic dengan berbagai
modifikasi, termasuk dukungan driver eksternal seperti MT7668S (SDIO) dan
UWE5621.

