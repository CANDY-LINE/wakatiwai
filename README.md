Wakatiwai
===

Wakatiwai is a revised version of Eclipse Wakaama LwM2M Client, providing IPC I/F via stdin and stdout.

Notice! The IPC communication is NOT encrypted and doesn't contain any authentication mechanism between processes.

LwM2M DM Server and Bootstrap Server are never modified in this project but you can build them as well as Wakatiwai client.

Server programs are out of Wakatiwai project scope so there are no TODOs for LwM2M Server stuff.

## Major differences between Wakaama LwM2M Client and Wakatiwai

- Strip predefined C-based LwM2M object implementation
- Redirect all log output to stderr
- Enable DTLS by default
- Add new options for providing manageable object IDs
- Add the lifetime query on Registration Update request if the lifetime is changed

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

Copyright (c) 2019 [CANDY LINE INC.](https://www.candy-line.io)

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

### 3.3.1

- Fix an issue where duplicate object IDs were never eliminated

### 3.3.0

- Add a new signal trapping for not sending a deregistration message on process exit
  - SIGTERM => NOT sending a deregistration message
  - SIGINT  => sending a dregistration message

### 3.2.0

- Fix an issue where the base64 length didn't set properly
- Fix an issue where handle_response failed to concatenate a series of response text

### 3.1.0

- Disable heartbeat process while performing bootstrapping and registration

### 3.0.0

- Change message format (not compatible with the older version)
- Increase the default max Block1-trasnfer size to 1048576 bytes from 4096 (can be configured with a gyp variable defined in wakatiwai.gyp)
- Add a new variable for defining max chunk size
- Fix an issue where the written length was wrong

### 2.3.0

- Add a new option to configure the maximum receivable packet (1024 to 65535, 1024  by default)

### 2.2.0

- Fix an issue where connecting to the same server always failed after re-bootstrapping
- Improve debug logs

### 2.1.0

- Add a new command to send heartbeat
- Add support for lifetime query on registration update if the lifetime is modified
- Fix an issue where setup_instance_ids() returned an error when the target object has no instances
- Fix an issue where setup_instance_ids() tried to add a new element to free'd list

### 2.0.5

- Fix an issue where Create command was always add a new instance even after failing to malloc
- Fix an issue where Delete command deleted an instance even after receiving error status code

### 2.0.4

- Accept OPAQUE value as well as STRING for the server URI as the both types can represent it

### 2.0.3

- Fix an issue where wakatiwai client failed to handle 2 or more object instances
- Fix memory issues

### 2.0.2

- Fix an issue where lwm2m_data_cp() failed to copy a valid string because of the wrong termination character position

### 2.0.1

- Fix memory leak issues

### 2.0.0

- Strip dependencies on Security Object and Server Object
- Add a new command argument to enable lwm2m message dump

### 1.3.0

- Add Create method and Delete method support
- Fix a potential buffer overflow error

### 1.2.0

- Bump wakaama revision to v1.0

### 1.1.0

- Bump wakaama revision

### 1.0.3

- Set TinyDTLS log level to CRIT so to output logs to stderr

### 1.0.2

- Suppress tinyDTLS debug logs for Release build

### 1.0.1

- Fix the output executable file name
- Replace Eclipse Distribution License with 3-Clause BSD

### 1.0.0

- Initial Release
