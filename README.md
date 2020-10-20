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

lock_test需要打开两个终端

先使用下述指令查询已启动的容器

> docker ps

然后使用下述指令进入我们的容器

> sudo docker exec -it cf0affa7d25c /bin/bash

在其中一个终端运行

> ./lock_server 3772

然后在另一个终端运行

> ./lock_tester 3772

## part3

首先需要在 yfs_client.cc | lock_smain.cc | lock_tester.cc 中把相关的内容注释掉

lock_client_cache 相当于 part2中的 lock_server ，这里相当于加了一层实现锁的缓存从而优化了性能

- acquire
  
  当一个 client clt 接下来会多次发送 acquire 给 server sv 时，只需要在第一次 acquire 时发送请求获得锁 lid ，然后 clt 保持拥有 lid 直到有另外的 client clt1 向 sv 发送 acquire（clt1 中 lid 处于 ACQUIRING 状态)，这时 sv 给 clt 发送 REVOKE
  
- release
  
  client clt 收到线程的 release 后，将 lid 状态置为 FREE，如果已经收到过 REVOKE，则立即向 sv 发送release，sv 立马将 lid 释放掉并给等待队列中的 clt1 发送 RETRY， clt1 处于 ACQUIRING 中，这时 clt1 成功获得锁

- revoke

  将对应的锁标记为收到 REVOKE， 如果这时刚好处于 FREE，则直接向 sv 发送 release

- retry

  若 ACQUIRING 则直接置为 LOCKED

我们来举出具体场景便于理解

- 00> clt0 thread0 acquires lid -> DONE | LOCKED

- 01> clt0 thread1 acquires lid -> WAIT | 

- 02> clt0 thread2 acquires lid -> BLOCKED

- 03> clt0 thread3 acquires lid -> BLOCKED

- 04> clt0 thread0 releases lid -> DONE | 01 DONE | 02 WAIT

- 05> clt1 thread0 acquires lid -> WAIT | sv --REVOKE--> clt0 -> clt0 revoke TRUE

- 06> clt0 thread4 acquires lid -> BLOCKED

- 07> clt1 thread1 acquires lid -> BLOCKED

- 08> clt1 thread2 acquires lid -> BLOCKED

- 09> clt0 thread1 releases lid -> DONE | sv --RETRY---> clt1 | 05 DONE

好吧我感觉上面这个也不是很好让人理解，害

---

***遇到的一些问题***

```shell
lock client
create lock_client_cache 0      127.0.0.1:20273
create lock_client_cache 1      127.0.0.1:3351
create lock_client_cache 2      127.0.0.1:7265
create lock_client_cache 3      127.0.0.1:22318
create lock_client_cache 4      127.0.0.1:3328
create lock_client_cache 5      127.0.0.1:25937
test1: acquire a release a acquire a release a
1602672600483:  lock_client_cache: 127.0.0.1:20273 acquiring 1 ...
1602672600484:  lock_client_cache: 127.0.0.1:20273 acquire 1 -> LOCKED
1602672600484:  lock_client_cache: 127.0.0.1:20273 releasing 1 ...
1602672600484:  lock_client_cache: 127.0.0.1:20273 release 1 -> FREE
1602672600484:  lock_client_cache: 127.0.0.1:20273 acquiring 1 ...
1602672600484:  lock_client_cache: 127.0.0.1:20273 acquire 1 -> LOCKED at once
1602672600484:  lock_client_cache: 127.0.0.1:20273 releasing 1 ...
1602672600484:  lock_client_cache: 127.0.0.1:20273 release 1 -> FREE
test1: acquire a acquire b release b release a
1602672600484:  lock_client_cache: 127.0.0.1:20273 acquiring 1 ...
1602672600484:  lock_client_cache: 127.0.0.1:20273 acquire 1 -> LOCKED at once
1602672600484:  lock_client_cache: 127.0.0.1:20273 acquiring 2 ...
1602672600486:  lock_client_cache: 127.0.0.1:20273 acquire 2 -> LOCKED
1602672600487:  lock_client_cache: 127.0.0.1:20273 releasing 2 ...
1602672600487:  lock_client_cache: 127.0.0.1:20273 release 2 -> FREE
1602672600487:  lock_client_cache: 127.0.0.1:20273 releasing 1 ...
1602672600487:  lock_client_cache: 127.0.0.1:20273 release 1 -> FREE
test1: pased
test2: client 2 acquire a release a
test2: client 2 acquire a
1602672600489:  lock_client_cache: 127.0.0.1:7265 acquiring 1 ...
test2: client 5 acquire a release a
test2: client 5 acquire a
1602672600491:  lock_client_cache: 127.0.0.1:25937 acquiring 1 ...
test2: client 4 acquire a release a
test2: client 4 acquire a
1602672600491:  lock_client_cache: 127.0.0.1:3328 acquiring 1 ...
test2: client 3 acquire a release a
test2: client 3 acquire a
1602672600491:  lock_client_cache: 127.0.0.1:22318 acquiring 1 ...
test2: client 0 acquire a release a
test2: client 0 acquire a
1602672600491:  lock_client_cache: 127.0.0.1:20273 acquiring 1 ...
1602672600491:  lock_client_cache: 127.0.0.1:20273 acquire 1 -> LOCKED at once
test2: client 0 acquire done
test2: client 1 acquire a release a
test2: client 1 acquire a
1602672600492:  lock_client_cache: 127.0.0.1:3351 acquiring 1 ...
1602672600492:  lock_client_cache: 127.0.0.1:20273 revoking 1 ...
1602672600492:  lock_client_cache: 127.0.0.1:20273 revoke 1 -> REVOKE
1602672600493:  lock_client_cache: 127.0.0.1:7265 revoking 1 ...
1602672600493:  lock_client_cache: 127.0.0.1:7265 revoke 1 -> REVOKE
1602672600498:  lock_client_cache: 127.0.0.1:25937 revoking 1 ...
1602672600498:  lock_client_cache: 127.0.0.1:25937 revoke 1 -> REVOKE
1602672600498:  lock_client_cache: 127.0.0.1:7265 acquire 1 -> LOCKED
test2: client 2 acquire done
error: server granted 0000000000000001 twice
error: server granted 0000000000000001 twice
client 0        0000000000000001        LOCKED
client 1        0000000000000001        ACQUIRING
client 2        0000000000000001        LOCKED
client 3        0000000000000001        ACQUIRING
client 4        0000000000000001        ACQUIRING
client 5        0000000000000001        ACQUIRING
```

