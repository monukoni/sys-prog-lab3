#!/bin/sh
cd "$(dirname "$0")/.." || exit 1
mkdir -p out
fail=0

for src in examples/*.c; do
    name=$(basename "$src" .c)
    gcc -w -o "out/$name.bin" "$src" -lm || { echo "FAIL $name: gcc"; fail=1; continue; }
    "./out/$name.bin" > "out/$name.expected"

    ./minic "$src" > "out/$name.interp" 2>&1
    ./minic -o "out/$name.js" "$src" && node "out/$name.js" > "out/$name.node" 2>&1

    if ! cmp -s "out/$name.expected" "out/$name.interp"; then
        echo "FAIL $name: інтерпретатор"; diff "out/$name.expected" "out/$name.interp" | head; fail=1
    elif ! cmp -s "out/$name.expected" "out/$name.node"; then
        echo "FAIL $name: js"; diff "out/$name.expected" "out/$name.node" | head; fail=1
    else
        echo "ok   $name"
    fi
done

for src in tests/errors/*.c; do
    name=$(basename "$src" .c)
    expected=$(sed -n 's|^// очікується: ||p' "$src")
    actual=$(./minic "$src" 2>&1 >/dev/null)
    code=$?
    if [ $code -ne 1 ] || ! printf '%s' "$actual" | grep -qF "$expected"; then
        echo "FAIL error/$name: код $code"; echo "$actual"; fail=1
    else
        echo "ok   error/$name"
    fi
done

exit $fail
