# zcsp
zero-copy CSP

## Features

* Coroutines;
* Channels;
* No assumptions about memory allocation;
* No memory footprint outside a coroutine's stack;
* No copies of data put into channels are made.
* No assumptions about polling model: plug in your own, like `epoll`.

## Misfeatures

* Coroutines currently use `ucontext`;
* Thread-unsafe;
* Channels are static;
* Channels only support 1 value;
* An obtained channel data pointer may be invalid after a coroutine suspends.

## API

`<zcsh/zcsh.h>` includes all of the below:

### `<zcsh/cr.h>`: Coroutines

`struct zcr`: Opaque coroutine type.

`size_t zcr_mem()`: Obtains the length in bytes needed for allocating a coroutine.

`struct zcr *zcr_current()`: Obtains the currently running coroutine, or `NULL` if outside one.

`void zcr_spawn_full(struct zcr *cr, void *stk, size_t stk_size, void (*free_cr)(struct zcr *), void (*free_stk)(void *), void (*proc)(va_list), ...)`: Spawns a coroutine with memory allocated at `cr` as large as `zcr_mem()`, with stack memory allocated at `stk` of `stk_size` size in bytes. After it returns, `free_cr` shall be called on `cr`, and `free_stk` on `stk`. The routine `proc` takes the remaining arguments, and they may be invalid after the coroutine suspends.

`void zcr_spawn(void (*proc)(va_list), ...)`: Like above, but automatically allocates needed memory with `malloc`, `mmap`, and frees with `free` and `munmap`.

`void zcr_spawn_flush()`: If `zcr_spawn` is called inside a coroutine, the operation is placed in a queue instead. Call this function outside one to actually perform it.

`void zcr_resume(struct zcr *cr)`: Resumes the given coroutine. Must be called outside one.

`void zcr_suspend_current()`: Suspends the currently running coroutine. Must be called inside one.

### `<zcsh/ch.h>`: Channels

`size_t zch_mem()`: Obtains the length in bytes needed for allocating a single channel.

`void zch_init_full(void *mem)`: Initializes channels with memory allocated at `mem`, which must be large enough for `number_of_desired_channels * zch_mem()`.

`void zch_init(int nids)`: Like above, but automatically allocates memory of size `nids * zch_mem()`.

`void zch_free()`: Frees memory allocated by `zch_init`.

`void *zch_data(int id)`: Obtains the data a channel currently holds.

`int zch_choose(int deadline, int nids, ...)`: Must be called inside a coroutine. Suspends the coroutine until one of the `nids` given channel indices has data put into it, or, when `deadline` is not `-1` and `zch_deadline_pop` is called. Returns the ID of the chosen channel, or `-1` if the deadline was met.

`void zch_put(int id, void *data)`: Puts a value into the channel.

`void zch_put_flush()`: If `zch_put` is called inside a coroutine, the operation is placed in a queue instead. Call this function outside one to actually perform it.

`int zch_deadline()`: Obtains the minimum deadline value from all currently choosing coroutines, or `-1` if there is none.

`void zch_deadline_pop()`: Has the currently choosing coroutine with the minimum deadline value return `-1`.

## Building

```sh
make
sudo make install
```

## TODO

* Add built-in optional polling API, with `select`/`poll`/`epoll`/`kqueue`;
* Support alternatives to `ucontext`, inline asm if needed;
* Remove static variables (hard as long as `makecontext` is required).
