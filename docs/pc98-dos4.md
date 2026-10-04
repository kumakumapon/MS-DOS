# MS-DOS 4.0 の PC-98 / WebNP2 移植

`v4.0` のソースからビルドした **MSDOS.SYS / COMMAND.COM 4.00** を、
PC-98 用 IO.SYS / IPL と組み合わせて WebNP2 上で起動します。
DOS 2.0 の移植も `ports/pc98/build.py` で引き続きビルドできます。

## Linux でのビルド

Python 3、Git、make、GCC、DOSBox 0.74-3 が必要です。
DOSBox は同梱の MASM / Microsoft C / LINK を動かすために使用し、
完成した OS の検証は WebNP2 の NEC BIOS / x86 CPU で行います。
Debian / Ubuntu の DOSBox パッケージを使用できます。

```sh
python3 ports/pc98/toolchain.py
SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy \
  python3 ports/pc98/dos4.py --dosbox /usr/bin/dosbox \
  --jwasm .work/pc98-tools/JWasm/GccUnixR/jwasm
```

新しい `.work/pc98-dos4` に固定基点
`2d04cacc5322951f187bb17e017c12920ac8ebe2` を展開し、既存の
[修復パッチと出典](../tools/dos4/PROVENANCE.md)に従ってルート NMAKE を実行します。
全62出力と成功マーカーを確認後、`P98INIT` 作業コピーで PC-98 用 SYSINIT を
組み立て、元の Microsoft LINK で OEM BIOS とリンクします。
チェックアウト内の歴史的ソースを直接編集しません。

生成物は `ports/pc98/build/dos4` に入ります。
`msdos4-pc98.xdf` は 1,261,568 バイトの FAT12 起動 FD です。
WebNP2 の FD1 に読み込み、386 設定で起動してください。
`--add /path/APP.COM` で 8.3 名のアプリを追加できます。

起動FDには二画面ファイラー `FD98.COM`、操作説明 `FILER.TXT`、
ライセンス `FILERLIC.TXT` も同梱します。`A>` で `FD98` を実行してください。
[操作・IBM PC版・ビルドと検証](filer.md)を参照してください。
`EDIT.COM` / `EDIT98.COM`、`EDIT.TXT`、`EDITLIC.TXT`、日本語サンプル
`JPHELLO.TXT`も同梱します。`EDIT MEMO.TXT`で編集を開始します。
[日本語表示・入力・エディタの操作](japanese-editor.md)を参照してください。

同じコマンドを再実行すると、成功済みのカーネルビルドを再利用し、OEM 層と FD を
作り直します。カーネルもクリーンに再ビルドする場合は `--work .work/dos4-repro`
のような新しいパスを指定してください。中断された作業は checkpoint に基づき
再ビルドします。同じ作業パスのビルドを同時に実行しないでください。
`toolchain.py` は既存ディレクトリを拒否するため初回だけ実行します。

## PC-98 への変更

- IPL は NEC INT 1Bh で連続配置した IO.SYS / MSDOS.SYS を読み込みます。
- CON は INT 18h と A000h のテキスト VRAM、CLOCK$ は INT 1Ch の RTC を使用します。
- ディスクは 77シリンダー、2ヘッド、8セクター、1024バイト/セクターの FD です。
  セクター境界に置いた転送バッファを使い、DMA の64 KiB境界をまたぎません。
- DOS 4.0 の初期化は起動ドライブの番号が1始まりです。BPBもDOS 4.0の形式へ拡張します。
- SYSINIT1 / SYSCONF / SYSINIT2 の IBM 用割り込みスタックを無効にします。
  `STACKS=` は CONFIG.SYS エラーとして拒否します。
  IBM INT 11h / 15h の機器・拡張メモリ照会と割り込み再設定用ポート出力を削除します。
  NEC BIOS の割り込みベクターを IBM の処理で上書きしません。
- DOS カーネルと COMMAND.COM は PC-98 用のバイナリ修正を加えません。
  以前の Windows / DOSBox-X ビルドで記録した SHA256 と一致することも検査します。

`CONFIG.SYS` は `FILES=20`, `BUFFERS=8`, `LASTDRIVE=A` です。
`P98TEST.COM` は INT 21h のバージョン API が4.00を返すことを確認して終了する検証用アプリです。
ライセンスは FD の `DOSLIC.TXT` とビルド出力の `LICENSE.TXT` に格納します。

## 検証

```sh
python3 -m unittest discover -s ports/pc98 -p 'test_*.py'
python3 -m unittest discover -s tools/dos4 -p 'test_*.py'
mkdir -p /path/to/webnp2/public/test
cp ports/pc98/build/dos4/msdos4-pc98.xdf /path/to/webnp2/public/test/msdos4-pc98.xdf
# webnp2 で npm ci と npm run dev -- --host 127.0.0.1 --port 5173 --strictPort を実行
DOS_VERSION=4 CHROMIUM=/usr/bin/chromium \
  node ports/pc98/verify-webnp2.mjs /path/to/webnp2 msdos4-pc98.xdf \
  ports/pc98/build/dos4/validation
```

WebNP2 の固定コミット `4c21552727cfe4019b4dc0f68f0dca124026f650` を対象に、
VER、ECHO のリダイレクト、TYPE、COPY、DATE、COM の EXEC と終了を検証します。
ゲストが書いた FD をエクスポートし、ファイルの実データと2個の FAT を照合します。
[検証記録](validation/pc98-dos4/verification.json)と
[画面](validation/pc98-dos4/screen.png)を同梱しています。
GitHub Actions の **PC-98 MS-DOS 4.0** でもビルド・ブラウザ検証を実施し、
COM、XDF、ビルドログ、ソースの変更前後のハッシュ、検証結果をアーティファクトに保存します。

## 対応範囲

対象は単一の1.23 MB FAT12 FD、640 KiBの通常メモリ、80×25のテキストです。
CONとEDIT98はShift_JIS日本語本文を表示します。日本語変換はWebNP2のホストIMEを使い、
DOS単体のFEPと日本語ファイル名は対象外です。PC-98 の AUX / PRN、HDD / FAT16、拡張メモリ、全付属ツールの
動作確認は含めません。特に FORMAT / SYS / FDISK / MODE / ANSI.SYS / EMM386 などの
IBM PC 用プログラムを PC-98 用として扱わないでください。FDにはそれらを格納しません。
COMMAND.COM の通常操作と DOS API を使うアプリを対象にしています。
