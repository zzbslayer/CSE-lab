// yfs client.  implements FS operations using extent and lock server
#include "yfs_client.h"
#include "extent_client.h"
#include <sstream>
#include <iostream>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#define FILENAME_MAX 60

const int step = FILENAME_MAX + 4;

yfs_client::yfs_client()
{
    ec = new extent_client();
}

yfs_client::yfs_client(std::string extent_dst, std::string lock_dst)
{
    ec = new extent_client();
    if (ec->put(1, "") != extent_protocol::OK)
        printf("error init root dir\n"); // XYB: init root dir
}

yfs_client::inum
yfs_client::n2i(std::string n)
{
    std::istringstream ist(n);
    unsigned long long finum;
    ist >> finum;
    return finum;
}

std::string
yfs_client::filename(inum inum)
{
    std::ostringstream ost;
    ost << inum;
    return ost.str();
}

bool
yfs_client::isfile(inum inum)
{
    extent_protocol::attr a;

    if (ec->getattr(inum, a) != extent_protocol::OK) {
        printf("error getting attr\n");
        return false;
    }

    if (a.type == extent_protocol::T_FILE) {
        printf("isfile: %lld is a file\n", inum);
        return true;
    }
    printf("isfile: %lld is not a file\n", inum);
    return false;
}
/** Your code here for Lab...
 * You may need to add routines such as
 * readlink, issymlink here to implement symbolic link.
 * 
 * */

bool 
yfs_client::issymlink(inum inum)
{
    extent_protocol::attr a;
    if (ec->getattr(inum, a) != extent_protocol::OK || a.type != extent_protocol::T_SYMLINK)
        return false;
    return true;
}

bool
yfs_client::isdir(inum inum)
{
    // Oops! is this still correct when you implement symlink?
    extent_protocol::attr a;

    if (ec->getattr(inum, a) == extent_protocol::OK && a.type == extent_protocol::T_DIR)
        return true;
    return false;
}

int
yfs_client::getfile(inum inum, fileinfo &fin)
{
    int r = OK;

    printf("getfile %016llx\n", inum);
    extent_protocol::attr a;
    if (ec->getattr(inum, a) != extent_protocol::OK) {
        r = IOERR;
        goto release;
    }

    fin.atime = a.atime;
    fin.mtime = a.mtime;
    fin.ctime = a.ctime;
    fin.size = a.size;
    printf("getfile %016llx -> sz %llu\n", inum, fin.size);

release:
    return r;
}

int
yfs_client::getsymlink(inum inum, symlinkinfo &sym)
{
    int r = OK;

    printf("getlink %016llx\n", inum);
    extent_protocol::attr a;
    if (ec->getattr(inum, a) != extent_protocol::OK) {
        r = IOERR;
        goto release;
    }
    sym.atime = a.atime;
    sym.mtime = a.mtime;
    sym.ctime = a.ctime;
    sym.size = a.size;
    printf("getlink %016llx -> sz %llu\n", inum, sym.size);
release:
    return r;
}

int
yfs_client::getdir(inum inum, dirinfo &din)
{
    int r = OK;

    printf("getdir %016llx\n", inum);
    extent_protocol::attr a;
    if (ec->getattr(inum, a) != extent_protocol::OK) {
        r = IOERR;
        goto release;
    }
    din.atime = a.atime;
    din.mtime = a.mtime;
    din.ctime = a.ctime;

release:
    return r;
}


#define EXT_RPC(xx) do { \
    if ((xx) != extent_protocol::OK) { \
        printf("EXT_RPC Error: %s:%d \n", __FILE__, __LINE__); \
        r = IOERR; \
        goto release; \
    } \
} while (0)

// Only support set size of attr
int
yfs_client::setattr(inum ino, size_t size)
{
    //std::cout << "YFS setattr: " << ino << endl;
    int r = OK;
    std::string content;
    extent_protocol::attr attr;
    
    r = ec->getattr(ino, attr);
    if (r != OK) 
        return r;
    r = ec->get(ino, content);
    if (r != OK)
        return r;
    if (attr.size < size) {
        int gap = size - attr.size;
        content.append(gap, 0);
    } 
    else
        content = content.substr(0, size);
    
    r = ec->put(ino, content);
    /*
     * your code goes here.
     * note: get the content of inode ino, and modify its content
     * according to the size (<, =, or >) content length.
     */

    return r;
}

int
yfs_client::create(inum parent, const char *name, mode_t mode, inum &ino_out)
{
    printf("YFS create name: %s\n", name);
    int r = OK;
    bool found = false;
    lookup(parent, name, found, ino_out);
    if (found == true)
        return EXIST;
    ec->create(extent_protocol::T_FILE, ino_out);
    std::string content;
    r = ec->get(parent, content);
    std::cout << "YFS \tparent inum " << parent << std::endl;
    content += name;
    int gap = step - strlen(name);
    content.append(gap, 0);
    *(uint32_t *)(content.c_str()+content.size()-4) = ino_out;    
    r = ec->put(parent, content);
    std::cout << "YFS \tcreate inum " << ino_out << std::endl;
    /*
     * your code goes here.
     * note: lookup is what you need to check if file exist;
     * after create file or dir, you must remember to modify the parent infomation.
     */

    return r;
}

