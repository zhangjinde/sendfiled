Sendfiled is a background server process which sends or reads files on behalf of
client applications.

# Goals

To provide event-based applications with:

* **Non-blocking file transfer**. Unlike other types of file descriptors,
   non-blocking semantics cannot be achieved reliably on disk-backed file
   descriptors. If the file data is not yet in the page cache, the read will
   block. For this reason the only way to achieve truly non-blocking file I/O is
   by doing it in a separate thread of execution.

* **Reduced copying**. *Sendfiled* makes use of platform facilities such as
   [sendfile] and [splice] to achieve file transfer with [minimal
   copying][data_copying].

* **Reduced contention**. See [this section][impl_processes] for the details
    (which would be out of scope at this point).

# Architectural overview

* The sendfiled server runs in a separate process and transfers files on behalf of
  client processes.

* File transfer requests are transported over a [UDP][impl_udp_vs_tcp] [UNIX
  domain][impl_unix_sockets] socket.

* File transfer status updates are transferred over a transaction-unique pipe
  (the [status channel][status_channel]).

* File data is written to a client-provided file descriptor (the [data
  channel][data_channel]) or to a pipe automatically created between the client
  and the server.

* Client applications interact with the server via a shared library written in
  C.

<h1 id="semantics">File transfer operations</h1>

* [Send File:] [Send File] the server writes the contents of a file to an
  arbitrary, client-provided file descriptor.

* [Read File:] [Read File] the server writes the contents of a file to a pipe
  connected to the client.

* [Send Open File:] [Send Open File] the server sends a previously-opened file
  to an arbitrary, client-provided file descriptor. This allows the client and
  server to synchronise on the point between the opening of the file (i.e., the
  provision of file metadata to the client) and the commencement of the
  transfer.

# More information

* [Client/server communications](messages.html)

* [Code examples] (code_examples.html)

* [Implementation issues](implementation.html)

  [status_channel]: messages.html#status_channel
  [data_channel]: messages.html#data_channel
  [data_copying]: implementation.html#data_copying
  [impl_unix_sockets]: implementation.html#unix_sockets
  [impl_udp_vs_tcp]: implementation.html#udp_vs_tcp
  [impl_processes]: implementation.html#processes
  [Read File]: messages.html#read_file
  [Send File]: messages.html#send_file
  [Send Open File]: messages.html#send_open_file
  [File Information]: messages.html#file_information "File Information Message"
  [Open File Information]: messages.html#open_file_information "Open File Information Message"
  [Transfer Status]: messages.html#transfer_status "Transfer Status Message"
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