# ケーブル揺すり試験 / Cable wiggle test

[日本語](#日本語) | [English](#english)

---

## 日本語

ドライバのリセット（#17536 の元）が、ケーブルのどこで起きているかを切り分ける方法です。このファームは、リセットが起きても止めずに log.txt へ記録するので、揺する場所を変えながら件数を数えられます。

### 準備

1. USB メモリを挿したまま **Save Logs To File をオン**にする（電源を切ると元に戻る）。
2. 調べるツールを掴ませ、5秒以上待つ（検出は掴んで約1秒後から働く）。
3. そのツールのノズル温度を **170℃** にして、到達するまで待つ（ヒーターに電流が流れている状態で試すため）。

### 目印の付け方

ログの時刻は時計の時刻ではないので、**区間ごとにノズルの目標温度を 1℃ ずつ変えて目印にします。** 温度を変えると、ログに `Set hotend temperature: 171` のような行が残ります。

| 目標温度 | 区間 | 内容（各60秒ほど） |
|---|---|---|
| 171℃ | A | 触らない |
| 172℃ | B | ツールヘッド側のコネクタの根元を、指で軽く揺する |
| 173℃ | C | ケーブルの中ほど（曲がる区間）を曲げ伸ばしする |
| 174℃ | D | 電子ボックスの出口付近を揺する |
| 175℃ | E | 触らない |
| 0℃ | — | 終わり |

- **温度を変えたら、10秒ほど待ってから揺すり始めてください。** ツールヘッドのログ置き場は256バイトしかなく、満杯のときに来た行は捨てられます。リセットが続くと、目印の行が落ちることがあります。
- ノズルは熱いので触らないでください。揺するのは軽く。

### 数え方

```
python3 tools/wiggle_sections.py log.txt 1     # 1 = ツール番号
```

区間ごとのリセット件数が出ます。件数は、ログの行数ではなく、行に入っている累計の件数 `n` の増え方で数えます（1秒に1行までしか書かないので、間引いた分は `skip` に入る）。

### 作者の機械での結果（ツール1）

| 区間 | 長さ | リセット |
|---|---|---|
| ツールを掴んだ直後（加熱中） | 約10秒 | 6 |
| B ツールヘッド側コネクタ | 50秒 | **72** |
| C ケーブルの中ほど | 57秒 | 0 |
| D 電子ボックスの出口 | 118秒 | 0 |
| E 触らない | 70秒 | 0 |

→ ツールヘッド側のコネクタの接触と判断し、ケーブルとコネクタを固定するカバーに替えた。替えた後、同じ試験で約9分間（掴む動作と揺すった区間を含む）リセットは **0回**。

---

## English

A way to find where on the cable the driver resets (the cause of #17536) happen. This firmware does not stop on a reset; it writes a line to log.txt, so you can count resets while wiggling different parts of the cable.

### Preparation

1. Keep a USB stick inserted and turn on **Save Logs To File** (it is reset at power-off).
2. Pick the tool under test and wait at least 5 s (detection starts about 1 s after the pick).
3. Set its nozzle to **170 °C** and wait until it is reached (so that heater current is flowing).

### Markers

The log time is not wall-clock time, so **change the nozzle target temperature by 1 °C for each section.** Every change leaves a line such as `Set hotend temperature: 171` in the log.

| Target | Section | What to do (about 60 s each) |
|---|---|---|
| 171 °C | A | do not touch |
| 172 °C | B | gently wiggle the connector at the toolhead |
| 173 °C | C | bend the middle of the cable (the flexing part) |
| 174 °C | D | wiggle the cable where it leaves the electronics box |
| 175 °C | E | do not touch |
| 0 °C | — | end |

- **After changing the temperature, wait about 10 s before you start wiggling.** The toolhead log buffer is only 256 bytes and lines arriving while it is full are dropped, so marker lines can be lost while resets keep coming.
- The nozzle is hot. Wiggle gently.

### Counting

```
python3 tools/wiggle_sections.py log.txt 1     # 1 = tool number
```

It prints the resets per section. Resets are counted from the cumulative counter `n` in the lines, not from the number of lines (at most one line per second is written; the rest are counted in `skip`).

### Result on the author's printer (tool 1)

| Section | Length | Resets |
|---|---|---|
| right after picking the tool (heating) | about 10 s | 6 |
| B connector at the toolhead | 50 s | **72** |
| C middle of the cable | 57 s | 0 |
| D exit of the electronics box | 118 s | 0 |
| E not touched | 70 s | 0 |

→ The contact at the toolhead connector was the cause. After a cover that clamps the cable and the connector was installed, the same test (about 9 minutes, including the pick and the wiggling) showed **0** resets.
