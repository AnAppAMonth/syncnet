syncnet
=======

A simple synchronous TCP library for Node.js. This is implemented as a [native addon]
(http://nodejs.org/api/addons.html).

**Note before proceeding** that Node is designed to be an asynchronous environment,
and if you want to use this module you should have a really good reason to do so.

Current Status
--------------

Currently only the client part is implemented, with the ability to open a connection
to the server, read/write to the connection, and close the connection. The server part
isn't implemented yet, as I can't think of a useful use case for it.

Origin
------

For example, this module is developed for the [DiffBug](https://github.com/halfninety/diffbug)
project, in which it's used to synchronize between a client process and a server
process. When the client needs to wait for an event to occur, it connects to the
server using this module and blocks on a synchronous `read()`. The server only
writes to the connection when the event occurs.

In most programs, this functionality can be implemented using the asynchronous `net`
module as well, but DiffBug is an automated debugging tool: it needs to be able to
pause at any point in the user's code by inserting a function call at that point.
Therefore, only synchronous blocking can do the trick.

One simple way to achieve synchronous blocking in Node.js is to call `readFileSync()`
on a Unix named pipe. However this isn't as flexible as using TCP and it doesn't work
on Windows.

License
-------

This piece of code is licensed under the MIT license.

