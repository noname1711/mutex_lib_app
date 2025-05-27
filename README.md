Firstly, open a terminal, that will be a server to control all of client.

Open more terminal,and they will run as a client.

In server, command:
```bash
make all
```
to run all of program.

Now, we can build a server by command:
```bash
./bin/server
```
1 client should be create and lock only 1 mutex, prevent deadlock 

And another terminal, build client:
```bash
./bin/client
```
