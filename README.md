# cse-2020-fall lab2

> 📕 从本lab开始，将在README中记录完成的过程

## getting-started

起步的时候遇到一些问题

```shell
starting ./lock_server 4255 > lock_server.log 2>&1 &
starting ./extent_server 4249 > extent_server.log 2>&1 &
starting ./yfs_client /home/stu/devlop/lab12/yfs1 4249 4255 > yfs_client1.log 2>&1 &
./start.sh: line 59:  3375 Aborted                 ./extent_server $EXTENT_PORT > extent_server.log 2>&1
starting ./yfs_client /home/stu/devlop/lab12/yfs2 4249 4255 > yfs_client2.log 2>&1 &
fusermount: failed to unmount /home/stu/devlop/lab12/yfs1: Invalid argument
fusermount: failed to unmount /home/stu/devlop/lab12/yfs2: Invalid argument
extent_server: no process found
./start.sh: line 75:  3387 Terminated              ./yfs_client $YFSDIR1 $EXTENT_PORT $LOCK_PORT > yfs_client1.log 2>&1
./start.sh: line 75:  3395 Terminated              ./yfs_client $YFSDIR2 $EXTENT_PORT $LOCK_PORT > yfs_client2.log 2>&1
Failed to mount YFS properly at ./yfs1
FATAL: Your YFS client has failed to mount its filesystem!
```

这个问题是由于rpc挂载（mount）失败，源于lab2合并的时候手动解决冲突的时候，很有可能是在[extent_client.cc](./extent_client.cc)中误删的一些内容（误以为与lab1一样）

## part1

这一部分主要是对lab1的检验，或者说是把lab1实现的功能转移到rpc中，主要是对[extent_client.cc](./extent_client.cc)进行更改，如：

before(lab1)

```cpp
extent_protocol::status
extent_client::put(extent_protocol::extentid_t eid, std::string buf)
{
  extent_protocol::status ret = extent_protocol::OK;
  int r;
  ret = es->put(eid, buf, r);
  return ret;
}
```

after(lab2)

```cpp
extent_protocol::status
extent_client::put(extent_protocol::extentid_t eid, std::string buf)
{
  int r;
  extent_protocol::status ret = extent_protocol::OK;
  // Your lab2 part1 code goes here
  ret = cl->call(extent_protocol::put, eid, buf, r);
  return ret;
}
```

这样就能通过rpc远程调用put函数，从而实现分布式文件系统

```shell
starting ./lock_server 24866 > lock_server.log 2>&1 &
starting ./extent_server 24860 > extent_server.log 2>&1 &
starting ./yfs_client /home/stu/devlop/lab12/yfs1 24860 24866 > yfs_client1.log 2>&1 &
starting ./yfs_client /home/stu/devlop/lab12/yfs2 24860 24866 > yfs_client2.log 2>&1 &
Passed part1 A
Passed part1 B
Passed part1 C
Passed part1 D
Passed part1 E
Passed part1 G (consistency)
Lab2 part 1 passed
Concurrent creates: test-lab2-part2-a: wanted 20 dir entries, got 10
Failed test-part2-a: pass 0/5
Create/delete in separate directories: tests completed OK
Passed part2 B
yfs_client: no process found

Score: 70/120
```

## part2

这一部分是完成lock的服务端和客户端

- 客户端很容易就像extent_client一样只用调rpc

- 服务端需要对lock_id lid上锁，使用mutex/cond

```cpp
lock_protocol::status
lock_server::acquire(int clt, lock_protocol::lockid_t lid, int &r)
{
  lock_protocol::status ret = lock_protocol::OK;
  // Your lab2 part2 code goes here

  pthread_mutex_lock(&mutex);

  if (locks_status.find(lid) != locks_status.end())
  {
    // if lock lid has been locked, wait until unlock, then lock it
    while (locks_status[lid] == lock_protocol::LOCKED) {
      pthread_cond_wait(&cond, &mutex);
    }
    locks_status[lid] = lock_protocol::LOCKED;
  }

  // if lock table don't have a lock lid, directly lock it
  else {
    locks_status[lid] = lock_protocol::LOCKED;
  }

  printf("lock_server: lock %llu\n", lid);
  r = lock_protocol::OK;
  pthread_mutex_unlock(&mutex);

  return ret;
}
```

