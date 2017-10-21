Wakatiwai
===

Wakatiwai is a revised version of Eclipse Wakaama, providing IPC I/F via stdin and stdout.

Notice! The IPC communication is NOT encrypted and doesn't contain any authentication mechanism between processes.

## IPC Message Format

See comments in `object_generic.c`.

## How to build

This project requires the following tools.

1. [GYP](https://github.com/mogemimi/pomdog/wiki/How-to-Install-GYP)
1. [Ninja](https://github.com/ninja-build/ninja/wiki/Pre-built-Ninja-packages)

Setup the above tools on on your environment.

**Windows is not supported**

Then, run the following command.

```
$ make
```

And you can get `wakatiwai` executable file under `build` directory.

## License

Copyright (c) 2017 CANDY LINE INC.

- [Eclipse Public License v2.0](https://www.eclipse.org/legal/epl-2.0/)
- [Eclipse Distribution License v1.0](https://www.eclipse.org/org/documents/edl-v10.php)
