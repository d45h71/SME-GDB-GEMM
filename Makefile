.DEFAULT_GOAL := help
SHELL := /bin/sh
PYTHON ?= python3
BUILD_DIR ?= build
OUTPUT_DIR ?= results
CONFIG ?= config/runtime.json
export PYTHONDONTWRITEBYTECODE := 1
RUN = $(PYTHON) -B tools/run.py --build-dir "$(BUILD_DIR)" --output-dir "$(OUTPUT_DIR)" --config "$(CONFIG)"

.PHONY: help setup build clean check inventory build-matrix build-stock build-first build-second build-accelerate build-libxsmm build-kleidiai build-openblas build-compute build-transfer build-ablation qualify-harness qualify-fp32-row-stock calibrate-fp32-row-stock qualify-fp32-row-second calibrate-fp32-row-second qualify-fp32-row-first calibrate-fp32-row-first qualify-fp32-row-accelerate calibrate-fp32-row-accelerate qualify-fp32-row-libxsmm calibrate-fp32-row-libxsmm qualify-fp32-row-kleidiai calibrate-fp32-row-kleidiai qualify-fp32-row-openblas calibrate-fp32-row-openblas qualify-fp32-column-stock calibrate-fp32-column-stock qualify-fp32-column-accelerate calibrate-fp32-column-accelerate qualify-fp32-column-libxsmm calibrate-fp32-column-libxsmm qualify-fp32-prepared-row-libxsmm calibrate-fp32-prepared-row-libxsmm qualify-fp32-prepared-column-libxsmm calibrate-fp32-prepared-column-libxsmm qualify-fp32-parallel-row-stock calibrate-fp32-parallel-row-stock qualify-fp32-parallel-row-accelerate calibrate-fp32-parallel-row-accelerate qualify-fp32-parallel-column-stock calibrate-fp32-parallel-column-stock qualify-fp32-parallel-column-accelerate calibrate-fp32-parallel-column-accelerate qualify-fp64-serial-stock calibrate-fp64-serial-stock qualify-fp64-serial-accelerate calibrate-fp64-serial-accelerate qualify-fp64-parallel-stock calibrate-fp64-parallel-stock qualify-fp64-parallel-accelerate calibrate-fp64-parallel-accelerate qualify-fp16-serial-stock calibrate-fp16-serial-stock qualify-fp16-parallel-stock calibrate-fp16-parallel-stock qualify-int8-serial-stock calibrate-int8-serial-stock qualify-int8-parallel-stock calibrate-int8-parallel-stock qualify-bf16-serial-kleidiai calibrate-bf16-serial-kleidiai qualify-ablation calibrate-ablation measure-ablation qualify-compute-int8 calibrate-compute-int8 measure-compute-int8 qualify-compute-int16 calibrate-compute-int16 measure-compute-int16 qualify-compute-int32 calibrate-compute-int32 measure-compute-int32 qualify-compute-fp16 calibrate-compute-fp16 measure-compute-fp16 qualify-compute-bf16 calibrate-compute-bf16 measure-compute-bf16 qualify-compute-fp32 calibrate-compute-fp32 measure-compute-fp32 qualify-compute-fp64 calibrate-compute-fp64 measure-compute-fp64 qualify-transfer calibrate-transfer measure-fp32 measure-fp64 measure-mixed measure-transfer analyze-comparisons analyze-irregular analyze-provenance analyze-efficiency analyze-ablation

help:
	@$(PYTHON) -B tools/help.py
inventory:
	@$(PYTHON) -B tools/help.py --inventory
setup:
	@$(PYTHON) -B tools/setup.py
build:
	@$(PYTHON) -B tools/build.py --build-dir "$(BUILD_DIR)" all
build-matrix:
	@$(PYTHON) -B tools/build.py --build-dir "$(BUILD_DIR)" matrix
build-stock:
	@$(PYTHON) -B tools/build.py --build-dir "$(BUILD_DIR)" stock
build-first:
	@$(PYTHON) -B tools/build.py --build-dir "$(BUILD_DIR)" first
build-second:
	@$(PYTHON) -B tools/build.py --build-dir "$(BUILD_DIR)" second
build-accelerate:
	@$(PYTHON) -B tools/build.py --build-dir "$(BUILD_DIR)" accelerate
build-libxsmm:
	@$(PYTHON) -B tools/build.py --build-dir "$(BUILD_DIR)" libxsmm
build-kleidiai:
	@$(PYTHON) -B tools/build.py --build-dir "$(BUILD_DIR)" kleidiai
build-openblas:
	@$(PYTHON) -B tools/build.py --build-dir "$(BUILD_DIR)" openblas
build-compute:
	@$(PYTHON) -B tools/build.py --build-dir "$(BUILD_DIR)" compute
build-transfer:
	@$(PYTHON) -B tools/build.py --build-dir "$(BUILD_DIR)" transfer
