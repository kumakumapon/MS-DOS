# PC-98 DOS 4.0の日本語表示とエディタ

[Issue #5](https://github.com/kumakumapon/MS-DOS/issues/5)で追跡します。
PC-98のテキストVRAMへShift_JIS本文を表示し、DOS上で動くEDIT98を同梱します。
日本語の変換はWebNP2のホストIMEを使います。本文はShift_JIS、改行はCRLF、
ファイル名はASCIIの8.3形式が対象です。

編集・保存・再読込・TYPE表示と、既存DOS／ファイラーの動作をゲスト上で検証します。
