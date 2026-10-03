# MS-DOS 2.0 を PC-98 / WebNP2 で起動する

`ports/pc98` は、このリポジトリに同梱された **Microsoft MS-DOS 2.00
カーネル**と Command 2.02 を、PC-98 上で起動する OEM BIOS / IPL です。
FreeDOS(98) のカーネルや起動セクタには依存しません。
`v4.0` の IBM PC 用イメージを PC-98 に流用するものではありません。

対象は ROM なしで起動できる [kumakumapon/webnp2](https://github.com/kumakumapon/webnp2)、
386 以上の CPU、77 シリンダ・2 ヘッド・8 セクタ・1024 バイト/セクタの
1232 KiB FAT12 フロッピーです。DOS 論理ドライブは A: のみです。
HDD、2 台目の FD、ほかのジオメトリ、実機での動作は対象外です。

## ビルド

Linux の Python 3、Git、GCC、GNU make が必要です。商用 MASM、DOSBox、
DOS のインストールは不要です。リポジトリのルートで実行します。

```sh
python3 ports/pc98/toolchain.py
python3 ports/pc98/build.py \
  --jwasm .work/pc98-tools/JWasm/GccUnixR/jwasm \
  --jwlink .work/pc98-tools/JWlink/build/jwlinkLR/jwlink
python3 ports/pc98/image.py
python3 -m unittest discover -s ports/pc98 -p 'test_*.py'
```

ツール取得先とコミットは `toolchain.py` に固定してあります。取得先の
`.work/pc98-tools` が存在する場合は上書きせず失敗します。
再取得する場合は `--output .work/pc98-tools-2` のように新しいパスを指定し、
ビルド時にもそのパスのツールを指定してください。

生成物は `ports/pc98/build/` に置かれます。

| ファイル | 内容 |
| --- | --- |
| `bios.exe`, `bios.map` | OEM BIOS と同梱 SYSINIT のリンク結果 |
| `ipl.bin`, `IO.SYS` | PC-98 用 IPL と固定ロード先に再配置した BIOS |
| `msdos2-pc98.xdf` | 起動可能な 1,261,568 バイトの FD イメージ |
| `manifest.json` | イメージと全格納ファイルのサイズ・SHA256 |

`image.py --add /path/to/APP.COM` で追加の DOS ファイルを格納できます。
名前は ASCII の 8.3 形式です。既存のシステムファイル名との重複、
容量超過、長い名前は拒否します。
`AUTOEXEC.BAT` を標準で格納するため、起動時の日付・時刻入力は不要です。

## WebNP2 で起動する

WebNP2 の FD1 選択欄で `msdos2-pc98.xdf` を選択して起動してください。
画面に `MS-DOS version 2.00` と `A>` が表示されます。
`VER`、`DIR`、`ECHO`、`TYPE`、`COPY` を使用できます。
WebNP2 のファイルマネージャで FD 内容を確認・保存できます。

ローカルで自動検証する場合は、別シェルで WebNP2 を起動します。

```sh
# WebNP2 リポジトリ
npm ci
cp /path/to/MS-DOS/ports/pc98/build/msdos2-pc98.xdf public/test/msdos2-pc98.xdf
npm run dev -- --host 127.0.0.1 --port 5173 --strictPort
```

```sh
# MS-DOS リポジトリ
CHROMIUM=/usr/bin/chromium node ports/pc98/verify-webnp2.mjs \
  /path/to/webnp2 msdos2-pc98.xdf
```

この検証は Chromium の実ブラウザ内でエミュレータを起動し、Microsoft の
カーネル・シェルの表示、DOS コマンドの実行、DOS が書いたファイルを確認します。
FD をエクスポートし、`PROBE.TXT` とコピー先の内容、FAT の両コピーを照合します。
出力は `build/validation/` の JSON・画面・書き込み済みディスクです。
標準の Chromium サンドボックスを使用します。隔離された CI / コンテナで
利用できない場合に限り `WEBNP2_NO_SANDBOX=1` を指定できます。
新しい検証イメージは新しいファイル名で公開してください。

GitHub Actions の **PC-98 MS-DOS 2.0** ワークフローでもビルドと実際の起動を
検証し、生成ディスクと記録を `msdos2-pc98` アーティファクトへ保存します。

## 移植の構成と互換範囲

- IPL は FAT データ領域の先頭から連続配置した IO.SYS と MSDOS.SYS を読みます。
  移植ツールが作ったディスクを前提とします。DOS の SYS / FORMAT コマンドで
  作り直したディスクの起動には対応しません。
- IO.SYS は DOS のデバイス要求パケットを実装し、NEC BIOS の INT 18h による
  キー入力・カーソル制御、INT 1Bh による FD 読み書き、INT 1Ch による RTC を使用します。
  テキスト VRAM は PC-98 の A000:0000 / 属性 A000:2000 を使用します。
- CON は通常のテキスト出力、スクロール、INT 29h、ANSI CSI の H/f・2J・
  30〜37m / 0m に対応します。完全な ANSI.SYS ではありません。
- CLOCK$ の分解能は 1 秒です。RTC の 2 桁年を 1980〜2079 年へ変換します。
  AUX / PRN の入出力は未対応で、未対応要求をエラーにします。
- カーネル、COMMAND.COM、SYSINIT.OBJ、SYSIMES.OBJ は元の同梱バイナリを使用します。
  カーネルを公開ソースから再アセンブルする手順ではありません。
  各バイナリの出典は `v2.0/bin`、API の資料は `v2.0/source/SYSINIT.txt` と
  `DEVDRIV.txt` です。既存の歴史的ソースは変更しません。
- 同梱 OBJ の MODEND 後のゼロ埋めを検証して除去し、LIDATA を LEDATA に展開して
  FIXUPP の位置も変換します。JWlink の LIDATA 参照処理では SYSINIT の EXEC
  パラメータが壊れるためです。元の OBJ は変更しません。チェックサムと
  最終 EXEC パラメータの参照をテストしています。

IBM PC の INT 10h / 13h / 16h や B800:0000 のテキスト画面を直接使用する
DOS ソフトは、この移植でも PC-98 では動きません。
DOS 2.0 自体の機能・ファイルシステムの制限も残ります。
[RetroBasic のネイティブ移植](https://github.com/kumakumapon/RetroBasic/pull/5) は
DOS API と PC-98 VRAM を使用して動作します。

ライセンスはリポジトリの MIT License に従います。
