symbol-file ./build/debug_bin/kernel.elf
target remote | qemu-system-x86_64 -hda ./build/debug_bin/image.hdd -S -gdb stdio -display none \
    -serial file:./build/KernLog.txt -smp 2 -machine q35 -m 2G

layout src

set tui tab-width 4
focus cmd

define q
	kill inferiors 1
	quit
end
