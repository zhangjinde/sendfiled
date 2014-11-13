# Implementation issues {#implementation}

<h1 id="opening_files">Server-side opening of files</h1>

On the server, the opening of a file consists of calls to [open] and [stat], and
the taking of a read lock which prevents the file from being modified while it
is being transferred. The lock is released when the transfer completes or fails.

# The client-to-server link (request channel)

The server accepts requests on a UDP [UNIX domain][unix] socket.

<h2 id="unix_sockets">Why UNIX domain sockets?</h2>

The entire architecture is [predicated][status_channel] on the ability to
transfer open file descriptors between processes, and UNIX domain sockets are
the only way of achieving that.

<h2 id="udp_vs_tcp">UDP vs. TCP</h2>

* All client/server communication is local so the reliability provided by TCP
  would be redundant and therefore pure overhead.

* Request PDUs are compact enough to always fit into a single datagram and
  therefore there is no danger of pieces from different requests being
  interleaved.

* Using a protocol wich preserves message boundaries simplifies the semantics of
  the code interacting with the BSD sockets API. (This is more of a convenient
  side-effect than a reason in its own right.)

## Protocol 'wire' format

Due to the fact that clients and servers are processes on the same machine,
compactness and portability of data representation are of no concern. Therefore,
for the sake of simplicity, there is no special wire format: the wire format is
identical to the in-memory data structure layout.

<h1 id="data_copying">Data copying</h1>

<h2 id="userspace_read_write">Userspace read/write</h2>

The usual way of transferring data from one file descriptor to another is by
[reading][read] into and [writing][write] from a userspace buffer. This is
inefficient because it consists of two copies of the same data (kernel to
userspace, userspace to kernel) and because it requires *two* switches between
kernel mode and user mode, an expensive operation.

<h2 id="reduced_copy">Reduced copy / zero-copy</h2>

Most platforms provide facilities that eliminate the kernelmode/usermode switch
involved in [userspace read/write][userspace_read_write] and make a best effort
at minimising the amount of copying, e.g., by moving instead of copying data
(zero-copy).

Linux has the [splice] and [sendfile] system calls, the former of which can
operate on any type of file descriptor (as long as one of the endpoints is a
pipe) and the latter of which writes a file to a descriptor of any type.

Other systems are more constrained, however. FreeBSD and Mac OS X, for instance,
do not have a [splice] equivalent and their [sendfile]s are able to write to
sockets only. (There may be ways of writing to other types of file descriptors,
but they look like [complicated hacks] [1].) In such cases there is no choice
but to fall back to [userspace read/write][userspace_read_write].

These facilities used to be truly zero-copy (or, at least, the possibility was
supported) on both Linux and FreeBSD, but this has changed at least once in the
recent past ([Linux commit] [2]; [FreeBSD commit] [3]). There have since been
discussions about making them zero-copy again. Either way, [splice] and
[sendfile] should be more efficient than [userspace
read/write][userspace_read_write] in most cases due to the elimination of the
kernelmode/usermode switch which has the convenient side-effect of requiring
only a single kernel-to-kernel copy instead of two.

<h1 id="transfer_concurrency">Concurrency of file transfers</h1>

When a transfer's destination file descriptor becomes writable, the server reads
from the file and writes to the destination file descriptor, in chunks sized
according to the `st_blksize` field returned by [fstat], until the destination
file descriptor's buffer is filled to capacity (an `errno` value of
`EAGAIN`/`EWOULDBLOCK`), at which point the next transfer is serviced.

Therefore, the number of blocks transferred consecutively during one such
'timeslice' depends on the amount of buffer space available, which depends, in
turn, on the size of the buffer and the rate at which data is consumed by the
receiver. Local sockets, for example, are therefore likely to get larger
'timeslices' than remote sockets.

The situation is probably far from optimal.

(*Note that this description applies equally to the [reduced copy][reduced_copy]
and the [userspace read/write][userspace_read_write] methods.*)

# Why not POSIX Async I/O (AIO)?

There is a lot of information on the Web about the [shortcomings] [4] of [this]
[5] [API] [6] and its various implementations, but the gist of it seems to be:

  * The only operations that are asynchronous are reading and writing. Common
    operations such as [open], [close], and [stat] may all block.

  * Implementation quality and consistency varies; some operations may still
    block, and an operation which *doesn't* block on one implementation *may*
    block on another.

  * The *AIO* API is more complicated. *Fiod*'s client API has the luxury of not
    having to support many different types of I/O, allowing its interface to be
    simple and to-the-point.

  * Inconsistent and non-portable integration with event-notification
    systems. *Fiod* was written with event-based clients in mind, hence its file
    descriptor-based interface. *AIO*, on the other hand, has two notification
    mechanisms: callbacks and signals, the former of which cannot be integrated.

    On Linux AIO would have to be integrated indirectly, through the use of
    [signalfd]. [kqueue][kqueue_freebsd] on FreeBSD has built-in support for
    *AIO* (reflecting the desirability of the unified event-processing loop),
    but [kqueue][kqueue_osx] on OS X 10.9.5 (Mavericks) does not support it at
    all, and thus one would have to resort to `EVFILT_SIGNAL`.

  * Some *AIO* implementations merely offload the I/O to a separate thread or
    thread pool anyway. One has to wonder how many of these threads there are
    and whether they're kernel or userspace threads.

