# X88000M

X88000M は、Manuke 氏による PC-8801 エミュレータ `X88000` の公開ソースをベースにしたリポジトリです。

このリポジトリはオリジナルの Linux 版ソースを土台にしつつ、macOS でもビルドできるように最小限の調整を加えています。元の説明文書は [X88000Src.txt](./src/X88000Src.txt) に含まれています。

エミュレータ本体のソースコードは [`src/`](./src) 配下にまとめてあります。リポジトリのルートには README、ライセンス、CMake 設定を置いています。

## 概要

- 作者: Manuke
- 元文書の表記: `Written by Manuke 1998-2018`
- ベース: X88000 Linux 版ソース
- GUI:
  - Windows 版は Win32 API / DirectX
  - Linux 版は X-Window / GTK+ 2.8 以降
- このリポジトリは Linux 版ベースで管理

`X88000Src.txt` によると、環境依存部分は主に `X88...` で始まるファイル群で、それ以外のエミュレーション本体は各環境でほぼ共通です。

## 現在の状態

- Linux 版ソースをベースにしています。
- macOS では GTK+ 2 系を使ったビルドを確認しています。
- macOS 向けの変更は、Linux 前提の一部 X11 経路の無効化と CMake / 既存ビルド設定の調整に留めています。
- OPNA (`YM2608`) エミュレーションは upstream 文書でも「大して機能してません」とされており、現状でも未完成です。
- BEEP / PCG のロジックはありますが、実際の音声出力実装は Windows の DirectSound 前提で、非 Windows 環境では音は未実装です。

## ビルド

### Linux

`gtk+-2.0` の開発環境、`pkg-config`、`cmake` が必要です。

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

実行ファイル `X88000` は通常 `build/` 配下に生成されます。

### macOS

Homebrew で `gtk+` を入れた状態を想定しています。

```sh
brew install gtk+
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

macOS では `build/X88000.app` が生成されます。端末から直接起動する場合は、実体の実行ファイル `build/X88000.app/Contents/MacOS/X88000` を使えます。
この GTK2 ベース実装では、メニューは macOS の上部メニューバーではなくアプリウィンドウ内に表示されます。

環境によって `pkg-config` が `gtk+-2.0` を見つけられない場合は、`PKG_CONFIG_PATH` に Homebrew の `gtk+` の `pkgconfig` ディレクトリを追加してください。

upstream 由来の [`src/Makefile`](./src/Makefile) は参考用として残していますが、このリポジトリでは CMake を正規のビルド入口としています。

## SDL3 frontend (WIP)

`SDL3 + ImGui` 移行に向けて、`X88000SDL3` ターゲットを追加しています。

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target X88000SDL3 -j
```

macOS では `build/X88000SDL3.app` が生成されます。
端末から直接起動する場合は `build/X88000SDL3.app/Contents/MacOS/X88000SDL3` を実行できます。

現時点では SDL3 の独立プロトタイプで、エミュレーション core との接続は最小限です。
ビルド構成として `x88core`（PC88/Z80/DiskImage/TapeImage などの共有 core ライブラリ）を `X88000SDL3` からリンクし、ROM が見つかった場合に `CPC88::Initialize/Reset/Execute` を SDL3 ループ内で呼ぶブリッジまで実装しています。
また、SDL3 のキーボード状態から PC-8801 キーマトリクスへ反映する入力マッピング（英数字、カーソル、テンキー、ファンクションキー、修飾キー）を実装しています。
実行ループは 60fps 相当の固定フレームペースで動作し、表示は 640x400 をウィンドウ内にレターボックス表示します。

現在の通常機能（SDL3 側）:

- ROM 読み込みと実行
- D88 の挿入/イジェクト
- T88/CMT の読み込み
- ファイルダイアログからのメディア読み込み（`Media` メニュー）
- ディスクイメージ管理ウィンドウ（`Media -> Disk Manager...`）— D88 内の複数イメージをツリー表示し、各ディスクを Drive 1〜4 に割り当て可能
- テープイメージ管理ウィンドウ（`Media -> Tape Manager...`）— ロード/Erase/FWD/REW 操作と進行状況表示
- 環境設定ウィンドウ（`System -> Environment Settings...`）— BASIC モード、CPU クロック、ドライブ数、Wait/Old/PCG 等を設定
- BASIC モード / クロックのクイックスイッチ（`System -> BASIC Mode` / `Clock`）
- 起動引数でのメディア指定: `--disk1=...` `--disk2=...` `--tape=...`
- ファイルのドラッグ&ドロップ読み込み（D88/T88/CMT、Shift+drop で Drive 2 / Cmd+Ctrl+drop で Drive 1）
- 設定の永続化: ウィンドウサイズや環境設定をレガシー GTK 版と同じ `~/Library/Application Support/X88000M/X88000.ini` に保存
- 実行中ショートカット:
  - `Ctrl+O`: メディアファイルを開く
  - `Ctrl+R`: リセット
  - `Ctrl+P`: 一時停止/再開
  - `Ctrl+1` / `Ctrl+2`: Drive1/Drive2 イジェクト
  - `Ctrl+Enter`: フルスクリーン切り替え

Dear ImGui は `third_party/imgui/` に source があればそれを使い、無い場合は CMake の `FetchContent` で自動取得します（デフォルト有効）。

`SDL3` / `Dear ImGui` を CMake で自動取得したい場合は、次のオプションを使えます。

```sh
cmake -S . -B build \
  -DX88000M_FETCH_SDL3=ON \
  -DX88000M_FETCH_IMGUI=ON
```

`X88000M_FETCH_SDL3` のデフォルトは `OFF`、`X88000M_FETCH_IMGUI` のデフォルトは `ON` です。ローカルに `pkg-config` 経由の `sdl3` と `third_party/imgui` がある場合はそちらを優先します。

SDL3 プロトタイプが参照する ROM ディレクトリは次の順です。

- `X88_ROM_DIR` 環境変数
- カレントディレクトリ
- macOS の `~/Library/Application Support/X88000M/`

## 実行に必要な ROM

ROM イメージは同梱していません。

コード上は、システム ROM を次の順に探します。

- 現在の作業ディレクトリ
- macOS では `~/Library/Application Support/X88000M/`
- 実行ファイルに対応する resource ディレクトリ
- 実行ファイルのあるディレクトリ

そのため、端末から起動する場合は ROM 一式をカレントディレクトリに置くか、macOS では `~/Library/Application Support/X88000M/` に配置すると扱いやすくなります。`.app` を Finder から起動する場合も、基本的にはこの `Application Support` 配下へ置く運用を想定しています。

必要なら、`build/X88000.app/Contents/Resources/` に ROM を置いて self-contained な bundle として扱うこともできます。

設定ファイル `X88000.ini` と `Debug.log` は、macOS では `~/Library/Application Support/X88000M/` に保存されます。

代表的に参照されるファイル名は以下です。

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

## ソース構成

`X88000Src.txt` では、クラス群はおおむね次のように整理されています。

- Z80 エミュレータクラスライブラリ
- PC-8801 エミュレータクラス群
- ディスク / テープイメージクラス群
- パラレルデバイスクラス群
- X88000 固有の GUI / 管理クラス群

いまのリポジトリでもこの構成はほぼそのまま維持されています。

## リポジトリ構成

- [`CMakeLists.txt`](./CMakeLists.txt): このリポジトリのビルド設定
- [`src/`](./src): エミュレータ本体のソースコード、upstream の説明文書、元の `Makefile`
- [`README.md`](./README.md): このリポジトリ向けの概要
- [`LICENSE`](./LICENSE): upstream のライセンス告知

## ライセンス

オリジナル配布物では、X88000 ソースは `PDS` (`Public Domain Software`) と表記されています。標準的な SPDX ライセンス識別子にそのまま対応するものではないため、このリポジトリでは upstream の文言を [LICENSE](./LICENSE) にそのまま収録しています。

`CC0-1.0` や `The Unlicense` のような近い性格の標準ライセンスはありますが、このリポジトリでは upstream の文面を別ライセンスへ読み替えることはしていません。