build-ablation:
	@$(PYTHON) -B tools/build.py --build-dir "$(BUILD_DIR)" ablation
qualify-harness:
	@$(RUN) qualification harness
qualify-fp32-row-stock:
	@$(RUN) qualification fp32-row-stock
calibrate-fp32-row-stock:
	@$(RUN) calibration fp32-row-stock
qualify-fp32-row-second:
	@$(RUN) qualification fp32-row-second
calibrate-fp32-row-second:
	@$(RUN) calibration fp32-row-second
qualify-fp32-row-first:
	@$(RUN) qualification fp32-row-first
calibrate-fp32-row-first:
	@$(RUN) calibration fp32-row-first
qualify-fp32-row-accelerate:
	@$(RUN) qualification fp32-row-accelerate
calibrate-fp32-row-accelerate:
	@$(RUN) calibration fp32-row-accelerate
qualify-fp32-row-libxsmm:
	@$(RUN) qualification fp32-row-libxsmm
calibrate-fp32-row-libxsmm:
	@$(RUN) calibration fp32-row-libxsmm
qualify-fp32-row-kleidiai:
	@$(RUN) qualification fp32-row-kleidiai
calibrate-fp32-row-kleidiai:
	@$(RUN) calibration fp32-row-kleidiai
qualify-fp32-row-openblas:
	@$(RUN) qualification fp32-row-openblas
calibrate-fp32-row-openblas:
	@$(RUN) calibration fp32-row-openblas
qualify-fp32-column-stock:
	@$(RUN) qualification fp32-column-stock
calibrate-fp32-column-stock:
	@$(RUN) calibration fp32-column-stock
qualify-fp32-column-accelerate:
	@$(RUN) qualification fp32-column-accelerate
calibrate-fp32-column-accelerate:
	@$(RUN) calibration fp32-column-accelerate
qualify-fp32-column-libxsmm:
	@$(RUN) qualification fp32-column-libxsmm
calibrate-fp32-column-libxsmm:
	@$(RUN) calibration fp32-column-libxsmm
qualify-fp32-prepared-row-libxsmm:
	@$(RUN) qualification fp32-prepared-row-libxsmm
calibrate-fp32-prepared-row-libxsmm:
	@$(RUN) calibration fp32-prepared-row-libxsmm
qualify-fp32-prepared-column-libxsmm:
	@$(RUN) qualification fp32-prepared-column-libxsmm
calibrate-fp32-prepared-column-libxsmm:
	@$(RUN) calibration fp32-prepared-column-libxsmm
qualify-fp32-parallel-row-stock:
	@$(RUN) qualification fp32-parallel-row-stock
calibrate-fp32-parallel-row-stock:
	@$(RUN) calibration fp32-parallel-row-stock
qualify-fp32-parallel-row-accelerate:
	@$(RUN) qualification fp32-parallel-row-accelerate
calibrate-fp32-parallel-row-accelerate:
	@$(RUN) calibration fp32-parallel-row-accelerate
qualify-fp32-parallel-column-stock:
	@$(RUN) qualification fp32-parallel-column-stock
calibrate-fp32-parallel-column-stock:
	@$(RUN) calibration fp32-parallel-column-stock
qualify-fp32-parallel-column-accelerate:
	@$(RUN) qualification fp32-parallel-column-accelerate
calibrate-fp32-parallel-column-accelerate:
	@$(RUN) calibration fp32-parallel-column-accelerate
qualify-fp64-serial-stock:
	@$(RUN) qualification fp64-serial-stock
calibrate-fp64-serial-stock:
	@$(RUN) calibration fp64-serial-stock
qualify-fp64-serial-accelerate:
	@$(RUN) qualification fp64-serial-accelerate
calibrate-fp64-serial-accelerate:
	@$(RUN) calibration fp64-serial-accelerate
qualify-fp64-parallel-stock:
	@$(RUN) qualification fp64-parallel-stock
calibrate-fp64-parallel-stock:
	@$(RUN) calibration fp64-parallel-stock
qualify-fp64-parallel-accelerate:
	@$(RUN) qualification fp64-parallel-accelerate
calibrate-fp64-parallel-accelerate:
	@$(RUN) calibration fp64-parallel-accelerate
qualify-fp16-serial-stock:
	@$(RUN) qualification fp16-serial-stock
calibrate-fp16-serial-stock:
	@$(RUN) calibration fp16-serial-stock
qualify-fp16-parallel-stock:
	@$(RUN) qualification fp16-parallel-stock
calibrate-fp16-parallel-stock:
	@$(RUN) calibration fp16-parallel-stock
qualify-int8-serial-stock:
	@$(RUN) qualification int8-serial-stock
calibrate-int8-serial-stock:
	@$(RUN) calibration int8-serial-stock
