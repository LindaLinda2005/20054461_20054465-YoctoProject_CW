DESCRIPTION = "Network file configuration"
LICENSE = "CLOSED"
FILESEXTRAPATHS:prepend = "${THISDIR}/files:"
SRC_URI += "file://wpa_supplicant.conf"
SRC_URI += "file://10-eth0.network" 

do_install() {
    install -D -m 0644 ${WORKDIR}/wpa_supplicant.conf ${D}${sysconfdir}/wpa_supplicant/wpa_supplicant.conf
    install -D -m 0644 ${WORKDIR}/10-eth0.network ${D}${sysconfdir}/systemd/network/10-eth0.network
}

#WPA CONFIG
inherit systemd
SYSTEMD_PACKAGES = "${PN}"
SYSTEMD_SERVICE_${PN} += " wpa_supplicant.service"
SYSTEMD_AUTO_ENABLE = "enable"
