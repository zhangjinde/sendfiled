Sendfiled is a local server process which sends or reads files on behalf of
client applications.

# Goals

To provide applications with:

* **Non-blocking file transfer**. Unlike other types of file descriptors,
   non-blocking semantics cannot be achieved reliably on disk-backed file
   descriptors: if the file data is not yet in the page cache, the read will
   block. Therefore the only way of achieving truly non-blocking file I/O is by
   doing it in a separate thread of execution.

* **Reduced copying**. Sendfiled makes use of platform facilities such as
   [sendfile] and [splice] to achieve file transfer with [minimal
   copying][data_copying].

* **Reduced contention**.

# Architectural overview

* The server runs in a separate process and transfers files on behalf of client
  processes.

* File transfer requests are transferred over a UDP UNIX domain socket.

* File transfer status updates are transferred over an automatically-created,
  transaction-unique pipe (aka the Status Channel).

* File data can be written to an arbitrary, client-provided file descriptor (aka
  the Data Channel) or to the Status Channel.

* Client applications interact with the server via a shared library written in
  C.

# File transfer operations

* Send File: the server writes the contents of a file to an arbitrary,
  client-provided file descriptor.

* Read File: the server writes the contents of a file to a pipe connected to the
  client.

* Send Open File: the server sends a previously-opened file to an arbitrary,
  client-provided file descriptor. This allows the client and server to
  synchronise on the point between the opening of the file (i.e., the provision
  of file metadata to the client) and the commencement of the transfer.

# Supported platforms

1. Linux

That's it, alas. But at least the platform-specific code is restricted to a few
source files.

# Dependencies

There are no runtime dependencies.

The build has the following dependencies:

* GNU Make

* A C99 compiler for the client library and daemon (tested with gcc 4.6.3)

* A C++14 compiler (tests only; tested with clang++ 3.4.2)

* Google Test (tests only)

# TODO

* Write more tests

* Measure for performance

  [splice]: http://linux.die.net/man/2/splice "splice(2)"
  [sendfile]: https://www.freebsd.org/cgi/man.cgi?query=sendfile "sendfile(2)"
