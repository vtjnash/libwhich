# libwhich

Like `which`, for dynamic libraries.

Also a bit like `ldd` and `otool -L`.

Sample usage:
```
$ ./libwhich libgobject-2.0.so
library:
  /usr/lib/x86_64-linux-gnu/libgobject-2.0.so

dependencies:
  /lib/x86_64-linux-gnu/libdl.so.2
  /lib/x86_64-linux-gnu/libc.so.6
  /lib64/ld-linux-x86-64.so.2
+ /usr/lib/x86_64-linux-gnu/libgobject-2.0.so
+ /lib/x86_64-linux-gnu/libglib-2.0.so.0
+ /usr/lib/x86_64-linux-gnu/libffi.so.6
+ /lib/x86_64-linux-gnu/libpcre.so.3
+ /lib/x86_64-linux-gnu/libpthread.so.0
```

On Darwin, this resolves the library paths fully.
To reverse that, feed the library path to `otool -D`.

On Linux, this returns the computed path from which the shared library was loaded.
To normalize the path instead, use `readlink`.

## Building

Building is straightforward: just run `make`.

Run tests with `./test-libwhich`.
