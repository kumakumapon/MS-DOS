# プログラム分類と整理方針（CUI / GUI の棚卸し）

リポジトリには、CUI（文字画面）と GUI（グラフィカルな画面）、16 ビット DOS 用と
32 ビット保護モード用、製品とテスト用のプログラムが混在しています。この文書は、
それらを「ユーザーに見える種類」で分類し、整理の方針を示します。

> **方針:** KGUI32 を将来の標準デスクトップにします。KDESK 系は、移行が終わるまで
> 現行の標準として維持し、その後は互換用に回します（第 4 節）。
>
> この文書は整理の**提案**です。現時点ではコードやディレクトリの移動は行っていません。
> 「現状」の節は Makefile とビルド成果物の名前から作成しました。個別の画面を
> すべて起動して確認したわけではありません（確認済みは KDESK、KGUI32、FreeDOS の起動画面）。
> ディレクトリ構成の既存の説明は [tools/README.md](../tools/README.md) を参照してください。

## 1. 現状の問題

1. **役割が重複している。**
   - ファイラ: `FD`（tools/filer）、KDESK の Files、KGUI32 の `KGFILES`
   - エディタ: `EDIT`（tools/editor）、KDESK の Memo、KGUI32 の `KGEDIT`
   - 画像: `KVIEW` / `KPAINT`（tools/window）、KGUI32 の `KGIMAGE`
   - 電卓・時計: KDESK 内蔵、KGUI32 の `KGCALC` / `KGCLOCK`
2. **実装の層が 3 つ並んでいる。** 16 ビット DOS（KDESK 系）、V86 実行基盤（K386）、
   32 ビット保護モード（K38632 / KGUI32）。それぞれにシェルとサンプルがあります。
3. **製品とテスト用が同じ場所にある。** `tools/kgui32/build` には `KGBAD` / `KGSAVENO`
   などの異常系の検証用 EXE が、本物のアプリと並んで出力されます。K386 / K38632 の
   サンプルも、シェルと同じ階層です。
4. **起動の入口が多い。** webnp2 の起動ボタンに、FreeDOS、MS-DOS 2.0 / 4.0、K386、
   KGUI32（通常・拡張メモリ・ハイカラー・FD 版）が並び、違いが分かりにくくなっています。

## 2. ユーザーに見せる区分

KGUI32 を将来の標準にするため、区分には「いまの位置づけ」と「移行後の位置づけ」を書きます。

| 区分 | 内容 | いま | 移行後 |
| --- | --- | --- | --- |
| KGUI32（標準へ移行中） | KGUI32 とそのアプリ | 実験中。別枠で案内する | 標準 |
| 現行の標準（KDESK） | MS-DOS 4.0 + KDESK、`FD`、`EDIT`、`CPANEL` | 既定 | 互換用。DOS 画面だけの環境や、16 ビット環境向け |
| 開発・検証 | K386 / K38632 のサンプル、異常系、プローブ | 開発者向け | 開発者向け |

KGUI32 を標準にするには、386 以上の PC-98、XMS、16MB 以上の拡張メモリが必要です
（[tools/kgui32/README.md](../tools/kgui32/README.md)）。この条件を満たさない環境では
KDESK を使い続けるので、KDESK 系は削除せず互換用として残します。

## 3. プログラム一覧

### 3-1. 現行の標準（16 ビット DOS）。移行後は互換用

| 名前 | 種類 | 場所 | 備考 |
| --- | --- | --- | --- |
| `KDESK.EXE` | GUI | tools/window | Launcher、Files、Memo、Tasks、Find apps（Ctrl+K） |
| `KVIEW.EXE` / `KPAINT.EXE` | GUI | tools/window | 画像ビューア / 画像エディタ |
| `CPANEL.EXE` | GUI | tools/control | コントロールパネル。KDESK 内のウィンドウとしても使う |
| `FD.COM` / `FD98.EXE` | CUI | tools/filer | 2 ペインのファイラ |
| `EDIT.COM` / `EDIT98.EXE` / `EDIT.EXE` | CUI | tools/editor | 日本語対応のテキストエディタ |
| `KDISP.COM` | CUI | tools/display | Cirrus 表示の確認用プローブ |

### 3-2. KGUI32（32 ビット保護モード）。標準へ移行中

| 名前 | 種類 | 備考 |
| --- | --- | --- |
| `KGUI32.EXE` / `KGUILDR.EXE` | GUI | デスクトップ本体とローダ |
| `KGCALC` / `KGCLOCK` / `KGCAL` | GUI | 電卓 / 時計 / カレンダー |
| `KGFILES` / `KGEDIT` / `KGIMAGE` / `KGWORDS` | GUI | ファイル / エディタ / 画像 / ユーザー辞書 |
| `KSH32.EXE` / `KTOOL32.EXE` | CUI | 32 ビットシェルとツール。KGUI32 のウィンドウからも開く |

### 3-3. 実行基盤（画面を持たない）

`K386.EXE`、`K32CPU.COM`、`K32IO.EXE`、`K32PROC.EXE`、`KXMS32.SYS`、`KXHIGH.SYS`、
`KV98VID.EXE`、`EMM386.EXE`。

### 3-4. 開発・検証用

