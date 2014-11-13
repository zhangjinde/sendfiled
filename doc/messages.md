# Client/server communications {#messages}

# Communication channels

<h2 id="status_channel">Status channel</h2>

This channel consists of an automatically-created pipe connecting the client and
server processes. Each request is acknowledged on this channel before the
commencement of the transfer. The server will also write [transfer
progress][transfer_status] and [error][errors] notifications to this channel.

This channel is not present in the case of the [Read File][read_file] operation.

<h2 id="data_channel">Data channel</h2>

This is the channel to which the file data is written. Its type depends on the
operation: for [Read File][read_file] operations, the data channel will always
be an automatically-created pipe; for [Send File][send_file] and [Send Open
File][send_open_file] operations the data channel is an arbitrary file
descriptor opened by the client process (and passed to the server process as
part of the request), so its type varies.

# Operations (requests)

<h2 id="send_file">Send file</h2>

The server writes the contents of a file to an arbitrary, client-provided file
descriptor.

Step-by-step:

* The server [opens][opening_files] the file and writes a [File
  Information][file_info] message to the [status channel][status_channel];

* The server [reads from the file and writes to the data
  channel][transfer_concurrency] as it is able, and it will periodically write
  [transfer status][transfer_status] messages to the [status
  channel][status_channel] as the transfer progresses;

* When the transfer completes or an error occurs, the server writes a special
  [transfer completion][transfer_completion] message (see below) or an [error
  message][errors], respectively, to the [status channel][status_channel], and
  closes the file.

Completion of the transfer is signified by a [transfer status][transfer_status]
message containing a special value in the *size* field, an event that can be
tested for using the fiod_xfer_complete() function.

This operation is [usually][data_copying] implemented in terms of one or more
calls to [sendfile] and should be the most commonly-used operation since it is
most likely to be efficient on all supported platforms.

Implemented by fiod_send().

<h2 id="read_file">Read file</h2>

The server writes the contents of a file to a pipe connected to the client.

The primary difference between this operation and [Send File][send_file] is that
there is no status channel ([status channel][status_channel] and [data
channel][data_channel] are one and the same).

Step-by-step:

* The server [opens][opening_files] the file and writes a [file information][file_info] message
  to the [data channel][data_channel]

* When the destination descriptor becomes writable, the server reads, in chunks
  sized according to the block size returned by [stat], from the file and writes
  this data to the [data channel][data_channel] until this descriptor has been
  filled to capacity;

* When the transfer completes or an [error][errors] occurs, the server closes
  the [data channel][data_channel] and closes the file.

After the initial [File Information][file_info] message which precedes the file
data, the server does not send any [transfer status][transfer_status] or [error
messages][errors] because, due to the fact that there is no separate status and
data channels, they would be interleaved with file content and thus the client
wouldn't be able to distinguish between the two types of data.

Clients are able to determine the outcome of the transfer by comparing the total
number of bytes received to the file size received in the inital [file
information][file_info] message.

This operation is [less efficient][data_copying] on non-Linux systems due to the
absence of a [splice] equivalent, so the [send file][send_file] and [send open
file][send_open_file] operations should be preferred on such systems.

Implemented by fiod_read().

<h2 id="send_open_file">Send open file</h2>

This is effectively a two-stage/two-request version of [Send File][send_file]:

1. In response to the first request ('*open file*'), the server
   [opens][opening_files] the file and responds with an [open file
   information][open_file_info] message, which includes a unique identifying
   token, on the [status channel][status_channel] (implemented by fiod_open());

2. In response to the second request ('*send open file*'), the server writes the
   file identified by the unique token to an arbitrary user-provided file
   descriptor (implemented by fiod_send_open()).

The server will automatically close open files after a configurable amount of
time if no *send open file* request has yet been received.

(This operation exists solely in order to support the [sending of headers]
[sending_headers] on non-Linux systems.)

# Responses

<h2 id="file_info">File information</h2>

Sent in response to a [Read File][read_file] or [Send File][send_file]
request. These messages include information such as the size of the file and the
various timestamps as reported by [stat] and are implemented in the form of
struct fiod_file_info.

<h2 id="open_file_info">Open file information</h2>

Sent in response to a [Send Open File][send_open_file] request. In addition to
the information contained in [file information][file_info] messages, they
include a unique token which identifies the opened file. Implemented in the form
of struct fiod_open_file_info.

<h2 id="transfer_status">Transfer progress notifications</h2>

Sent while a transfer is in progress and contain the size, in bytes, of the most
recent write (or group of writes), or an error code if the transfer has
failed. Implemented in the form of struct fiod_xfer_stat.

