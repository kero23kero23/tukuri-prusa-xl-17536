# tukuri-prusa-xl-17536

Original Prusa XL のエラー **#17536「Extruder motor is not spinning」** を、ツールヘッド側のファームで「止めずに復帰させる」改変と、その原因をログで突き止めた記録です。
A toolhead firmware patch that recovers from error #17536 on the Original Prusa XL instead of stopping the print, and the measurements that located the cause.

[日本語](#日本語) | [English](#english)

> **AI の利用 / Use of AI:** 調査・コード・文書は AI（Anthropic の Claude）が担当し、判断と実機での検証は有限会社丹羽電機（つくりこむ.jp）が行いました。
> Investigation, code and documents: AI (Anthropic Claude). Decisions and verification on a real printer: Niwa Denki (つくりこむ.jp, Japan).

---

## 日本語

### 先に読んでください

- **非公式の改変です。** Prusa Research とは関係ありません。無保証です（GPL-3.0）。
- **このファームは、ドライバのリセットでは印刷を止めません。** 接点の接触不良などの原因は残ったままです。根本の対策は、ケーブルとコネクタの固定（カバー）や端子の手直しです。
- **log.txt でリセットの件数を見てください**（`tools/count_17536.py`）。件数が多い、または増えていくときは、接点を直してください。24V のピンの変色（発熱）も点検してください。
- 過熱（DRV_STATUS の ot）と、短絡保護（drv_err）の繰り返しでは、従来どおり #17536 で止めます。
- ビルド済みのファイル（.bbf）は置いていません。ソースからビルドしてください（[BUILD.md](BUILD.md)）。対象は **v6.2.6 だけ**です。ほかの版への移植や、個別のサポートは行いません。
- 署名のないファームを書き込むには、メイン基板（xLBuddy）の appendix を折る必要がありました（元に戻せません）。扱いは Prusa の案内を確認してください。
- これは、**自社の機械でここまで計測・診断した事例**です。客先の装置に、このような改変はしません。

### 何が起きていたか（作者の機械）

- 印刷中に、使っているツールで #17536 の赤画面になり、印刷が止まる。複数のツールで起きていた。
- エラー #17536 は、ツールヘッド（Dwarf）のモータドライバ TMC2130 が自分の状態（GSTAT: reset / drv_err / uv_cp）を報告すると出る。公式のファームは、1回で印刷を止める。
- このファームで「止めずに記録」し、ケーブルの場所ごとに揺すって件数を数えた（[docs/wiggle_test.md](docs/wiggle_test.md)）。**ツールヘッド側のコネクタを揺すった50秒だけで72回**、ケーブルの中ほどと電子ボックスの出口は0回だった。
- ツールヘッド側のコネクタの接触が原因と判断した。Prusa のヘルプ記事も、原因を Dwarf とケーブルのコネクタの間の抵抗の増加としている（[#17536 の記事](https://help.prusa3d.com/ja/article/dwarf-tmc-error-17536-xl_822367)）。
- ケーブルとコネクタを固定するカバーに替えた（Prusa の新しいカバー（[交換ガイド](https://help.prusa3d.com/ja/guide/how-to-replace-the-main-cable-connector-cover-xl_831370)）を元にした、Spengilang 氏の改良版（[Printables](https://www.printables.com/model/1172015-prusa-xl-dwarf-cover-and-cable-mount-threaded-inse)））。替えた後は、同じ揺すり試験で約9分間リセット0回。

### 改変の中身

変えたのはツールヘッド側の1ファイル（`src/puppy/dwarf/modbus/ModbusControl.cpp`）だけです。メイン基板側は変えていません。

1. GSTAT の読み値に、ありえないビット（bit0〜2 以外）が立っていたら、もう一度読む。また変なら、読み取りの化けとして無視する。
2. 正しい報告（reset・uv_cp・drv_err）では、ドライバをいったん止め（TOFF=0）、`TMC2130Stepper::push()` で設定を書き戻して、すぐに復帰させる。reset と uv_cp では、何回起きても印刷を止めない。
3. メイン基板（xLBuddy）が Modbus 経由で書いた設定（電流 450mA など13項目）を覚えておき、push() の後に書き戻す。push() だけでは、ツールヘッド側の既定値（400mA）に戻ってしまうため。
4. drv_err（過熱・短絡による停止）は、1回では止めない。起きるたびに +1、起きない1秒ごとに −1 のカウンタが10に達したら、従来どおり #17536 で止める。DRV_STATUS の過熱ビットは、すぐ止める（Marlin の `monitor_tmc_driver()` と同じ考え方）。
5. 起きるたびに、log.txt に1行書く（1秒に1行まで）。

```
tkr17536 GSTAT=0x01 DRV=0x00000000 24V=23950mV n=1 skip=0 de=0 r=13
```

| 項目 | 意味 |
|---|---|
| GSTAT | ドライバの報告。0x01 = reset、0x02 = drv_err、0x04 = uv_cp |
| DRV | DRV_STATUS の読み値 |
| 24V | ツールヘッドで測った 24V（平均化された値。瞬間の落ち込みは映らない） |
| n | ツールヘッドが起動してからの累計件数 |
| skip | 前の行から後、行を書かずに数えた件数 |
| de | drv_err のカウンタ（10で停止） |
| r | 書き戻したメイン基板の設定の数（通常は13） |

### 検証

| 内容 | 結果 |
|---|---|
| PC 上の模擬試験（20項目。`test/`） | 全項目合格 |
| 実機（作者の XL、ツール1、揺すり試験） | 78回のリセット（掴んだ直後6回＋コネクタを揺すった区間72回）で、一度も止まらずに復帰した。ログに残った行はすべて r=13（メイン基板の設定13項目を書き戻した）。赤画面なし |
| カバーを替えた後（同じ試験） | リセット0回 |

### 版名

- `tkr` は tukuri（つくりこむ.jp）の略です。公表版は tkr1 から数えます。
- 作者の社内版との対応:

| 公表版 | 社内版 | 関係 |
|---|---|---|
| 6.2.6-tkr1 | 6.2.6-ndk2 | 中身は同じ。違うのは版名とログの目印（社内版は ndk17536）だけ。機械語を比べ、違いが文字列とハッシュだけであることを確かめた。上の実機の結果は社内版で取ったもの。公開の時点では、tkr1 そのものでの実機運転はまだしていない（結果が出たら上の表に追記する） |

### ファイル

| ファイル | 中身 |
|---|---|
| `patches/tkr17536_v6.2.6.patch` | Prusa-Firmware-Buddy v6.2.6 への差分 |
| `BUILD.md` | ビルドと書き込みの手順 |
| `test/` | PC 上の模擬試験と、その期待結果 |
| `tools/count_17536.py` | log.txt から、ツールごとのリセットの件数を数える |
| `tools/wiggle_sections.py` | 揺すり試験の区間ごとに件数を数える |
| `tools/extract_modularbed.py` | 公式 .bbf からベッド用のファームを取り出す（任意） |
| `docs/wiggle_test.md` | ケーブル揺すり試験のやり方 |

### ライセンス・商標

- GPL-3.0（公式のファームのソースと同じ）。[LICENSE](LICENSE)
- 公式のファームの画像・デザイン（CC BY-NC-SA 4.0）は含みません。
- Original Prusa XL は Prusa Research の登録商標です。

### 作成

有限会社丹羽電機（愛知県）／つくりこむ.jp — <https://www.tukuri.com/>

事例記事（経緯と試験の結果）: <https://www.tukuri.com/prusaxl17536.html>

---

## English

### Read this first

- **Unofficial modification.** Not affiliated with Prusa Research. No warranty (GPL-3.0).
- **This firmware does not stop the print on a driver reset.** The cause, such as a poor contact, is still there. The real fix is to secure the cable and connector (cover) or to repair the terminals.
- **Watch the number of resets in log.txt** (`tools/count_17536.py`). If it is high or growing, fix the contact, and check the 24 V pins for discoloration (heat).
- Overtemperature (DRV_STATUS ot) and repeated drv_err (short protection) still stop the print with #17536, as before.
- No prebuilt firmware (.bbf) is provided; build from source ([BUILD.md](BUILD.md)). **v6.2.6 only.** No ports to other versions and no individual support.
- Flashing unsigned firmware required breaking the appendix of the main board (xLBuddy) on the author's printer. This cannot be undone. Check Prusa's documentation.
- This is **a case study of how far we measured and diagnosed our own printer.** We do not make modifications like this to customers' equipment.

### What happened (the author's printer)

- During prints, the active tool stopped with the #17536 red screen. It happened on several tools.
- Error #17536 is raised when the toolhead (Dwarf) motor driver TMC2130 reports its state (GSTAT: reset / drv_err / uv_cp). The official firmware stops the print on the first report.
- With this firmware the events were logged instead, and the cable was wiggled part by part ([docs/wiggle_test.md](docs/wiggle_test.md)). **Wiggling the connector at the toolhead for 50 s gave 72 resets**; the middle of the cable and the exit of the electronics box gave 0.
- The contact at the toolhead connector was the cause. Prusa's help article also names increased resistance between the Dwarf and the cable connector as the cause ([#17536 article](https://help.prusa3d.com/article/extruder-error-17536-xl_822367)).
- A cover that clamps the cable and the connector was installed: Spengilang's modification ([Printables](https://www.printables.com/model/1172015-prusa-xl-dwarf-cover-and-cable-mount-threaded-inse)) of Prusa's updated cover ([replacement guide](https://help.prusa3d.com/guide/how-to-replace-the-main-cable-connector-cover-xl_831370)). After that, the same wiggle test showed 0 resets in about 9 minutes.

### What the patch does

Only one toolhead file changes (`src/puppy/dwarf/modbus/ModbusControl.cpp`). The main board source is unchanged.

1. A GSTAT value with impossible bits (other than bit 0-2) is read again and ignored as a read glitch if it is still invalid.
2. On a valid report (reset, uv_cp, drv_err) the driver is switched off (TOFF=0), the configuration is written back with `TMC2130Stepper::push()`, and the extruder resumes at once. reset and uv_cp never stop the print, however often they occur.
3. The settings the main board (xLBuddy) wrote over Modbus (13 registers, e.g. 450 mA) are remembered and written again after push(). push() alone would restore the toolhead's own defaults (400 mA).
4. drv_err (driver shut down by overtemperature or short) does not stop on the first event. A counter that goes +1 per event and -1 per second without one stops the print with #17536 at 10, as before. The DRV_STATUS overtemperature bit stops at once (the same idea as Marlin's `monitor_tmc_driver()`).
5. Every event writes one line to log.txt (at most one line per second).

```
tkr17536 GSTAT=0x01 DRV=0x00000000 24V=23950mV n=1 skip=0 de=0 r=13
```

| Field | Meaning |
|---|---|
| GSTAT | driver report: 0x01 = reset, 0x02 = drv_err, 0x04 = uv_cp |
| DRV | DRV_STATUS value |
| 24V | 24 V measured on the toolhead (averaged; short dips are not visible) |
| n | events since the toolhead started |
| skip | events counted without a line since the previous line |
| de | drv_err counter (stops at 10) |
| r | number of main board settings written back (normally 13) |

### Verification

| Test | Result |
|---|---|
| Host test on a PC (20 cases, `test/`) | all passed |
| Real printer (author's XL, tool 1, wiggle test) | 78 resets (6 right after the pick + 72 while wiggling the connector), recovered every time without stopping; every logged line shows r=13 (13 main board settings written back); no red screen |
| After the cover was replaced (same test) | 0 resets |

### Version name

- `tkr` stands for tukuri (つくりこむ.jp). Public builds are numbered from tkr1.
- Mapping to the author's internal builds:

| Public | Internal | Relation |
|---|---|---|
| 6.2.6-tkr1 | 6.2.6-ndk2 | Same content. Only the version name and the log marker differ (internal: ndk17536). A binary comparison showed that only strings and hashes differ. The hardware results above were taken with the internal build. At the time of publication, tkr1 itself has not yet been run on the printer (the result will be added to the table above) |

### Files

| File | Content |
|---|---|
| `patches/tkr17536_v6.2.6.patch` | patch for Prusa-Firmware-Buddy v6.2.6 |
| `BUILD.md` | how to build and flash |
| `test/` | host test and its expected output |
| `tools/count_17536.py` | counts resets per tool in log.txt |
| `tools/wiggle_sections.py` | counts resets per section of a wiggle test |
| `tools/extract_modularbed.py` | extracts the bed firmware from an official .bbf (optional) |
| `docs/wiggle_test.md` | how to do the cable wiggle test |

### License and trademarks

- GPL-3.0 (same as the official firmware source). See [LICENSE](LICENSE).
- The graphics and design of the official firmware (CC BY-NC-SA 4.0) are not included.
- Original Prusa XL is a registered trademark of Prusa Research.

### Author

Niwa Denki (有限会社丹羽電機, Aichi, Japan) / つくりこむ.jp — <https://www.tukuri.com/>

Case study (Japanese): <https://www.tukuri.com/prusaxl17536.html>
