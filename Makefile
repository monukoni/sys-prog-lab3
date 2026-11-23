CC     ?= gcc
CFLAGS ?= -O2 -g -Wall -Wno-unused-function
LDLIBS  = -lm

BUILD = build
OBJS  = $(BUILD)/parser.tab.c $(BUILD)/lex.yy.c src/ast.c src/sema.c src/interp.c src/codegen_js.c src/main.c

minic: $(OBJS) src/ast.h
	$(CC) $(CFLAGS) -Isrc -I$(BUILD) -o $@ $(OBJS) $(LDLIBS)

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/parser.tab.c $(BUILD)/parser.tab.h: src/parser.y | $(BUILD)
	bison -t --report=state,solved -d -o $(BUILD)/parser.tab.c src/parser.y

$(BUILD)/lex.yy.c: src/lexer.l $(BUILD)/parser.tab.h | $(BUILD)
	flex -o $@ src/lexer.l

test: minic
	sh tests/run_tests.sh

clean:
	rm -rf $(BUILD) out minic

.PHONY: test clean
