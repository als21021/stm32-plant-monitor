# 호스트(PC) 단위 테스트. CubeIDE 빌드에는 포함되지 않음 (.cproject 소스 경로는 Core, Drivers만)
# 실행: make -C test

CC      ?= cc
CFLAGS  := -std=c11 -Wall -Wextra -Werror -g -I../Core/Inc
BUILD   := build

.PHONY: all clean

all: $(BUILD)/test_watering
	./$(BUILD)/test_watering

$(BUILD)/test_watering: test_watering.c ../Core/Src/watering.c ../Core/Inc/watering.h ../Core/Inc/pump.h ../Core/Inc/config.h | $(BUILD)
	$(CC) $(CFLAGS) -o $@ test_watering.c ../Core/Src/watering.c

$(BUILD):
	mkdir -p $(BUILD)

clean:
	rm -rf $(BUILD)