release 与此类似

[这篇文章](https://zhuanlan.zhihu.com/p/58838318)讲解得很详细了

***但是到这里还没由完成这个部分***，需要在[yfs_client.cc](./yfs_client.cc)中调用extent_client的地方进行加/解锁的操作，这一操作简而言之就是在需要对某一inode操作的时候对这个inode上锁即可

遇到了一些坑

```cpp
int
yfs_client::create(inum parent, const char *name, mode_t mode, inum &ino_out)
{
    // printf("YFS: _create '%s' in %llu\n", name, parent);
    int r = OK;
    bool found = false;

    if ((r = lookup(parent, name, found, ino_out)) != OK)
    {
        printf("YFS: _create Error: cannot open dir inode %llu", parent);
        return r;
    }

    if (found)
    {
        printf("YFS: _create Error: file '%s' has been existed\n", filename(name).c_str());
        return EXIST;
    }


    LOCK(parent);
    // alloc inode for the new file
    if ((r = ec->create(extent_protocol::T_FILE, ino_out)) != extent_protocol::OK)
    {
        printf("YFS: _create Error: cannot allocate inode for file '%s'\n", filename(name).c_str());
        UNLOCK(parent);
        return r;
    }

    /* add entry to the dir */
    // get dir
    string buf;
    if ((r = ec->get(parent, buf)) != extent_protocol::OK)
    {
        printf("YFS: _create Error: cannot open dir inode %llu\n", parent);
        UNLOCK(parent);
        return r;
    }

    // write back to parent
    string _entry_;
    if ((r = ec->put(parent, buf.insert(buf.size(), (_entry_ = entry(name, ino_out))))) != extent_protocol::OK)
    {
        printf("YFS: _create Error: connot write entry '%s' to dir\n", _entry_.c_str());
        UNLOCK(parent);
        return r;
    }

    // printf("\tYFS: _create: create file '%s' with inode %llu in dir\n", name, ino_out);
    UNLOCK(parent);
    return OK;
}
```

这里 lookup函数里面也是对parent上锁，这步没问题。但是当lookup完后的一段时间里，parent是没有锁的，这时候两个进程同时lookup发现 parent 目录上都没有 'zz-0' 这一文件，就会出现重复创建如下

```shell
Concurrent creates of the same file:
./yfs1/d313/zz-0
./yfs2/d313/zz-0
./yfs1/d313/zz-1
./yfs2/d313/zz-2
./yfs1/d313/zz-2
./yfs2/d313/zz-3
./yfs1/d313/zz-3
./yfs2/d313/zz-4
./yfs1/d313/zz-4
./yfs2/d313/zz-5
./yfs1/d313/zz-5
./yfs2/d313/zz-6
./yfs1/d313/zz-6
./yfs2/d313/zz-7
./yfs1/d313/zz-7
./yfs2/d313/zz-8
./yfs1/d313/zz-8
./yfs2/d313/zz-9
./yfs1/d313/zz-9
dircheckn:
./yfs1/d313/zz-0 got
./yfs1/d313/zz-0 got
./yfs1/d313/zz-1 got
./yfs1/d313/zz-2 got
./yfs1/d313/zz-3 got
./yfs1/d313/zz-4 got
./yfs1/d313/zz-5 got
./yfs1/d313/zz-6 got
./yfs1/d313/zz-7 got
./yfs1/d313/zz-8 got
./yfs1/d313/zz-9 got
test-lab2-part2-a: wanted 10 dir entries, got 11
```

所以这里把 lookup 中的锁去掉，我们把调用 lookup 的函数提前对 parent 上锁，然后就通过了
