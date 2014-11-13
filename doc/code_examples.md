# Code Examples {#code_examples}

**Note**: Throughout these examples, error handling and other complications such
as partial reads and writes have mostly been ignored for the sake of brevity and
clarity.

# Starting a server instance from the shell

~~~{.sh}
        $ /usr/bin/fiod -s disk0 -n 100 -t 10000
~~~

* Server instance name: 'disk0'
* Maximum number of concurrent transfers: 100
* [Open files][send_open_file] are automatically closed after 10,000ms

# Starting a server instance programmatically

(**Note** that this form is far from ideal since no attempt is made at limiting
the privileges of the server process; instead, this task is left to
operating-system-provided facilities such as `chroot`, `jail`, *cgroups*, *LXC*,
etc. As it stands, this form is only provided for the sake of convenience during
testing.)

@include fiod_spawn.c

# Shutting down a server instance programmatically

@include fiod_shutdown.c

# Connecting to a server instance

@include fiod_connect.c

# Disconnecting from a server

@include fiod_disconnect.c

# [Reading][read_file] a file

@include fiod_read1.c

`file_fd` is the [data channel][data_channel] (the read end of a pipe); when it
becomes readable:

@include fiod_read2.c

# Sending a file

## Send the request

@include fiod_send1.c

* At this point the file is open and locked and the transfer may already be in
  progress;

* `stat_fd` is the [status channel][status_channel], the read end of a pipe.

## Receive transfer updates

When the [status channel][status_channel] becomes readable:

@include fiod_send2.c

# Sending a file with headers

(See [sending headers] for more information.)

## Using [send open file][send_open_file]

@include fiod_send_with_headers1.c

At this point the file is open and locked, but no transfer has been scheduled;
`stat_fd` is the [status channel][status_channel], the read end of a pipe; when
it becomes readable:

@include fiod_send_with_headers2.c

## Using [read file][read_file]

@include fiod_read_with_headers1.c

`data_fd` is the [data channel][data_channel]; at this point the server has
opened and locked the file and has started the transfer; when it becomes
readable:

@include fiod_read_with_headers2.c

  [status_channel]: messages.html#status_channel
  [data_channel]: messages.html#data_channel
  [sending_headers]: messages.html#sending_headers
  [read_file]: messages.html#read_file
  [send_file]: messages.html#send_file
  [send_open_file]: messages.html#send_open_file
  [file_information]: messages.html#file_information "File Information Message"
  [open_file_information]: messages.html#open_file_information "Open File Information Message"
  [transfer_status]: messages.html#transfer_status "Transfer Status Message"