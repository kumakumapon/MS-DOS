# MS-DOS PC-98・32bitアプリ環境

[English](README.md) · [文書一覧](docs/README.md) · [ツール構成](tools/README.md)

このforkはMS-DOS 2.0/4.0のPC-98 OEM移植、再現可能な起動媒体、KDESK、
保護モードのKGUI32デスクトップと32bitアプリを開発しています。
[webnp2](https://github.com/kumakumapon/webnp2)と組み合わせてブラウザ配布・E2Eを検証します。
参照SHAの更新は[CIの組み合わせ更新手順](docs/ci-pairing.md)を参照してください。

本forkの移植・ツール・アプリ・連携へのIssueとPRを受け付けます。
上流の歴史的ソースは保存し、改良は`ports/`、`tools/`、連携・文書に実装します。
上流READMEの「PRを送らないで」という記述は歴史的ソースに対する上流の方針です。

- [MS-DOS 4.0ビルド](docs/build-msdos4.md) / [検証](docs/validation-msdos4.md)
- [PC-98 DOS 4.0](docs/pc98-dos4.md) / [PC-98 DOS 2.0](docs/pc98-webnp2.md)
- [KDESK操作](docs/window-desktop.md)、[Files](docs/kdesk-explorer.md)、[画像](docs/kdesk-media.md)、[ユーティリティ](docs/kdesk-utilities.md)
- [FD98](docs/filer.md)、[EDIT98の日本語](docs/japanese-editor.md)
- [KGUI32](tools/kgui32/README.md)、[32bit SDK](tools/k38632/README.md)

ライセンスは[MIT](LICENSE)です。歴史・商標・上流の原文は[英語README](README.md#historical-upstream-readme)に保存しています。
