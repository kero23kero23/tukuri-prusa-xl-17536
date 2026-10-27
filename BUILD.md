# ビルドと書き込み / Build and flash

[日本語](#日本語) | [English](#english)

---

## 日本語

ビルド済みのファイル（.bbf）は配布していません。公式のソースに差分を当てて、自分でビルドしてください。

### 1. 用意するもの

- Prusa-Firmware-Buddy の **v6.2.6**（タグ）。ほかの版には当てられません
- Python **3.11**（v6.2.6 は pillow 9.5 系を指定しているため。新しい Python ではビルド用の部品が入らないことがあります）
- git、ネット接続（初回のビルドで、コンパイラなどが自動でダウンロードされます）
- 作者は Linux（Python 3.11、arm-none-eabi-gcc 13.2.1）でビルドしました。ほかの OS は公式の README に従ってください

### 2. ソースを取り出して差分を当てる

```
git clone --branch v6.2.6 --depth 1 https://github.com/prusa3d/Prusa-Firmware-Buddy.git
cd Prusa-Firmware-Buddy
git apply /path/to/tukuri-prusa-xl-17536/patches/tkr17536_v6.2.6.patch
```

変わるのは `src/puppy/dwarf/modbus/ModbusControl.cpp` の1ファイルだけです（ツールヘッド側）。

### 3. ビルド

```
python utils/build.py --preset xl --build-type release --bootloader yes \
  --version-suffix="-tkr1+8948" --version-suffix-short="-tkr1" \
  -D BUILD_NUMBER:STRING=8948 -D DEVELOPMENT_ITEMS_ENABLED:BOOL=NO
```

- `DEVELOPMENT_ITEMS_ENABLED=NO` を付けてください（ソースの既定はオン。公式のリリースと同じにするため）。
- できたファイル: `build/products/xl_release_boot_6.2.6-tkr1+8948.bbf`
- **（任意）モジュラーベッドは公式のまま使う:** 公式 v6.2.6 の XL 用 .bbf からベッド用のファームを取り出し、ビルドに渡します。こうすると、書き込み後に書き換わるのはツールヘッドだけになります。

```
pip install littlefs-python
python /path/to/tukuri-prusa-xl-17536/tools/extract_modularbed.py XL_firmware_6.2.6.bbf fw-modularbed.bin
# 上のビルドのコマンドに追加:
  -D MODULARBED_BINARY_PATH:FILEPATH=/path/to/fw-modularbed.bin
```

作者のビルドはこの指定ありです。公式 6.2.6 から取り出したベッド用ファームの SHA-256 は `49659a222e1beb27a96274ebe24cc97e51dbcd86f24f1f52f14af106ca108b9b` でした。

### 4. 書き込み

1. USB メモリのルートに、できた .bbf を**1個だけ**置きます（ほかの .bbf は消す）。
2. USB メモリを挿してリセットし、Prusa のロゴが出たらノブを押して、「FLASH」を選びます。
3. Prusa の署名がないので、**#17606 の警告**が出ます。書き込むには IGNORE を選びます。
4. 起動するとき、メイン基板がツールヘッド（5本）へ新しいファームを書き込みます。終わるまで電源を切らないでください。
5. 書き込んだら USB メモリから .bbf を消します。
6. Info → Version Info に `6.2.6-tkr1+8948` と出れば完了です。

- 署名のないファームを書き込むには、メイン基板（xLBuddy）の **appendix を折る**必要がありました（作者の機械。元に戻せません）。扱いは Prusa の案内を確認してください。
- 戻すときは、公式の .bbf を同じ手順で書き込みます。ツールヘッドも公式に戻ります。

### 5. 模擬試験（PC 上・任意）

改変部分を、モックを使って PC 上で動かします（20項目）。

```
python3 test/extract_block.py /path/to/Prusa-Firmware-Buddy/src/puppy/dwarf/modbus/ModbusControl.cpp > test/patched_block.inc
g++ -std=c++17 -O1 -Wall -Wextra -o test/hosttest test/tkr17536_hosttest.cpp
./test/hosttest > result.txt
diff result.txt test/expected_result.txt   # 差がなければ合格（最後の行は ALL PASS）
```

---

## English

No prebuilt firmware (.bbf) is distributed. Apply the patch to the official source and build it yourself.

### 1. Requirements

- Prusa-Firmware-Buddy tag **v6.2.6**. The patch does not apply to other versions
- Python **3.11** (v6.2.6 pins pillow 9.5; newer Python versions may fail to install the build requirements)
- git and an internet connection (the first build downloads the compiler and tools)
- The author built on Linux (Python 3.11, arm-none-eabi-gcc 13.2.1). For other systems follow the official README

### 2. Get the source and apply the patch

```
git clone --branch v6.2.6 --depth 1 https://github.com/prusa3d/Prusa-Firmware-Buddy.git
cd Prusa-Firmware-Buddy
git apply /path/to/tukuri-prusa-xl-17536/patches/tkr17536_v6.2.6.patch
```

Only one file changes: `src/puppy/dwarf/modbus/ModbusControl.cpp` (toolhead side).

### 3. Build

```
python utils/build.py --preset xl --build-type release --bootloader yes \
  --version-suffix="-tkr1+8948" --version-suffix-short="-tkr1" \
  -D BUILD_NUMBER:STRING=8948 -D DEVELOPMENT_ITEMS_ENABLED:BOOL=NO
```

- Keep `DEVELOPMENT_ITEMS_ENABLED=NO` (the source default is on; official releases are built without it).
- Output: `build/products/xl_release_boot_6.2.6-tkr1+8948.bbf`
- **(Optional) keep the official modular bed firmware:** extract it from the official v6.2.6 XL .bbf and pass it to the build, so that only the toolheads are re-flashed.

```
pip install littlefs-python
python /path/to/tukuri-prusa-xl-17536/tools/extract_modularbed.py XL_firmware_6.2.6.bbf fw-modularbed.bin
# add to the build command above:
  -D MODULARBED_BINARY_PATH:FILEPATH=/path/to/fw-modularbed.bin
```

The author's build used this option. The bed firmware extracted from official 6.2.6 has SHA-256 `49659a222e1beb27a96274ebe24cc97e51dbcd86f24f1f52f14af106ca108b9b`.

### 4. Flash

1. Put **only** the new .bbf in the root of a USB stick (remove other .bbf files).
2. Insert the stick, reset the printer, press the knob while the Prusa logo is shown, and select "FLASH".
3. The firmware is not signed by Prusa, so **warning #17606** appears. Select IGNORE to flash.
4. On start-up the main board writes the new firmware to the toolheads (all five). Do not switch off until it finishes.
5. Remove the .bbf from the USB stick.
6. Info → Version Info shows `6.2.6-tkr1+8948`.

- Flashing unsigned firmware required **breaking the appendix** of the main board (xLBuddy) on the author's printer. This cannot be undone. Check Prusa's documentation before you do it.
- To go back, flash the official .bbf the same way. The toolheads return to the official firmware as well.

### 5. Host test (on a PC, optional)

Runs the patched code against mocks on a PC (20 cases).

```
python3 test/extract_block.py /path/to/Prusa-Firmware-Buddy/src/puppy/dwarf/modbus/ModbusControl.cpp > test/patched_block.inc
g++ -std=c++17 -O1 -Wall -Wextra -o test/hosttest test/tkr17536_hosttest.cpp
./test/hosttest > result.txt
diff result.txt test/expected_result.txt   # no difference = pass (the last line is ALL PASS)
```
