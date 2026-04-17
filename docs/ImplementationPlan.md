# X88000M 移植・YM2203実装プラン

> **ステータス (2026-04-18, v1.0.2 時点): Phase A–D およびその後の音質改善まで達成済み。**
> このプランは当初立てた移植ロードマップを記録として残したものです。本文中の各フェーズは下記の通りすべて完了しており、v1.0.2 は「SDL3 版が旧 frontend と同等の操作性で動作し、YM2203 (FM 3ch + SSG 3ch) が fmgen 参照実装と A/B で区別が難しいレベルに近い音色で鳴る」段階に到達しました。残っているのは decap 由来 LUT 相当の精度が必要な領域のみ (詳細は `docs/YM2203.md` 末尾の「残課題 (v1.0.2 時点)」を参照)。

## Context

X88000 (Manuke氏作 PC-8801 エミュレータ, 元は Linux 版) を macOS に移植。出発時点からの進捗は次の通り:

1. **macOSアプリバンドル化** (commit `1bf1115`, `3b10327`)
2. **CMake化と src 配下整理** (commit `3380c8e`)
3. **SDL3+ImGui フロントエンドのブートストラップ** (commit `01b950f`, `2d9d80f`, `3b10327`)
   — `src/sdl3/Sdl3FrontendMain.cpp` に集約、`x88core` 静的ライブラリと分離済み。
