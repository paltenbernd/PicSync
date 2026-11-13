#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Peter Altenbernd <peter.altenbernd@h-da.de>
#
# UI smoke test on an Android emulator: installs PicSync.apk and, for each
# variant (with and without the full-flush repaint fix), types into the
# settings fields and scrolls, taking a screenshot after every step.
set -x
PKG=de.altenbernd.picsync
OUT=shots
mkdir -p "$OUT"

SIZE=$(adb shell wm size | sed -n 's/.*: \([0-9]*x[0-9]*\).*/\1/p' | tail -1)
W=${SIZE%x*}; H=${SIZE#*x}
at() { echo "$(( W * $1 / 1000 )) $(( H * $2 / 1000 ))"; }
echo "screen $W x $H, density $(adb shell wm density)"

# Let the freshly booted emulator settle; its launcher often shows an
# "isn't responding" dialog in the first minute.
adb wait-for-device
until [ "$(adb shell getprop sys.boot_completed | tr -d '\r')" = "1" ]; do sleep 2; done
sleep 45
adb shell input keyevent KEYCODE_HOME
adb shell am broadcast -a android.intent.action.CLOSE_SYSTEM_DIALOGS
adb shell settings put global hide_error_dialogs 1

adb install -r -g PicSync.apk
adb shell appops set $PKG MANAGE_EXTERNAL_STORAGE allow

run_variant() {
    NAME=$1; ARGS=$2
    D="$OUT/$NAME"; mkdir -p "$D"
    shot() { sleep 2; adb exec-out screencap -p > "$D/$1.png"; echo "$1 $(adb shell date +%s%3N)" >> "$D/times.txt"; }
    adb shell am force-stop $PKG
    adb shell am broadcast -a android.intent.action.CLOSE_SYSTEM_DIALOGS
    adb shell am start -W -n $PKG/org.qtproject.qt.android.bindings.QtActivity --es applicationArguments "'$ARGS'"
    sleep 8
    shot 01-start
    adb shell input tap $(at 500 222)
    shot 02-server-focused
    adb shell input text "magentacloud"
    shot 03-server-typed
    adb shell input text ".de"
    shot 04-server-more
    adb shell input tap $(at 500 309)
    adb shell input text "120049"
    shot 05-user-typed
    # Keyboard stays open while scrolling (like a user who keeps typing).
    adb shell input swipe $(at 25 750) $(at 25 450) 400
    shot 07-scroll-1
    adb shell input swipe $(at 25 750) $(at 25 450) 400
    shot 08-scroll-2
    adb shell input swipe $(at 25 750) $(at 25 300) 400
    shot 09-scroll-3
    adb shell input swipe $(at 25 300) $(at 25 700) 400
    shot 10-back
    adb shell dumpsys input_method | grep -o "inputType=0x[0-9a-f]*" | sort | uniq -c > "$D/ime.txt"
}

run_variant fixed ""
run_variant without-fix "--partial-flush"

adb logcat -d | grep -iE "picsync|qt" | tail -300 > "$OUT/logcat.txt"
