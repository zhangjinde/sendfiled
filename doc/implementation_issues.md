# Implementation: details & issues {#implementation}

<h1 id="opening_files">Server-side opening of files</h1>

On the server, the opening of a file consists of calls to `open(2)` and `fstat(2)`, and
the taking of a read lock which prevents the file from being modified while it
is being transferred. The lock is released when the transfer completes or fails.

# The client-to-server link (request channel)

Requests are sent from the client to the server over a UDP [UNIX domain][unix]
socket because:

* UNIX domain sockets are the only means by which a client process is able to
  pass an open file descriptor to a server process

* TCP's reliability features are redundant and therefore pure overhead for IPC

* All request types easily fit into a single datagram which means that a single
  server socket is sufficient for dealing with multiple concurrent client
  requests (as opposed to the connection-orientation of TCP)

# Protocol wire format

The PDU data structures are copied byte-for-byte between the client and server
because both are processes on the same machine and therefore compactness and
portability of data representation are of no concern.

<h1 id="data_copying">Data copying</h1>

<h2 id="userspace_read_write">Userspace read/write</h2>

The usual, portable way of transferring data from one file descriptor to another
is by `read(2)`ing into and `write(2)`ing from a buffer in userspace. This is
reasonably expensive because it comprises two copies of the same data (kernel to
userspace, userspace to kernel) and two switches between kernel mode and user
mode.

<h2 id="low_copy">Low/zero copy</h2>

Most platforms provide facilities that eliminate the switch between kernel and
user mode and make a best effort at minimising the amount of copying, e.g., by
moving instead of copying data (aka *zero-copy*).

Linux has the [splice] and [sendfile][sendfile_linux] system calls, the former
of which can operate on any type of file descriptor (as long as one of the
endpoints is a pipe) and the latter of which writes a file to a descriptor of
any type.

Other systems are more constrained, however. FreeBSD and Mac OS X, for instance,
do not have a `splice(2)` equivalent and their [sendfile][sendfile_freebsd]s are
able to write to sockets only. (There may be ways of writing to other types of
file descriptors, but they look like [complicated hacks] [1].) In such cases
there is no choice but to fall back to [userspace
read/write][userspace_read_write].

@note These facilities used to be truly zero-copy (or, at least, the possibility
was supported) on both Linux and FreeBSD, but this has changed at least once in
the recent past ([Linux commit] [2]; [FreeBSD commit] [3]). There have since
been discussions about making these system calls zero-copy again but, either
way, `splice(2)` and `sendfile(2)` should be more efficient than [userspace
read/write][userspace_read_write] in most cases due to the elimination of the
switch between kernel and user modes which has the convenient side-effect of
requiring only a single (kernel-to-kernel) copy instead of the two involved in
the usual [userspace read/write][userspace_read_write].

# I/O

The server uses non-blocking file descriptors and edge-triggered I/O event
notification (only [epoll] is currently implemented).

<h1 id="transfer_concurrency">Concurrency of file transfers</h1>

[Send File][send_file] and [Send Open File][send_open_file], the out-of-band
transfers:

    recipient [R] <----- [W] server [R] <----- disk

[Read File][read_file], the only in-band transfer:

    recipient [R] <----- [W] client [R] <-- IPC -- [W] server [R] <----- disk

When a transfer's destination file descriptor becomes writable, the server reads
from the file and writes to the destination file descriptor, in chunks sized
according to the `st_blksize` field returned by `fstat(2)`, until the
destination file descriptor's buffer is filled to capacity (an `errno(3)` value
of `EAGAIN`/`EWOULDBLOCK`), at which point the next transfer is serviced.

Therefore, the number of blocks transferred consecutively during one such
'timeslice' depends on the amount of buffer space available in the destination
file descriptor which is a function of its capacity and the rate at which data
is consumed by the receiver.

A file recipient could deny service to other clients and file recipients by
reading faster than the server can write, a rate which is ultimately limited by
the speed of the disk and therefore easily achievable by recipients on fast
links to the server (e.g., a local process or one on a very fast network
connection connected to a server with high load on its disks).

