#!/bin/bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Peter Altenbernd <peter.altenbernd@h-da.de>
#
# Builds PicSync for Android (arm64-v8a) entirely from source, as required by
# F-Droid: OpenSSL and Qt (qtbase) come from the pinned git submodules in
# 3rdparty/, nothing prebuilt is downloaded except the Android SDK/NDK and the
# Gradle/Maven dependencies of the Android packaging step.
#
# Requirements: Android SDK (platform android-34, build-tools), NDK r26b,
# JDK 17, cmake, ninja, perl, python3, lrelease (Debian: qt6-l10n-tools).
#
# Environment:
#   ANDROID_SDK_ROOT   Android SDK                      (required)
#   ANDROID_NDK_ROOT   Android NDK r26b                 (required)
#   JAVA_HOME          JDK 17                           (required)
#   WORK               build directory  (default: <repo>/build-fromsource)
#   LRELEASE           lrelease binary  (default: found in PATH or /usr/lib/qt6/bin)
#   PICSYNC_GRADLE     gradle command to use instead of the Gradle wrapper
#                      shipped with Qt (F-Droid sets this to its own gradle)
#   JOBS               parallel build jobs (default: number of CPUs)
#
# Result: $WORK/PicSync-release-unsigned.apk
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
WORK="${WORK:-$ROOT/build-fromsource}"
JOBS="${JOBS:-$(nproc)}"
ABI=arm64-v8a
API=33                       # minimum SDK of PicSync
PLATFORM=android-34          # compile SDK used by Qt 6.8

: "${ANDROID_SDK_ROOT:?set ANDROID_SDK_ROOT}"
: "${ANDROID_NDK_ROOT:?set ANDROID_NDK_ROOT}"
: "${JAVA_HOME:?set JAVA_HOME}"

QT_SRC="$ROOT/3rdparty/qtbase"
SSL_SRC="$ROOT/3rdparty/openssl"
QT_HOST="$WORK/qt-host"
QT_ANDROID="$WORK/qt-android"
SSL_OUT="$WORK/openssl"      # same layout as KDAB/android_openssl: ssl_3/<abi>/
TOOLCHAIN="$ANDROID_NDK_ROOT/toolchains/llvm/prebuilt/linux-x86_64/bin"

if [ -z "${LRELEASE:-}" ]; then
    LRELEASE="$(command -v lrelease || true)"
    [ -n "$LRELEASE" ] || LRELEASE=/usr/lib/qt6/bin/lrelease
fi
[ -x "$LRELEASE" ] || { echo "lrelease not found (set LRELEASE)"; exit 1; }
[ -f "$QT_SRC/CMakeLists.txt" ] || { echo "Run: git submodule update --init"; exit 1; }

mkdir -p "$WORK"
step() { echo; echo "=== $* ==="; }

# --- 1. OpenSSL for Android ---------------------------------------------------
# Qt loads OpenSSL at runtime and expects libcrypto_3.so / libssl_3.so without
# version suffixes, so the shared library variant is set to "_3" and the
# symbol version names are kept unchanged.
if [ ! -f "$SSL_OUT/ssl_3/$ABI/libssl_3.so" ]; then
    step "Building OpenSSL $(cd "$SSL_SRC" && git describe --tags 2>/dev/null || echo)"
    rm -rf "$WORK/openssl-build"
    cp -a "$SSL_SRC" "$WORK/openssl-build"
    cd "$WORK/openssl-build"
    perl -0pi -e 's/(shared_extension\s*=>\s*"\.so",)/$1\n        shlib_variant    => "_3",/' \
        Configurations/15-android.conf
    perl -pi -e 's/OPENSSL\$\{SO_VARIANT\}_/OPENSSL_/g' util/mkdef.pl
    export PATH="$TOOLCHAIN:$PATH"
    ./Configure android-arm64 shared no-tests -U__ANDROID_API__ \
        -D__ANDROID_API__=$API -Wl,-z,max-page-size=16384
    make -j"$JOBS" SHLIB_VERSION_NUMBER= build_libs
    llvm-strip --strip-all libcrypto_3.so libssl_3.so
    mkdir -p "$SSL_OUT/ssl_3/$ABI"
    cp libcrypto_3.so libssl_3.so "$SSL_OUT/ssl_3/$ABI/"
    rm -rf "$SSL_OUT/ssl_3/include"
    cp -a include "$SSL_OUT/ssl_3/include"
    cd "$ROOT"
