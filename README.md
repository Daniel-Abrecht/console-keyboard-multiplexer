# libconsolekeyboard

This library implements the console-keyboard-multiplexer protocol.
Using this library, a program can control the console-keyboard-multiplexer
program to some extent, including sending keypresses to it. This
program is for simplifying creating console keyboards.

## API Methods

### lck_send_cmd
```
int lck_send_cmd(enum lck_cmd cmd, size_t size, uint8_t data[size]);
```
This function is for sending commands to the console-keyboard-multiplexer program manually.

### lck_send_key
```
int lck_send_key(const char* key);
```

This function is for sending special keys such as `PAGE_UP`, `PAGE_DOWN`, `ENTER`,
etc. to the console-keyboard-multiplexer program. See the `libttymultiplex` documentation
for a list of currently supportet special keys.

### lck_send_string
```
int lck_send_string(const char* key);
```

Send multiple charcters to the console-keyboard-multiplexer. Please note that
sending control characters is not recommended.

### lck_set_height
```
int lck_set_height(struct lck_super_size size);
```

Set the height of the console-keyboard. This won't take effect immediately, and
there is no garantee that the requested size can be set. Make sure to handle the
WINCH signal properly. The size is specified in multiple units and the sum of
all of them is used to determine the final size. At the moment, only the size in
characters can be specified, but this may be extended with other unit such as
the size in pixels in the future.

## Constants & command numbers

| CMD  | CONSTANT        |
|------|-----------------|
| 0x01 | LCK_SEND_KEY    |
| 0x02 | LCK_SEND_STRING |
| 0x03 | LCK_SET_HEIGHT  |

## Protocol

By convention, a pipe for sending commands to the console-keyboard-multiplexer
is opened on file descriptor 3. Receiving data from the console-keyboard-multiplexer
isn't yet possible, but the protocol may be extended in the future in a backward
compatible way if a need for this arrises.

Whenever an integer is sent, it is encoded in big endian.

Every command to be sent starts with 1 byte specifying the length of the remaining
command. A length of zero is a noop command. The next byte is the command number.
At the moment, invalid command numbers are ignored. The remaining bytes are command speciffc.
The commands not explicitly documented yust take a string or some other sequence
of bytes, see the API Method description to see what those should contain.

### CMD 0x03 - LCK_SET_HEIGHT

This command contains the following data:
 - uint64_t - desired keyboard height in characters

Additional data are currently ignored, but may be interpreted as an additional
height specification in future versions.