<h1 id="processes">Processes vs. threads</h1>

The primary advantage of processes over threads is that it makes it possible to
share a single server instance between multiple clients.

Some of the convenient side-effects include:

1. Fewer system-wide threads of execution, reducing contention on system
   resources such as CPU, disk, memory, etc.

2. The degree of file I/O concurrency is completely decoupled from the degree of
   other types of concurrency (CPU, memory, etc.). Some very well-known and
   modern server software performs file I/O directly from within 'worker'
   threads, with the result that increasing the worker thread count (in order to
   better utilise all the available CPUs, say) has the unfortunate side-effect
   of also increasing contention on the file I/O stack (file stystem, volume
   manager, block device interface, disk driver, etc.) and the physical disk
   itself. Applications that have a dedicated file I/O thread are less of a
   problem, but may still contend for the file I/O stack with *other
   applications*. Because *fiod* is implemented in processes instead of threads,
   it is possible to run one server instance per disk or disk controller, for
   example, without having to be concerned with the effect this may have on
   contention for other resources.

3. Clients benefit from process environment isolation.

# Sending headers

One may wonder why the headers are not sent from the client to the server as
part of the single [Send File][send_file] request. This was considered but
decided against for the following reasons:

* It would involve the copying of potentially large header data (consider HTTP,
  for instance).

* The server would need to be able to detect the file metadata insertion
  point(s) within the headers, something which would require the examination of
  header data and, in the absence of knowledge of the application-layer protocol
  (e.g., HTTP), a significantly more complicated fiod client/server protocol.

Another option that was considered was *POSIX shared memory*. Considering that a
*fiod* client may be a server, each client may be processing multiple
file-serving requests at a given time, these are the ways in which shared memory
can be used to share headers with the fiod server:

1. Each client has a single, application-wide shared memory segment to which the
  headers of multiple requests are written, requiring the synchronisation of
  writes not only between its own request processors, but also with the server
  process. Additionally, the fact that there would need to be a segment per
  client means that the server's total number of shared memory segments could
  quickly approach the implementation limits:

  <table>
  <tr><th>Operating System</th><th>Max. shared memory segments, *per-process*</th></tr>
  <tr><td>FreeBSD</td><td>128</td></tr>
  <tr><td>Mac OS X 10.6.8</td><td>8</td></tr>
  <tr><td>Solaris</td><td>128</td></tr>
  <tr><td>Linux</td><td>4,096</td></tr>
  </table>

2. Each fiod client's client has its own shared memory segment. Besides the fact
  that access to each segment will still need to be synchronised with the
  server, having one for each request means that the total system-wide number of
  shared memory segments is very likely to hit implementation limits:

  <table>
  <tr><th>Operating System</th><th>Max. shared memory segments, *system-wide*</th></tr>
  <tr><td>FreeBSD</td><td>192</td></tr>
  <tr><td>Mac OS X 10.6.8</td><td>32</td></tr>
  <tr><td>Solaris</td><td>128</td></tr>
  <tr><td>Linux</td><td>4,096</td></tr>
  </table>

  [status_channel]: messages.html#status_channel
  [data_channel]: messages.html#data_channel
  [sending_headers]: messages.html#sending_headers
  [data_copying]: implementation.html#data_copying
  [userspace_read_write]: implementation.html#userspace_read_write "Userspace read/write"
  [reduced_copy]: implementation.html#reduced_copy
  [read_file]: messages.html#read_file "Read File Request"
  [send_file]: messages.html#send_file "Send File Request"
  [send_open_file]: messages.html#send_open_file "Send Open File Request"
  [file_info]: messages.html#file_info "File Information Message"
  [open_file_info]: messages.html#open_file_info "Open File Information Message"
  [transfer_status]: messages.html#transfer_status "Transfer Status Message"
  [1]: http://adrianchadd.blogspot.com/2013/12/experimenting-with-zero-copy-network-io.html
  [2]: https://git.kernel.org/cgit/linux/kernel/git/stable/linux-stable.git/commit/?id=485ddb4b9741bafb70b22e5c1f9b4f37dc3e85bd
  [3]: https://svnweb.freebsd.org/base?view=revision&revision=255608
  [4]: http://neugierig.org/software/blog/2011/12/nonblocking-disk-io.html
  [5]: http://bert-hubert.blogspot.com/2012/05/on-linux-asynchronous-file-io.html
  [6]: http://blog.libtorrent.org/2012/10/asynchronous-disk-io/
  [unix]: http://linux.die.net/man/7/unix "unix(7)"
  [read]: http://linux.die.net/man/2/read "read(2)"
  [write]: http://linux.die.net/man/2/write "write(2)"
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