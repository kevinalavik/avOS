From repo root:

unzip -o avOS_input_fixed_dropin.zip -d .
find . -name '*.rej' -delete
find . -name '*.orig' -delete
make clean
make image

This replaces the broken partial input patch with a coherent keyboard-state polling path.