@todo Limit the maximum amount of data that can be transferred from a file to a
destination file descriptor in one 'time slice'.

@note This description applies equally to the [low copy][low_copy] and the
[userspace read/write][userspace_read_write] methods.

# Why not POSIX Async I/O (AIO)?

There is a lot of information on the Web about the [shortcomings] [4] of [this]
[5] [API] [6] and its various implementations, but the gist of it seems to be:

  * Inconsistent and non-portable integration with event-notification systems
    (based on file descriptors). sendfiled was written with event-based
    clients in mind, hence its file descriptor-based interface. AIO, on the
    other hand, has two notification mechanisms: callbacks and signals, the
    former of which cannot be integrated.

    On Linux AIO would have to be integrated indirectly through the use of
    [signalfd]. [kqueue on FreeBSD][kqueue_freebsd] has built-in support for AIO
    but [kqueue on OS X][kqueue_osx] 10.9.5 (Mavericks) does not support it at
    all, and thus one would have to resort to `kqueue(2)`'s `EVFILT_SIGNAL`.

  * The only I/O operations supported by the API are reading, writing, and
    fsync'ing. Other means need to be employed in order to prevent common
    operations such as `open(2)`, `close(2)`, and `fstat(2)` from blocking.

  * Implementation quality and consistency varies; some operations may still
    block, and an operation which *doesn't* block on one implementation *may*
    block on another.

  * Some implementations merely offload the I/O to a separate thread or thread
    pool.


That said, as in the case of zero-copy I/O, some implementations are still being
improved (Linux, for one), so some of the claims made above may already be
outdated.

<h1 id="processes">Processes vs. threads</h1>

The primary reason Sendfiled is implemented as a process instead of a thread is
that it makes it possible to share a single server instance between multiple
client applications, with the following convenient consequences:

1. Fewer system-wide threads of execution compared to a thread-per-application
   design, reducing contention on system resources such as CPU, disk, cache,
   etc.

2. Clients benefit from process environment isolation.

3. The degree of file I/O concurrency is decoupled from the degree of other
   types of concurrency.

   Some very well-known and modern server software performs file I/O directly
   from within 'worker' threads, with the result that increasing the worker
   thread count (in order to better utilise all the available CPUs, say) has the
   undesirable side-effect of simultaneously increasing contention on the file
   I/O stack (file stystem, volume manager, block device interface, disk driver,
   etc.) and the physical disk itself.

   Because the sendfiled server is implemented as a processes instead of a
   thread, it is possible to run the optimal number of server instances (one per
   disk or disk controller, for example) without having to be concerned about
   the effect this may have on the levels of contention for (and over- or
   under-utilisation of) other shared system resources.

   Applications that have a dedicated file I/O thread do not have this problem,
   but may still contend for the file I/O stack with *other applications*.

  [status_channel]: messages.html#status_channel
  [data_channel]: messages.html#data_channel
  [sending_headers]: messages.html#sending_headers
  [data_copying]: implementation.html#data_copying
  [userspace_read_write]: implementation.html#userspace_read_write "Userspace read/write"
  [low_copy]: implementation.html#low_copy
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
  [signalfd]: http://linux.die.net/man/2/signalfd "signalfd(2)"
  [splice]: http://linux.die.net/man/2/splice "splice(2)"
  [sendfile_linux]: http://linux.die.net/man/2/sendfile "sendfile(2) on Linux"
  [sendfile_freebsd]: https://www.freebsd.org/cgi/man.cgi?query=sendfile "sendfile(2) on FreeBSD"
  [epoll]: http://linux.die.net/man/7/epoll "epoll(7)"
  [kqueue]: https://www.freebsd.org/cgi/man.cgi?query=kqueue "kqueue(2)"
  [kqueue_freebsd]: https://www.freebsd.org/cgi/man.cgi?query=kqueue "kqueue(2)"
  [kqueue_osx]: https://developer.apple.com/library/Mac/documentation/Darwin/Reference/ManPages/man2/kqueue.2.html "kqueue(2)"