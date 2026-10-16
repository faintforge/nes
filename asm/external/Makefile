OUT_DIR := out

all: $(OUT_DIR)/6502_functional_test.bin $(OUT_DIR)/6502_decimal_test.bin

$(OUT_DIR)/6502_functional_test.bin: 6502_functional_test.ca65 config.cfg
	@mkdir -p $(OUT_DIR)
	ca65 6502_functional_test.ca65 -o $(OUT_DIR)/6502_functional_test.o -l $(OUT_DIR)/6502_functional_test.lst
	ld65 $(OUT_DIR)/6502_functional_test.o -o $(OUT_DIR)/6502_functional_test.bin -m $(OUT_DIR)/6502_functional_test.map -C config.cfg

$(OUT_DIR)/6502_decimal_test.bin: 6502_decimal_test.ca65 config.cfg
	@mkdir -p $(OUT_DIR)
	ca65 6502_decimal_test.ca65 -o $(OUT_DIR)/6502_decimal_test.o -l $(OUT_DIR)/6502_decimal_test.lst
	ld65 $(OUT_DIR)/6502_decimal_test.o -o $(OUT_DIR)/6502_decimal_test.bin -m $(OUT_DIR)/6502_decimal_test.map -C config.cfg
