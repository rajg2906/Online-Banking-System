# Online-Banking-System

A multithreaded banking database system built in C that utilizes a client-server architecture. It supports multiple concurrent users with distinct access roles to perform CRUD operations safely. The system guarantees data consistency and safe concurrent access by employing rigorous synchronization, file locking, and inter-process communication mechanisms.

# Features
1. Role-Based Authorization: Clients provide credentials upon connection, mapping to specific roles that dictate their permissions.
2. Concurrency Control: Spawns a detached POSIX thread for every accepted client connection. Uses a global pthread_mutex_t to serialize database access and strictly prevent race conditions.
3. File Locking: Safeguards database integrity at the file system level using POSIX fcntl locks directly on .db files.
4. Data Consistency: Prevents dirty reads and lost updates by combining thread-level mutexes with disk-level file locking

 # How To Run
 This project includes a Makefile for easy compilation.
1. Compile the Project using make file.
2. Run Server Side on one terminal using
   ./server
3. In a seperate terminal window run the client to connect to the server using
   ./client
