DEBIAN_FRONTEND=noninteractive
sudo apt-get update
sudo apt-get purge --yes dphys-swapfile
sudo apt autoremove
sudo apt-get upgrade --yes
sudo apt-get install --yes --no-install-recommends lightdm xserver-xorg accountsservice xserver-xorg-video-fbturbo xserver-xorg-input-evdev xinput-calibrator matchbox-keyboard xinput
sudo apt-get install --yes --no-install-recommends spectrwm x11-xserver-utils unclutter epiphany-browser zsh tmux vim libpam-dbus libpam-systemd rsync