这里 client 0 和 3 同时获得了锁，是因为在 revoke_handler 中 lid 没有 FREE 时的返回值也为 OK 导致 server 中调用 revoke 后的处理语块（如下）以为当前拥有锁的 clt0 已经释放了，sv 就给了 clt3 锁

```cpp
if (cl) // safebind success
{
  int r;
  pthread_mutex_unlock(&mutex); // avoid deadlock
  ret = cl->call(rlock_protocol::revoke, lid, r);
  pthread_mutex_lock(&mutex);

  if (ret != rlock_protocol::OK) {
    pthread_mutex_unlock(&mutex);
    tprintf("lock_server_cache: %s acquire %llu -> RETRY\n", id.c_str(), lid);
    return rlock_protocol::RETRY;
  }

  else {
    pthread_mutex_unlock(&mutex);
    tprintf("lock_server_cache: %s acquire %llu -> OK\n", id.c_str(), lid);
    return rlock_protocol::OK;
  }
}
```

---

在 test 4 and 5 中遇到 ***其中一个线程不执行*** 的问题

```shell
test 4
test4: thread 0 acquire a release a concurrent; same clnt
test4-0-0: start
test4: thread 1 acquire a release a concurrent; same clnt
test4-1-0: start
1602676434913:  lock_client_cache: 127.0.0.1:22461--0 acquiring 1 ...
1602676434913:  lock_client_cache: 127.0.0.1:22461--1 acquiring 1 ...
1602676434913:  lock_client_cache: 127.0.0.1:22461--1 acquire 1 -> wait free
1602676434913:  lock_client_cache: 127.0.0.1:22461--0 send acquire 1 to server
1602676434914:  lock_client_cache: 127.0.0.1:21556 revoking 1 ...
1602676434914:  lock_client_cache: 127.0.0.1:22461 retrying 1 ...
1602676434914:  lock_client_cache: 127.0.0.1:22461 retry -> 1
1602676434914:  lock_client_cache: 127.0.0.1:22461--1 acquire 1 -> wait free
1602676434915:  lock_client_cache: 127.0.0.1:21556 revoke 1 -> RELEASE
1602676434917:  lock_client_cache: 127.0.0.1:22461--0 acquire 1 -> LOCKED
test4-0-0: thread 0 on client 0 got lock
1602676434917:  lock_client_cache: 127.0.0.1:22461--0 releasing 1 ...
1602676434917:  lock_client_cache: 127.0.0.1:22461--0 release 1 -> FREE
test4-0-0: thread 0 on client 0 release lock
test4-0-1: start
1602676434917:  lock_client_cache: 127.0.0.1:22461--0 acquiring 1 ...
1602676434917:  lock_client_cache: 127.0.0.1:22461--0 acquire 1 -> LOCKED at once
test4-0-1: thread 0 on client 0 got lock
1602676434918:  lock_client_cache: 127.0.0.1:22461--0 releasing 1 ...
1602676434918:  lock_client_cache: 127.0.0.1:22461--0 release 1 -> FREE
test4-0-1: thread 0 on client 0 release lock
test4-0-2: start
1602676434918:  lock_client_cache: 127.0.0.1:22461--0 acquiring 1 ...
1602676434918:  lock_client_cache: 127.0.0.1:22461--0 acquire 1 -> LOCKED at once
test4-0-2: thread 0 on client 0 got lock
1602676434918:  lock_client_cache: 127.0.0.1:22461--0 releasing 1 ...
1602676434918:  lock_client_cache: 127.0.0.1:22461--0 release 1 -> FREE
test4-0-2: thread 0 on client 0 release lock
test4-0-3: start
1602676434918:  lock_client_cache: 127.0.0.1:22461--0 acquiring 1 ...
1602676434918:  lock_client_cache: 127.0.0.1:22461--0 acquire 1 -> LOCKED at once
test4-0-3: thread 0 on client 0 got lock
1602676434918:  lock_client_cache: 127.0.0.1:22461--0 releasing 1 ...
1602676434918:  lock_client_cache: 127.0.0.1:22461--0 release 1 -> FREE
test4-0-3: thread 0 on client 0 release lock
test4-0-4: start
1602676434918:  lock_client_cache: 127.0.0.1:22461--0 acquiring 1 ...
1602676434918:  lock_client_cache: 127.0.0.1:22461--0 acquire 1 -> LOCKED at once
test4-0-4: thread 0 on client 0 got lock
1602676434918:  lock_client_cache: 127.0.0.1:22461--0 releasing 1 ...
1602676434918:  lock_client_cache: 127.0.0.1:22461--0 release 1 -> FREE
test4-0-4: thread 0 on client 0 release lock
test4-0-5: start
1602676434918:  lock_client_cache: 127.0.0.1:22461--0 acquiring 1 ...
1602676434918:  lock_client_cache: 127.0.0.1:22461--0 acquire 1 -> LOCKED at once
test4-0-5: thread 0 on client 0 got lock
1602676434918:  lock_client_cache: 127.0.0.1:22461--0 releasing 1 ...
1602676434918:  lock_client_cache: 127.0.0.1:22461--0 release 1 -> FREE
test4-0-5: thread 0 on client 0 release lock
test4-0-6: start
1602676434918:  lock_client_cache: 127.0.0.1:22461--0 acquiring 1 ...
1602676434918:  lock_client_cache: 127.0.0.1:22461--0 acquire 1 -> LOCKED at once
test4-0-6: thread 0 on client 0 got lock
1602676434918:  lock_client_cache: 127.0.0.1:22461--0 releasing 1 ...
1602676434918:  lock_client_cache: 127.0.0.1:22461--0 release 1 -> FREE
test4-0-6: thread 0 on client 0 release lock
test4-0-7: start
1602676434918:  lock_client_cache: 127.0.0.1:22461--0 acquiring 1 ...
1602676434918:  lock_client_cache: 127.0.0.1:22461--0 acquire 1 -> LOCKED at once
test4-0-7: thread 0 on client 0 got lock
1602676434918:  lock_client_cache: 127.0.0.1:22461--0 releasing 1 ...
1602676434918:  lock_client_cache: 127.0.0.1:22461--0 release 1 -> FREE
test4-0-7: thread 0 on client 0 release lock
test4-0-8: start
1602676434919:  lock_client_cache: 127.0.0.1:22461--0 acquiring 1 ...
1602676434919:  lock_client_cache: 127.0.0.1:22461--0 acquire 1 -> LOCKED at once
test4-0-8: thread 0 on client 0 got lock
1602676434919:  lock_client_cache: 127.0.0.1:22461--0 releasing 1 ...
1602676434919:  lock_client_cache: 127.0.0.1:22461--0 release 1 -> FREE
test4-0-8: thread 0 on client 0 release lock
test4-0-9: start
1602676434919:  lock_client_cache: 127.0.0.1:22461--0 acquiring 1 ...
1602676434920:  lock_client_cache: 127.0.0.1:22461--0 acquire 1 -> LOCKED at once
test4-0-9: thread 0 on client 0 got lock
1602676434920:  lock_client_cache: 127.0.0.1:22461--0 releasing 1 ...
1602676434920:  lock_client_cache: 127.0.0.1:22461--0 release 1 -> FREE
test4-0-9: thread 0 on client 0 release lock
```

