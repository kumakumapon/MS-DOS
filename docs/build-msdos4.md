# WindowsでMS-DOS 4.00をビルドし、WSLのQEMUで起動する

同梱のDOS用ツールをWindows版DOSBox-Xで実行し、1.44 MB FAT12
フロッピーを生成します。起動試験はWSL UbuntuのQEMUで行います。
有料の開発製品、Ruby、FreeDOS、別途入手したDOS起動ディスクは不要です。

## 必要なもの

| ソフトウェア | 検証バージョン | 入手先・用途 |
| --- | --- | --- |
| Git for Windows | 2.52.0.windows.1 | [公式](https://git-scm.com/downloads/win)：固定コミットの展開・パッチ適用 |
| PowerShell | 7.6.6 | [公式](https://learn.microsoft.com/powershell/scripting/install/installing-powershell-on-windows)：一括実行 |
| Python | Windows 3.12.12 / Ubuntu 3.12 | [公式](https://www.python.org/downloads/windows/)：標準ライブラリのみ使用 |
| DOSBox-X | 2026.08.31、VS Windows x64 portable | [公式リリース](https://github.com/joncampbell123/dosbox-x/releases/tag/dosbox-x-v2026.08.31)：16ビットビルドツール実行 |
| WSL Ubuntu | Ubuntu-24.04 | [WSL](https://learn.microsoft.com/windows/wsl/install)：QEMUの実行環境 |
| QEMU | 8.2.2、Ubuntu `1:8.2.2+ds-0ubuntu1.18` | [公式入手方法](https://www.qemu.org/download/)：`qemu-system-x86`パッケージ |

既存のUbuntu環境を利用できます。QEMU未導入の場合はUbuntu側で
`sudo apt update && sudo apt install qemu-system-x86` を実行します。
Windows側の `python --version` と、次のコマンドを確認してください。

```powershell
wsl -l -v
wsl -d Ubuntu-24.04 --exec qemu-system-i386 --version
```

## 一括実行

リポジトリのルートでPowerShellから実行します。
初回は約44 MBのDOSBox-X ZIPをダウンロードし、固定SHA256を検証して
`.work/dosbox` に展開します。システムへのインストールは行いません。

```powershell
./tools/dos4/run.ps1 -Work .work/build -Distro Ubuntu-24.04
```

PowerShellの実行ポリシーでスクリプトがブロックされる環境では、組織の
ポリシーに従って実行してください。既存のDOSBox-Xを指定する場合は
`-DosBox 'C:/path/to/dosbox-x.exe'` を追加できます。
作業パスにはASCII文字を使い、カンマや引用符を含めないでください。

処理は以下の順序です。

1. 固定基点 `2d04cacc5322951f187bb17e017c12920ac8ebe2` の `v4.0` を
   新しい作業ディレクトリへ展開します。元のソースやユーザーの編集は変更しません。
2. [出典を記録した修復パッチ](../tools/dos4/PROVENANCE.md)を検証・適用し、
   明示したテキスト形式だけをCRLFにします。バイナリは変換しません。
3. 作業コピーの `v4.0` をDOSBox-XのDドライブに割り当て、
   `SETENV.BAT` とルートの `NMAKE` を実行します。`-I` は使用しません。
4. 成功マーカー、ログ内エラー、`CPY.BAT` の全62ファイルの存在を確認します。
5. ビルドした `MSBOOT.BIN` の `0x7C00` から512バイトを取り出し、
   1.44 MBのBPB、2個のFAT、ルートディレクトリを構築します。
   `IO.SYS` と `MSDOS.SYS` を先頭2エントリー・先頭の連続クラスタへ配置します。
6. 全62ファイルと自動検証用 `AUTOEXEC.BAT` を格納し、内容を再読込して照合します。
7. QEMU/TCGでイメージのコピーを起動します。DOS自身の `VER`、`DIR`、
   `ECHO`、`TYPE` が書いたファイルを終了後に確認します。

ビルドでは同梱のMASM、Microsoft C、LINK、ライブラリや既存OBJなどを
使用します。これは公開ソースツリーのルートMakefileを再実行する手順であり、
同梱の開発ツールや全ライブラリをソースから作り直すブートストラップではありません。

## 生成物と検証結果

| 作業ディレクトリ内のファイル | 内容 |
| --- | --- |
| `msdos4-boot.img` | 起動用の未変更イメージ、1,474,560バイト |
| `image-manifest.json` | 格納ファイルのサイズ・SHA256とイメージのSHA256 |
| `v4.0/src/BUILD.LOG` | NMAKEと各ツールの出力 |
| `v4.0/src/BUILD.OK` / `BUILD.ERR` | DOS側の成功／失敗マーカー |
| `dosbox-host.log` | DOSBox-X側のログ |
| `checkpoint.json` | `prepared` / `building` / `built` の進捗 |
| `qemu-test.img` | 検証結果のファイルが書き込まれたイメージのコピー |
| `verification.json` | QEMUバージョンとDOSコマンドの実出力。成功時のみ作成 |
| `qemu-screen.ppm` | 検証時のQEMU画面 |
| `qemu.log` | QEMUの診断ログ |

`verification.json` の成功条件は `MS-DOS Version 4.00`、
`DIR` 出力の `COMMAND`、`PROBE.TXT` と `READ.TXT` の
`DOS4-WRITE-READ` の一致です。120秒で完了しない場合は失敗し、
可能なら `qemu-timeout.ppm` も保存します。
未変更イメージに以前の検証ファイルがある場合は、使い回しによる誤判定を防ぐため拒否します。

イメージのシリアル番号・ディレクトリ時刻は固定です。同じ入力ファイルから
同じイメージを生成できます。QEMU試験用コピーには実行時刻と試験結果が書かれるため、
再現性の比較には `msdos4-boot.img` を使ってください。

## 中断後の再開と個別実行

既存の作業ディレクトリを自動削除・上書きして準備し直すことはしません。
ビルド途中で中断した場合は、DOSBox-Xが既に終了していることを確認してから実行します。
同じ作業ディレクトリで2個のビルドを同時に走らせないでください。

```powershell
./tools/dos4/run.ps1 -Work .work/build -Distro Ubuntu-24.04 -Resume
```

`-Resume` は準備を省き、NMAKEによる増分ビルドから再開します。
準備自体が中断して `checkpoint.json` がない場合は、新しい作業パスを指定してください。
全工程をクリーンに再現する場合も新しいパスを使います。

```powershell
./tools/dos4/run.ps1 -Work .work/repro -Distro Ubuntu-24.04
Get-FileHash .work/build/msdos4-boot.img, .work/repro/msdos4-boot.img
```

各段階を個別に実行することもできます。前の段階の終了コードが0であることを
確認してから、次の段階に進んでください。

```powershell
$dosbox = ./tools/dos4/get-dosbox.ps1
python tools/dos4/build.py prepare --work .work/manual
python tools/dos4/build.py build --work .work/manual --dosbox $dosbox
python tools/dos4/image.py --work .work/manual
```

QEMU検証だけを再実行する場合は、Ubuntu側でリポジトリへ移動します。

```bash
cd /mnt/e/src/git/fork_repo/MS-DOS  # 自分の配置先に置き換える
python3 tools/dos4/verify.py --work .work/manual
```

対話的に操作する場合は、WSLgのあるUbuntu側で以下を実行できます。
`-snapshot` を付けるため、終了後に起動イメージへ変更は残りません。

```bash
qemu-system-i386 -machine pc -accel tcg -m 16 \
  -drive file=.work/build/msdos4-boot.img,format=raw,if=floppy,index=0 \
  -boot order=a -nic none -snapshot
```

## 失敗の切り分け

- `stdio.h` が見つからない：作業コピーの `SETENV.BAT` のINCLUDEを確認します。
  正しい同梱Cランタイムの場所は `D:\src\TOOLS\BLD\INC`、
  LIBは `D:\src\TOOLS\BLD\LIB` です。`src/INC` や `src/H` だけでは不足します。
- ソース準備のパッチ検証失敗：基点コミットが取得されているか、パッチのSHA256、
  改行・文字コード変換が行われていないかを確認してください。
- ビルド失敗：`BUILD.LOG` と `dosbox-host.log` を確認します。
  `LINK warning L4021: no stack segment` はCOM形式等で出ます。
  初回ビルドの存在しないファイルに対する `del` 診断もあります。
  エラーを無視するNMAKEオプションで成功扱いにしないでください。
- QEMU起動失敗：`qemu.log`、タイムアウト画像、Ubuntuディストリビューション名を確認します。
  `BUILD.OK` だけでDOSの起動成功とは扱いません。
- FAT16 HDD、SELECTによるインストール操作、全コマンドや全ドライバーの動作は
  この試験の対象外です。ここで検証するのはIssue #1の1.44 MB起動・基本操作です。

イメージ形式の単体テストは `python -m unittest discover -s tools/dos4 -p 'test_*.py'` です。
実際の検証記録は [validation-msdos4.md](validation-msdos4.md) を参照してください。
