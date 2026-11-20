# プログラム分類と整理方針（CUI / GUI の棚卸し）

リポジトリには、CUI（文字画面）と GUI（グラフィカルな画面）、16 ビット DOS 用と
32 ビット保護モード用、製品とテスト用のプログラムが混在しています。この文書は、
それらを「ユーザーに見える種類」で分類し、整理の方針を示します。

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

## 2. ユーザーに見せる 3 つの区分

| 区分 | 内容 | 案内の仕方 |
| --- | --- | --- |
| 標準環境 | MS-DOS 4.0 + KDESK、`FD`、`EDIT`、`CPANEL` | 既定。通常はこれだけを案内する |
| 次世代デスクトップ（実験） | KGUI32 とそのアプリ | 「実験中」と明記して別枠にする |
| 開発・検証 | K386 / K38632 のサンプル、異常系、プローブ | 開発者向けにまとめ、通常は見せない |

## 3. プログラム一覧

### 3-1. 標準環境（16 ビット DOS）

| 名前 | 種類 | 場所 | 備考 |
| --- | --- | --- | --- |
| `KDESK.EXE` | GUI | tools/window | Launcher、Files、Memo、Tasks、Find apps（Ctrl+K） |
| `KVIEW.EXE` / `KPAINT.EXE` | GUI | tools/window | 画像ビューア / 画像エディタ |
| `CPANEL.EXE` | GUI | tools/control | コントロールパネル。KDESK 内のウィンドウとしても使う |
| `FD.COM` / `FD98.EXE` | CUI | tools/filer | 2 ペインのファイラ |
| `EDIT.COM` / `EDIT98.EXE` / `EDIT.EXE` | CUI | tools/editor | 日本語対応のテキストエディタ |
| `KDISP.COM` | CUI | tools/display | Cirrus 表示の確認用プローブ |

### 3-2. 次世代デスクトップ（32 ビット保護モード、実験）

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

### 4-1. 機能の正本を決める
同じ役割のものは、正本を 1 つに決め、ほかは「互換」「軽量版」「移行中」の位置づけにします。

| 機能 | 正本（案） | ほか |
| --- | --- | --- |
| ファイラ | KDESK の Files | `FD`: DOS 画面だけで使う軽量版 / `KGFILES`: KGUI32 側の実装 |
| エディタ | `EDIT`（日本語対応済み） | KDESK の Memo: 簡易メモ / `KGEDIT` |
| 画像 | `KVIEW` / `KPAINT` | `KGIMAGE`: KGUI32 側の実装 |

KGUI32 を将来の標準にする場合は、この表を逆にして移行手順を決めます。どちらを
標準にするかは、方針の判断が必要です。

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
1. MS-DOS 4.0 + KDESK（標準）
2. KGUI32（実験。通常・拡張メモリ・ハイカラーはメニューの中の選択肢にする）
3. FreeDOS(98)、MS-DOS 2.0、ディスクなし
4. K386 などの開発者向け（折りたたみ、または「開発者向け」メニュー）

### 4-5. ドキュメント
- 各 README の冒頭に、区分（標準 / 実験 / 基盤 / 開発）と正本の有無を書く。
- この文書と `tools/README.md` の表を、同じ分類で揃える。

## 5. 段階的な進め方
1. **この文書の合意**（区分と正本の決定）。
2. **見せ方だけの変更**: README と webnp2 の起動ボタンを新しい区分に合わせる。
3. **出力先の分離**: `build/` を apps / fixtures に分け、配布物から検証用を除く。
4. **ディレクトリの移動**: `tools/` を `apps/` `runtime/` `dev/` に分ける。MS-DOS と webnp2 で
   同時に更新する。

手順 2 と 3 までは、リポジトリの構成を変えずに進められます。手順 4 は影響範囲が大きいので、
先に CI のパス参照を洗い出してから行ってください。
