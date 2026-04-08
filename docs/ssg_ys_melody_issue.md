# SSG melody not audible in イース (Ys) — known issue

## Symptom

PC-8801 game イース (Ys) BGM:
- **Intro (0–37 s in-game)** — FM channels play correctly. SSG mixer state is `$38`
  (all 3 tones enabled, no noise). `tone=[189 188 1] vol=[10 10 0]` etc — short
  unison/harmony fragments on ch A,B at ~660 Hz.
- **SSG melody phase (37 s onward)** — on real hardware the SSG carries the
  main melody. In X88000M the SSG is silent during this phase.

ハイドライド3 など他の SSG メロディを使うゲームは正しく鳴っているので SSG コア
全般の問題ではなく、Ys 固有のドライバ idiom が再現できていない。

## What the debug log shows

`X88_FM_DEBUG=1` で 0.25 秒ごとにスナップショットを取ると、SSG メロディフェーズ
全体 (12 秒分 / 51 スナップショット) で SSG mixer が **`$DB` で固定** に見える。

`$DB` = `0b11011011`:
- bit 0,1 = 1 → ch A, ch B の **tone disable**
- bit 2 = 0 → ch C tone enable
- bit 3,4 = 1 → ch A, ch B の noise disable
- bit 5 = 0 → ch C noise enable
- bit 6,7 = 1 → I/O port A,B output direction (joystick output mode)

しかし同じスナップショットで:
- `vol=[8. 7. 0.]` `tone=[168 167 15]` のように **ch A,B には明らかにメロディの
  音程と vol が書き込まれている**
- ch C は `tone[C]=15` (≈4 kHz、メロディに不適) で `vol[C]` も小さい

つまり driver は ch A/ch B にメロディを書き込んでいるのに、mixer 上では disable
されている — この状態でなぜ実機では音が鳴るのかが現状の謎。

## 検証済みで NG だった仮説

### 仮説 1: 「両方 disable のとき constant DC が出る idiom (vol register modulation)」
YM2149 の式 `out = (tone|tone_dis) AND (noise|noise_dis)` から、両方 disable で
出力は constant 1 となり、それが vol DAC を通って高速 vol 書き換えで音を作る、と
いう古典 PSG 技。

→ この技を許可するため `if (tone_dis && noise_dis) continue;` を一時的に外し、
   代わりに DC をそのまま出すようにしてもメロディは鳴らなかった。
   (vol 変化のレートが BGM tick の 60 Hz オーダーで、可聴域に達していないため
   そもそもこの仮説では音にならない)

→ 元の silence 動作に戻した。ユーザー指摘どおり「disable は disable で音が出ない
  のが正しい」。

## 残っている可能性

1. **driver は 0.25 秒よりずっと短い周期で mixer を toggle している** が、現状の
   スナップショットがそれを見逃している。例: BGM tick (60 Hz / 16 ms) ごとに
   `$38 → 鳴らす → $DB → joystick poll → $38` のような流れがあれば、スナップ
   ショットは常に joystick poll 後の `$DB` を捉えてしまう。
2. **タイマ起因の再現性問題** — Ys の SSG メロディは Timer A/B 割り込み周期で
   駆動される。我々の Timer 実装に subtle なズレがあって、driver の状態機械が
   想定外のフェーズで止まっている可能性。
3. **サブ CPU 経由の何らかの副作用** — Ys は disk loader でサブ CPU を多用する。
   メイン/サブ間の通信レイテンシで音楽 driver の進行が壊れている可能性。

## 次に試すべきこと

- $07 ($00–$0F 全部でも可) への書き込みを histogram/履歴で取り、本当に `$DB` で
  固定なのか、それとも頻繁に toggle しているのかを測る (0.25 秒スナップショットは
  当てにならない)。
- 上記が toggle していたら、`$38` の瞬間に SSG が音を出しているか、出ていれば
  音量・タイミングが正しいかを波形ダンプで検証する。
- 上記が `$DB` 固定なら、Ys driver の動作仮定が崩れているので、Timer / IRQ / sub
  CPU 周りに原因がある可能性が高い。レガシー GTK 版 (動くか不明) や別エミュでの
  挙動と比較する。

## 影響範囲

- イース のメロディが完全に欠落 (Ys の体験を著しく損なう)
- 同種の idiom を使う他ゲームでも同様の不具合が出る可能性
- ハイドライド 3 など普通の SSG メロディドライバを使うゲームは影響なし
