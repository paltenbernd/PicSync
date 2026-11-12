#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Peter Altenbernd <peter.altenbernd@h-da.de>
#
# UI smoke test on an Android emulator: installs PicSync.apk and, for each
# repaint variant, types into the settings fields and scrolls, taking a
# screenshot after every step. The "default" variant also stores the app's
# own rendering (/sdcard/Download/picsync-diag) for comparison.
set -x
PKG=de.altenbernd.picsync
OUT=shots
mkdir -p "$OUT"

SIZE=$(adb shell wm size | sed -n 's/.*: \([0-9]*x[0-9]*\).*/\1/p' | tail -1)
W=${SIZE%x*}; H=${SIZE#*x}
at() { echo "$(( W * $1 / 1000 )) $(( H * $2 / 1000 ))"; }
echo "screen $W x $H, density $(adb shell wm density)"

adb install -r -g PicSync.apk
adb shell appops set $PKG MANAGE_EXTERNAL_STORAGE allow

run_variant() {
    NAME=$1; ARGS=$2
    D="$OUT/$NAME"; mkdir -p "$D"
    shot() { sleep 2; adb exec-out screencap -p > "$D/$1.png"; echo "$1 $(adb shell date +%s%3N)" >> "$D/times.txt"; }
    adb shell am force-stop $PKG
    adb shell rm -rf /sdcard/Download/picsync-diag
    adb shell am start -W -n $PKG/org.qtproject.qt.android.bindings.QtActivity --es applicationArguments "'$ARGS'"
    sleep 8
    shot 01-start
    adb shell input tap $(at 500 246)
    shot 02-server-focused
    adb shell input text "magentacloud"
    shot 03-server-typed
    adb shell input text ".de"
    shot 04-server-more
    adb shell input tap $(at 500 343)
    adb shell input text "120049"
    shot 05-user-typed
    adb shell input keyevent KEYCODE_BACK
    shot 06-keyboard-hidden
    adb shell input swipe $(at 25 750) $(at 25 450) 400
    shot 07-scroll-1
    adb shell input swipe $(at 25 750) $(at 25 450) 400
    shot 08-scroll-2
    adb shell input swipe $(at 25 750) $(at 25 300) 400
    shot 09-scroll-3
    adb shell input swipe $(at 25 300) $(at 25 700) 400
    shot 10-back
    adb shell dumpsys input_method | grep -o "inputType=0x[0-9a-f]*" | sort | uniq -c > "$D/ime.txt"
    adb pull /sdcard/Download/picsync-diag "$D/diag" > /dev/null 2>&1
}

run_variant default "--debug-grab"
run_variant fastscroll "--fast-scroll"
run_variant fullupdate "--full-update"
run_variant fullflush "--full-flush"
run_variant both "--full-update --full-flush"

adb logcat -d | grep -iE "picsync|qt" | tail -300 > "$OUT/logcat.txt"
