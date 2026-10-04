# DOS 4.0 用 FD Filer

`tools/filer` にキーボードで操作する二画面のファイラーを実装しています。
FD-clone のような操作を持つ独自実装で、ソースを移植したものではありません。
ファイラーは [MIT License](../tools/filer/LICENSE) です。

## ビルド

Linux の GCC / GNU binutils / make / Python 3、386以上のCPUが必要です。

```sh
make -C tools/filer all
make -C tools/filer test
```

生成物は `tools/filer/build/FD.COM`（IBM PC）、`FD98.COM`（PC-98）、
`FILER.TXT`（CRLFの説明）と `FILERLIC.TXT`（ライセンス）です。
共通の C 実行系を実際のファイル操作と AddressSanitizer / UBSan で検証します。
COM は freestanding の16bitリアルモードで動き、386の命令を使用します。
リンク時にスタック12 KiB以上を確保し、起動時に自分のメモリを64 KiBへ縮小して
アプリの EXEC に必要な空きメモリを返します。

## 起動と操作

DOS のプロンプトから IBM PC では `FD`、WebNP2 / PC-98 では `FD98` を実行します。
PC-98 版は A000h のテキストVRAMと NEC INT 18h、IBM PC 版は B800h / INT 10h / INT 16h を使います。
ファイル操作は両方とも DOS INT 21h です。ANSI.SYS は不要です。

| キー | 動作 |
| --- | --- |
| ↑↓ / J・K、PgUp・PgDn、Home | 選択・ページ移動 |
| Tab / ←→ | 左右のパネル切り替え |
| Enter / Backspace | ディレクトリを開く / 親へ移動 |
| F1 / ? | ヘルプ |
| F2 / R | 名前変更 |
| F3 / V | ファイル閲覧 |
| F4 | 再読み込み |
| F5 / C、F6 / M | コピー、移動（既定の保存先は反対側パネル） |
| F7 / N | ディレクトリ作成 |
| F8 / D | ファイル・空ディレクトリを削除（Yで確認） |
| F9 / X | COM/EXEの起動。BAS は起動ドライブの `RBASIC.COM` で実行 |
| E | 選択したファイルを起動ドライブの `EDIT98.COM` で編集（PC-98版） |
| F10 / Q / Esc | DOSへ終了 |

入力欄は Enter で確定、Esc で取消、Ctrl-U でクリア、←→で編集します。
既存の保存先や同じファイルへのコピー・移動は拒否し、上書きしません。
コピーは新規作成 API を使い、短い書き込み・読込失敗・close失敗・取消時には
未完成の保存先を除去します。元ファイルは維持します。
別ドライブへのファイル移動はコピー成功後に元を削除し、削除失敗時はコピーを戻します。
後片付けも失敗した場合は、両方のファイルを確認するよう画面に表示します。

ディレクトリ優先・名前順、属性・サイズ・日付を表示します。
各パネルは256件（サブディレクトリでは親を含む）までで、上限は画面に表示します。
閲覧はASCIIと、PC-98版ではShift_JISの日本語本文・半角カナに対応します。
不正な文字バイトは `.` で表示します。
Space / PgDn で次ページ、PgUp で直近127ページを戻り、Homeで先頭、Escで戻ります。
再帰コピー・別ドライブへのディレクトリ移動・一括選択・アーカイブ・長い名前・
日本語ファイル名には対応しません。[EDIT98](japanese-editor.md)は本文を編集します。

## 起動イメージと検証

[DOS 4.0 PC-98 のビルド](pc98-dos4.md)はファイラーもビルドし、
起動FDに `FD98.COM`、`FILER.TXT`、`FILERLIC.TXT` を含めます。
WebNP2 は既存の DOS 4.0 + BASIC 起動FDを更新して同じファイルを同梱します。
PC-98 OEM の A: FAT12 の制限は変わりません。

```sh
# WebNP2 の開発サーバーを起動してから:
cp ports/pc98/build/dos4/msdos4-pc98.xdf /path/to/webnp2/public/test/dos4-filer.xdf
CHROMIUM=/usr/bin/chromium node tools/filer/verify-webnp2.mjs /path/to/webnp2
# クリーンな DOS 4.0 カーネルビルドと QEMU がある場合:
python3 tools/filer/verify_ibm.py --work .work/pc98-dos4
```

ゲストの通常キー操作で閲覧、コピーと上書き拒否、名前変更、移動、ディレクトリ作成・
移動・削除・取消、子COM実行とファイラーへの復帰、DOSへの終了を確認します。
書込後のFAT12からデータを読み戻します。CIはIBM PC版をQEMU、
PC-98版をWebNP2で実行し、COM・画面・書込後ディスク・検証JSONを保存します。
