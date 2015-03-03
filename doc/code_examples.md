# Code Examples {#code_examples}

@note Throughout these examples, error handling and other inconvenient realities
such as partial reads and writes have been ignored for the sake of brevity.

# Example 1: starting a server instance from the shell

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

# Example 2: starting a server instance programmatically

@include sfd_spawn.c

# Example 3: shutting down a server instance programmatically

@include sfd_shutdown.c

# Example 4: connecting to a server instance

@include sfd_connect.c

@note The socket directory is relative to the @e client's root directory, hence
the inclusion of the `/mnt/disk0` prefix this time.

# Example 5: disconnecting from a server

@include sfd_disconnect.c

# Example 6: [reading a file][read_file]

## Send the request

@include sfd_read1.c

@note The file path is relative to the directory specified as the server's new
root when it was spawned, so `/www/abc.html` resolves to
`/mnt/disk0/www/abc.html` in the running example.

* `data_fd` is the [Data Channel][data_channel];

* The server has [opened][opening_files] the file and has started writing its
  metadata and contents to the [Data Channel][data_channel]; when it becomes
  readable:

## Receive file metadata and file data

@include sfd_read2.c

# Example 7: [sending a file][send_file]

## Send the request

@include sfd_send1.c

@note The file path is relative to the directory specified as the server's new
root when it was spawned, so `/www/abc.html` resolves to
`/mnt/disk0/www/abc.html` in the running example.

* At this point the file is [open][opening_files] and the transfer between the
  server and the reader of the destination file descriptor *may already be in
  progress*;

* `stat_fd` is the [Status Channel][status_channel]; when it becomes readable:

## Receive file metadata and transfer updates

@include sfd_send2.c

# Example 8: sending headers

@note See [sending headers][sending_headers] for context.

## Method 1: using [Send Open File][send_open_file]

This method has more complicated semantics but is more efficient on systems
without an equivalent to Linux's `splice(2)` facility.

### Send the request

@include sfd_send_with_headers1.c

@note The file path is relative to the directory specified as the server's new
root when it was spawned, so `/www/abc.html` resolves to
`/mnt/disk0/www/abc.html` in the running example.

At this point:

* the file has been [opened][opening_files] but no transfer has been scheduled;

* `stat_fd` is the [Status Channel][status_channel] and when it becomes
  readable:

### Receive file metadata, send headers, receive transfer updates

@include sfd_send_with_headers2.c

## Method 2: using [Read File][read_file]

This method has simpler semantics that method 1 but is only efficient on systems
with a facility equivalent to Linux's `splice(2)`.

### Send the request

@include sfd_read_with_headers1.c

@note The file path is relative to the directory specified as the server's new
root when it was spawned, so `/www/abc.html` resolves to
`/mnt/disk0/www/abc.html` in the running example.

At this point:

* the server has [opened][opening_files] the file and *has started the
transfer*;

* `data_fd` is the [Data Channel][data_channel]; when it becomes readable:

### Receive file metadata and read file data

@include sfd_read_with_headers2.c

  [status_channel]: messages.html#status_channel
  [data_channel]: messages.html#data_channel
  [opening_files]: implementation.html#opening_files
  [sending_headers]: messages.html#sending_headers
  [read_file]: messages.html#read_file
  [send_file]: messages.html#send_file
  [send_open_file]: messages.html#send_open_file
  [file_information]: messages.html#file_information "File Information Message"
  [open_file_information]: messages.html#open_file_information "Open File Information Message"
  [transfer_status]: messages.html#transfer_status "Transfer Status Message"