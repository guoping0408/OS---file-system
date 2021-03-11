# OS---file-system
This project implements a file system in the Pintos with features like Buffer Cache, Indexed and Extensible Files using inode structure, and Subdirectories.







**Task 1 – Buffer Cache**


**Buffer Cache Structure**

We define buffer\_cache as global in inode.c, and it is initialized when inode\_init() is called. It will

contain cached\_inodes, a list of cached items. We also keep track of the size of the buffer using

buffer\_size. We have a lock to ensure synchronized access to the cached list.

We define buffer\_cache\_item as a representation of the cached items. The field sector defines

the sector number of the cached block, which we use for reference. We have fields that contain

the data, and a flag that indicates whether this block is dirty. We also have a lock to ensure

synchronization between threads when accessing the cached block.

        struct buffer\_cache {

                struct list cached\_inodes; // ordered by earliest access first

                struct lock cached\_inode\_lock;

                int buffer\_size;

        }

        struct buffer\_cache\_item {

                list\_elem buffer\_elem;

                block\_sector\_t sector; // inode number

                byte data[BLOCK\_SECTOR\_SIZE];

                bool isDirty;

                struct lock buffer\_cache\_item\_lock;

        }

Algorithms

Note: All the synchronization steps for the various algorithms is discussed inside the

Synchronization section.

**Buffer Eviction Policy**

In order to decide when we need to evict buffer\_cache\_item structs from the cached\_inodes list

in buffer\_cache struct, we use the following algorithm whenever we need to add a new buffer:

● We check if the size of the caches\_inodes list is size == 64

● If size == 64, we evict the buffer cache item at the head (front) of the buffer\_cache’s list

● After removing the buffer\_cache\_item, we check the evicted buffer\_cache\_item->isDirty





● If the buffer\_cache\_item’s isDirty flag is TRUE, we call block\_write() to write back to disk

We are hence evicting based on the LRU replacement policy and have designed the cache as a

write-back cache.

**Updates to inode\_reopen & inode\_open**

● We check if the buffer\_cache\_item is inside the buffer\_cache’s list

○ We find the buffer\_cache\_item in the cached\_inodes list by checking if

buffer\_cache\_item->sector is the same as the the output of inode\_get\_inumber()

of the concerned inode

● If it IS NOT inside the buffer cache’s list

○ We create a new buffer\_cache\_tem struct and initialize the struct

○ We byte\_read into the byte[] data block inside the buffer\_cache\_item struct, and

then memcpy the data into the caller’s buffer from our buffer\_cache\_item

○ Then we continue our algorithm as if the inode is inside the buffer\_cache’s list

● If it IS inside the buffer\_cache’s cached\_inodes list

○ We pop the buffer\_cache\_item

○ We push the recently popped buffer\_cache\_item back to the back (push\_back)

**Updates to inode\_read\_at**

● In the while loop that is used to read from disk, we check if the sector\_idx is in the cache.

● If sector\_idx is available in cache, we skip the block read in inode’s block\_read in

inode\_read\_at() and rather return the information from the buffer\_cache to the caller’s

buffer.

● If sector\_idx is not available in cache, we perform the following steps:

○ Read the block from disk using the block\_read function in block.c

○ Check if an item needs to be evicted from the buffer\_cache’s cached\_inodes list,

and take the appropriate steps using the Buffer Eviction Algorithm mentioned

above.

○ Store the read block in the buffer cache

○ Run this algorithm as if the block is available in cache

● Write the desired bytes into the buffer that was provided for output in the block\_read call

**Updates to inode\_write\_at**

● In the while loop that is used to read from disk, check if the sector\_idx is in the cache.

● If sector\_idx is available in the cache, we perform the following steps:

○ Edit the appropriate bytes of the block in the cache

○ Mark the block as dirty (for write to disk during eviction)

● If sector\_idx is not available in the cache, we perform the following steps:

○ Read the block from disk using the block\_read function in block.c

○ Store the read block in the buffer cache

○ Run the write algorithm as if the block is available in cache

○ Mark the block as dirty (for write to disk during eviction)

● Return the bytes written into the block





**Updates to inode\_close**

● If the inode we are closing is dirty, we block\_write the contents from the buffer

**Updates to filesys\_done**

● We flush out all dirty buffer\_cache\_item data

**Updates to inumber(int fd)**

● From project 1 we are able to get the associated file for a given fd using lookup\_fd().

