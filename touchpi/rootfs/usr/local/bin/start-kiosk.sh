#!/bin/sh
KIOSKURI="http://localhost/"

export DISPLAY=:0.0

xset dpms 0 0 0
xset dpms force on
xset s off
xset s noblank

matchbox-window-manager -use_cursor no -use_titlebar no -use_desktop_mode plain -use_dialog_mode const-horiz &


BDIR=/run/user/1000/browser
BCDIR=/run/user/1000/browsercache
mkdir -p "$BDIR"
mkdir -p "$BCDIR"
#create empty First\ Run file
touch "$BDIR"/First\ Run
chromium-browser --kiosk --noerrdialogs --incognito --disable --disable-translate --disable-infobars --disable-suggestions-service --disable-save-password-bubble --disk-cache-dir="$BCDIR" --user-data-dir="$BDIR" --start-maximized --window-size=${resolution} --disable-bundled-ppapi-flash "$KIOSKURI"

exit 0
