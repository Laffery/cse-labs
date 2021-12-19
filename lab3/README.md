# CSE-2020-fall lab3 YourDataBase

> 😊 Ao Yuchen | 🔑 518021910545

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

3. complex

    我们直接运行，debug输出为

    ```sh
    user0 successfully finishes 69 trades
    user1 successfully finishes 71 trades
    user2 successfully finishes 72 trades
    user3 successfully finishes 74 trades
    user4 successfully finishes 70 trades
    user5 successfully finishes 75 trades
    user6 successfully finishes 67 trades
    user7 successfully finishes 71 trades
    user8 successfully finishes 73 trades
    user9 successfully finishes 69 trades
    user0 money should be 2, actual is 2
        user0 should have 78 product0, actual is 78
        user0 should have 63 product1, actual is 63
        user0 should have 80 product2, actual is 80
        user0 should have 61 product3, actual is 61
        user0 should have 62 product4, actual is 62
    user1 money should be 1, actual is 1
        user1 should have 71 product0, actual is 71
        user1 should have 62 product1, actual is 62
        user1 should have 73 product2, actual is 73
        user1 should have 75 product3, actual is 75
        user1 should have 57 product4, actual is 57
    user2 money should be 1, actual is 1
        user2 should have 79 product0, actual is 79
        user2 should have 62 product1, actual is 62
        user2 should have 64 product2, actual is 64
        user2 should have 51 product3, actual is 51
        user2 should have 80 product4, actual is 80
    user3 money should be 2, actual is 2
        user3 should have 93 product0, actual is 93
        user3 should have 77 product1, actual is 77
        user3 should have 67 product2, actual is 67
        user3 should have 70 product3, actual is 70
        user3 should have 54 product4, actual is 54
    user4 money should be 0, actual is 0
        user4 should have 58 product0, actual is 58
        user4 should have 52 product1, actual is 52
        user4 should have 81 product2, actual is 81
        user4 should have 75 product3, actual is 75
        user4 should have 59 product4, actual is 59
    user5 money should be 3, actual is 3
        user5 should have 80 product0, actual is 80
        user5 should have 75 product1, actual is 75
        user5 should have 72 product2, actual is 72
        user5 should have 64 product3, actual is 64
        user5 should have 59 product4, actual is 59
    user6 money should be 3, actual is 3
        user6 should have 72 product0, actual is 72
        user6 should have 65 product1, actual is 65
        user6 should have 66 product2, actual is 66
        user6 should have 73 product3, actual is 73
        user6 should have 61 product4, actual is 61
    user7 money should be 0, actual is 0
        user7 should have 64 product0, actual is 64
        user7 should have 57 product1, actual is 57
        user7 should have 74 product2, actual is 74
        user7 should have 60 product3, actual is 60
        user7 should have 72 product4, actual is 72
    user8 money should be 0, actual is 0
        user8 should have 87 product0, actual is 87
        user8 should have 64 product1, actual is 64
        user8 should have 76 product2, actual is 76
        user8 should have 63 product3, actual is 63
        user8 should have 61 product4, actual is 61
    user9 money should be 3, actual is 3
        user9 should have 64 product0, actual is 64
        user9 should have 73 product1, actual is 73
        user9 should have 85 product2, actual is 85
        user9 should have 73 product3, actual is 73
        user9 should have 48 product4, actual is 48
    product0 count should be 254, actual is 310
    error: product count error
    product1 count should be 350, actual is 440
    error: product count error
    product2 count should be 262, actual is 443
    error: product count error
    product3 count should be 335, actual is 508
    error: product count error
    product4 count should be 387, actual is 550
    error: product count error
    [x_x] Fail test-lab3-part2-3-complex
    ```

    可以看到目前的问题在于商品的数量存在问题，实际数量显然高于理论数量

    我们来梳理一下set的过程

    - 首先获取对应的锁
    - 查询change_id表看上一次修改这个值的事务tid，如果就是本事务则直接put
    - 不是本事务，那么就检查tid的状态
      - BEGIN: tid事务还在进行中，需要进行死锁探测
        - 有死锁: rollback，abort
        - 无死锁: 等待tid COMMIT/ABORT，然后执行COMMIT/ABORT对应的操作，如下
      - COMMIT/ABORT: get出之前的值存放到origin_value中，change_id修改为当前id
    - 释放锁

    这样看起来貌似没有问题，问题出在哪里呢，就是在于，set之后将锁放了，这样不符合2pl的原则，应该**在事务中止或者提交的时候一并放锁**

    另一方面注意到，ydb_server含有一个lock_client，由于只有一个lock_client，lab2中的锁这里用到就相当于是假锁，所以我们需要再根据lab2的原理，在ydb_server中实现一个锁，参见acquire和release函数

    前面提到，无死锁的时候事务需要等待tid提交或者中止。最开始的时候我是使用condition variable来处理这个等待的过程，但是考虑到可能有多个事务同时等待这个事务，而signal一次只能唤醒一个并且可能错过，所以在实现上并不友好。
    于是我们在每个事务开始时初始化一个mutex并将其锁上，每个事务都拥有一个，参见map trans_mutex。这样一来，事务id等待事务tid时只需要调用

    > pthread_mutex_lock(&trans_mutex[tid]);

    当事务tid提交或者中止时unlock，即可达成预期的效果

至此 part2全部通过

## Part3 OCC

根据OCC协议的精神，我们选择这样的版本号控制的实现方式

- 为inode加上一个版本map ino_version。当inode被修改时，对应的version值会+1。

- set时直接将要写入的内容存在trans_write对应的位置上

- get时若trans_write中有相应的inode数据，直接从其中取得；否则调用ec获取，同时会读取version值并存在trans_read中对应的位置

- set的数据并没有直接写入持久化中，而是在提交事务时，需要验证成功后才能存入

- 事务提交时的验证，对于需要写入的数据，若trans_read留存的对应的version值与当前版本表中的version相等时才写入，否则事务中止

基于以上实现方式，很意外一次性通过了所有的测试

```sh
Start testing part1 : no transaction

start no transaction test-lab3-durability
[^_^] Pass test-lab3-durability
Passed part1

Finish testing part1 : no transaction

----------------------------------------

Start testing part2 : 2PL

start 2PL test-lab3-durability
[^_^] Pass test-lab3-durability
Passed part2 durability

start 2PL test-lab3-part2-3-basic
[^_^] Pass test-lab3-part2-3-basic
Passed part2 basic

start 2PL test-lab3-part2-a
[^_^] Pass test-lab3-part2-a
Passed part2 special a

start 2PL test-lab3-part2-b
[^_^] Pass test-lab3-part2-b
Passed part2 special b

start 2PL test-lab3-part2-3-complex
[^_^] Pass test-lab3-part2-3-complex
Passed part2 complex

Finish testing part2 : 2PL

----------------------------------------

Start testing part3 : OCC

start OCC test-lab3-durability
[^_^] Pass test-lab3-durability
Passed part3 durability

start OCC test-lab3-part2-3-basic
[^_^] Pass test-lab3-part2-3-basic
Passed part3 basic

start OCC test-lab3-part3-a
[^_^] Pass test-lab3-part3-a
Passed part3 special a

start OCC test-lab3-part3-b
[^_^] Pass test-lab3-part3-b
Passed part3 special b

start OCC test-lab3-part2-3-complex
[^_^] Pass test-lab3-part2-3-complex
Passed part3 complex

Finish testing part3 : OCC

----------------------------------------

Your passed 11/11 tests.
```
