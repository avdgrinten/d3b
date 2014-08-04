# d3b

d3b is a lightweight document-oriented database.

## Features

- Fast queries and transactions; no unnecessary abstractions.
  Data is stored in a "storages" which are key-value stores with auto-generated keys.
  It can be accessed by building "views" which map user-defined properties to documents.
- ACID semantics. All guarantees about transactions you know from SQL. If a transaction completes
  it will stay in the database even if the server crashes.
- Multiversioning. Access past revisions of the database just like the current revision.
- Fast API based on Google Protocol Buffers. If there is no native API for your
  programming language it is quite easy to build it yourself provided there is a Google Protocol Buffers binding.

## Non-features

- No complex queries. No equivalent for JOIN and similar SQL features.
- No complex constraints.


## Building

Run `make -C shard` to build the server.
