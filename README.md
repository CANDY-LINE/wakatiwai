Wakatiwai
===

Wakatiwai is a revised version of Eclipse Wakaama LwM2M Client, providing IPC I/F via stdin and stdout.

Notice! The IPC communication is NOT encrypted and doesn't contain any authentication mechanism between processes.

LwM2M DM Server and Bootstrap Server are never modified in this project but you can build them as well as Wakatiwai client.

Server programs are out of Wakatiwai project scope so there are no TODOs for LwM2M Server stuff.

## Major differences between Wakaama LwM2M Client and Wakatiwai

- Strip some of predefined C-based LwM2M object impl. except for Security objects and Server objects
- Redirect all log output to stderr
- Enable DTLS by default
- Add new options for providing Server ID and manageable object IDs

## IPC Message Format

See comments in `object_generic.c`.

## How to build

This project requires the following tools.

1. [GYP](https://github.com/mogemimi/pomdog/wiki/How-to-Install-GYP)
1. [Ninja](https://github.com/ninja-build/ninja/wiki/Pre-built-Ninja-packages)

Setup the above tools on on your environment.

**Windows is not supported**

Clone dependencies.

```
# clone submodules (wakaama and tinydtls)
$ git submodule update --init --recursive
```

Then, run the following command.

```
$ make
```

And you can get `wakatiwaiclient` executable file under `build` directory.

## License

Copyright (c) 2017 [CANDY LINE INC.](https://www.candy-line.io)

- [Eclipse Public License v2.0](https://www.eclipse.org/legal/epl-2.0/)
- [3-Clause BSD License](https://opensource.org/licenses/BSD-3-Clause)

## Appendix

### LwM2M Server

Run the following command on your local machine, which allows you to access LwM2M Server with `localhost` host address with the default port `5683`.

```
$ ./build/lwm2mserver -4
```

Note that this server does NOT accept DTLS connections.

### LwM2M Bootstrap Server

Run the following command on your local machine to start LwM2M Bootstrap Server at the port `5685`.

```
$ ./build/bootstrapserver -4 -f ./build/bootstrap_server.ini
```

## Revision History

### 1.0.3

- Set TinyDTLS log level to CRIT so to output logs to stderr

### 1.0.2

- Suppress tinyDTLS debug logs for Release build

### 1.0.1

- Fix the output executable file name
- Replace Eclipse Distribution License with 3-Clause BSD

### 1.0.0

- Initial Release
