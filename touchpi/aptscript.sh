DEBIAN_FRONTEND=noninteractive
sudo apt-get update
sudo apt-get purge --yes dphys-swapfile avahi-daemon
sudo apt autoremove
sudo apt-get upgrade --yes
sudo apt-get install --yes --no-install-recommends xserver-xorg xinit accountsservice xserver-xorg-video-fbturbo xserver-xorg-input-evdev xinput-calibrator matchbox-keyboard xinput
sudo apt-get install --yes --no-install-recommends x11-xserver-utils unclutter rpi-chromium-mods epiphany-browser zsh tmux vim libpam-dbus libpam-systemd rsync matchbox-window-manager