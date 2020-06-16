# About ottercat

`ottercat` works like socat or netcat, but specifically for sending commands to an instance of [otter](https://github.com/jpnorair/otter) that is running in socket mode.

otter is not strictly request-response, so socat won't work.  otter will send an acknowledgement to your request together with an integer session ID, and then it may return additional data that's marked with this session ID.  `ottercat` will return all the session data, whereas socat will exit after getting just the acknowledgement.

## Building ottercat

### External Dependencies

The only external dependecies are listed below.  talloc is easy to download via most package managers.

* [talloc](https://talloc.samba.org/talloc/doc/html/index.html)

### HB Dependencies

`ottercat` is part of the H[HB Distribution](https://github.com/jpnorair/hbdist), so the easiest way to build it is via this distribution.

1. Install external dependencies.
2. Clone/Download hbdist repository, and `cd` into it.
3. Do the normal: `make all; sudo make install` 
4. Everything will be installed into a `/opt/` directory tree.  Make sure your `$PATH` has `/opt/bin` in it.

### Building without HB Distribution

If you want to build `ottercat` outside of the [HB Distribution](https://github.com/jpnorair/hbdist), you'll need to clone/download the following repositories.  You should have all these repo directories stored flat inside a root directory.

* _hbsys
* argtable
* cJSON
* bintex

From this point:

```
$ cd ottercat
$ make pkg
```

You can find the binary inside `ottercat/bin/`

## Otter Functional Synopsis

Otter is a terminal shell that operates on a POSIX command line, between a TTY client/host and a binary MPipe target/server.  It implements a human-interface shell for many of the M2DEF-based protocols used by OpenTag, although it is different than a normal terminal shell because all of the translation between the binary interface and the human interface takes place on the client (i.e. the otter app) rather than the server.

Usage of Otter could be compared to usage of the standard, command line version of FTP (ftp).

## Otter Implementation Notes

Otter is implemented entirely in POSIX Standard C99.  It is a multi-threaded app, making use of the PThreads library to do the threading.  It has no dependencies other than Bintex and M2DEF, and these are shipped as source with Otter (you can optionally build it with Bintex and M2DEF libraries).

## Version Notes

Versions 0.1 to 1.0 will use only the normal POSIX command line interface (stdin, stdout).  More features of M2DEF will be introduced gradually, and more advanced shell features will be introduced gradually.  Future versions (after 1.0) will introduce NCurses UI, and potentially also a shell interpreter for NDEF (although this really depends on the prevalance of NDEF libraries in the open source domain).

## MPipe Synopsis

MPipe is the wrapper protocol that Otter and the target must use to transport packets of data.  It is simple enough that any embedded target able to print over a canonical TTY should also be sufficient to use MPipe.  The packet structure of MPipe is shown below:

Byte 0:1 -> Sync (FF55)
Byte 2:3 -> CRC-16
Byte 4:5 -> Payload Length
Byte 6 -> Session Number
Byte 7 -> Control Field
Byte 8:- -> Payload

The MPipe header is 8 bytes, and it may be extended in the future via the Control Field.  All values are big-endian.  The CRC-16 is calculated from Byte 4 to the end of the packet.

Depending on the value of the Control Field, the Payload can be arbitrary data, an M2DEF message (used by OpenTag), or an NDEF message (used by NFC).  M2DEF was previously called "ALP," but it is now officially called M2DEF in order to highlight the large degree of compatibility it has with NDEF.

More information on MPipe is covered in official MPipe documentation.
http://www.indigresso.com/wiki/doku.php?id=opentag:otlib:mpipe