This returns the associated file descriptor struct, which contains the file struct of the

given fd. In the file struct, we have the associated inode. Given this inode, we can return

the unique inode number of the given fd.

Synchronization

○ In all inode functions where we are accessing/modifying the buffer\_cache’s

cached\_inodes list, we acquire buffer\_cache’s cached\_inode\_lock at the start of the

function call, and release the lock at the end of the function call.

○ In all inode functions where we are accessing/modifying items in the buffer\_cache’s

cached\_inodes list, we acquire buffer\_cache\_item’s buffer\_cache\_item\_lock as soon as

we remove the item from the list. We release the lock after inserting it back into the

cached\_inodes list or before discarding the struct.

Rationale

The design decisions of the buffer cache is designed with the acknowledged constraint of a

buffer with a capacity of 64 blocks. We chose to use the LRU eviction policy based on MIN

based on locality assumptions. This design ensures a simple approach to implementing a

buffer.

We also took advantage of the double-ended nature of the pintos list implementation to

efficiently implement the LRU eviction policy - new items are added to the end of the list, and

the next items to be evicted evicted items are removed from the head (front of the list), allowing

this process to run in constant time.

In regards to the write-back nature of the buffer cache, we use a dirty bit to denote if the block

has been modified. As aligned with the expectations of a write-back cache, our code only writes

the data to disk when the data block is evicted, improving the performance by reducing the need

for unnecessary writes back to disk.





Task 2 – Extensible Files

Data Structures

We modify the struct inode\_disk, such that it contains multiple direct data blocks (inode disks)

as well as indirect layers. Given that we need to support file growth of upto 8MiB (2^23 bytes),

we can support the file structure using the following pointers:

○ block\_sector\_t double\_indirect\_ptr = 2^7 \* 2^7 \* 2^9 = 2^23B

○ block\_sector\_t indirect\_ptr = 2^7 \* 2^9 = 2^16B

○ block\_sector\_t direct\_ptr[12] = 2^9 \* 12

○ Total storage exceeds 8MiB

We also add a lock to ensure synchronization when modifying the inode\_disk.

**Changes in inode.c:**

        struct inode {

                …

                struct lock inode\_lock;

        }

        struct inode\_disk {

                …

                block\_sector\_t direct\_ptr[12]

                block\_sector\_t indirect\_ptr

                block\_sector\_t double\_indirect\_ptr

        }

Algorithms

Note: All the synchronization steps for the various algorithms is discussed inside the

Synchronization section.

**File Growth**

We keep track of the inode\_disk’s size through the length field. Whenever we call

inode\_write\_at(), we check if (offset + size) exceeds (start + current size), that means we need

to expand. We use the following steps to check if we need to expand the file:

○ We calculate an int index = (offset + size) / (2 ^ 9):

■ For index 0 to 11, we place it at the direct ptr.

■ For index 11 to 11 + 2^7, we place it to the appropriate block using indirect\_ptr





■ For index 12 + 2^7 through (12 + 2^7 +2^7 \* 2^7), we place it to the appropriate

block using double\_indirect\_ptr

○ In order to execute file growth, we do the following:

■ We use bytes\_to\_sectors(offset + size) - bytes\_to\_sectors(start + old\_length) to

calculate the additional number of sectors we need. Let’s call this

addl\_sector\_count.

■ Recall that we calculate an “index” such that int index = (offset + size) / (2 ^ 9).

■ For *i* ∈ {*old*\_*sector*\_*count*, ... , *addl*\_*sector*\_*count*}

● We need to reference a new pointer block\_sector\_t\* write\_sector\_here,

which will be:

○ &direct\_ptr[i] if i < 12

○ &indirect\_ptr[(index - 12) - i] if 11 < i < 2^7

○ &double\_indirect\_ptr[(index - 12)/2^7][(index - 12) - i] otherwise

● We call file\_map\_allocate(1, write\_sector\_here). We initialize these

sectors with zeros.

■ For the additional sectors we initialized, we find the appropriate index in our

block\_sector\_t pointer tracking and store map the block\_sector\_t.

Synchronization

● Whenever we start the file growth algorithm, we acquire the inode\_lock in the inode

struct. After we finish the file growth algorithm, we release the lock.

Rationale

We designed an effective approach to handling file growth, which needs to be performed

appropriately and safely, and a file storage design which allows fast random accesses. By

