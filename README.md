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