qualify-int8-parallel-stock:
	@$(RUN) qualification int8-parallel-stock
calibrate-int8-parallel-stock:
	@$(RUN) calibration int8-parallel-stock
qualify-bf16-serial-kleidiai:
	@$(RUN) qualification bf16-serial-kleidiai
calibrate-bf16-serial-kleidiai:
	@$(RUN) calibration bf16-serial-kleidiai
qualify-ablation:
	@$(RUN) qualification ablation
calibrate-ablation:
	@$(RUN) calibration ablation
measure-ablation:
	@$(RUN) measurement ablation
qualify-compute-int8:
	@$(RUN) qualification compute-int8
calibrate-compute-int8:
	@$(RUN) calibration compute-int8
measure-compute-int8:
	@$(RUN) measurement compute-int8
qualify-compute-int16:
	@$(RUN) qualification compute-int16
calibrate-compute-int16:
	@$(RUN) calibration compute-int16
measure-compute-int16:
	@$(RUN) measurement compute-int16
qualify-compute-int32:
	@$(RUN) qualification compute-int32
calibrate-compute-int32:
	@$(RUN) calibration compute-int32
measure-compute-int32:
	@$(RUN) measurement compute-int32
qualify-compute-fp16:
	@$(RUN) qualification compute-fp16
calibrate-compute-fp16:
	@$(RUN) calibration compute-fp16
measure-compute-fp16:
	@$(RUN) measurement compute-fp16
qualify-compute-bf16:
	@$(RUN) qualification compute-bf16
calibrate-compute-bf16:
	@$(RUN) calibration compute-bf16
measure-compute-bf16:
	@$(RUN) measurement compute-bf16
qualify-compute-fp32:
	@$(RUN) qualification compute-fp32
calibrate-compute-fp32:
	@$(RUN) calibration compute-fp32
measure-compute-fp32:
	@$(RUN) measurement compute-fp32
qualify-compute-fp64:
	@$(RUN) qualification compute-fp64
calibrate-compute-fp64:
	@$(RUN) calibration compute-fp64
measure-compute-fp64:
	@$(RUN) measurement compute-fp64
qualify-transfer:
	@$(RUN) qualification transfer
calibrate-transfer:
	@$(RUN) calibration transfer
measure-fp32:
	@$(RUN) measurement fp32
measure-fp64:
	@$(RUN) measurement fp64
measure-mixed:
	@$(RUN) measurement mixed
measure-transfer:
	@$(RUN) measurement transfer-all
analyze-comparisons:
	@$(PYTHON) -B tools/analyze.py comparisons --build-dir "$(BUILD_DIR)" --output-dir "$(OUTPUT_DIR)" --config "$(CONFIG)"
analyze-irregular:
	@$(PYTHON) -B tools/analyze.py irregular --build-dir "$(BUILD_DIR)" --output-dir "$(OUTPUT_DIR)" --config "$(CONFIG)"
analyze-provenance:
	@$(PYTHON) -B tools/analyze.py provenance --build-dir "$(BUILD_DIR)" --output-dir "$(OUTPUT_DIR)" --config "$(CONFIG)"
analyze-efficiency:
	@$(PYTHON) -B tools/analyze.py efficiency --build-dir "$(BUILD_DIR)" --output-dir "$(OUTPUT_DIR)" --config "$(CONFIG)"
analyze-ablation:
	@$(PYTHON) -B tools/analyze.py ablation --build-dir "$(BUILD_DIR)" --output-dir "$(OUTPUT_DIR)" --config "$(CONFIG)"
check:
	@$(PYTHON) -B tools/check.py
clean:
	@$(PYTHON) -B tools/clean.py --build-dir "$(BUILD_DIR)" --output-dir "$(OUTPUT_DIR)"

.PHONY: measure-fp32-row
measure-fp32-row:
	@$(RUN) measurement fp32-row

.PHONY: measure-fp32-column
measure-fp32-column:
	@$(RUN) measurement fp32-column

.PHONY: measure-fp32-prepared
measure-fp32-prepared:
	@$(RUN) measurement fp32-prepared

.PHONY: measure-fp32-parallel-row
measure-fp32-parallel-row:
	@$(RUN) measurement fp32-parallel-row

.PHONY: measure-fp32-parallel-column
measure-fp32-parallel-column:
	@$(RUN) measurement fp32-parallel-column

.PHONY: measure-fp64-serial
measure-fp64-serial:
	@$(RUN) measurement fp64-serial

.PHONY: measure-fp64-parallel
measure-fp64-parallel:
	@$(RUN) measurement fp64-parallel

.PHONY: measure-mixed-serial
measure-mixed-serial:
	@$(RUN) measurement mixed-serial

.PHONY: measure-mixed-parallel
measure-mixed-parallel:
	@$(RUN) measurement mixed-parallel
