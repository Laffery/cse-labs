# CSE-2020-fall Lab4-Data Cache

## Getting Started

将lab4的文件合并后

> ./grade_lab2.sh

120/120

> make lab3
>
> ./grade_lab3.sh

11/11

## Part1-Improve YFS with Data Cache

同lab1pp，首先测试lab1的性能

```sh
./start.sh
./fxmark/bin/fxmark --type=YFS --root=./yfs1 --ncore=1 --duration=10
./stop.sh
```

得到结果

```sh
# native # ncpu secs works works/sec
1 10.452665 2176.000000 208.176575
# yfs # ncpu secs works works/sec
1 11.201853 640.000000 57.133405
```

在ec中加入cache，考虑对inode的内容和属性加cache，使用map存放数据，从而实现缓存，实现得不是很复杂，这里就不具体展开叙述

为了显示出ec_cache对性能的提升作用，我们将不在yfs中使用锁（lab1中未使用），得到结果如下

```sh
# natice # ncpu secs works works/sec
1 10.458508 2304.000000 220.299110
# yfs # ncpu secs works works/sec
1 10.269063 1152.000000 112.181608
```

## Part2-Design a Cache Coherence Protocol

当存在两个clt的时候，问题就显现出来了，在lab2-part1-g，yfs1在根目录下创建了文件，而yfs2在初始化的时候会创建根目录，因而根目录也在缓存中，但是yfs2中却不是yfs1创建文件后的新值，因而出现了一致性的冲突

因此我们需要为cache设计一套协议，我初步的想法是，server为每个inode维护一个版本号，当一个clt更新某个值后，版本号增加；而在client端，get的时候首先检查本地的版本号是否与服务端的一致。但是这样如果版本不一致就需要与服务端进行两次通信，就此作罢

那么好点的办法就是当一个clt更新数据时，server发送消息给所有的client让它们更新缓存（如果正好在缓存中的话），这样在别的clt空闲的时候也能同步数据，减少了闲暇时间

由于extent_server中要调用extent_client中的函数，我们仿造lock_client_cache中的代码，同时在GNUmakefile中yfs_client(line 76)和extent_server(line 82)加上handle.cc

实现了之后遇到一个新的问题，就是lookup的时候找不到文件了，输出后发现最终是cache中取出的data值有问题，这让人感到十分迷惑，即直接赋值给cache[eid].data为什么会不一样

由于时间关系就先做到这里，此时grade_lab2.sh是通过的

## Performance Evaluation

我们

> ./grade_lab4.sh

```log
# ncpu secs works works/sec
1 1.876785 128.000000 68.201739
./grade_lab4.sh: line 14: bc: command not found
./grade_lab4.sh: line 14: [: -eq: unary operator expected
Failed fxmark performance
```

看起来与目标远远不够，可能由于计算机性能的问题，我又在云上跑了一遍

```log
# ncpu secs works works/sec
1 1.090053 1024.000000 939.403864
./grade_lab4.sh: line 14: bc: command not found
./grade_lab4.sh: line 14: [: -eq: unary operator expected
Failed fxmark performance
```

差不多达到了要求（这个地方的bc报错很奇怪）

## Attention

提交的版本中

1. extent_client带有cache，若要运行原生extent_client，将extent_protocol.h中的line 8

    ```cpp
    #define CACHE
    ```

    注释掉即可

2. yfs_client带有lock，若要运行不带有lock的yfs_client，将yfs_client.h中的line 16

    ```cpp
    #define _LOCK_
    ```

    注释掉即可

另一方面，本人在测试的时候遇到很迷惑的情形，即未作任何修改时，不同次运行grade_lab2.sh有时候会不能通过，大部分时间是通过的，而在做lab2的时候并未遇见

---

由于期末临近时间关系，此版本仓促之间完成，后续若有时间将继续更新优化

于2020.12.06 22:06
