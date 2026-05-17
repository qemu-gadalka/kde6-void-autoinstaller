#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#define TRY(cmd) do { \
    if (system(cmd) != 0) { \
        fprintf(stderr, "error: command failed -> %s\n", cmd); \
        return 1; \
    } \
} while(0)

int ask_user(const char *question) {
    char answer;
    printf("%s [y/n]: ", question);
    if (scanf(" %c", &answer) != 1) {
        return 0;
    }
    return (answer == 'y' || answer == 'Y');
}

int main() {
    if (getuid() != 0) {
        fprintf(stderr, "error: this installer must be run as root (sudo)!\n");
        return 1;
    }

    printf("checking internet connection...\n");
    if (system("/usr/bin/ping -c 1 -W 2 1.1.1.1 > /dev/null 2>&1") != 0) {
        fprintf(stderr, "error: no internet connection!\n");
        return 1;
    }
    
    if (ask_user("update repos?")) {
        TRY("/usr/bin/xbps-install -S");
    }

    printf("installing xdg-user-dirs\n");
    TRY("/usr/bin/xbps-install -y xdg-user-dirs");

    printf("updating xdg-user-dirs...\n");
    
    const char *sudo_user = getenv("SUDO_USER");
    if (sudo_user && strcmp(sudo_user, "root") != 0) {
        char xdg_cmd[256];
        snprintf(xdg_cmd, sizeof(xdg_cmd), "sudo -u %s /usr/bin/xdg-user-dirs-update", sudo_user);
        TRY(xdg_cmd);
    } else {
        TRY("/usr/bin/xdg-user-dirs-update");
    }

    printf("installing xdg-utils, net-tools\n");
    TRY("/usr/bin/xbps-install -y xdg-utils net-tools");

    printf("installing base components\n");
    TRY("/usr/bin/xbps-install -y elogind dbus-elogind polkit-elogind rtkit NetworkManager");

    printf("making symlinks...\n");
    TRY("/usr/bin/ln -sf /etc/sv/NetworkManager /var/service");
    TRY("/usr/bin/ln -sf /etc/sv/dbus /var/service");
    TRY("/usr/bin/ln -sf /etc/sv/polkitd /var/service");
    TRY("/usr/bin/ln -sf /etc/sv/rtkit /var/service");

    printf("installing kde6 (plasma), sddm...\n");
    TRY("/usr/bin/xbps-install -y xorg-minimal kde-plasma-desktop kde-apps sddm");

    if (ask_user("install firefox?")) {
        TRY("/usr/bin/xbps-install -y firefox");
    }

    printf("detecting environment and video card...\n");
    TRY("/usr/bin/xbps-install -y pciutils");

    int is_vm = (system("/usr/bin/grep -iq 'hypervisor' /proc/cpuinfo 2>/dev/null") == 0 ||
                 system("/usr/bin/grep -iq 'vmware\\|qemu\\|virtualbox' /sys/class/dmi/id/product_name 2>/dev/null") == 0 ||
                 system("/usr/bin/lspci | /usr/bin/grep -iq 'vmware\\|virtualbox\\|qemu'") == 0);

    int is_nvidia = 0;
    int is_intel  = 0;
    int is_amd    = 0;

    if (!is_vm) {
        is_nvidia = (system("/usr/bin/lspci | /usr/bin/grep -iq nvidia") == 0);
        is_intel  = (system("/usr/bin/lspci | /usr/bin/grep -iq intel") == 0);
        is_amd    = (system("/usr/bin/lspci | /usr/bin/grep -iq 'amd\\|ati'") == 0);

        if (is_nvidia) {
            printf("detected NVIDIA GPU\n");
            if (ask_user("install proprietary nvidia driver (nvidia)?")) {
                TRY("/usr/bin/xbps-install -y nvidia");
            } else if (ask_user("install opensource nvidia driver (nouveau)?")) {
                TRY("/usr/bin/xbps-install -y nouveau");
            }
        }

        if (is_intel) {
            printf("detected Intel GPU\n");
            TRY("/usr/bin/xbps-install -y linux-firmware-intel");
        }

        if (is_amd) {
            printf("detected AMD GPU\n");
            TRY("/usr/bin/xbps-install -y linux-firmware-amd");
        }
    } else {
        printf("virtual environment detected, skipping physical GPU firmware.\n");
    }

    if (is_intel || is_amd || is_nvidia || is_vm) {
        TRY("/usr/bin/xbps-install -y mesa-dri");
    }

    if (ask_user("install open-vm-tools (vmware tools)?")) {
        TRY("/usr/bin/xbps-install -y open-vm-tools && /usr/bin/ln -sf /etc/sv/vmtoolsd /var/service");
    }

    printf("installing sddm symlink...\n");
    TRY("/usr/bin/ln -sf /etc/sv/sddm /var/service");

    printf("installation finished successfully!\n");
    return 0;
}
