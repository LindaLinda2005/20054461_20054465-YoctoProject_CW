SUMMARY = "Custom C program to monitor RPI5 CPU temperature"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

SRC_URI = "file://custom-temp-monitor-9.tar.gz"

S = "${WORKDIR}/myprogram"

#OLD - inherit autotools
inherit cmake

#DEPENDS = "libmodbus"
DEPENDS = "libmodbus pkgconfig-native"

#do_compile() {
#	${CC} ${CFLAGS} -o libmod_demo main.c -lmodbus
#}

do_install () {
	install -d ${D}${bindir}
	install -m 0755 libmod_demo ${D}${bindir}/
}
