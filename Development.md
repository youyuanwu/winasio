
Testing commands:

```ps1
Invoke-WebRequest http://localhost:12356/winhttpapitest/

Invoke-WebRequest -UseBasicParsing  http://localhost:12356/winhttpapitest/ `
    -ContentType "application/json" -Method POST -Body "{ 'ItemID':3661515, 'Name':'test'}" `
    -Headers @{"myheader"="myvalue"}
```

FYI http.sys supports trailers
```
https://techcommunity.microsoft.com/t5/networking-blog/windows-server-insiders-getting-grpc-support-in-http-sys/ba-p/1534273
https://docs.microsoft.com/en-us/windows/win32/api/http/ne-http-http_data_chunk_type
```

# Clang format
There is a target in cmake dir to run clang format