如上，线程 1 调用了但是不执行，原因在于 clt0 thread0 release 后，thread 还处于 cond_wait，需要使用 pthread_cond_signal(&(locks_cond[lid])) 唤醒

然后我们比较前后的 RPC 数量

> export RPC_LOSSY = 5
>
> export RPC_COUNT = 25

然后运行test-lab2-part3-b

- [无 cache](./test-lab2-part3-b.log) 大约为2800
- 有 cache

```log
RPC STATS: 1:2 7001:20 7002:3
RPC STATS: 1:2 7001:45 7002:3
RPC STATS: 1:2 7001:70 7002:3
RPC STATS: 1:2 7001:95 7002:3
RPC STATS: 1:2 7001:120 7002:3
RPC STATS: 1:2 7001:145 7002:3
RPC STATS: 1:2 7001:170 7002:3
RPC STATS: 1:2 7001:195 7002:3
```

基本满足性能提升的要求

## passed all

part3 最后成功的输出在 [test-lab2-part3.out](./test-lab2-part3.out) 中

测试文件中较原版本中新增的为方便 debug 的输出（提交版本已注释）

提交

> make handin

重命名为 lab2_${student id}.tgz，提交到 [FTP](ftp://esdeath:public@public.sjtu.edu.cn/upload/cse/lab2/)