fi

# --- 2. Qt for the build machine (tools: qmake, moc, rcc, androiddeployqt) -----
if [ ! -x "$QT_HOST/bin/qmake" ]; then
    step "Building Qt (host tools)"
    rm -rf "$WORK/qt-host-build"
    mkdir -p "$WORK/qt-host-build"
    cd "$WORK/qt-host-build"
    "$QT_SRC/configure" -prefix "$QT_HOST" -release -opensource -confirm-license \
        -nomake tests -nomake examples -no-opengl -no-feature-vulkan -no-xcb \
        -no-dbus -no-icu -no-glib -no-feature-sql -qpa offscreen
    cmake --build . --parallel "$JOBS"
    cmake --install .
    cd "$ROOT"
fi

# --- 3. Qt for Android --------------------------------------------------------
if [ ! -x "$QT_ANDROID/bin/qmake" ]; then
    step "Building Qt for Android ($ABI)"
    rm -rf "$WORK/qt-android-build"
    mkdir -p "$WORK/qt-android-build"
    cd "$WORK/qt-android-build"
    "$QT_SRC/configure" -prefix "$QT_ANDROID" -release -opensource -confirm-license \
        -platform android-clang -android-abis "$ABI" \
        -android-sdk "$ANDROID_SDK_ROOT" -android-ndk "$ANDROID_NDK_ROOT" \
        -qt-host-path "$QT_HOST" -nomake tests -nomake examples \
        -no-feature-vulkan -no-feature-sql -openssl-runtime \
        -- -DOPENSSL_INCLUDE_DIR="$SSL_OUT/ssl_3/include"
    cmake --build . --parallel "$JOBS"
    cmake --install .
    cd "$ROOT"
fi

# F-Droid builds with its own gradle; replace Qt's wrapper by a forwarding script.
if [ -n "${PICSYNC_GRADLE:-}" ]; then
    cat > "$QT_ANDROID/src/3rdparty/gradle/gradlew" <<EOF
#!/bin/sh
exec $PICSYNC_GRADLE "\$@"
EOF
    chmod +x "$QT_ANDROID/src/3rdparty/gradle/gradlew"
fi

# --- 4. PicSync ---------------------------------------------------------------
step "Building PicSync"
rm -rf "$WORK/picsync"
mkdir -p "$WORK/picsync"
cd "$WORK/picsync"
# Exported, because qmake reads it again when make creates the sub-Makefiles.
export ANDROID_OPENSSL_DIR="$SSL_OUT"
"$QT_ANDROID/bin/qmake" "$ROOT/PicSync.pro" \
    -spec android-clang ANDROID_ABIS="$ABI" CONFIG+=release \
    QT_TOOL.lrelease.binary="$LRELEASE"
make -j"$JOBS"
make -C src/app INSTALL_ROOT="$WORK/picsync/android-build" install
"$QT_HOST/bin/androiddeployqt" \
    --input src/app/android-PicSync-deployment-settings.json \
    --output "$WORK/picsync/android-build" --android-platform "$PLATFORM" \
    --jdk "$JAVA_HOME" --gradle --release
APK="$(ls "$WORK"/picsync/android-build/build/outputs/apk/release/*-unsigned.apk | head -1)"
cp "$APK" "$WORK/PicSync-release-unsigned.apk"
unzip -l "$WORK/PicSync-release-unsigned.apk" | grep -q "lib/$ABI/libssl_3.so"
step "Done: $WORK/PicSync-release-unsigned.apk"
