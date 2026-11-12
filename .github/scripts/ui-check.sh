#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Peter Altenbernd <peter.altenbernd@h-da.de>
#
# UI smoke test on an Android emulator: installs PicSync.apk, types into the
# settings fields, scrolls, and takes a screenshot after every step.
set -x
PKG=de.altenbernd.picsync
OUT=shots
mkdir -p "$OUT"

shot() { sleep 2; adb exec-out screencap -p > "$OUT/$1.png"; }
SIZE=$(adb shell wm size | sed -n 's/.*: \([0-9]*x[0-9]*\).*/\1/p' | tail -1)
W=${SIZE%x*}; H=${SIZE#*x}
at() { echo "$(( W * $1 / 1000 )) $(( H * $2 / 1000 ))"; }
echo "screen $W x $H, density $(adb shell wm density)"

adb install -r -g PicSync.apk
adb shell appops set $PKG MANAGE_EXTERNAL_STORAGE allow
adb shell am start -W -n $PKG/org.qtproject.qt.android.bindings.QtActivity
sleep 10
shot 01-start

# Server field (first field on the settings page).
adb shell input tap $(at 500 246)
shot 02-server-focused
adb shell input text "magentacloud"
shot 03-server-typed
adb shell input text ".de"
shot 04-server-typed-more

# User name field.
adb shell input tap $(at 500 343)
shot 05-user-focused
adb shell input text "120049"
shot 06-user-typed

# Hide the keyboard, then scroll down step by step and back up. The swipes
# run along the left margin, so they do not focus any input field.
adb shell input keyevent KEYCODE_BACK
shot 07-keyboard-hidden
adb shell input swipe $(at 25 750) $(at 25 450) 400
shot 08-scrolled-1
adb shell input swipe $(at 25 750) $(at 25 450) 400
shot 09-scrolled-2
adb shell input swipe $(at 25 750) $(at 25 300) 400
shot 10-scrolled-3
adb shell input swipe $(at 25 750) $(at 25 300) 400
shot 11-bottom
adb shell input swipe $(at 25 300) $(at 25 600) 400
shot 12-back-1
adb shell input swipe $(at 25 300) $(at 25 800) 300
shot 13-back-2

# Type into the folder field after scrolling (field below the fold).
adb shell input tap $(at 300 552)
shot 14-folder-focused
adb shell input text "Test"
shot 15-folder-typed

adb shell dumpsys input_method | grep -iE "mInputType|inputType|mCurId|imeOptions" > "$OUT/ime.txt"
adb logcat -d > "$OUT/logcat.txt"
