# Client/server communications {#messages}

# Communication channels

<h2 id="status_channel">Status Channel</h2>

This channel consists of an automatically-created pipe connecting the client and
server processes. Each request is acknowledged on this channel with a response
code and file metadata before the commencement of the transfer. The server will
also write [transfer progress][transfer_status] and [error
notifications][errors] to this channel.

@note This channel is not created in the case of the [Read File][read_file]
operation.

<h2 id="data_channel">Data Channel</h2>

This is the file descriptor to which the server writes the file data.

Its type depends on the operation: for [Read File][read_file] operations, the
Data Channel is an automatically-created pipe; for [Send File][send_file] and
[Send Open File][send_open_file] operations the Data Channel is an arbitrary
file descriptor opened by the client process (and passed to the server process
along with the request).

# File operations/request types

<h2 id="send_file">Send File</h2>

The server process writes the contents of a file to an arbitrary,
client-provided file descriptor.

Step-by-step:

1. The server [opens the file][opening_files] and writes a [File
   Information][file_info] message to the [Status Channel][status_channel];

2. The server [reads from the file and writes to the data
   channel][transfer_concurrency] as it is able, and it will periodically write
   [transfer status][transfer_status] messages to the [Status
   Channel][status_channel];

3. When the transfer completes or an error occurs, the server writes a special
   [transfer completion][transfer_completion] message (see below) or an [error
   message][errors], respectively, to the [Status Channel][status_channel], and
   closes the file.

Completion of the transfer is signified by a [transfer status][transfer_status]
message containing a special value, an event that can be tested for using the
sfd_xfer_complete() function.

@note This operation is [usually][data_copying] implemented in terms of one or
more calls to [sendfile] and should be the most commonly-used operation since it
is most likely to be efficient on all supported platforms.

@sa sfd_send()

<h2 id="read_file">Read File</h2>

The server process writes the contents of a file to an automatically-created
pipe connected to the client process (the [Data Channel][data_channel]). There
is no distinct [Status Channel][status_channel].

Step-by-step:

1. The server [opens the file][opening_files] and writes a [File
   Information][file_info] message to the [Data Channel][data_channel];

2. The server [reads from the file and writes to the data
   channel][transfer_concurrency] as it is able;

3. When the transfer completes or an [error][errors] occurs, the server closes
   the [Data Channel][data_channel] and closes the file.

@note @li The server does not send any [transfer status][transfer_status] or
[error messages][errors] because there is no [Status
Channel][status_channel]. Clients are able to determine the outcome of the
transfer by comparing the total number of bytes received to the file size
received in the inital [file information][file_info] message.

@note @li This operation is [less efficient][data_copying] on systems without an
equivalent of Linux's [splice] facility, so the [send file][send_file] and [send
open file][send_open_file] operations should be preferred on such systems.

@sa sfd_read().

<h2 id="send_open_file">Send Open File</h2>

This is essentially a two-stage/request version of [Send File][send_file] which
gives the client an opportunity to inspect the file metadata before the
commencement of the transfer of the file data.

1. In response to the first request (*Open File*), the server
   [opens][opening_files] the file and responds with the file metadata and a
   unique identifying token in message of type sfd_file_info on the [Status
   Channel][status_channel]. *It does not start transferring the file data.*

2. Once the server receives the second request (*Send Open File*), the server
   writes the open file identified by the unique token to an arbitrary
   user-provided file descriptor.

3. The server will close the [Status Channel][status_channel] and the file if
   the *Send Open File* request is not received within a configurable period,
   the transfer completes, or an error occurs.

@note The same semantics are achievable using the conceptually simpler [Read
File][read_file], but this operation is [inefficient][data_copying] on systems
lacking an equivalent to Linux's `splice(2)` facility.

@sa [Sending Headers] [sending_headers]
@sa sfd_open()
@sa sfd_send_open()

# Responses

<h2 id="headers">Headers</h2>

All response message types start with a two-byte header which consist of the
following:

* *Command ID*: identifies the command

* *Response/status code*: indicates the result of the request. `SFD_STAT_OK`
  signifies success, while a non-negative, non-zero positive integer signifies
  an error condition to be interpreted as a standard `errno(3)` value.

@sa sfd_get_cmd()
@sa sfd_get_stat()

<h2 id="file_info">File Information</h2>

Sent in response to a [Read File][read_file] or [Send File][send_file] request
in order to indicate whether or not the request was accepted.

@sa struct sfd_file_info
@sa sfd_unmarshal_file_info()

<h2 id="open_file_info">Open File Information</h2>

Sent in response to a [Send Open File][send_open_file] request in order to
indicate whether or not the request was accepted.

@sa sfd_file_info
@sa sfd_unmarshal_file_info()

<h2 id="transfer_status">Transfer status notifications</h2>

Sent to notify the client that a chunk of the file has been sent. Specifies the
size, in bytes, of the most recent write (or group of writes).

@sa sfd_xfer_stat

These notifications are never sent during [Read File][read_file] operations.

<h2 id="transfer_completion">Transfer completion notifications</h2>

Sent to notify the client of the completion of a transfer initiated with a [Send
File][send_file] or a [Send Open File][send_open_file] request. These are
special instances of the [Transfer Status][transfer_status] message for which
the sfd_xfer_complete() function returns `true`.

@sa sfd_unmarshal_xfer_stat()
@sa sfd_xfer_complete()
@sa sfd_xfer_stat

<h2 id="errors">Error notifications</h2>

Sent when a fatal error has occured, either in the reading of the file or in the
writing to the destination file descriptor.

These messages consist only of a [header](#headers) and therefore does not have
a corresponding data structure. Instead the receive buffer can be inspected
directly using sfd_get_cmd() and sfd_get_stat() and indirectly via the
response-unmarshaling functions such as sfd_unmarshal_xfer_stat() which return
`false` in the case of an unexpected command ID or an error response code.

@sa sfd_get_cmd()
@sa sfd_get_stat()

# Reliability of response delivery

[Transfer completion notifications][transfer_completion] and [error
notifications][errors] are critical and will be retried until they have been
successfully written.

[File Information][file_info] and [Transfer Progress][transfer_status] messages
will not be retried because the former are sent right after the creation of the
[Status][status_channel] or [Data][data_channel] channel pipes and therefore
should have more than enough capacity to accept the PDU, and the latter are
merely intermediate, informational notification messages, the terminal [transfer
completion][transfer_completion] message being the funcionally significant one.

<h1 id="sending_headers">Sending headers</h1>

A client may need to precede file content with headers containing file metadata
such as file size. Due to the fact that file metadata only becomes available
after the server has opened the file the client and server need to synchronise
at the point between the client's receipt of the metadata and the server's
commencement of the transfer, something for which [Send File][send_file] does
not have the right semantics (file data is transferred out-of-band).

Recommendations:

* **Systems *with* a `splice(2)` facility** should use the [Read
  File][read_file] operation because `splice(2)` minimises copying, is
  conceptually simpler, and requires less signaling from the server.

* **Systems *lacking* a `splice(2)` facility** should use the [Send Open
  File][send_open_file] operation because [Read File][read_file] is implemented
  as a kernel-to-userspace-to-kernel copy on these systems while [Send Open
  File][send_open_file] is implemented in terms of `sendfile(2)`.

* **In case of doubt** use [Send Open File][send_open_file] because `sendfile(2)`
    is supported on all systems.

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
  [splice]: http://linux.die.net/man/2/splice "splice(2)"
  [sendfile]: https://www.freebsd.org/cgi/man.cgi?query=sendfile "sendfile(2)"
