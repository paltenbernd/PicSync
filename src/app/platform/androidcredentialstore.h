// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Peter Altenbernd <peter.altenbernd@h-da.de>
// This file is part of PicSync.

#pragma once

#include "interfaces.h"

#include <QByteArray>
#include <QJniObject>

namespace PicSync {

/// CredentialStore using the Android Keystore.
///
/// An AES-256 key is generated inside the Android Keystore (it never leaves
/// the secure hardware); the app password is encrypted with AES-GCM and the
/// ciphertext is kept in the app's private settings file.
class AndroidCredentialStore : public CredentialStore
{
public:
    // Encrypts and stores the app password for a user.
    bool save(const QString &user, const QString &password) override;

    // Decrypts the stored app password of a user.
    bool load(const QString &user, QString &password) override;

    // Deletes the stored ciphertext and the Keystore key.
    void clear() override;

private:
    // Returns the Keystore key, generating it on first use.
    static QJniObject key();

    // Runs AES-GCM encryption (mode 1) or decryption (mode 2).
    static QByteArray crypt(int mode, const QByteArray &input, QByteArray &iv);
};

} // namespace PicSync
