# Code Examples {#code_examples}

@note Throughout these examples, error handling and other inconvenient realities
such as partial reads and writes have been ignored for the sake of brevity.

# Contents

* [Example 1: starting a server instance from the shell](#ex1)

* [Example 2: starting a server instance programmatically](#ex2)

* [Example 3: shutting down a server instance programmatically](#ex3)
* [Example 4: connecting to a server instance](#ex4)
* [Example 5: disconnecting from a server](#ex5)
* [Interlude: reading file metadata](#read_file_metadata)
* [Example 6: reading a file](#ex6)
* [Example 7: sending a file](#ex7)

* [Example 8: sending headers](#ex8)
 * [by means of Send Open File](#ex8.1)
 * [by means of Read File](#ex8.2)

<h1 id="ex1">Example 1: starting a server instance from the shell</h1>

~~~{.sh}
        $ sendfiled -s disk0 -r /mnt/disk0 -d
~~~

* `-s <string>`: Server instance name: 'disk0'

* `-r <path>`: Server's new root directory: `/mnt/disk0`; the server will
   `chroot(2)` and `chdir(2)` to this directory as soon as possible.

* `-d`: become a daemon

The server binary will need root privileges in order to invoke `chroot(2)`
unless the new root directory is `/`, in which case no `chroot(2)` is done. The
server process will drop its root privileges as soon as possible (see the `-u`
and `-g` options below).

## Other options:

* `-u <string>`: The new username; defaults to the real UID; mandatory if the
  real UID is root (0)

* `-g <string>`: The new group name; defaults to the real GID; mandatory if the
  real GID is root (0)

* `-S <path>`: The directory in which to create the UNIX socket file; relative
  to the new root directory

* `-n <integer>`: The maximum number of concurrent file transfers

* `-t <integer>`: The number of milliseconds after which files opened with
  sfd_open() but not yet transferred with sfd_send_open() are assumed to have
  been abandoned and therefore closed

<h1 id="ex2">Example 2: starting a server instance programmatically</h1>

@include sfd_spawn.c

<h1 id="ex3">Example 3: shutting down a server instance programmatically</h1>

@include sfd_shutdown.c

<h1 id="ex4">Example 4: connecting to a server instance</h1>

@include sfd_connect.c

@note The socket directory is relative to the @e client's root directory, hence
the inclusion of the `/mnt/disk0` prefix this time.

<h1 id="ex5">Example 5: disconnecting from a server</h1>

@include sfd_disconnect.c

<h1 id="read_file_metadata">Interlude: reading file metadata</h1>

The client's first task after having sent the transfer request is to read the
file metadata sent by the server in response to the transfer request:

@include read_file_metadata.c

@note This code is identical for all transfer types and therefore is omitted
from subsequent examples.

<h1 id="ex6">Example 6: [reading a file][read_file]</h1>

## Send the request

@include sfd_read1.c

@note The file path is relative to the directory specified as the server's new
root when it was spawned, so `/www/abc.html` resolves to
`/mnt/disk0/www/abc.html` in the running example.

* `data_fd` is the [Data Channel][data_channel]

* The server has [opened][opening_files] the file and has started writing its
  metadata and contents to the [Data Channel][data_channel]

When the [Data Channel][data_channel] becomes readable, [read the file
metadata][read_metadata] and then:

## Receive file data

@include sfd_read2.c

<h1 id="ex7">Example 7: [sending a file][send_file]</h1>

## Send the request

@include sfd_send1.c

@note The file path is relative to the directory specified as the server's new
root when it was spawned, so `/www/abc.html` resolves to
`/mnt/disk0/www/abc.html` in the running example.

* At this point the file is [open][opening_files] and the transfer between the
  server and the reader of the destination file descriptor *may already be in
  progress*

* `stat_fd` is the [Status Channel][status_channel]

When the [Status Channel][status_channel] becomes readable, [read the file
metadata][read_metadata] and then:

## Receive transfer updates

@include sfd_send2.c

<h1 id="ex8">Example 8: sending headers</h1>

@note See [sending headers][sending_headers] for context.

<h2 id="ex8.1">Sending headers using [Send Open File][send_open_file]</h2>

This method has more complicated semantics but is more efficient on systems
without an equivalent to Linux's `splice(2)` facility.

### Send the request

@include sfd_send_with_headers1.c

@note The file path is relative to the directory specified as the server's new
root when it was spawned, so `/www/abc.html` resolves to
`/mnt/disk0/www/abc.html` in the running example.

At this point:

* the file has been [opened][opening_files] but no transfer has been scheduled

* `stat_fd` is the [Status Channel][status_channel]

When the [Status Channel][status_channel] becomes readable, [read the file
metadata][read_metadata] and then:

### Send headers and receive transfer updates

@include sfd_send_with_headers2.c

<h2 id="ex8.2">Sending headers using [Read File][read_file]</h2>

This method has simpler semantics that method 1 but is only efficient on systems
with a facility equivalent to Linux's `splice(2)`.

### Send the request

@include sfd_read_with_headers1.c

@note The file path is relative to the directory specified as the server's new
root when it was spawned, so `/www/abc.html` resolves to
`/mnt/disk0/www/abc.html` in the running example.

At this point:

* the server has [opened][opening_files] the file and *has started the
transfer*

* `data_fd` is the [Data Channel][data_channel]

When the [Data Channel][data_channel] becomes readable, [read the file
metadata][read_metadata] and then:

### Receive file data

@include sfd_read_with_headers2.c

  [read_metadata]: #read_file_metadata "Read file metadata"
  [status_channel]: messages.html#status_channel
  [data_channel]: messages.html#data_channel
  [opening_files]: implementation.html#opening_files
  [sending_headers]: messages.html#sending_headers
  [read_file]: messages.html#read_file
  [send_file]: messages.html#send_file
  [send_open_file]: messages.html#send_open_file
  [file_information]: messages.html#file_info "File Information Message"
  [open_file_information]: messages.html#open_file_information "Open File Information Message"
  [transfer_status]: messages.html#transfer_status "Transfer Status Message"