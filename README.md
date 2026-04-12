# X88000M

X88000M は、Manuke 氏による PC-8801 エミュレータ `X88000` をベースにした macOS 向け移植版です。現在のアプリ本体は `SDL3 + Dear ImGui` ベースの `X88000M.app` です。

メニュー操作、ドラッグ&ドロップ、メディア管理、デバッガをまとめて使えるように整理してあり、「まず起動して遊びたい」用途にも、「動作確認や解析に使いたい」用途にも向いています。

## X88000M の強み

- **SDL3 + Dear ImGui ベースの単一 frontend**
  macOS で扱いやすい構成に整理しており、アプリ本体は `X88000M.app` に統一しています。
- **メディア操作が速い**
  `D88`、`T88`、`CMT` をメニュー、ドラッグ&ドロップ、起動引数から読み込めます。
- **D88 の複数イメージ管理に対応**
  `Disk Manager` で D88 内のイメージを一覧し、Drive 1〜4 に割り当てできます。
- **デバッグ用途にも強い**
  別ウィンドウのデバッガ、逆アセンブル、メモリダンプ、ブレークポイント、RAM エクスポート、実行ログ記録を利用できます。
- **普段使いに必要な機能を搭載**
  BASIC モード切り替え、4/8 MHz 切り替え、環境設定、スクリーンショット保存、画面テキストのコピー、テキスト貼り付け、プリンタプレビュー、フルスクリーンに対応しています。
- **設定を保持**
  ウィンドウサイズや各種設定は `~/Library/Application Support/X88000M/X88000.ini` に保存されます。

## 主な機能

- ROM 読み込みと実行
- `D88` の挿入・イジェクト
- `T88` / `CMT` の読み込みとテープ操作
- メディアファイルのドラッグ&ドロップ
- 起動引数 `--disk1=...` `--disk2=...` `--tape=...`
- `Disk Manager` / `Tape Manager`
- `Environment Settings`
- BEEP / PCG / OPNA の音声出力
- `Debug Window` / `Record Execution Log` / `Export RAM`
- スクリーンショット保存、クリップボード連携、プリンタプレビュー

## 使い始める前に

`X88000M` には ROM イメージは含まれていません。macOS では、ROM 一式を次の場所に置くのが一番簡単です。

- `~/Library/Application Support/X88000M/`

端末から起動する場合はカレントディレクトリからも探します。必要に応じて `X88_ROM_DIR` 環境変数でも ROM ディレクトリを指定できます。

代表的に参照されるファイル名:

- `pc88.rom`
- `n88.rom`
- `n80.rom`
- `n88_0.rom`
- `n88_1.rom`
- `n88_2.rom`
- `n88_3.rom`
- `disk.rom`
- `kanji1.rom`
- `kanji2.rom`
- `font80sr.rom`
- `optfont.rom`

`pc88.rom` がある場合はそこからまとめて読み込み、無い場合は分割された ROM 名を順に探します。

## 基本操作

- `Media -> Open Media...` でディスクやテープを開けます。
- `D88` / `T88` / `CMT` をウィンドウへドラッグ&ドロップしても読み込めます。
- `System -> Environment Settings...` から BASIC モード、CPU クロック、ドライブ数、各種オプションを変更できます。
- `Debug -> Debug Window...` で別ウィンドウのデバッガを開けます。

よく使うショートカット:

- `Ctrl+O`: メディアを開く
- `Ctrl+R`: リセット
- `Ctrl+P`: 一時停止 / 再開
- `Ctrl+1` / `Ctrl+2`: Drive 1 / 2 をイジェクト
- `Ctrl+Enter`: フルスクリーン切り替え

ドラッグ&ドロップ時は `Shift+drop` で Drive 2、`Cmd+Ctrl+drop` で Drive 1 に優先投入できます。

## macOS でソースからビルドする場合

`cmake` が必要です。ローカルの `SDL3` を使う場合は `pkg-config` も必要です。Dear ImGui は `third_party/imgui/` を優先し、無い場合はデフォルトで自動取得します (`X88000M_FETCH_IMGUI=ON`)。

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target X88000M -j
```

`SDL3` をローカルに入れていない場合は、次のように自動取得できます。

```sh
cmake -S . -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DX88000M_FETCH_SDL3=ON
cmake --build build --target X88000M -j
```

`X88000M_FETCH_IMGUI=ON` は現在のデフォルト値なので、通常は明示しなくて構いません。

macOS では `build/X88000M.app` が生成されます。端末から直接起動する場合は `build/X88000M.app/Contents/MacOS/X88000M` を実行してください。

Linux を含め、frontend は SDL3 + Dear ImGui のみを前提にしています。

## 現在の注意点

- ROM イメージは同梱していません。
- 互換性向上と細部の追い込みは継続中です。特に OPNA (`YM2608`) まわりは今後も改善対象です。
- オリジナルの Linux 版 / Windows 版と完全に同一の挙動を保証するものではありません。

## クレジット

- Original X88000 by Manuke
- macOS / SDL3+ImGui port maintained by bubio
- オリジナル配布物の説明文書は [src/X88000Src.txt](./src/X88000Src.txt) に収録しています。

## ライセンス

オリジナル配布物では、X88000 ソースは `PDS` (`Public Domain Software`) と表記されています。標準的な SPDX ライセンス識別子にそのまま対応するものではないため、このリポジトリでは upstream の文言を [LICENSE](./LICENSE) にそのまま収録しています。
