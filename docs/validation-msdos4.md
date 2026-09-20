# MS-DOS 4.00 検証記録

実施日：2026-09-20。WindowsホストでDOSBox-X 2026.08.31、
WSL Ubuntu-24.04でQEMU 8.2.2（`1:8.2.2+ds-0ubuntu1.18`）を使用。
実行手順は [build-msdos4.md](build-msdos4.md) を参照してください。

## 結果

- 元の `v4.0` ツリーへの変更なし。
- 固定コミットから独立した作業コピー `.work/build` と `.work/repro` を作成。
- DOSBox-XでルートNMAKEが成功し、CPY.BAT対象62ファイルを確認。
- `.work/repro` は空の作業ディレクトリから準備・全体ビルドを実行。
- 全格納ファイルのSHA256と起動イメージのSHA256が2つの作業コピーで一致。
- それぞれのイメージをQEMUで起動し、DOS自身のVER、DIR、書き込み・読み取りを確認。
- `run.ps1 -Resume` でも、ビルドからWSL起動検証まで一括実行に成功。
- FAT12の配置、往復読み込み、決定性、不正ブートセクター・ファイル名・
  重複・容量超過・FAT破損の拒否を確認する3単体テストが成功。

未変更の起動イメージは1,474,560バイト、SHA256は次のとおりです。

```text
f5e8ec0b90a9db0441539cc1afdece3a0c223b196cd8a9d4d99c7240587e64cd
```

| ファイル | サイズ | SHA256 |
| --- | ---: | --- |
| IO.SYS | 33321 | `755c79c16d3db5280b5286f1032a4f77bb8556b79f0c22f9c9f85556c7b69e39` |
| MSDOS.SYS | 37376 | `573626782a3c45f0d9e5f00af1bd0f52a7de44bea96d2a2d3d660e9ae06158d5` |
| COMMAND.COM | 37556 | `19ebe2e5a8e18ca5d447a3d1fc42c42e4942ff5f3ef50e39ab01374bb01d8ea9` |

## 保存した証拠

- [クリーン全体ビルドのログ](validation/dos4/build.log)（169,320バイト、
  SHA256 `3d598e5bf8d095ea6d211bafb587088c0b5c83bd67a2cb0a9f5859420f6806e8`）
- [全ファイルのハッシュ一覧](validation/dos4/image-manifest.json)
- [QEMU検証の実出力](validation/dos4/verification.json)
- [QEMU画面](validation/dos4/qemu-screen.png)

ログには `fatal error`、`error <英字><数字>`、`Stop.` に一致する診断はありません。
LINKのスタックセグメント警告と、初回の削除対象が存在しない診断は記録されています。
DOSBox-Xのホストログには `Packed file is corrupt` の検出警告と
`DIRCACHE: ... All slots full. Resetting` の診断が出ましたが、NMAKEは成功し、
全配布対象の確認と独立再ビルドの一致、QEMU起動試験も通過しました。
これを根拠に全コマンドの動作まで保証するものではありません。

実際のVER出力：

```text
MS-DOS Version 4.00
```

DOSの `echo DOS4-WRITE-READ> PROBE.TXT` と
`type PROBE.TXT > READ.TXT` が作成した2ファイルはいずれも
`DOS4-WRITE-READ\r\n` でした。DIR出力はCOMMAND.COMを含む一覧です。
DIR.TXT自身の表示サイズが0なのは、その一覧を同ファイルへ出力中だからです。

検証イメージは元イメージのコピーです。QEMU終了後も元イメージのハッシュは
変化していません。ホスト側で結果ファイルを作成して成功扱いにする処理はありません。

## 対象範囲

公開ツリー同梱のツール・ライブラリ・既存オブジェクトを使う全体ビルドと、
1.44 MB FAT12からのDOS起動・基本操作を確認しました。
FAT16 HDD、全ドライバー、SELECTインストーラーの操作、実機互換性は未検証です。