These notifications are never sent during [Read File][read_file] operations.

<h2 id="transfer_completion">Transfer completion notifications</h2>

Sent only once, when a transfer completes. Implemented in the form of a
[transfer status][transfer_status] message containing a special value which can
be tested for using the fiod_xfer_complete() function.

<h2 id="errors">Error notifications</h2>

Sent when a fatal error has occured, either in the reading of the file or in the
writing to the destination file descriptor.

# Reliability of response delivery

[Transfer completion notifications][transfer_completion] and [error
notifications][errors] are critical and will be retried until they have been
successfully written.

[File Information][file_info] and [Transfer Progress][transfer_status] messages
will not be retried because the former are sent when the pipe has just been
created and therefore should have more than enough capacity to accept the PDU,
and the latter are only sent to provide the client with an idea of the
transfer's progress. Also, due to these notifications being non-terminal,
retrying them would require suspending the transfer and probably a response
queue for each request context.

<h1 id="sending_headers">Sending headers</h1>

Clients may need to precede file content with headers containing file metadata
such as its size. Due to the fact that file metadata only becomes available
after the server has opened the file, the client and server need to synchronise
at the point between the client's receipt of the metadata and the server's
commencement of the transfer, something for which [Send File][send_file] does
not have the right semantics.

On Linux [Read File][read_file] is the recommended solution because it is
efficient due to the presence of [splice] and has the right semantics in that
the file metadata precedes the file content in the [data
channel][data_channel]'s buffer.

The recommended solution on non-Linux systems is the [Send Open
File][send_open_file] operation because, although [Read File][read_file] has the
right semantics, it is inefficient due to the lack of [splice].

  [status_channel]: messages.html#status_channel
  [data_channel]: messages.html#data_channel
  [sending_headers]: messages.html#sending_headers
  [opening_files]: implementation.html#opening_files "Server-side opening of files"
  [data_copying]: implementation.html#data_copying
  [transfer_concurrency]: implementation.html#transfer_concurrency "File transfer mechanism"
  [read_file]: messages.html#read_file "Read File Request"
  [send_file]: messages.html#send_file "Send File Request"
  [send_open_file]: messages.html#send_open_file "Send Open File Request"
  [file_info]: messages.html#file_info "File Information Message"
  [open_file_info]: messages.html#open_file_info "Open File Information Message"
  [transfer_status]: messages.html#transfer_status "Transfer Status Message"
  [errors]: messages.html#errors "Error Notifications"
  [transfer_completion]: messages.html#transfer_completion "Transfer completion message"
  [1]: http://adrianchadd.blogspot.com/2013/12/experimenting-with-zero-copy-network-io.html
  [2]: https://git.kernel.org/cgit/linux/kernel/git/stable/linux-stable.git/commit/?id=485ddb4b9741bafb70b22e5c1f9b4f37dc3e85bd
  [3]: https://svnweb.freebsd.org/base?view=revision&revision=255608
  [4]: http://neugierig.org/software/blog/2011/12/nonblocking-disk-io.html
  [5]: http://bert-hubert.blogspot.com/2012/05/on-linux-asynchronous-file-io.html
  [6]: http://blog.libtorrent.org/2012/10/asynchronous-disk-io/
  [unix]: http://linux.die.net/man/7/unix "unix(7)"
  [open]: http://linux.die.net/man/2/open "open(2)"
  [close]: http://linux.die.net/man/2/close "close(2)"
  [stat]: http://linux.die.net/man/2/stat "stat(2)"
  [fstat]: http://linux.die.net/man/2/fstat "fstat(2)"
  [signalfd]: http://linux.die.net/man/2/signalfd "signalfd(2)"
  [splice]: http://linux.die.net/man/2/splice "splice(2)"
  [sendfile]: https://www.freebsd.org/cgi/man.cgi?query=sendfile "sendfile(2)"
  [select]: http://linux.die.net/man/2/select "select(2)"
  [poll]: http://linux.die.net/man/2/poll "poll(2)"
  [epoll]: http://linux.die.net/man/7/epoll "epoll(7)"
  [kqueue]: https://www.freebsd.org/cgi/man.cgi?query=kqueue "kqueue(2)"
  [kqueue_freebsd]: https://www.freebsd.org/cgi/man.cgi?query=kqueue "kqueue(2)"
  [kqueue_osx]: https://developer.apple.com/library/Mac/documentation/Darwin/Reference/ManPages/man2/kqueue.2.html "kqueue(2)"