# Windows overlapped io and asio
Entry level overview of how things work for asio on windows.
## Overlapped io
windows async io operations are called overlapped io. [overlapped](https://docs.microsoft.com/en-us/windows/win32/sync/synchronization-and-overlapped-input-and-output)
All the io system calls has parameter `LPOVERLAPPED`, and if called with a value, the function call returns immediately, but the actual work is deferred to the kernel, and may complete at a later time.

For example:
```
BOOL WriteFile(
  [in]                HANDLE       hFile,
  [in]                LPCVOID      lpBuffer,
  [in]                DWORD        nNumberOfBytesToWrite,
  [out, optional]     LPDWORD      lpNumberOfBytesWritten,
  [in, out, optional] LPOVERLAPPED lpOverlapped
);
```

When the user process wants to know if the write completes, it needs to call one of
* [GetOverlappedResult](https://docs.microsoft.com/en-us/windows/win32/api/ioapiset/nf-ioapiset-getoverlappedresult)
* [GetQueuedCompletionStatus](https://docs.microsoft.com/en-us/windows/win32/api/ioapiset/nf-ioapiset-getqueuedcompletionstatus)

either will return the numeber of bytes written.

asio uses `GetQueuedCompletionStatus`.

## I/O Completion Ports (iocp)
[I/O Completion Ports](https://docs.microsoft.com/en-us/windows/win32/fileio/i-o-completion-ports)
is an handle/object that user process creates.
User process can associate file (file handle) to the iocp, and all async(overlapped) io status of this file can be retrived from `GetQueuedCompletionStatus` mentioned above.

## asio usage of overlapped io and iocp
In each asio executor, an iocp will be created.
```cpp
net::io_context ioc;
```
All socket and file handles created with the executor will be associated with the iocp.
```cpp
net::basic_stream_socket<tcp, net::io_context::executor_type> mysock(ioc);
```
socket mysock handle is associated with the iocp inside ioc.
```cpp
ioc.run()
```
After ioc is started to run, it will execute work/callbacks if available, otherwise it will call `GetQueuedCompletionStatus` and wait for async io to complete and then invoke user callbacks.

## How this project uses asio
`boost::asio::windows::overlapped_ptr` is used to communicate between iocp and executor.
```cpp
auto callback = [](boost::system::error_code ec, std::size_t){};
boost::asio::windows::overlapped_ptr optr(ioc.get_executor(), callback);
bool ok = SomeIoSysCall(hHandle, optr.get());
if (!ok) {
  // failure
  ec = boost::system::error_code(::GetLastError(),
                                     boost::system::system_category());
  optr.complete(ec, 0); // callback will be posted to executor immediately.
}else{
  // success
  // release the ownership of the callback to executor.
  // callback will be posted to executor when the async io completed.
  optr.release(); 
}
```
 
In this way all of the overlapped syscalls can be intergrated with asio executors.