adding three attributes: block\_sector\_t direct\_ptr[12], block\_sector\_t

indirect\_ptr, and block\_sector\_t double\_indirect\_ptr to our inode\_disk,

making the filesystem similar to the Unix FFS file system. This format also ensures efficiency for

smaller sized files, while still being able to accommodate larger files.





Task 3 – Subdirectories

Data Structures

We modify struct thread to include a char array of the absolute path of the current working

directory. We also create a new field called curr\_inode to keep track of the current working

directory’s inode.

In directory.c, we make modifications to the struct dir\_entry and struct file to include a boolean

flag isDir. This indicates whether the file/directory entry is a directory, which will be useful as

explained in the following sections.

**In thread.h:**

        struct thread {

                …

                char\* currentWorkingDirectory;

                inode curr\_inode;

        }

**In directory.c:**

        struct dir\_entry {

                …

                bool isDir;

        }

        struct file {

                …
        
                bool isDir;

        }



**Directory Traversal Algorithm**

Implement our directory\_traverse() function which, given an argument (char\* path), recursively

find the inode in our inode array down the path, and return that **inode** back

directory\_tranverse(char\*path) always recombine the thread\_current()->currentWorkingDirectory and the given char\* path into one absolute path (we will 

implement and function called always\_absolute() to do that), and use the absolute path to traverse from the root node to the destination node. (the combination 

will deal with different scenarios when it’s given an absolute, relative, .. or . path)

**always\_absolute(char\* path)**:

always return absolute path given any types of path (relative, ./..)

**->supporting the special “.” and “..” names, when they appear in file path arguments,**

**such as open("../logs/foo.txt").**

➢ if the starting character is not ‘/’, ‘.’, nor ‘..’ (which means it’s relative path), we

concatenate the path to the end of thread\_current()->currentWorkingDirectory

➢ ‘..’ : When dealing with a path that has “../”, we will cancel it out with the last /directory/ of

the thread\_current()->currentWorkingDirectory

➢ “.”: we will remove ‘./’ from the given path and concatenate the rest with the

thread\_current()->currentWorkingDirectory

➢ Everytime we create a new file, we call dir\_add() to create two dir\_entry with names “..”

and “.”

**Support for relative paths for any syscall with a file path argument during inode traversal**

➢ Whenever we call filesys\_open(), filesys\_close(), filesys\_create() that inputs a path, we

must modify these functions to traverse the inputted path

➢ We first use always\_absolute() on the given path to get the an absolute path

➢ Then, we use this absolute path to traverse to the appropriate directory as follows (in the

implementation of directory\_traverse()). We start from the root inode. We use the

get\_next\_part code(path) given to get the next part to search for the dir\_entry with the

matching name. If at any point we cannot find a dir\_entry with this name, we terminate

and return NULL. Else we return the appropriate dir\*

➢ At the end of the traversal, we know we are at the appropriate directory and we can

continue as the normal to create/remove/open a file or directory.

**Child processes should inherit the parent’s current working directory**

➢ We can add a field char \* currentWorkingDirectory for thread struct

➢ The first process starts with root directory as the currentWorkingDirectory

➢ Else, subsequent processes should copy the parent’s currentWorkingDirectory, when we

call syscall\_exec()

New Syscalls Implementation

**chdir(args[1])**

➢ Given the directory path in arg[1] parameter, we call directory\_traverse(arg[1]) to find the

inode associated with the aforementioned directory.

➢ If directory\_traverse returns NULL, we return False for the syscall.





➢ Otherwise, we change thread\_current()->curr\_node = directory\_traverse(arg[1]) and

return true. We also update thread\_current()->currentWorkingDirectory given arg[1].

**mkdir(args[1])**

➢ Given the path in arg[1], we always turn the relative path into an absolute path (by using

always\_absolute(arg[1]) ), and use it to create the new directory. We also check that

there are no duplicate files with the same name.

➢ We call free\_map\_allocate to receive a block\_sector\_t where we can place our directory.

➢ In the parent directory, we call dir\_add using the block\_sector\_t created above. We use

args[1] as the name argument

➢ We call dir\_create using the newly received block\_sector\_t and call dir\_open using the

inode produced by dir\_create. This will result in the creation of a struct dir. In this

directory we add two new dir\_entry structs “.” “..” using dir\_add as well using . and .. as

name arguments. (Everytime we create a new file, we call dir\_add() to create two

dir\_entry with names “..” and “.”)

