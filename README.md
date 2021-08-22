# win-remote-control

This is a small Windows program that sits on the systray accepting network connections on TCP port `5555`.
Incoming connections with key names cause keyboard events to be sent to Windows.

[Netcat](https://en.wikipedia.org/wiki/Netcat) is an easy way to do send keys. If the IP address of
your Windows PC is `192.179.1.20`, running

    echo "ALT+F4" | nc 192.168.1.20 5555

somewhere on your network will tell `win-remote-control` to send `ALT+F4` to the active window. Key
sequences are supported too, so if you leave a `notepad` window focused and

    echo "SHIFT+H E L L O COMMA SPACE W O R L D SHIFT+1" | nc 192.168.1.20 5555

you'll see `Hello, world!` appear.

Single keys are sent with alternating press and release inputs, so sending "`A B`" will generate the sequence:
- press `A`
- release `A`
- press `B`
- release `B`

Key combinations (i.e., keys joined by `+`) are all pressed one after another and then released in the reverse order:
sending "`ALT+F4 ENTER`" will generate the sequence
- press `ALT`
- press `F4`
- release `F4`
- release `ALT`
- press `ENTER`
- release `ENTER`

## Compilation

To compile under MinGW-w64, just

    $ git clone https://github.com/moefh/win-remote-control
    $ cd win-remote-control
    $ make

## Limitations

- TCP port `5555` is hard-coded.
- There's a limit of 50 keys on a connection (multiple connections must be used to send more keys).
- Some key names (like "`COLON`") are specific to US keyboards and may have different effects if Windows has the keyboard configured for a different language.
- No attempt at protecting from malicious or ill-behaved network clients is made. A client that connects and doesn't send any data will stop all incoming connections.
