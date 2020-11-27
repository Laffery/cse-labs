# CSE-2020-fall lab3 YourDataBase

## Getting started

git合并后运行

> ./grade_lab2.sh

## Part1 NONE

在ydb_server.cc里面差不多只用调ec就行了，主要问题是这个测试中间会把ydb_server重启，如果将key保存在这里面重启的时候将get不到任何数据

所以选择使用hash，这里用的简单的hash，将字符串转化成一个uint32_t即inum，由于重启的原因就不缓存了

于是

```sh
Start testing part1 : no transaction

start no transaction test-lab3-durability
[^_^] Pass test-lab3-durability
Passed part1

Finish testing part1 : no transaction

----------------------------------------
```

## Part2 2PL

1. basic

    主要应对错误的transaction id和abort的情况

    我们在server里面加上curr_id来给事务分发id；使用map origin_value来记录事务中set开始前ino对应的数据，change_id则记录ino最近被哪个事务更改过，如果是当前事务则无法修改origin_value的值，保证该map中存的是事务开始前的数据

2. 2-b

    主要应对简单的多线程及死锁的情况

    首先我们给curr_id加锁trans_id_mutex，保证trans_id的唯一性

    ```log
    # op id key val
    set 1, a, 0100
    set 2, b, 1200
    set 3, c, 2300
    set 2, c, 1300
    set 1, b, 0200
    set 3, a, 2100

    get 4, a, 2100
    get 4, b, 0200
    get 4, c, 1300
    ```

    我们看到事务1改变a b，事务2改变b c，事务3改变c a，都会发生死锁，所以当死锁将要发生时，我们按照文档中说的直接返回ABORT。最开始我的实现会让三个都abort，解决的办法就是不直接abort，而是建立依赖关系map depend，探测是否存在闭环的死锁，参见函数detectDL()