4. **Phase A〜D 完了**: 環境設定・メディア管理・D&D・フルスクリーン・設定永続化・オーディオ経路・YM2203 自作 (FM 3ch + SSG 3ch)・デバッガ/プリンタ UI の SDL3 移植まで一通り実装。
5. **FM 音質の二次改善 (v1.0.2)**: op output 14-bit 化、modulation `>> 1` + feedback `>> (10-FB)` の整合、envelope rate 一律 4x 低速化で fmgen 参照実装に近接 (詳細は `docs/YM2203.md` のバグ修正履歴 #6, #7, #11, #12)。

最終マイルストーン (=SDL3版が旧 frontend と同等の操作性で動作し、YM2203 から実機近似の音が出る) は v1.0.2 で達成。以後は回帰防止と細部の詰めが中心となる。

## スコープ方針 (絶対)

- **X88000 が元々持っている機能はすべて SDL3版でも実装する。** デバッガ機能は X88000 の売りなので必ず移植する (Phase D)。プリンタも同様 (Phase D)。新ハード対応・新機能の追加はしない。
- **OPNA には対応しない。** YM2203 (FM 3ch + SSG 3ch) のみが対象。OPNA固有の FM 6ch / リズム / ADPCM / LFO は実装しない。クラス名 `CPC88Opna` は既存のまま保持するが、実体は YM2203 機能のみ。
- UI は旧 frontend と同等の構成を基本とし、わかりにくい箇所のみ ImGui ならではの形に調整。
- サウンドコアは ymfm / fmgen / MAME などいかなる外部FM音源コードも持ち込まず、仕様書ベースで1からスクラッチ実装する。X88000 のフリーライセンスを他プロジェクトのコードで汚染しないため。アルゴリズム名・定数テーブルも自作する。

## フェーズ構成

4フェーズに分割し、各フェーズ末で動作確認できる状態を維持する。Phase A → B → C → D の順。

| Phase | 内容 | ステータス |
|---|---|---|
| **A** | SDL3 フロント基本機能 (音以外) — 設定/Env/イメージ管理/D&D/フルスクリーン | ✅ 完了 |
| **B** | 音声出力経路 (Sdl3AudioOutput + Beep/PCG) | ✅ 完了 (BEEP は v1.0.1 で窓トグルカウント方式に書き換え) |
| **C** | YM2203 自前実装 (FM 3ch + SSG 3ch) | ✅ 完了 + v1.0.2 で音質改善 |
| **D** | デバッガ & プリンタの SDL3 移植 (X88000 機能パリティの完成) | ✅ 完了 |

---

## Phase A — SDL3フロントエンドの機能パリティ仕上げ (音声以外)

ゴール: SDL3版だけで「ROMロード → ディスク挿入 → BASIC起動 → ゲーム操作」が旧 frontend と同じ感覚でできる。

### A-1. 設定永続化レイヤーの追加 (新規)

- 新規ファイル: `src/sdl3/Sdl3Settings.{h,cpp}`
- 役割: ImGuiのウィンドウ状態とは別に、エミュレーション設定 (BASICモード、CPUクロック、最後に開いたメディアパス、フルスクリーン状態など) を JSON または単純な key=value テキストで `~/Library/Application Support/X88000M/settings.cfg` に読み書き。
- 既存リソース不要。`X88Utility.cpp` 内のパス取得ヘルパは流用検討。
- 起動時ロード → `g_settings` に格納 → 各UIコードから参照、変更時に即保存。

### A-2. 環境設定 (Env) ImGui ウィンドウ

- 既存 `src/X88EnvSetDlg.{h,cpp}` を読み、扱っている項目を抽出 (BASICモード [N/V1S/V1H/V2/N80V1/N80V2]、ベースクロック [4MHz/8MHz]、ブーストモード、サウンドON/OFF、その他)。
- `src/sdl3/Sdl3FrontendMain.cpp` に `DrawEnvSettingsWindow()` を追加。System メニューに「Environment...」項目を追加。
- 設定変更は `CPC88::SetXxx()` 系API (要実装確認、なければ既存リセットフローに合わせて `CPC88::Reset()` をフック) に反映。BASICモード切替後は自動リセット。
- `Sdl3Settings` と双方向バインド。

### A-3. ディスク/テープイメージ管理ウィンドウ

- 既存 `src/X88DiskImageDlg.{h,cpp}` と `src/X88TapeImageDlg.{h,cpp}` を読み、機能 (複数イメージのリスト表示、追加、削除、ドライブ割り当て、順序変更) を確認。
- `src/sdl3/Sdl3FrontendMain.cpp` に `DrawDiskImageManagerWindow()` / `DrawTapeImageManagerWindow()` を追加。Media メニューに「Disk Manager...」「Tape Manager...」を追加。
- バックエンドは既存の `CPC88::GetDiskImageCollection()`, `CPC88Fdc::SetDiskImage()` を使用。`Sdl3FrontendMain.cpp` 既存の `MountDiskImageByIndex()`, `EjectDiskImageFromDrive()`, `g_anDriveDiskIndex[]` をそのまま流用。
- 順序入れ替えは ImGui の `Selectable` + 上下ボタンで十分。

### A-4. ドラッグ&ドロップ

- `Sdl3FrontendMain.cpp` のメインイベントループ (現在 `SDL_EVENT_*` を分岐している箇所付近) に `SDL_EVENT_DROP_FILE` ハンドラを追加。
- 受け取ったパスを既存 `AddMediaImage()` に渡す。拡張子で D88 → ドライブ自動割当、T88/CMT → テープ。

### A-5. フルスクリーン仕上げ

- 既存の Ctrl+Enter トグルコードを確認、`SDL_SetWindowFullscreen` 経由でデスクトップフルスクリーンへ。
- フルスクリーン状態を `Sdl3Settings` に保存して次回復元。
- レターボックス処理 (`CalcLetterboxRect`) はそのまま使える。

### A-6. メニュー再構成 (わかりにくい箇所のみ調整)

- 現状の System / Media / View に加え、Settings (Env, Audio[後で], Input) を追加。
- 旧 frontend の IDM_BASICMODE_* / IDM_BASE_CLOCK_* は Settings → Environment ウィンドウに集約 (元はメニュー直下にズラッと並んでわかりにくい)。
- 旧 frontend の Disk/Tape の挿入と管理が分離している箇所は、Media メニュー内で「Drive 1: ▸ (Empty / Manage...)」のようにサブメニュー化。

**A 完了の検証:** SDL3バンドルを起動し、Env ダイアログから N88-V2 を選んで Reset、D88 を D&D で投入、BASIC が立ち上がってゲームがロード & プレイできる。設定が次回起動でも復元される。

---

## Phase B — オーディオ出力経路の整備 (YM2203実装の前段)

ゴール: 鳴らすべき音 (Beep, PCG, あとで OPN) を SDL3 オーディオに流し込むパイプラインを完成させる。Beep と PCG はこの段階で鳴らし、YM2203 用のフックポイントだけ用意しておく。

### B-1. SDL3 オーディオドライバ初期化

- 新規ファイル: `src/sdl3/Sdl3AudioOutput.{h,cpp}`
- `SDL_OpenAudioDeviceStream()` で 44.1kHz / 16bit / stereo のストリームを開く。リングバッファ (1〜2フレーム分) を保持。
- `Push(const int16_t* samples, int frames)` インターフェイスを公開。

### B-2. ミキサ層

- 同 `Sdl3AudioOutput` 内に簡易ミキサを実装。複数チャンネル (Beep / PCG / OPN) からの int16 サンプルを線形加算 → クリップ → 出力ストリームへ。
- 各チャンネルにマスタゲインスライダ (Settings → Audio で後付け)。

### B-3. Beep / PCG 接続

- `Sdl3FrontendMain.cpp::OnCoreBeepOutput(bool, bool)` を実装。元 `X88BeepPlayer.cpp` のロジック (2400Hz矩形波) を SDL3版として書き直し、サンプル生成して `Sdl3AudioOutput::Push()`。
- 同様に `OnCorePcgOutput(int, int)` を `X88BeepPlayer` の PCG 部分を参考に実装 (これも自作なのでライセンス問題なし、元 X88000 の自作コード)。
- DirectSound 依存の `X88BeepPlayer.h` (Windows 用) は使わない。

### B-4. OPN 用スロットの予約

- `Sdl3FrontendMain.cpp` に `OnCoreOpnaSampleOutput(const int16_t* samples, int frames)` の空関数を追加。
- `CPC88Opna` 側に `typedef void (*SampleOutputCallback)(const int16_t*, int);` と `SetSampleOutputCallback()` を追加 (Phase C で実体を埋める)。

**B 完了の検証:** SDL3版で BASIC `BEEP` 命令、`PLAY` (PCG経由) が鳴る。OPN は無音のまま。

---

## Phase C — YM2203 自前スクラッチ実装

ゴール: `src/PC88Opna.{h,cpp}` に YM2203 (FM 3ch + SSG 3ch) を1から実装し、SDL3版でゲームミュージックが鳴る。**OPNAは実装しない。** クラス名は既存のまま `CPC88Opna` を維持するが、合成部の実体は YM2203 のみ。

**実装方針: YM2203 アプリケーションマニュアル(Yamaha公開資料) を一次資料とし、コードはすべて自筆。テーブルは生成コードで導出。**

### C-1. データ構造の刷新 (`PC88Opna.h`)

現在は static メンバのスタブ。これを実体を持つクラスに作り変える (互換性のため API シグニチャは維持しつつ拡張)。

- FMチャンネル × 3 (これがYM2203の上限。OPNAの6chは作らない)
  - 各チャンネル: 4オペレータ
  - 各オペレータ: 位相累算器 (φ), エンベロープ状態 (Attack/Decay/Sustain/Release), TL, AR, DR, SR, RR, SL, KS, MUL, DT, AM フラグ
- アルゴリズム & フィードバック (YM2203の8アルゴリズム)
- SSG (PSG) チャンネル × 3
  - トーンジェネレータ, ノイズジェネレータ, エンベロープジェネレータ, ミキサ, レベル
- 既存のタイマ A/B 部分は保持 (動作しているので壊さない)
- **対象外:** LFO/AMS/PMS, リズム音源, ADPCM, FM 4-6ch — OPNA固有のためレジスタも実装しない (書き込みは無視)。

### C-2. テーブル生成

- `Initialize()` 内で次のテーブルを生成 (define 値ベースで計算、定数表のコピペ禁止):
  - sin テーブル (1/4 周期, 256 or 1024 ステップ, 対数値)
  - 指数テーブル (log → linear 変換)
  - エンベロープ ratesテーブル (AR/DR/SR/RR の rate → クロック減算量)
  - DT (デチューン) テーブル (KC/KF からの周波数オフセット, 仕様の数式を実装)
- 生成過程に出典(マニュアルの章番号)をコメントで残す。

### C-3. レジスタ書き込みハンドラ

- `WriteAddress` / `WriteData` を拡張し、FM 0x30〜0xB6 / SSG 0x00〜0x0F / モード 0x20〜0x2F を全て分岐。
- 既存のタイマ部 (0x24〜0x27, 0x2D〜0x2F) は変更せず維持。

### C-4. サンプル生成ループ

- `Generate(int16_t* outBuf, int frames)` を新設。
- 内部クロック (3.9936 MHz / プリスケーラ) を出力サンプリングレート (44.1kHz) に変換するためのフラクショナル累積器を持つ。
- FM: 位相進行 → sin lookup → エンベロープ乗算 → アルゴリズムに従ってオペレータ加算 → ミックス。
- SSG: tone counter → on/off → ノイズ → エンベロープ → 加算。
- 出力は L/R 同一値 (YM2203 はモノラル) で stereo インターリーブ。

### C-5. クロック駆動 & コールバック

- 既存 `PassClock()` 内で「N サンプルぶんが貯まったら `Generate()` してコールバック」する形に拡張。
- フロントエンドコールバック (Phase B-4 で予約) を呼ぶ → `Sdl3AudioOutput::Push()`。

### C-6. 検証用の単体ハーネス (任意)

- `tools/opna_dump.cpp` (新規, CMake オプショントグル) を用意し、特定のレジスタ列を流して WAV を吐けるようにする。リグレッションに有用。
- 任意。本流が動けば省略可。

**C 完了の検証:**
- BASIC `PLAY "CDEFGAB"` で正しい音階が鳴る。
- 既知の YM2203 対応 PC-8801 ゲーム (例: ザナドゥ, テグザー等) で BGM が認識できる音色で鳴る。
- タイマ割込ベースの BGM 駆動 (Music Macro Language) が安定して動作する (タイマ部はもともと動いていたので回帰しないこと)。

---

## Phase D — デバッガ & プリンタの SDL3 移植 (X88000 機能パリティの完成)

ゴール: X88000 の売りであるデバッガ機能と、プリンタエミュレーション一式を SDL3版にも移植し、旧 frontend と完全に同等の機能セットにする。Phase A〜C で本体動作と音が安定してから着手する (デバッガを早期に着手するとフロント基盤の変動に振り回されるため)。

### D-1. デバッガ UI 移植

- 既存ファイルを参照しつつ、ImGui で再構築:
  - `src/X88DebugWnd.{h,cpp}` — メインデバッガウィンドウ (CPU状態, 実行/ステップ/トレース)
  - `src/X88DebugDisAssembleDlg.{h,cpp}` — 逆アセンブル表示
  - `src/X88DebugDumpDlg.{h,cpp}` — メモリダンプ
  - `src/X88DebugBreakPointDlg.{h,cpp}` — ブレークポイント管理
  - `src/X88DebugWriteRamDlg.{h,cpp}` — RAM書き込み
- バックエンドは既存:
  - `Z80Adapter` / `Z80DisAssembler` (既に core 側に存在) を流用
  - `CPC88::Z80Main()` / `Z80Sub()` から Z80 状態を取得
  - 既存のブレークポイント機構をそのまま利用
- ImGui ウィンドウ構成案:
  - **Debugger Main**: レジスタ表示 (Main/Sub Z80 切替), 実行制御ボタン (Run/Pause/Step/Step Over/Trace)
  - **Disassembly**: 逆アセンブルビュー, PC追随, ブレークポイント設定 (左クリック)
  - **Memory Dump**: 16進ダンプ + ASCII, アドレスジャンプ, Main RAM/Sub RAM/VRAM 切替
  - **Breakpoints**: ブレークポイント一覧, 追加/削除/有効化
  - **Trace Log**: 実行ログ (X88000 が出力する形式に合わせる)
- メニュー追加: Debug → Main CPU / Sub CPU / Disassembly / Dump / Breakpoints / Trace / Write RAM
- 元 IDM_DEBUG_* (X88Resource.h) を参照して漏れがないようにする。

### D-2. プリンタエミュレーション移植

- 既存ファイル:
  - `src/X88PrinterDlg.{h,cpp}` — メインプリンタウィンドウ
  - `src/X88PrinterPreviewWnd.{h,cpp}` — 印刷プレビュー
  - `src/X88PrinterCopyDlg.{h,cpp}` — コピー
  - `src/X88PrinterPaperFeedDlg.{h,cpp}` / `X88PrinterPaperDelDlg.{h,cpp}` — 用紙操作
  - `src/X88PrinterDrawer.{h,cpp}` — 描画ロジック
  - `src/X88ParallelManager.{h,cpp}`, `src/Parallel*` — パラレル接続デバイス管理
- バックエンドはそのまま利用 (`CParallelPrinter`, `CParallelPR201` 等は core 側に既存)。
- ImGui で:
  - **Printer Window**: プレビュー領域 + 用紙制御ボタン (Feed / Eject / Delete)
  - **Settings**: パラレルデバイス選択 (PR201 / 汎用 / Null), プリンタ機種選択
- 印刷出力はテクスチャに描画して ImGui::Image で表示。PDF/PNG エクスポート機能があれば踏襲。

### D-3. その他 X88000 機能の取りこぼしチェック

Phase D 着手時に `src/X88Resource.h` の IDM_* と `src/X88Frame.cpp` のメニュー構築コードを全件レビューし、SDL3版に未移植のものがないか確認:

- メモリイメージ管理 (IDM_MEMORY_IMAGE) — セーブステート相当
- バージョン情報ダイアログ (IDM_VERSION)
- クリップボード操作 (ビットマップ/テキストのコピー&ペースト)
- 各種クロック制御 (IDM_BOOST_MODE 等)

すべて移植して旧 frontend との機能差をゼロにする。これが Phase D 完了 = SDL3版の機能パリティ達成。

**D 完了の検証:**
- SDL3版だけで旧 frontend でできた操作がすべて行える (機能差リストを作成して照合)。
- デバッガで Z80 のステップ実行・ブレーク・逆アセンブルが動作。
- 既知のテストROM/ゲームでブレークを張り、レジスタ追跡ができる。
- パラレルプリンタにテキスト出力 → プレビューに反映 → 用紙操作可能。
- `X88000M` の単一ターゲットで SDL3 版がビルドでき、挙動に差異がないことを確認。

---

## 触る/参照する主要ファイル

| ファイル | 役割 | フェーズ |
|---|---|---|
| `src/sdl3/Sdl3FrontendMain.cpp` | フロントエンドのほぼ全部 | A, B |
| `src/sdl3/Sdl3Settings.{h,cpp}` (新規) | 設定永続化 | A-1 |
| `src/sdl3/Sdl3AudioOutput.{h,cpp}` (新規) | SDL3 audio + ミキサ | B |
| `src/X88EnvSetDlg.{h,cpp}` | 元 Env ダイアログ (参照のみ, 移植元) | A-2 |
| `src/X88DiskImageDlg.{h,cpp}` | 元 ディスクマネージャ (参照) | A-3 |
| `src/X88TapeImageDlg.{h,cpp}` | 元 テープマネージャ (参照) | A-3 |
| `src/X88Resource.h`, `src/X88Frame.cpp` | 元メニュー構造の参照元 | A-6, D-3 |
| `src/X88BeepPlayer.{h,cpp}` | 元 Beep/PCG 実装 (X88000自作なので流用OK) | B-3 |
| `src/PC88Opna.{h,cpp}` | YM2203 本体 — タイマ部は維持し合成部を新規追加 | C |
| `src/PC88.{h,cpp}` | OPNA 統合点 (`m_opna`) | C-5 |
| `src/PC88Z80Main.cpp` | I/O 0x44/0x45 ハンドラ (現状すでにOK) | C (確認のみ) |
| `src/X88DebugWnd.{h,cpp}` 系 | 元デバッガ実装 (移植元) | D-1 |
| `src/X88PrinterDlg.{h,cpp}` 系 | 元プリンタ実装 (移植元) | D-2 |
| `CMakeLists.txt` | 新規ソースの登録 | A, B, C, D |

## 既存の再利用ポイント

- `CPC88::GetDiskImageCollection()`, `CPC88Fdc::SetDiskImage()` — Phase A-3 のディスク管理 UI のバックエンドにそのまま使う。
- `Sdl3FrontendMain.cpp` 既存の `MountDiskImageByIndex()`, `EjectDiskImageFromDrive()`, `g_anDriveDiskIndex[]`, `g_astrDriveMediaPath[]`。
- `CDiskImageFile::SetDiskImageFileOpenCallback()`, `CPC88::SetSysFileOpenCallback()`, `CPC88Opna::SetIntVectChangeCallback()` のコールバック注入機構 — フロントエンドからコアへの逆向き通信は全部この方式で揃える。
- `CalcLetterboxRect()` — フルスクリーン仕上げで再利用。
- `PC88Opna::PassClock()`、Timer A/B 動作 — Phase C で壊さずそのまま温存。
- `Z80Adapter`, `Z80DisAssembler` — Phase D デバッガで再利用。

## 検証手順 (エンドツーエンド)

各フェーズ完了時に以下を実行:

```sh
cmake -S . -B build -DX88000M_FETCH_IMGUI=ON
cmake --build build -j
open build/X88000M.app
```

- **Phase A 完了時:**
  - Env ダイアログから BASIC モード (N88-V2 / N80) を切替 → Reset 後に画面が変化。
  - D88 ファイルをアプリにドラッグ&ドロップ → ドライブ1に自動マウント、BASIC からロードできる。
  - フルスクリーン (Ctrl+Enter) 切替 OK、終了→再起動で前回ウィンドウ状態と直近メディアパスが復元。
- **Phase B 完了時:**
  - BASIC `BEEP` で音が鳴る。
  - PCG を使うソフトで効果音が鳴る。
  - 設定 → Audio でマスタゲインスライダが効く。
- **Phase C 完了時:**
  - BASIC `PLAY "CDEFGAB"` が正しい音階で鳴る (FM/SSG 切替テスト含む)。
  - YM2203 対応の市販ゲームの BGM が認識できる音色とテンポで鳴る。
  - タイマ割込駆動の BGM が崩れない (Phase C 着手前のリグレッション確認: Phase A/B のうちにタイマ動作だけ確認しておく)。
- **Phase D 完了時:**
  - SDL3版だけで旧 frontend でできた操作がすべて行える。
  - デバッガで Z80 のステップ実行・ブレーク・逆アセンブルが動作。
  - パラレルプリンタにテキスト出力 → プレビューに反映 → 用紙操作可能。
  - `X88000M` 単一ターゲットのビルドが継続して通る。

## 実装しないもの (明示的に対象外)

- **OPNA関連すべて** (FM 4〜6ch / リズム音源 / ADPCM / LFO) — X88000 の搭載機能ではないため実装しない。
- X88000 が元々搭載していない新ハード対応・新機能全般。

---

## v1.0.2 到達後の残課題 (参考)

移植プランとしては本書の範囲はすべて実装完了。以下は「さらに詰めるなら」の領域:

- **FM 音色の decap 精度領域**: Attack curve table、envelope rate の段付き挙動。ライセンス方針により ymfm/fmgen/Nuked OPN2 等の decap 由来コード・定数は参照不可。YM2608 application manual の一次情報と fmgen 録音の FFT 差分解析を足がかりに経験的 fit するのが現実的。
- **SSG の細部**: 既知の小さな不具合 (Ys メロディ等) は `docs/ssg_ys_melody_issue.md` 系のドキュメントにまとめ済み。回帰確認は随時。
- **回帰防止**: 今後の改善で常に fmgen 参照実装との A/B を行い、v1.0.2 の音色感から後退しないようにする。
