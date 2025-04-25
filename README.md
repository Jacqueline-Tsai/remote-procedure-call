# Remote File Operations via Remote Procedure Call

## Overview
This project implements a distributed file operations system using Remote Procedure Calls (RPC). It enables applications to perform file operations (`open`, `read`, `write`, `close`, etc.) transparently on a remote server while maintaining the standard POSIX API interface locally.

The system works through two mechanisms:
1. **System Call Interception**: Intercepts file-related syscalls and redirects them
2. **RPC Communication**: Forwards intercepted calls to a remote server for execution

This architecture allows applications to operate on remote files as if they were local, without modification to their source code.

All development is targeted at **64-bit x86 Linux systems**.

## Project Structure
```
.
├── Makefile            # Top-level build script
├── README              # Project documentation
├── docs/
│   └── design.pdf      # Design document
├── include/
│   └── dirtree.h       # Header for directory tree operations
├── lib/
│   └── libdirtree.so   # Shared library for directory tree functions
├── src/
│   ├── Makefile        # Build script for source files
│   ├── client          # Client executable (connects to server)
│   ├── client.c        # Client implementation
│   ├── mylib.c         # System call interception library
│   ├── server          # Server executable (handles requests)
│   └── server.c        # Server implementation
└── tools/
    ├── cat             # Simulates cat command
    ├── ls              # Simulates ls command
    ├── read            # Reads a file
    ├── rm              # Removes a file
    ├── tree            # Displays directory tree
    └── write           # Writes to a file
```

## Requirements

### System Requirements
* 64-bit x86 Linux environment (kernel 3.10+)
* Network connectivity between client and server machines
* Minimum 256MB RAM (client), 512MB RAM (server)
* Permissions to use `LD_PRELOAD` on client systems

### Build Dependencies
* GCC 5.0+ or Clang 3.8+ compiler
* Standard Linux development tools (`make`, `ld`, etc.)
* Development libraries:
  * `libc6-dev`
  * `libssl-dev` (for secure communication)
  * `libprotobuf-dev` (optional, for enhanced serialization)

## Build Instructions

### Build
```bash
make
```

The build produces the following components:

* `src/mylib.so` - Interception library for client systems
* `src/server` - RPC server executable
* `lib/libdirtree.so` - Directory tree operations library
* `src/client` - Standalone RPC client (for testing)


## Usage

### Client-Server RPC Mode
The primary usage mode is client-server:

1. **Start the server** (on the remote machine):
```bash
./src/server [options]
```

Server options:
- `-p PORT`: Specify listening port (default: 8080)
- `-d DIRECTORY`: Set root directory for file operations (default: current directory)
- `-v`: Enable verbose logging

2. **Configure the client** (on the local machine):
```bash
export RPC_SERVER=server_hostname_or_ip
export RPC_PORT=8080  # Must match server port
```

3. **Run applications with intercepted file operations**:
```bash
LD_PRELOAD=./src/mylib.so <path-to-executable> ./tools/read <path-to-remote-file> <parameters>
```

All file operations performed by the application will be transparently forwarded to the remote server.

### Local Interception Mode
For testing or debugging without a remote server:

```bash
export RPC_MODE=LOCAL  # Forces local operation
LD_PRELOAD=./src/mylib.so ./tools/ls directory/
```

This mode intercepts syscalls but processes them locally, useful for logging or debugging.

## Example Tools and Applications

The `tools/` directory contains sample utilities that demonstrate the system's capabilities:

| Tool | Description |
|------|-------------|
| `cat` | Display file contents |
| `ls` | List directory contents |
| `read` | Read file with specific parameters |
| `write` | Write or append to files |
| `rm` | Remove files or directories |
| `tree` | Show directory structure |

These applications work unmodified with either local or remote files, demonstrating the transparency of the RPC layer.

## How It Works

### System Architecture
The system consists of three main components:

1. **Interception Library** (`mylib.so`): 
   - Uses `LD_PRELOAD` to intercept standard C library calls
   - Converts file operations into RPC messages
   - Handles connection management and client-side caching

2. **RPC Client** (`client.c`):
   - Manages the communication channel to the server
   - Implements the RPC protocol (serialization/deserialization)
   - Provides error handling and reconnection logic

3. **RPC Server** (`server.c`):
   - Listens for incoming client connections
   - Executes file operations on behalf of clients
   - Implements access control and security measures
   - Manages concurrent client sessions

### RPC Protocol
The system uses a custom binary protocol for efficiency:
- 4-byte operation code
- 4-byte payload length
- Variable-length payload (serialized arguments)
- 4-byte status code in response
- Variable-length result data

## Documentation
- Detailed design document: `docs/design.pdf`
