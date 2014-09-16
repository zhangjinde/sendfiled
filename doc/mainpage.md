# Overview {#mainpage}

## What

* All file I/O is done in one or more separate processes

* File operation requests are transported over UDP through a UNIX domain socket
  (for passing file descriptors between the client and server processes)

* File operation status updates are transferred over a pipe (the *status* file
  descriptor)

* File data is written to the *destination* file descriptor (which may or may
  not equal the status file descriptor), a userspace buffer, or a socket

* Best attempts are made at achieving zero-copy by using platform-specific
   mechanisms such as *splice(2)* on Linux and *sendfile(2)* on other systems.

## Why?

### Zero-Copy

Copying files is expensive, so it is worthwhile trying to keep this to a
minimum. This library tries to use the best method of achieving zero-zopy I/O on
each supported platform, which is *splice(2)* on Linux and *sendfile(2)* on
other platforms.

### Non-Blocking

Event-based applications usually make heavy use of non-blocking operations in
order to improve concurrency. Most types of I/O interfaces (e.g., sockets,
pipes, etc.) provide non-blocking modes of operation.

One notable exception, however, is disk-backed file descriptors. I/O on these
will block even if non-blocking mode was explicitly requested, until the
corresponding portion of the file is in the file cache (i.e., has been read off
of the disk).

There are basically only two ways of avoiding this:

1. **POSIX Asynchronous I/O**. There is a lot of information on the Web about
the [shortcomings] [1] of [this] [2] [API] [3] and its various implementations, but
the gist of it seems to be:

  [1]: http://neugierig.org/software/blog/2011/12/nonblocking-disk-io.html
  [2]: http://bert-hubert.blogspot.com/2012/05/on-linux-asynchronous-file-io.html
  [3]: http://blog.libtorrent.org/2012/10/asynchronous-disk-io/

  * Implementation quality and consistency varies; some operations can still
    block, and an operation which doesn't block on one implementation may block
    on another

  * The API is complicated to use; e.g., how well does it integrate with
    *select*, *poll*, *epoll*, *kqueue*?

  * Some implementations simply use a background thread or thread pool (read on
    for why this is not ideal)
  
2. **Blocking file I/O in background thread(s) or process(es)**.

### Processes over threads

The benefits of running in a separate process instead of a thread include:

1. A single instance can be shared between multiple applications

2. Clients benefit from process environment isolation

3. Reduced system thread count (a single process vs. an additional thread per
  client), resulting in:

  * reduced contention on the OS' file I/O stack (file system, volume manager,
    block device interface, disk driver, etc.) and the physical disk(s)

  * more regular file I/O usage patterns

  * a decoupling of the application thread count and the I/O thread count

Although various optimisations such as caching and I/O operation serialisation
are transparently employed by the operating system and the physical disk,
reducing contention at the application level can still be beneficial to
performance.

#### Contention in the File I/O Stack

There is a multi-layer software stack between the application and the physical
disk within which there are multiple points of contention such as file system
locks and kernel I/O queues. Contention on these points increases with thread
count.

#### More Regular Usage Patterns

For example, consider a server sending a file to a client over a network. It
would make sense for the server to fill the client socket's send buffer with as
much file data as it will hold before moving on to the next client's
transfer. Assuming that the socket's send buffer is larger than the file's
optimal I/O block size (a very realistic assumption), multiple reads (of
contiguous blocks) would be required.

If this is the only thread using the disk, these reads would be issued to the
disk controller sequentially, which is ideal. However, the more other threads
are trying to do the same, the more likely these reads are to be interleaved
with other, unrelated requests, possibly for data on the other side of the disk,
in spite of the OS' and/or disk's best attempts. If the reading thread were to
be suspended by the CPU scheduler, for example, the OS would have no choice but
to send another thread's requests to the disk.

#### Decoupling Application and I/O Thread Counts 
