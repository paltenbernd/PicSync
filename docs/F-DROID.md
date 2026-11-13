# PicSync in F-Droid (und damit in der /e/OS App Lounge) veröffentlichen

Die App Lounge von /e/OS zeigt unter „Open Source“ den Katalog des offiziellen
F-Droid-Repositorys. PicSync muss also in F-Droid aufgenommen werden.

## Was im Repository schon vorbereitet ist

| Was | Wo |
|---|---|
| Lizenz GPL-3.0-or-later | `LICENSE` |
| Store-Texte, Icon, Screenshots (de/en), Changelog | `fastlane/metadata/android/` |
| Versionsname und versionCode im Quellcode | `common.pri` (`PICSYNC_VERSION`, `PICSYNC_VERSION_CODE`) |
| Qt 6.8.3 und OpenSSL 3.0.22 als Quellcode (Submodule) | `3rdparty/` |
| Build komplett aus dem Quellcode | `tools/build-android-from-source.sh` |
| Prüfung dieses Builds bei jedem Push | `.github/workflows/fdroid-check.yml` |
| Entwurf der F-Droid-Metadaten | `tools/fdroid/de.altenbernd.picsync.yml` |

## Schritte

1. **Repository öffentlich machen**: GitHub › PicSync › Settings › General ›
   Danger Zone › *Change visibility* › Public.
2. **Release-Tag setzen** (falls noch nicht geschehen): `v1.0.0` auf dem Commit,
   der in der Metadaten-Datei unter `commit:` steht.
3. **GitLab-Konto** anlegen (gitlab.com), falls noch keines vorhanden ist.
4. **fdroiddata forken**: https://gitlab.com/fdroid/fdroiddata › *Fork*.
5. Im Fork einen Branch `de.altenbernd.picsync` anlegen und die Datei
   `tools/fdroid/de.altenbernd.picsync.yml` als
   `metadata/de.altenbernd.picsync.yml` hinzufügen (geht auch im Web-Editor:
   *+ › New file*).
6. **Merge Request** gegen `fdroid/fdroiddata` (Branch `master`) stellen.
   Die Vorlage „App inclusion“ auswählen und die Checkliste ausfüllen.
   Hilfreich für die Prüfer:
   - Qt und OpenSSL werden aus den Submodulen gebaut, keine vorgefertigten
     Bibliotheken; Gradle-Abhängigkeiten kommen nur aus Google Maven / Maven Central.
   - Die Berechtigung `MANAGE_EXTERNAL_STORAGE` ist nötig, weil die App Fotos der
     Galerie ohne Einzelbestätigung lesen, schreiben und löschen muss.
7. Die automatische Pipeline im Merge Request baut die App. Fragen der Prüfer
   beantworten; Änderungswünsche am Build-Skript kann Claude umsetzen.
8. Nach dem Merge erscheint PicSync innerhalb einiger Tage in F-Droid und
   danach in der App Lounge.

## Neue Versionen

1. `PICSYNC_VERSION` und `PICSYNC_VERSION_CODE` in `common.pri` erhöhen.
2. Changelog anlegen: `fastlane/metadata/android/<sprache>/changelogs/<versionCode>.txt`.
3. Commit und Tag `v<Version>` pushen. F-Droid erkennt den Tag automatisch
   (`UpdateCheckMode: Tags`) und baut die neue Version.

## Hinweise

- F-Droid signiert die App mit einem eigenen Schlüssel. Wer PicSync vorher von
  GitHub installiert hat, muss einmal deinstallieren und neu installieren.
- F-Droid erwartet, dass die App weiter gepflegt wird.
