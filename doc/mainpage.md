# Overview {#mainpage}

## What

* All file I/O is done in a separate process

* File operation requests and responses are transferred through a pipe

* File data can be written to a pipe (kernel buffer; using *splice(2)* on
  Linux), a userspace buffer (not zero-copy), or a socket (using *sendfile(2)*)

## Why?

### Zero-Copy

Copying files is expensive, so it is worthwhile trying to keep this to a
minimum. This library tries to use the best method of achieving zero-zopy I/O on
each supported platform, which is *splice(2)* on Linux and *sendfile(2)* on
other platforms.

### Non-Blocking

I/O on disk-backed file descriptors will block, even if non-blocking I/O was
explicitly requested, until the corresponding portion of the file is in the file
cache (i.e., has been read off of the disk).

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
  
2. **Blocking file I/O in background thread(s)**.

### Processes trump threads