**readdir (bool readdir(int fd, char name[READDIR\_MAX\_LEN + 1]))**

➢ Use the lookup\_fd(args[1]) to find the inode associated with the directory.

➢ Call dir\_open(inode) to get access to the struct dir\* x

➢ Check if the file associated with the provided FD is either “.” or “..”. If so, we return false

➢ Used the struct dir’s reference to call dir\_readdir(x, args[2]) such that we can store the

output of dir\_readdir in args[2].

➢ Return True

**isdir (bool isdir(int fd))**

➢ We use the provided file descriptor and lookup the inode using the lookup\_fd() function

provided. From the file descriptor, we can check the corresponding file struct and check

file->isDir to check whether it is a directory or not.

➢ The struct file isDir is populated by making modifications to filesys\_open().

○ When we open a new file, we first check the dir\_entry->isDir of the corresponding

file

○ If it is a directory, we set the new file->isDir flag before returning it.

Updates to Existing Syscalls

**open**:

➢ With our above changes to filesys\_open(), we do not need to make any modifications for

the syscall open.

**close**:

➢ Before termination, we need to close the inode using inode\_close(lookup\_fd(fd)->file->inode)


**exec**:

➢ We call always\_absolute() to get the absolute path, and use it go to the appropriate

directory according to the absolute path. Once we are in the appropriate directory, we

look up the file name from the dir\_entry, and we check whether the corresponding

dir\_entry is a file or a directory with dir\_entry->isDir

➢ If it is a directory, we do not execute.

**remove**:

➢ We must check that the only dir\_entry present in the directory are “.” and “..”; if not we

cannot remove. Else, we can call dir\_remove() on the directory

➢ We also may not remove the root directory. We can check by seeing if the directory to be

removed has an inode equal to the root inode

**inumber**:

➢ As in task 2, we get the associated file for a given fd using lookup\_fd(). This returns the

associated file descriptor struct, which contains the file struct of the given fd. In the file

struct, we have the associated inode. Given this inode, we can return the unique inode

number of the given fd.

Synchronization

➢ To ensure synchronization between independent operations, we make use of the lock

we introduced to the inode struct in the previous task.

➢ Whenever we call operations such as write or read, we use lookup\_fd() to retrieve the

associated file\_descriptor struct. From this struct, we can look up file\_descriptor->file->inode to check the file’s inode

➢ Once we have the file’s inode, we can acquire the lock before we do operations such as read and write. Once we are done, we release the lock.

➢ This ensures synchronization at a more granular level, ensuring synchronized operations

between processes on the same file.

Rationale

Initially we wanted to use a global variable to keep track of the current working directory, but this

wouldn’t work well with multithreaded applications. So the current working directory is stored in

the thread struct, so each thread has access to this current working directory and can pass it to

their child processes.

For easy access when dealing with “.” and “..”, we add inode pointers to the current and parent

directories respectively and keep them accessible in every directory so that no matter which

directory we’re in, we can access the current and parent directory when performing directory

traversal.





When implementing the new syscalls, and modifying the existing syscalls, and especially when

handling edge cases a question of finding a method to determine whether or not the file

descriptor passed in is a directory or a file becomes particularly important. The solution that our

group came up with resolved this by checking to see if the dir\_entry only contains ”.” and “..”,

which at this point we know it is a file. For convenience, we added an isDir boolean attribute in

our file and dir\_entry structs to tell us whether we are working with a directory or file.

Additional Questions

**Strategy for write-behind**

We can add a modification to timer\_tick() where after every set number of ticks (we can

calculate if ticks % a number == 0), we perform the following steps:

● We disable interrupts so that no other thread writes to the buffer cache when we are

doing our periodic save.

● Trigger a write call to the buffer\_cache, where for each block flagged as dirty, we write

the data back to disk. We then mark each block that is written as not dirty.

● Once we write back all the dirty blocks back to disk, we enable interrupts.

**Strategy for read-ahead**

● Whenever a cache miss for a block happens, we also pull in blocks subsequent to the

missed block. We put these blocks in the buffer. This improves spatial locality, as

sequential reads would be greatly improved by pulling in subsequent blocks in advance.

● To determine how many subsequent blocks we pull in for read-ahead, we could either

determine a predetermined amount of blocks (ex. 3). We could also dynamically

determine the amount of blocks by seeing how much capacity there is in the buffer.


