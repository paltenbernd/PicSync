#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Peter Altenbernd
#
# End-to-end test on an Android emulator against a real Nextcloud (Docker):
#  - many photos in the gallery, notification permission revoked,
#  - "Sync now" via the test hook, then PicSync is sent to the background,
#  - success = every photo has arrived on the server.
set -x
PKG=de.altenbernd.picsync
OUT=e2e
NC=http://localhost:8080
NC_USER=testuser
NC_PASS=Secret-Pass-123
FOLDER=PicSync/e2e
mkdir -p "$OUT"

shot() { adb exec-out screencap -p > "$OUT/$1.png"; }
server_count() {
    curl -s -u "$NC_USER:$NC_PASS" -X PROPFIND -H "Depth: 1" \
        "$NC/remote.php/dav/files/$NC_USER/$FOLDER/DCIM/Camera/" | grep -o "<d:response>" | wc -l
}

adb wait-for-device
until [ "$(adb shell getprop sys.boot_completed | tr -d '\r')" = "1" ]; do sleep 2; done
sleep 45
adb shell input keyevent KEYCODE_HOME
adb shell am broadcast -a android.intent.action.CLOSE_SYSTEM_DIALOGS
adb shell settings put global hide_error_dialogs 1
adb logcat -G 16M || true
adb logcat -c

# Test photos in the camera folder, indexed by the media scanner.
adb shell mkdir -p /sdcard/DCIM/Camera
adb push photos/. /sdcard/DCIM/Camera/ > /dev/null
adb shell content call --uri content://media --method scan_volume --arg external_primary
for f in $(ls photos); do
    adb shell am broadcast -a android.intent.action.MEDIA_SCANNER_SCAN_FILE \
        -d "file:///sdcard/DCIM/Camera/$f" > /dev/null
done
sleep 10
LOCAL=$(ls photos | wc -l)
INDEXED=$(adb shell content query --uri content://media/external/images/media \
          --projection _display_name | grep -c "Row:")
echo "local photos: $LOCAL, indexed: $INDEXED" | tee "$OUT/counts.txt"

# A file on the server whose name Android does not accept (":" like in
# "SharePic 2025-04-14 15:52:53 +0000.jpg"): PicSync must rename it.
for d in PicSync PicSync/e2e PicSync/e2e/apple PicSync/e2e/apple/2025 PicSync/e2e/apple/2025/04; do
    curl -s -o /dev/null -u "$NC_USER:$NC_PASS" -X MKCOL "$NC/remote.php/dav/files/$NC_USER/$d/"
done
curl -s -o /dev/null -w "upload of odd name: %{http_code}\n" -u "$NC_USER:$NC_PASS" -T "photos/$(ls photos | head -1)" \
    "$NC/remote.php/dav/files/$NC_USER/$FOLDER/apple/2025/04/SharePic%202025-04-14%2015:52:53%20+0000.jpg" \
    | tee -a "$OUT/counts.txt"

adb install -r -g PicSync.apk
adb shell appops set $PKG MANAGE_EXTERNAL_STORAGE allow
# Like a phone on which notifications were not allowed.
adb shell pm revoke $PKG android.permission.POST_NOTIFICATIONS

adb shell "am start -W -n $PKG/org.qtproject.qt.android.bindings.QtActivity --es applicationArguments '--test-config=http://10.0.2.2:8080|$NC_USER|$NC_PASS|$FOLDER --test-sync'"
# The test hook presses "Sync now" after 4 s (manual sync as foreground service).
sleep 12
shot 01-sync-started
echo "server after start: $(server_count)" | tee -a "$OUT/counts.txt"

# Send PicSync to the background while it synchronises.
adb shell input keyevent KEYCODE_HOME
adb shell am start -a android.settings.SETTINGS > /dev/null
sleep 5
adb shell dumpsys activity services $PKG > "$OUT/services-while-background.txt"
adb shell dumpsys activity processes > "$OUT/processes-while-background.txt"
shot 02-other-app
for t in 20 40; do
    sleep 10
    adb shell dumpsys activity services $PKG | grep -E "ServiceRecord|isForeground|startRequested" \
        > "$OUT/services-t$t.txt"
done
FOREGROUND=$(grep -c "isForeground=true" "$OUT/services-while-background.txt")
BOUND_BY_JOB=$(grep -c "PicSyncJobService" "$OUT/services-while-background.txt")
echo "foreground service: $FOREGROUND, job service present: $BOUND_BY_JOB" | tee -a "$OUT/counts.txt"

EXPECTED=$((INDEXED + 1))   # +1: the folder itself is a <d:response> too
for i in $(seq 1 60); do
    N=$(server_count)
    echo "t=$((i * 10))s server responses: $N / $EXPECTED" | tee -a "$OUT/counts.txt"
    [ "$N" -ge "$EXPECTED" ] && break
    sleep 10
done

adb shell am start -n $PKG/org.qtproject.qt.android.bindings.QtActivity > /dev/null
sleep 6
shot 03-back-in-app

# The status message must stay usable after the app was in the background.
adb shell input tap 540 450
sleep 2
shot 04-status-message
adb shell input keyevent KEYCODE_HOME
sleep 3
adb shell am start -n $PKG/org.qtproject.qt.android.bindings.QtActivity > /dev/null
sleep 4
shot 05-message-after-resume
adb shell input tap 540 1380
sleep 2
shot 06-after-ok-tap
adb shell input keyevent KEYCODE_BACK
sleep 2
shot 07-after-back
adb logcat -d -s default:* ActivityManager:* AndroidRuntime:* System.err:* JobScheduler:* \
    | grep -v "AiAi" > "$OUT/logcat.txt"

FINAL=$(server_count)
echo "final: $FINAL / $EXPECTED" | tee -a "$OUT/counts.txt"

# Opening the app again must not start a sync by itself.
adb shell am force-stop $PKG
sleep 2
adb shell am start -n $PKG/org.qtproject.qt.android.bindings.QtActivity > /dev/null
sleep 20
AUTO_SYNC=$(adb shell pidof $PKG:sync | wc -w)
adb shell dumpsys jobscheduler | grep -A3 "JOB #.*$PKG" > "$OUT/jobs.txt" || true
echo "sync process after reopening: $AUTO_SYNC" | tee -a "$OUT/counts.txt"
shot 08-reopened

APPLE=$(curl -s -u "$NC_USER:$NC_PASS" -X PROPFIND -H "Depth: 1" \
        "$NC/remote.php/dav/files/$NC_USER/$FOLDER/apple/2025/04/")
RENAMED=$(echo "$APPLE" | grep -c "15-52-53")
ODD_LEFT=$(echo "$APPLE" | grep -c "15:52:53\|15%3a52\|15%3A52")
ON_PHONE=$(adb shell ls /sdcard/apple/2025/04/ 2>/dev/null | grep -c "15-52-53")
echo "renamed on server: $RENAMED, old name left: $ODD_LEFT, on phone: $ON_PHONE" | tee -a "$OUT/counts.txt"

[ "$INDEXED" -gt 0 ] && [ "$FINAL" -ge "$EXPECTED" ] && [ "$FOREGROUND" -gt 0 ] \
    && [ "$RENAMED" -gt 0 ] && [ "$ODD_LEFT" -eq 0 ] && [ "$ON_PHONE" -gt 0 ] \
    && [ "$AUTO_SYNC" -eq 0 ]