| 種類 | 例 | 場所 |
| --- | --- | --- |
| K386 のサンプル | `COUNTER` `ECHO` `FAULT` `KFILE` `PORT` `DF` `FPU` `FLAGS` `DTA` `KEYCHK` `TIMER` `MEMORY` `PROCESS` `BADTIMER` `ROMWRITE` `INTWAIT` `CASE` `KHELLO` `MZTEST` `XARRAY` `XMEM` `XCHILD` | tools/k386 |
| K38632 のサンプル | `HELLO32` `ARGS32` `COUNT32` `FILE32` `HEAP32` `INPUT32` `MEM32` `PRACT32` `SPIN32` `VIEW32` `XARRAY32` `PROCTEST` | tools/k38632 |
| 異常系の検証 | `FAULT32` `BADPTR32` `BADABI` `BADELF` `BADENT` `BADOVR` `BADSIZE`、`KGBAD` `KGBITBAD` `KGDIRBAD` `KGDIRNO` `KGRECNEG` `KGRECNON` `KGSAVEB` `KGSAVENO` `KGSAVE64` | tools/k38632、tools/kgui32 |
| プローブ | `KGPROOF.COM`、`HDDTEST.EXE` | tools/kgui32、ports/pc98/hdd |
| シェル | `KSH.EXE`（K386）、`VDOS.EXE` | tools/k386 |

## 4. 整理方針（提案）

### 4-1. 機能の正本と、移行に必要な機能の差
KGUI32 を正本にします。ただし、KGUI32 に同等の機能がない部分は、足りるまで KDESK 系を
正本として残します。

| 機能 | 正本（移行後） | KGUI32 の現状 | 移行の条件 |
| --- | --- | --- | --- |
| ファイラ | `KGFILES` | あり（読込専用の A: 表示、作業用ディレクトリ） | FD の書込み、新規フォルダ、削除、名前変更を `FD` と同等にする |
| エディタ | `KGEDIT` | あり（日本語 IME、Shift-JIS、復旧、Undo / Redo） | `EDIT` の機能との差を洗い出す |
| 画像 | `KGIMAGE` | あり（読込、保存、反転、描画モード、復旧） | `KPAINT` の描画機能との差を洗い出す |
| 電卓・時計 | `KGCALC` / `KGCLOCK` | あり | なし |
| カレンダー | `KGCAL` | あり | なし |
| ユーザー辞書 | `KGWORDS` | あり | なし |
| システム設定 | （なし） | **未対応** | `CPANEL` の XMS/EMS、FILES/BUFFERS、FD98 自動起動の設定を移す |
| Memo / Tasks | （なし） | **未対応** | KGEDIT で代替するか、タスク一覧を System ウィンドウへ移すかを決める |
| DOS コマンド | `KSH32`、DOS 端末 | あり | なし |

KGUI32 の側の `System` ウィンドウは、現在は検証用の表示（CPU workers など）が中心です。
設定アプリへ育てる場合は、開発者向けの表示を分けます（kumakumapon/MS-DOS#71 の
D 項目を参照）。

### 4-2. ディレクトリ構成
`tools/` を、役割別の 3 つに分けます。

- `apps/`: ユーザー向け（filer、editor、control、window、kgui32）
- `runtime/`: 実行基盤（k386、k38632、k32io、kproc、kxms32、kvideo、mz、display）
- `dev/`: サンプル、異常系、プローブ

移動に伴い、Makefile、`ports/pc98/dos4.py`、CI のワークフロー、webnp2 の取り込み
スクリプト（`scripts/update-*.py`）のパスをまとめて更新する必要があります。webnp2 は
MS-DOS の固定コミットを参照するため、2 つのリポジトリで同時に更新する手順が必要です
（[ci-pairing.md](ci-pairing.md) を参照）。

### 4-3. 配布物から検証用を分ける
- 異常系の EXE は、検証用のディスクイメージにだけ入れ、配布する FD / HDD に含めない。
- `build/` の出力先を `build/apps` と `build/fixtures` に分ける。

### 4-4. 起動の入口
webnp2 の起動画面は、次の順に整理します。
1. **KGUI32**（標準へ移行中）。通常・拡張メモリ・ハイカラーは、メニューの中の選択肢にする。
2. MS-DOS 4.0 + KDESK（互換用）
3. FreeDOS(98)、MS-DOS 2.0、ディスクなし
4. K386 などの開発者向け（折りたたみ、または「開発者向け」メニュー）

KGUI32 を先頭にするのは、4-1 の「移行の条件」のうち、少なくともファイラの書込みと
システム設定が揃ってからにします。それまでは、現行の標準を先頭に置き、KGUI32 を
「新しいデスクトップ（実験）」として 2 番目に並べます。

### 4-5. ドキュメント
- 各 README の冒頭に、区分（KGUI32 / 現行の標準 / 基盤 / 開発）と、正本かどうかを書く。
- この文書と `tools/README.md` の表を、同じ分類で揃える。

## 5. 段階的な進め方
1. **この文書の合意**（KGUI32 を標準にする方針と、移行の条件）。
2. **見せ方だけの変更**: README と webnp2 の起動ボタンを、第 2 節の区分に合わせる。
3. **機能の差を埋める**（4-1 の「移行の条件」）。ファイラの書込み、システム設定、
   Memo / Tasks の扱いを決めて実装する。
4. **標準の切り替え**: 3 が終わったら、起動画面の先頭と既定の起動先を KGUI32 に変える。
   KDESK 系は「互換用」と明記して残す。
5. **出力先の分離**: `build/` を apps / fixtures に分け、配布物から検証用を除く。
6. **ディレクトリの移動**: `tools/` を `apps/` `runtime/` `dev/` に分ける。MS-DOS と webnp2 で
   同時に更新する。

手順 2 と 5 までは、リポジトリの構成を変えずに進められます。手順 6 は影響範囲が大きいので、
先に CI のパス参照を洗い出してから行ってください。
