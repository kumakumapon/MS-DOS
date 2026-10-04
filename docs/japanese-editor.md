# PC-98 DOS 4.0の日本語表示とエディタ

[Issue #5](https://github.com/kumakumapon/MS-DOS/issues/5)で追跡します。
PC-98のテキストVRAMへShift_JIS本文を表示し、DOS上で動くEDIT98を同梱します。
日本語の変換はWebNP2のホストIMEを使います。本文はShift_JIS、改行はCRLF、
ファイル名はASCIIの8.3形式が対象です。

`A>`で`EDIT MEMO.TXT`または`EDIT98 MEMO.TXT`を実行します。
引数なしで新規ファイル、存在しない名前でも新規ファイルを作成できます。
`FD98`ではファイルを選んで`E`を押すと、起動ドライブのエディタを実行し、終了後に戻ります。

| キー | 操作 |
| --- | --- |
| F1 | ヘルプ |
| F2 | 開く・新規作成 |
| F3 / Ctrl-S、F4 | 保存、名前を付けて保存 |
| 矢印、HOME / Ctrl-A、Ctrl-E | 文字・行移動、行頭、行末 |
| ROLL UP / DOWN | ページ移動 |
| Backspace / Delete、Ctrl-Y | 文字を削除、行を削除 |
| Ctrl-Z | 最後の編集の取消・再実行 |
| F5 / Ctrl-F、F6 | 検索、次を検索 |
| F10 / Esc | 終了（未保存なら保存・破棄・取消を選択） |

日本語はWebNP2のテキスト送信欄でホストOSのIMEを使い、変換・確定後に送信します。
変換途中のキーはゲストへ送りません。`TYPE JPHELLO.TXT`で日本語表示を確認できます。
CONはShift_JISをPC-98の漢字コードに変換し、全角文字を2セルで表示します。
右端の全角文字は次行へ送り、スクロール時に文字と属性を一緒に移動します。
FD98のF3もShift_JIS本文を表示します。

本文は最大16 KiB（改行をLFへ正規化したサイズ）。半角カナ、タブ、長い行の横スクロールに
対応し、カーソルと削除は全角2バイトを分割しません。CRLF・CR・LFを読み込めますが、
保存はShift_JIS / CRLFです。バイナリと不正なShift_JISは拒否します。
読み取り専用ファイルは閲覧・編集し、別名で保存できます。開く・保存すると取消履歴を消去します。

保存は同じディレクトリの新規`EDxxxxxx.TMP`へ書き、close成功後に元を`EDxxxxxx.BAK`へ
移動して新ファイルを配置します。書込・close・配置の失敗時は元ファイルを保持または戻します。
復旧や後片付けに失敗した場合は残ったファイルのパスを表示します。置換中の電源断では
`.BAK`が残る場合があります。内容を確認してDOSまたはFD98で復旧してください。

EDIT98は独立したMITライセンスの新規実装です。MicrosoftのEDITのソースは使いません。
386以降のPC-98が対象で、IBM版エディタ、日本語ファイル名、DOS単体のかな漢字変換は含みません。
COMMAND.COMのコマンド名・パスにはASCIIを使ってください。

```sh
make -C tools/editor all test
cp ports/pc98/build/dos4/msdos4-pc98.xdf /path/to/webnp2/public/test/dos4-editor.xdf
CHROMIUM=/usr/bin/chromium node tools/editor/verify-webnp2.mjs /path/to/webnp2
```

ホスト試験はASan/UBSanで全角境界・取消・容量・不正本文・書込失敗・close失敗・置換復旧を確認します。
ブラウザ試験は実際のゲストキーで日本語表示、編集、保存、検索、終了の取消・破棄、
FD98から起動、全角右端折返し・スクロールを実行し、保存ファイルのShift_JIS / CRLFを照合します。
