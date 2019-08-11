# console-keyboard-multiplexer

The purpose of the console-keyboard-multiplexer is to run a console-keyboard at the bottom of a terminal and run another program in the remaining space above.
This repo doesn't include a console-keyboard, it's just the multiplexer. Look for the console-keyboard-basic for a proof of concept console-keyboard. A
console-keyboard usually also needs the libconsolekeyboard library to interface with the console-keyboard-multiplexer, you can find that library in the
libconsolekeyboard branch of this repo.

## Building the console-keyboard-multiplexer

The console-keyboard-multiplexer needs the libttymultiplex library, make sure to build/install it first.

If you are using a debian or devuan system, you can build packages using "debuild -us -uc -b". It'll create 3 Packages:
 * console-keyboard-multiplexer_0.0.1_amd64.deb - Just the console-keyboard-multiplexer
 * console-keyboard-multiplexer-config_0.0.1_all.deb - Replaces the getty on the VTs with this program. This gives your VTs a keyboard.
 * console-keyboard-multiplexer-initramfs-config_0.0.1_all.deb - This puts the console-keyboard-multiplexer and the current console-keyboard into the initramfs. The console keyboard will then be availeble after te premount stage of the initramfs. This can be useful for unlocking luks partitions, interfacing with the initramfs prompt, and similar interactive things which may need user intervention at early boot.

Alternatively, you can build this project using ```make```.
There are variouse install targets, they handle the same things as the packages above.
The ```install-bin``` target only instally this program.
Other make targets are ```install-config```, ```install-initramfs-tools-config``` and ```install``` (which combines all other install targets.)
If you use the ```install-initramfs-tools-config```, you'll have to regenerate the initramfs yourself afterwards. Make sure to make a backup
before that, messing whith the boot procedure is always a delicate thing to do.

Don't forget to also install a console-keyboard, this is just the multiplexer first after all, and without the keyboard it won't work.

## Usage

Look at the man page for an overview of all parameters and what they do.
You can also view it using the command ```console-keyboard-multiplexer --help```.