int
yfs_client::mkdir(inum parent, const char *name, mode_t mode, inum &ino_out)
{
    fprintf(stderr, "make dir name: %s\n", name);
    int r = OK;
    bool flag = false;
    inum inode;
    lookup(parent, name, flag, inode);
    if (flag)
        return EXIST;

    r = ec->create(extent_protocol::T_DIR, ino_out);
    std::string content;
    r = ec->get(parent, content);
    content += name;
    int gap = step - strlen(name);
    content.append(gap, 0);

    *(uint32_t *)(content.c_str()+content.size()-4) = ino_out;
    ec->put(parent, content);

    /*
     * your code goes here.
     * note: lookup is what you need to check if directory exist;
     * after create file or dir, you must remember to modify the parent infomation.
     */

    return r;
}

int
yfs_client::lookup(inum parent, const char *name, bool &found, inum &ino_out)
{
    
    int r = OK;
    std::string content;
    r = ec->get(parent, content);
    const char *c_str = content.c_str();
    for (int pos = 0; pos < content.size(); pos += step){
        /*
        std::string temp = content.substr(pos, strlen(name));
        std::string temp2 = std::string(name);
        if (temp == temp2){

            I don't know why the above code cannot work at all
                but the following works well
            
        std::cout << "\tTo find : temp name: " << temp  << "\n";
        */
        if (strcmp(c_str + pos, name) == 0){
            ino_out = *(uint32_t *)(content.c_str() + pos + FILENAME_MAX);
            found = true;
            return r;
        }
    }
    found = false;
    std::cout << "lookup fail: " << name << std::endl;

    /*
     * your code goes here.
     * note: lookup file from parent dir according to name;
     * you should design the format of directory content.
     */

    return r;
}

int
yfs_client::readdir(inum dir, std::list<dirent> &list)
{
    int r = OK;
    std::string content;
    r = ec->get(dir, content);
    int len = content.size();
    for (int pos = 0; pos < len; pos += step){
        struct dirent child;
        child.name = content.substr(pos, FILENAME_MAX);
        child.inum = *(uint32_t *)(content.c_str() + pos + FILENAME_MAX);
        list.push_back(child);
    }
    /*
     * your code goes here.
     * note: you should parse the dirctory content using your defined format,
     * and push the dirents to the list.
     */

    return r;
}

int
yfs_client::read(inum ino, size_t size, off_t off, std::string &data)
{
    int r = OK;
    std::string content;
    r = ec->get(ino, content);
    int len = content.size();

    if (off >= len)
        data = "";
    else {
        int remain = len - off;
        if (size <= remain)
            data = content.substr(off, size);
        else
            data = content.substr(off);
    }
    /*
     * your code goes here.
     * note: read using ec->get().
     */

    return r;
}

int
yfs_client::write(inum ino, size_t size, off_t off, const char *data,
        size_t &bytes_written)
{
    int r = OK;
    std::string content;
    bytes_written = 0;
    r = ec->get(ino, content);
    int len = content.size();

    //std::string sdata = std::string(data);
    std::string sdata(size, 0);
    for (int i = 0; i < size; i++)
        sdata[i] = data[i];
    sdata = sdata.substr(0, size);
    if (len < off){
        int gap = off - len;
        content.append(gap, 0);
        content += sdata;
        bytes_written = off - len + size;
    }
    else {
        content.replace(off, size, sdata);
        bytes_written = size;
    }
    
    ec->put(ino, content);
    /*
     * your code goes here.
     * note: write using ec->put().
     * when off > length of original file, fill the holes with '\0'.
     */

    return r;
}

int yfs_client::unlink(inum parent,const char *name)
{
    int r = OK;
    std::string content;
    r = ec->get(parent, content);

    int len = content.size();
    for (int pos = 0; pos < len; pos += step){
        std::string temp = content.substr(pos, strlen(name));
        std::string temp2 = std::string(name);
        if (temp == temp2){
            uint32_t ino = *(uint32_t *)(content.c_str() + pos + FILENAME_MAX);
            content.erase(pos, step);
            ec->remove(ino);
            ec->put(parent, content);
            return r;
        }     
    }
    /*
     * your code goes here.
     * note: you should remove the file using ec->remove,
     * and update the parent directory content.
     */
    return r;
}


int yfs_client::symlink(const char *link, inum parent, const char *name, inum &ino)
{
    //fprintf(stderr, "YFS symlink : link:%s\n", link);
    //fprintf(stderr, "YFS symlink : name:%s\n", link);
    int r = OK;
    bool flag = false;
    inum inode;
    std::string content;

    lookup(parent, name, flag, inode);
    if (flag == true)
        return EXIST;
    ec->create(extent_protocol::T_SYMLINK, ino);
    r = ec->get(parent, content);
    content += name;
    int gap = step - strlen(name);
    content.append(gap, 0);
    *(uint32_t *)(content.c_str()+content.size()-4) = ino;
    
    ec->put(parent, content);
    ec->put(ino, std::string(link));
    return r;
}

int yfs_client::readlink(inum ino, std::string &result)
{
    int r = OK;
    ec->get(ino, result);
    return r;
}