// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Peter Altenbernd <peter.altenbernd@h-da.de>
// This file is part of PicSync.

#include "androidcredentialstore.h"
#include "jniutils.h"

#include <QSettings>

namespace PicSync {

namespace {

constexpr char kKeyAlias[] = "picsync_nextcloud_login";
constexpr char kKeyStore[] = "AndroidKeyStore";
constexpr jint kPurposeEncryptDecrypt = 3;   // PURPOSE_ENCRYPT | PURPOSE_DECRYPT
constexpr jint kEncryptMode = 1;             // Cipher.ENCRYPT_MODE
constexpr jint kDecryptMode = 2;             // Cipher.DECRYPT_MODE
constexpr jint kTagBits = 128;

// Opens the Android Keystore.
QJniObject openKeyStore()
{
    QJniObject store = QJniObject::callStaticObjectMethod(
        "java/security/KeyStore", "getInstance", "(Ljava/lang/String;)Ljava/security/KeyStore;",
        Jni::string(QString::fromLatin1(kKeyStore)).object<jstring>());
    if (store.isValid())
        store.callMethod<void>("load", "(Ljava/security/KeyStore$LoadStoreParameter;)V",
                               static_cast<jobject>(nullptr));
    return store;
}

} // namespace

// Returns the Keystore key, generating it on first use.
QJniObject AndroidCredentialStore::key()
{
    QJniObject store = openKeyStore();
    if (!store.isValid())
        return QJniObject();
    const QJniObject alias = Jni::string(QString::fromLatin1(kKeyAlias));

    if (!store.callMethod<jboolean>("containsAlias", "(Ljava/lang/String;)Z",
                                    alias.object<jstring>())) {
        QJniObject builder("android/security/keystore/KeyGenParameterSpec$Builder",
                           "(Ljava/lang/String;I)V", alias.object<jstring>(),
                           kPurposeEncryptDecrypt);
        const QJniObject modes = Jni::stringArray({ QStringLiteral("GCM") });
        const QJniObject paddings = Jni::stringArray({ QStringLiteral("NoPadding") });
        builder.callObjectMethod("setBlockModes",
                                 "([Ljava/lang/String;)"
                                 "Landroid/security/keystore/KeyGenParameterSpec$Builder;",
                                 modes.object());
        builder.callObjectMethod("setEncryptionPaddings",
                                 "([Ljava/lang/String;)"
                                 "Landroid/security/keystore/KeyGenParameterSpec$Builder;",
                                 paddings.object());
        builder.callObjectMethod("setKeySize",
                                 "(I)Landroid/security/keystore/KeyGenParameterSpec$Builder;",
                                 jint(256));
        const QJniObject spec = builder.callObjectMethod(
            "build", "()Landroid/security/keystore/KeyGenParameterSpec;");

        QJniObject generator = QJniObject::callStaticObjectMethod(
            "javax/crypto/KeyGenerator", "getInstance",
            "(Ljava/lang/String;Ljava/lang/String;)Ljavax/crypto/KeyGenerator;",
            Jni::string(QStringLiteral("AES")).object<jstring>(),
            Jni::string(QString::fromLatin1(kKeyStore)).object<jstring>());
        if (!generator.isValid() || !spec.isValid())
            return QJniObject();
        generator.callMethod<void>("init", "(Ljava/security/spec/AlgorithmParameterSpec;)V",
                                   spec.object());
        generator.callObjectMethod("generateKey", "()Ljavax/crypto/SecretKey;");
    }

    return store.callObjectMethod("getKey", "(Ljava/lang/String;[C)Ljava/security/Key;",
                                  alias.object<jstring>(), static_cast<jobject>(nullptr));
}

// Runs AES-GCM encryption (mode 1) or decryption (mode 2).
QByteArray AndroidCredentialStore::crypt(int mode, const QByteArray &input, QByteArray &iv)
{
    const QJniObject secretKey = key();
    if (!secretKey.isValid())
        return QByteArray();
    QJniObject cipher = QJniObject::callStaticObjectMethod(
        "javax/crypto/Cipher", "getInstance", "(Ljava/lang/String;)Ljavax/crypto/Cipher;",
        Jni::string(QStringLiteral("AES/GCM/NoPadding")).object<jstring>());
    if (!cipher.isValid())
        return QByteArray();

    if (mode == kEncryptMode) {
        cipher.callMethod<void>("init", "(ILjava/security/Key;)V", jint(kEncryptMode),
                                secretKey.object());
        iv = Jni::toByteArray(cipher.callObjectMethod("getIV", "()[B"));
    } else {
        const QJniObject ivArray = Jni::byteArray(iv);
        const QJniObject spec("javax/crypto/spec/GCMParameterSpec", "(I[B)V", kTagBits,
                              ivArray.object());
        cipher.callMethod<void>("init",
                                "(ILjava/security/Key;Ljava/security/spec/AlgorithmParameterSpec;)V",
                                jint(kDecryptMode), secretKey.object(), spec.object());
    }
    const QJniObject data = Jni::byteArray(input);
    return Jni::toByteArray(cipher.callObjectMethod("doFinal", "([B)[B", data.object()));
}

// Encrypts and stores the app password for a user.
bool AndroidCredentialStore::save(const QString &user, const QString &password)
{
    QByteArray iv;
    const QByteArray encrypted = crypt(kEncryptMode, password.toUtf8(), iv);
    if (encrypted.isEmpty() || iv.isEmpty())
        return false;
    QSettings settings;
    settings.setValue(QStringLiteral("credentials/user"), user);
    settings.setValue(QStringLiteral("credentials/iv"), QString::fromLatin1(iv.toBase64()));
    settings.setValue(QStringLiteral("credentials/data"),
                      QString::fromLatin1(encrypted.toBase64()));
    settings.sync();
    return settings.status() == QSettings::NoError;
}

// Decrypts the stored app password of a user.
bool AndroidCredentialStore::load(const QString &user, QString &password)
{
    QSettings settings;
    if (settings.value(QStringLiteral("credentials/user")).toString() != user)
        return false;
    QByteArray iv = QByteArray::fromBase64(
        settings.value(QStringLiteral("credentials/iv")).toString().toLatin1());
    const QByteArray encrypted = QByteArray::fromBase64(
        settings.value(QStringLiteral("credentials/data")).toString().toLatin1());
    if (iv.isEmpty() || encrypted.isEmpty())
        return false;
    const QByteArray plain = crypt(kDecryptMode, encrypted, iv);
    if (plain.isEmpty())
        return false;
    password = QString::fromUtf8(plain);
    return true;
}

// Deletes the stored ciphertext and the Keystore key.
void AndroidCredentialStore::clear()
{
    QSettings settings;
    settings.remove(QStringLiteral("credentials"));
    QJniObject store = openKeyStore();
    if (store.isValid())
        store.callMethod<void>("deleteEntry", "(Ljava/lang/String;)V",
                               Jni::string(QString::fromLatin1(kKeyAlias)).object<jstring>());
}

} // namespace PicSync
